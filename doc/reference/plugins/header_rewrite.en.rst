.. _header_rewrite-plugin

Header Rewrite Plugin
*********************

.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at
 
   http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing,
  software distributed under the License is distributed on an
  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  KIND, either express or implied.  See the License for the
  specific language governing permissions and limitations
  under the License.


This is a plugin for Apache Traffic Server that allows you to
modify various headers based on defined rules (operations) on a request or
response. Currently, only one operation is supported.

Using the plugin
----------------

This plugin can be used as both a global plugin, enabled in plugin.config::

  header_rewrite.so config_file_1.conf config_file_2.conf ...

These are global rules, which would apply to all requests. Another option is
to use per remap rules in remap.config::

  map http://a http://b @plugin=header_rewrite.so @pparam=rules1.conf ...

In the second example, hooks which are not to be executed during the remap
phase (the default) causes a transaction hook to be instantiated and used
at a later time. This allows you to setup e.g. a rule that gets executed
during the origin response header parsing, using READ_RESPONSE_HDR_HOOK.

Operators
---------

The following operators are available::

  rm-header header-name                 [flags]
  add-header header <value>             [flags]
  set-status <status-code>              [flags]
  set-status-reason <value>             [flags]
  set-config config <value>             [flags]
  no-op                                 [flags]
  counter counter-name                  [flags]

The following operator(s) currently only works when instantiating the
plugin as a remap plugin::

  set-destination [qual] value

Where qual is one of the support URL qualifiers::

  HOST
  PORT
  PATH
  QUERY

For example (as a remap rule)::

  cond %{HEADER:X-Mobile} = "foo"
  set-destination HOST foo.mobile.bar.com [L]

Operator flags
--------------
::

  [L]   Last rule, do not continue

Variable expansion
------------------
Currently only limited variable expansion is supported in add-header. Supported
substitutions include::

  %<proto>      Protocol
  %<port>       Port
  %<chi>        Client IP
  %<cqhl>       Client request length
  %<cqhm>       Client HTTP method
  %<cquup>      Client unmapped URI

Conditions
----------
The conditions are used as qualifiers: The operators specified will
only be evaluated if the condition(s) are met::

  cond %{STATUS} operand                        [flags]
  cond %{RANDOM:nn} operand                     [flags]
  cond %{ACCESS:file}                           [flags]
  cond %{TRUE}                                  [flags]
  cond %{FALSE}                                 [flags]
  cond %{HEADER:header-name} operand            [flags]
  cond %{COOKIE:cookie-name} operand            [flags]
  cond %{CLIENT-HEADER:header-name} operand     [flags]
  cond %{PROTOCOL} operand                      [flags]
  cond %{PORT} operand                          [flags]
  cond %{HOST} operand                          [flags]
  cond %{TOHOST} operand                        [false]
  cond %{FROMHOST} operand                      [false]
  cond %{PATH} operand                          [false]
  cond %{PARAMS} operand                        [false]
  cond %{QUERY} operand                         [false]

The difference between HEADER and CLIENT-HEADER is that HEADER adapts to the
hook it's running in, whereas CLIENT-HEADER always applies to the client
request header. The %{TRUE} condition is also the default condition if no
other conditions are specified.

These conditions have to be first in a ruleset, and you can only have one in
each rule. This implies that a new hook condition starts a new rule as well.::

  cond %{READ_RESPONSE_HDR_HOOK}   (this is the default hook)
  cond %{READ_REQUEST_HDR_HOOK}
  cond %{SEND_REQUEST_HDR_HOOK}
  cond %{SEND_RESPONSE_HDR_HOOK}

For remap.config plugin instanations, the default hook is named
REMAP_PSEUDO_HOOK. This can be useful if you are mixing other hooks in a
configuration, but being the default it is also optional.

---------------
Condition flags
---------------
::

  [NC]  Not case sensitive condition (when applicable)
  [AND] AND with next condition (default)
  [OR]  OR with next condition
  [NOT] Invert this condition

Operands to conditions
----------------------
::

  /string/  # regular expression
  <string   # lexically lower
  >string   # lexically greater
  =string   # lexically equal

The absence of a "matcher" means value exists).

Values
------
Setting e.g. a header with a value can take the following formats:

- Any of the cond definitions, that extracts a value from the request
- $N 0 <= N <= 9, as grouped in a regular expression
- string (which can contain the above)
- null

Examples
--------
::

  cond %{HEADER:X-Y-Foobar}
  cond %{COOKIE:X-DC}  =DC1
  add-header X-Y-Fiefum %{HEADER:X-Y-Foobar}
  add-header X-Forwarded-For %<chi>
  rm-header X-Y-Foobar
  rm-header Set-Cookie
  counter plugin.header_rewrite.x-y-foobar-dc1
  cond %{HEADER:X-Y-Foobar} "Some string" [AND,NC]
