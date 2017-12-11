#ifndef MACHINE_ONESIDED_H_
#define MACHINE_ONESIDED_H_

#include "mempool.h"

typedef struct _cmi_gni_ack_op {
  CmiRdmaAck * ack;
  gni_mem_handle_t mem_hndl;
} CmiGNIAckOp_t;

typedef struct _cmi_gni_rzv_rdma_op {
  int size;
  uint64_t remote_addr;
  gni_mem_handle_t mem_hndl;
} CmiGNIRzvRdmaOp_t;

typedef struct _cmi_gni_rzv_rdma {
  int numOps;
  int srcPE;
  CmiGNIRzvRdmaOp_t rdmaOp[0];
} CmiGNIRzvRdma_t;

typedef struct _cmi_gni_rzv_rdma_recv_op {
  int size;
  uint64_t remote_addr;
  uint64_t local_addr;
  gni_mem_handle_t local_mem_hndl;
  gni_mem_handle_t remote_mem_hndl;
  int opIndex;
  void * src_info;
} CmiGNIRzvRdmaRecvOp_t;


typedef struct _cmi_gni_rzv_rdma_recv {
  int numOps;
  int comOps;
  int destNode;
  int srcPE;
  void* msg;
  CmiGNIRzvRdmaRecvOp_t rdmaOp[0];
} CmiGNIRzvRdmaRecv_t;

void _initOnesided();

void  rdma_sendAck (CmiGNIRzvRdmaRecvOp_t* recvOpInfo, int src_pe);
void  rdma_sendMsgForPutCompletion (CmiGNIRzvRdmaRecv_t* recvInfo, int dest_pe);

void LrtsIssueRgets(void *recv, int pe);
void LrtsIssueRputs(void *recv, int pe);

int LrtsGetRdmaOpInfoSize(){
  return sizeof(CmiGNIRzvRdmaOp_t);
}
int LrtsGetRdmaGenInfoSize(){
  return sizeof(CmiGNIRzvRdma_t);
}
int LrtsGetRdmaInfoSize(int numOps){
  return sizeof(CmiGNIRzvRdma_t) + numOps * sizeof(CmiGNIRzvRdmaOp_t);
}

int LrtsGetRdmaOpRecvInfoSize(){
  return sizeof(CmiGNIRzvRdmaRecvOp_t);
}

int LrtsGetRdmaGenRecvInfoSize(){
  return sizeof(CmiGNIRzvRdmaRecv_t);
}

int LrtsGetRdmaRecvInfoSize(int numOps){
  return sizeof(CmiGNIRzvRdmaRecv_t) + numOps * sizeof(CmiGNIRzvRdmaRecvOp_t);
}

void LrtsSetRdmaRecvInfo(void *rdmaRecv, int numOps, void *msg, void *rdmaSend, int msgSize){
  CmiGNIRzvRdmaRecv_t *rdmaRecvInfo = (CmiGNIRzvRdmaRecv_t *)rdmaRecv;
  CmiGNIRzvRdma_t *rdmaSendInfo = (CmiGNIRzvRdma_t *)rdmaSend;

  rdmaRecvInfo->srcPE = rdmaSendInfo->srcPE;
  rdmaRecvInfo->destNode = CmiMyNode();
  rdmaRecvInfo->numOps = numOps;
  rdmaRecvInfo->comOps = 0;
  rdmaRecvInfo->msg = msg;
}

void LrtsSetRdmaRecvOpInfo(void *rdmaRecvOp, void *buffer, void *src_ref, int size, int opIndex, void *rdmaSend){
  CmiGNIRzvRdmaRecvOp_t *rdmaRecvOpInfo = (CmiGNIRzvRdmaRecvOp_t *)rdmaRecvOp;
  CmiGNIRzvRdma_t *rdmaSendInfo = (CmiGNIRzvRdma_t *)rdmaSend;

  rdmaRecvOpInfo->src_info = src_ref;
  rdmaRecvOpInfo->size = rdmaSendInfo->rdmaOp[opIndex].size;
  rdmaRecvOpInfo->opIndex = opIndex;
  rdmaRecvOpInfo->local_addr = (uint64_t)buffer;

  rdmaRecvOpInfo->remote_addr = rdmaSendInfo->rdmaOp[opIndex].remote_addr;
  rdmaRecvOpInfo->remote_mem_hndl = rdmaSendInfo->rdmaOp[opIndex].mem_hndl;
}

