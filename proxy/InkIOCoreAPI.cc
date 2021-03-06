/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/*
 * This file contains all the functions exported by the IOCore to the SDK.
 * Any IOCore symbol accessed by a plugin directly should be called in this
 * file to ensure that it gets exported as a global symbol in TS
 */

#include "libts.h"
#include "api/ts/InkAPIPrivateIOCore.h"
#if defined(solaris) && !defined(__GNUC__)
#include "P_EventSystem.h" // I_EventSystem.h
#include "P_Net.h"         // I_Net.h
#else
#include "I_EventSystem.h"
#include "I_Net.h"
#endif
#include "I_Cache.h"
#include "I_HostDB.h"

// This assert is for internal API use only.
#if TS_USE_FAST_SDK
#define sdk_assert(EX) (void)(EX)
#else
#define sdk_assert(EX)                                          \
  ( (void)((EX) ? (void)0 : _TSReleaseAssert(#EX, __FILE__, __LINE__)) )
#endif


TSReturnCode
sdk_sanity_check_mutex(TSMutex mutex)
{
  if (mutex == NULL)
    return TS_ERROR;

  ProxyMutex *mutexp = (ProxyMutex *) mutex;

  if (mutexp->m_refcount < 0)
    return TS_ERROR;
  if (mutexp->nthread_holding < 0)
    return TS_ERROR;

  return TS_SUCCESS;
}


TSReturnCode
sdk_sanity_check_hostlookup_structure(TSHostLookupResult data)
{
  if (data == NULL)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_iocore_structure(void *data)
{
  if (data == NULL)
    return TS_ERROR;

  return TS_SUCCESS;
}

// From InkAPI.cc
TSReturnCode sdk_sanity_check_continuation(TSCont cont);
TSReturnCode sdk_sanity_check_null_ptr(void *ptr);


////////////////////////////////////////////////////////////////////
//
// Threads
//
////////////////////////////////////////////////////////////////////
struct INKThreadInternal:public EThread
{
  INKThreadInternal()
    : EThread(DEDICATED, -1)
  {  }

  TSThreadFunc func;
  void *data;
};

static void *
ink_thread_trampoline(void *data)
{
  INKThreadInternal *thread;
  void *retval;

  thread = (INKThreadInternal *) data;
  thread->set_specific();
  retval = thread->func(thread->data);
  delete thread;

  return retval;
}

/*
 * INKqa12653. Return TSThread or NULL if error
 */
TSThread
TSThreadCreate(TSThreadFunc func, void *data)
{
  INKThreadInternal *thread;

  thread = new INKThreadInternal;

  ink_assert(thread->event_types == 0);

  thread->func = func;
  thread->data = data;

  if (!(ink_thread_create(ink_thread_trampoline, (void *)thread, 1))) {
    return (TSThread)NULL;
  }

  return (TSThread)thread;
}

TSThread
TSThreadInit()
{
  INKThreadInternal *thread;

  thread = new INKThreadInternal;

#ifdef DEBUG
  if (thread == NULL)
    return (TSThread)NULL;
#endif

  thread->set_specific();

  return reinterpret_cast<TSThread>(thread);
}

void
TSThreadDestroy(TSThread thread)
{
  sdk_assert(sdk_sanity_check_iocore_structure(thread) == TS_SUCCESS);

  INKThreadInternal *ithread = (INKThreadInternal *)thread;
  delete ithread;
}

TSThread
TSThreadSelf(void)
{
  TSThread ithread = (TSThread)this_ethread();
  return ithread;
}


////////////////////////////////////////////////////////////////////
//
// Mutexes
//
////////////////////////////////////////////////////////////////////
TSMutex
TSMutexCreate()
{
  ProxyMutex *mutexp = new_ProxyMutex();

  // TODO: Remove this when allocations can never fail.
  sdk_assert(sdk_sanity_check_mutex((TSMutex)mutexp) == TS_SUCCESS);

  return (TSMutex)mutexp;
}

/* The following two APIs are for Into work, actually, APIs of Mutex
   should allow plugins to manually increase or decrease the refcount
   of the mutex pointer, plugins may want more control of the creation
   and destroy of the mutex.*/
TSMutex
TSMutexCreateInternal()
{
  ProxyMutex *new_mutex = new_ProxyMutex();

  // TODO: Remove this when allocations can never fail.
  sdk_assert(sdk_sanity_check_mutex((TSMutex)new_mutex) == TS_SUCCESS);

  new_mutex->refcount_inc();
  return reinterpret_cast<TSMutex>(new_mutex);
}

int
TSMutexCheck(TSMutex mutex)
{
  ProxyMutex *mutexp = (ProxyMutex *)mutex;

  if (mutexp->m_refcount < 0)
    return -1;
  if (mutexp->nthread_holding < 0)
    return -1;
  return 1;
}

void
TSMutexLock(TSMutex mutexp)
{
  sdk_assert(sdk_sanity_check_mutex(mutexp) == TS_SUCCESS);
  MUTEX_TAKE_LOCK((ProxyMutex *)mutexp, this_ethread());
}


TSReturnCode
TSMutexLockTry(TSMutex mutexp)
{
  sdk_assert(sdk_sanity_check_mutex(mutexp) == TS_SUCCESS);
  return (MUTEX_TAKE_TRY_LOCK((ProxyMutex *)mutexp, this_ethread()) ? TS_SUCCESS : TS_ERROR);
}

void
TSMutexUnlock(TSMutex mutexp)
{
  sdk_assert(sdk_sanity_check_mutex(mutexp) == TS_SUCCESS);
  MUTEX_UNTAKE_LOCK((ProxyMutex *)mutexp, this_ethread());
}

/* VIOs */

void
TSVIOReenable(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  vio->reenable();
}

TSIOBuffer
TSVIOBufferGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return reinterpret_cast<TSIOBuffer>(vio->get_writer());
}

TSIOBufferReader
TSVIOReaderGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return reinterpret_cast<TSIOBufferReader>(vio->get_reader());
}

int64_t
TSVIONBytesGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return vio->nbytes;
}

void
TSVIONBytesSet(TSVIO viop, int64_t nbytes)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);
  sdk_assert(nbytes >= 0);

  VIO *vio = (VIO *)viop;
  vio->nbytes = nbytes;
}