void LrtsSetRdmaInfo(void *dest, int destPE, int numOps){
  CmiGNIRzvRdma_t *rdma = (CmiGNIRzvRdma_t*)dest;
  rdma->srcPE = CmiMyPe();
  rdma->numOps = numOps;
}

void LrtsSetRdmaOpInfo(void *dest, const void *ptr, int size, void *ack, int destPE){
  gni_mem_handle_t mem_hndl;
  gni_return_t status = GNI_RC_SUCCESS;

  status = GNI_MemRegister(nic_hndl, (uint64_t)ptr,  (uint64_t)size, NULL,  GNI_MEM_READ_ONLY, -1, &mem_hndl);
  GNI_RC_CHECK("Error! Exceeded Allowed Pinned Memory Limit! GNI_MemRegister on Sender Buffer (source) Failed before sending metadata message", status);

  CmiGNIRzvRdmaOp_t *rdmaOp = (CmiGNIRzvRdmaOp_t *)dest;
  rdmaOp->remote_addr = (uint64_t)ptr;
  rdmaOp->size = size;
  rdmaOp->mem_hndl = mem_hndl;
}

void PumpOneSidedRDMATransactions(gni_cq_handle_t rdma_cq, CmiNodeLock rdma_cq_lock);


/* Support for Nocopy Direct API */

/* Type of RDMA transaction
   This is used in handling each type of RDMA transaction appropriately on completion
 */
enum CMK_RDMA_TYPE {
  INDIRECT_SEND=1,
  DIRECT_SEND_RECV,
  DIRECT_SEND_RECV_UNALIGNED
};

// Machine specific information for a nocopy pointer
typedef struct _cmi_gni_rzv_rdma_pointer {
  // memory handle for the destination buffer
  gni_mem_handle_t mem_hndl;
} CmiGNIRzvRdmaPtr_t;

/* Compiler checks to ensure that CMK_NOCOPY_DIRECT_BYTES in conv-common.h
 * is set to sizeof(CmiGNIRzvRdmaPtr_t). CMK_NOCOPY_DIRECT_BYTES is used in
 * ckrdma.h to reserve bytes for source or destination metadata info.           */
#define DUMB_STATIC_ASSERT(test) typedef char sizeCheckAssertion[(!!(test))*2-1]

/* Check the value of CMK_NOCOPY_DIRECT_BYTES if the compiler reports an
 * error with the message "the size of an array must be greater than zero" */
DUMB_STATIC_ASSERT(sizeof(CmiGNIRzvRdmaPtr_t) == CMK_NOCOPY_DIRECT_BYTES);

/* Machine specific metadata information required for a PUT operation
 * This structure is used for an unaligned GET, which uses PUT underneath
 */
typedef struct _cmi_gni_rzv_rdma_put_op {
  gni_mem_handle_t dest_mem_hndl;
  uint64_t dest_addr;
  gni_mem_handle_t src_mem_hndl;
  uint64_t src_addr;
  int destPe;
  int size;
  uint64_t ref;
} CmiGNIRzvRdmaPutOp_t;

/* Machine specific metadata information required to register a buffer and perform
 * an RDMA operation with a remote buffer. This metadata information is used to perform
 * registration and a PUT operation when the remote buffer wants to perform a GET with an
 * unregistered buffer. Similary, the metadata information is used to perform registration
 * and a GET operation when the remote buffer wants to perform a PUT with an unregistered
 * buffer.*/
typedef struct _cmi_gni_rzv_rdma_reverse_op {
  const void *destAddr;
  int destPe;
  int destMode;
  const void *srcAddr;
  int srcPe;
  int srcMode;

  gni_mem_handle_t rem_mem_hndl;
  int ackSize;
  int size;
} CmiGNIRzvRdmaReverseOp_t;

gni_mem_handle_t registerDirectMemory(const void *ptr, int size, unsigned short int mode) {
  CmiAssert(mode == GNI_MEM_READWRITE || mode == GNI_MEM_READ_ONLY);
  gni_mem_handle_t mem_hndl;
  gni_return_t status = GNI_RC_SUCCESS;
  status = GNI_MemRegister(nic_hndl, (uint64_t)ptr, (uint64_t)size, NULL, mode, -1, &mem_hndl);
  GNI_RC_CHECK("Error! Memory registration failed inside LrtsRegisterMemory!", status);
  return mem_hndl;
}

// Set the machine specific information for a nocopy source pointer
void LrtsSetRdmaSrcInfo(void *info, const void *ptr, int size, unsigned short int mode){

  gni_mem_handle_t mem_hndl;
  gni_return_t status = GNI_RC_SUCCESS;

  if(mode == CMK_BUFFER_PREREG && SIZEFIELD(ptr) < BIG_MSG) {
    // Allocation for CMK_BUFFER_PREREG happens through CmiAlloc, which is allocated out of a mempool
    if(IsMemHndlZero(GetMemHndl(ptr))) {
      // register it and get the info
      status = registerMemory(GetMempoolBlockPtr(ptr), GetMempoolsize(ptr), &(GetMemHndl(ptr)), NULL);
      if(status == GNI_RC_SUCCESS) {
        // registration successful, get memory handle
        mem_hndl = GetMemHndl(ptr);
      } else {
        GNI_RC_CHECK("Error! Source memory registration failed!", status);
      }
    }
    else {
      // get the handle
      mem_hndl = GetMemHndl(ptr);
    }
  } else {
    status = GNI_MemRegister(nic_hndl, (uint64_t)ptr, (uint64_t)size, NULL, GNI_MEM_READWRITE, -1, &mem_hndl);
    GNI_RC_CHECK("Error! Source memory registration failed!", status);
  }
  CmiGNIRzvRdmaPtr_t *rdmaSrc = (CmiGNIRzvRdmaPtr_t *)info;
  rdmaSrc->mem_hndl = mem_hndl;
}

// Set the machine specific information for a nocopy destination pointer
void LrtsSetRdmaDestInfo(void *info, const void *ptr, int size, unsigned short int mode){
  gni_mem_handle_t mem_hndl;
  gni_return_t status = GNI_RC_SUCCESS;

  if(mode == CMK_BUFFER_PREREG && SIZEFIELD(ptr) < BIG_MSG) {
    // Allocation for CMK_BUFFER_PREREG happens through CmiAlloc, which is allocated out of a mempool
    if(IsMemHndlZero(GetMemHndl(ptr))) {
      // register it and get the info
      status = registerMemory(GetMempoolBlockPtr(ptr), GetMempoolsize(ptr), &(GetMemHndl(ptr)), NULL);
      if(status == GNI_RC_SUCCESS) {
        // registration successful, get memory handle
        mem_hndl = GetMemHndl(ptr);
      } else {
        GNI_RC_CHECK("Error! Destination memory registration failed!", status);
      }
    }
    else {
      // get the handle
      mem_hndl = GetMemHndl(ptr);
    }
  } else {
    status = GNI_MemRegister(nic_hndl, (uint64_t)ptr, (uint64_t)size, NULL, GNI_MEM_READWRITE, -1, &mem_hndl);
    GNI_RC_CHECK("Error! Destination memory registration failed!", status);
  }
  CmiGNIRzvRdmaPtr_t *rdmaDest = (CmiGNIRzvRdmaPtr_t *)info;
  rdmaDest->mem_hndl = mem_hndl;
}

// Method performs RDMA operations
gni_return_t post_rdma(
  uint64_t remote_addr,
  gni_mem_handle_t remote_mem_hndl,
  uint64_t local_addr,
  gni_mem_handle_t local_mem_hndl,
  int length,
  uint64_t post_id,
  int destNode,
  int type,
  unsigned short int mode);

// Method deregisters local and remote memory handles
void DeregisterMemhandle(gni_mem_handle_t mem_hndl, int pe);
#endif /* end if for MACHINE_ONESIDED_H_ */