int64_t
TSVIONDoneGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return vio->ndone;
}

void
TSVIONDoneSet(TSVIO viop, int64_t ndone)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);
  sdk_assert(ndone >= 0);

  VIO *vio = (VIO *)viop;
  vio->ndone = ndone;
}

int64_t
TSVIONTodoGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return vio->ntodo();
}

TSCont
TSVIOContGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return (TSCont) vio->_cont;
}

TSVConn
TSVIOVConnGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return (TSVConn)vio->vc_server;
}

TSMutex
TSVIOMutexGet(TSVIO viop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(viop) == TS_SUCCESS);

  VIO *vio = (VIO *)viop;
  return (TSMutex)((ProxyMutex *)vio->mutex);
}

/* High Resolution Time */

ink_hrtime
INKBasedTimeGet()
{
  return ink_get_based_hrtime();
}

/* UDP Connection Interface */

TSAction
INKUDPBind(TSCont contp, unsigned int ip, int port)
{  
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
    
  FORCE_PLUGIN_MUTEX(contp);

  struct sockaddr_in addr;
  ats_ip4_set(&addr, ip, htons(port));
  
  return reinterpret_cast<TSAction>(udpNet.UDPBind((Continuation *)contp, ats_ip_sa_cast(&addr), INK_ETHERNET_MTU_SIZE, INK_ETHERNET_MTU_SIZE));
}

TSAction
INKUDPSendTo(TSCont contp, INKUDPConn udp, unsigned int ip, int port, char *data, int64_t len)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);
  UDPPacket *packet = new_UDPPacket();
  UDPConnection *conn = (UDPConnection *)udp;

  ats_ip4_set(&packet->to, ip, htons(port));

  IOBufferBlock *blockp = new_IOBufferBlock();
  blockp->alloc(BUFFER_SIZE_INDEX_32K);

  if (len > index_to_buffer_size(BUFFER_SIZE_INDEX_32K)) {
    len = index_to_buffer_size(BUFFER_SIZE_INDEX_32K) - 1;
  }

  memcpy(blockp->start(), data, len);
  blockp->fill(len);

  packet->append_block((IOBufferBlock *)blockp);
  /* (Jinsheng 11/27/00) set connection twice which causes:
     FATAL: ../../../proxy/iocore/UDPPacket.h:136:
     failed assert `!m_conn` */

  /* packet->setConnection ((UDPConnection *)udp); */
  return reinterpret_cast<TSAction>(conn->send((Continuation *)contp, packet));
}


TSAction
INKUDPRecvFrom(TSCont contp, INKUDPConn udp)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);
  UDPConnection *conn = (UDPConnection *)udp;
  return reinterpret_cast<TSAction>(conn->recv((Continuation *)contp));
}

int
INKUDPConnFdGet(INKUDPConn udp)
{
  UDPConnection *conn = (UDPConnection *)udp;
  return conn->getFd();
}

/* UDP Packet */
INKUDPPacket
INKUDPPacketCreate()
{
  UDPPacket *packet = new_UDPPacket();
  return ((INKUDPPacket)packet);
}

TSIOBufferBlock
INKUDPPacketBufferBlockGet(INKUDPPacket packet)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)packet) == TS_SUCCESS);

  UDPPacket *p = (UDPPacket *)packet;
  return ((TSIOBufferBlock)p->getIOBlockChain());
}

unsigned int
INKUDPPacketFromAddressGet(INKUDPPacket packet)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)packet) == TS_SUCCESS);

  UDPPacket *p = (UDPPacket *)packet;
  return ats_ip4_addr_cast(&p->from);
}

int
INKUDPPacketFromPortGet(INKUDPPacket packet)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)packet) == TS_SUCCESS);

  UDPPacket *p = (UDPPacket *)packet;
  return ats_ip_port_host_order(&p->from);
}

INKUDPConn
INKUDPPacketConnGet(INKUDPPacket packet)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)packet) == TS_SUCCESS);

  UDPPacket *p = (UDPPacket *)packet;
  return ((INKUDPConn)p->getConnection());
}

void
INKUDPPacketDestroy(INKUDPPacket packet)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)packet) == TS_SUCCESS);

  UDPPacket *p = (UDPPacket *)packet;
  p->free();
}

/* Packet Queue */

INKUDPPacket
INKUDPPacketGet(INKUDPacketQueue queuep)
{
  if (queuep != NULL) {
    UDPPacket *packet;
    Queue<UDPPacket> *qp = (Queue<UDPPacket> *)queuep;

    packet = qp->pop();
    return (packet);
  } 

  return NULL;
}


/* Buffers */

TSIOBuffer
TSIOBufferCreate()
{
  MIOBuffer *b = new_empty_MIOBuffer();

  // TODO: Should remove this when memory allocations can't fail.
  sdk_assert(sdk_sanity_check_iocore_structure(b) == TS_SUCCESS);
  return reinterpret_cast<TSIOBuffer>(b);
}

TSIOBuffer
TSIOBufferSizedCreate(TSIOBufferSizeIndex index)
{
  sdk_assert((index >= TS_IOBUFFER_SIZE_INDEX_128) && (index <= TS_IOBUFFER_SIZE_INDEX_32K));

  MIOBuffer *b = new_MIOBuffer(index);

  // TODO: Should remove this when memory allocations can't fail.
  sdk_assert(sdk_sanity_check_iocore_structure(b) == TS_SUCCESS);
  return reinterpret_cast<TSIOBuffer>(b);
}

void
TSIOBufferDestroy(TSIOBuffer bufp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);
  free_MIOBuffer((MIOBuffer *)bufp);
}

TSIOBufferBlock
TSIOBufferStart(TSIOBuffer bufp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);

  MIOBuffer *b = (MIOBuffer *)bufp;
  IOBufferBlock *blk = b->get_current_block();

  if (!blk || (blk->write_avail() == 0))
    b->add_block();
  blk = b->get_current_block();

  // TODO: Remove when memory allocations can't fail.
  sdk_assert(sdk_sanity_check_null_ptr((void*)blk) == TS_SUCCESS);

  return (TSIOBufferBlock)blk;
}

int64_t
TSIOBufferCopy(TSIOBuffer bufp, TSIOBufferReader readerp, int64_t length, int64_t offset)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);
  sdk_assert((length >= 0) && (offset >= 0));

  MIOBuffer *b = (MIOBuffer *)bufp;
  IOBufferReader *r = (IOBufferReader *)readerp;

  return b->write(r, length, offset);
}

int64_t
TSIOBufferWrite(TSIOBuffer bufp, const void *buf, int64_t length)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)buf) == TS_SUCCESS);
  sdk_assert(length >= 0);

  MIOBuffer *b = (MIOBuffer *)bufp;
  return b->write(buf, length);
}

// not in SDK3.0
void
TSIOBufferReaderCopy(TSIOBufferReader readerp, const void *buf, int64_t length)
{
  IOBufferReader *r = (IOBufferReader *)readerp;
  r->memcpy(buf, length);
}

void
TSIOBufferProduce(TSIOBuffer bufp, int64_t nbytes)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);
  sdk_assert(nbytes >= 0);

  MIOBuffer *b = (MIOBuffer *)bufp;
  b->fill(nbytes);
}

// dev API, not exposed
void
TSIOBufferBlockDestroy(TSIOBufferBlock blockp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;
  blk->free();
}

TSIOBufferBlock
TSIOBufferBlockNext(TSIOBufferBlock blockp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;
  return (TSIOBufferBlock)((IOBufferBlock *)blk->next);
}

// dev API, not exposed
int64_t
TSIOBufferBlockDataSizeGet(TSIOBufferBlock blockp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;
  return (blk->read_avail());
}

const char *
TSIOBufferBlockReadStart(TSIOBufferBlock blockp, TSIOBufferReader readerp, int64_t *avail)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;
  IOBufferReader *reader = (IOBufferReader *)readerp;
  char *p;

  p = blk->start();
  if (avail)
    *avail = blk->read_avail();

  if (blk == reader->block) {
    p += reader->start_offset;
    if (avail) {
      *avail -= reader->start_offset;
      if (*avail < 0) {
        *avail = 0;
      }
    }
  }

  return (const char *)p;
}

int64_t
TSIOBufferBlockReadAvail(TSIOBufferBlock blockp, TSIOBufferReader readerp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;
  IOBufferReader *reader = (IOBufferReader *)readerp;
  int64_t avail;

  avail = blk->read_avail();

  if (blk == reader->block) {
    avail -= reader->start_offset;
    if (avail < 0) {
      avail = 0;
    }
  }

  return avail;
}

char *
TSIOBufferBlockWriteStart(TSIOBufferBlock blockp, int64_t *avail)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;

  if (avail)
    *avail = blk->write_avail();
  return blk->end();
}

int64_t
TSIOBufferBlockWriteAvail(TSIOBufferBlock blockp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(blockp) == TS_SUCCESS);

  IOBufferBlock *blk = (IOBufferBlock *)blockp;
  return blk->write_avail();
}

int64_t
TSIOBufferWaterMarkGet(TSIOBuffer bufp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);

  MIOBuffer *b = (MIOBuffer *)bufp;
  return b->water_mark;
}

void
TSIOBufferWaterMarkSet(TSIOBuffer bufp, int64_t water_mark)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);
  sdk_assert(water_mark >= 0);

  MIOBuffer *b = (MIOBuffer *)bufp;
  b->water_mark = water_mark;
}

TSIOBufferReader
TSIOBufferReaderAlloc(TSIOBuffer bufp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);

  MIOBuffer *b = (MIOBuffer *)bufp;
  TSIOBufferReader readerp = (TSIOBufferReader)b->alloc_reader();

  // TODO: Should remove this when memory allocation can't fail.
  sdk_assert(sdk_sanity_check_null_ptr((void*)readerp) == TS_SUCCESS);
  return readerp;
}

TSIOBufferReader
TSIOBufferReaderClone(TSIOBufferReader readerp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);

  IOBufferReader *r = (IOBufferReader *)readerp;
  return (TSIOBufferReader)r->clone();
}

void
TSIOBufferReaderFree(TSIOBufferReader readerp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);

  IOBufferReader *r = (IOBufferReader *)readerp;
  r->mbuf->dealloc_reader(r);
}

TSIOBufferBlock
TSIOBufferReaderStart(TSIOBufferReader readerp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);

  IOBufferReader *r = (IOBufferReader *)readerp;

  if (r->block != NULL)
    r->skip_empty_blocks();
  return reinterpret_cast<TSIOBufferBlock>(r->get_current_block());
}

void
TSIOBufferReaderConsume(TSIOBufferReader readerp, int64_t nbytes)
{
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);
  sdk_assert(nbytes >= 0);

  IOBufferReader *r = (IOBufferReader *)readerp;
  r->consume(nbytes);
}

int64_t
TSIOBufferReaderAvail(TSIOBufferReader readerp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);

  IOBufferReader *r = (IOBufferReader *)readerp;
  return r->read_avail();
}
