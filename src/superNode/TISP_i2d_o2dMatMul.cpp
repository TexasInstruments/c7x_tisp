// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_superNode_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_util.hpp>
#include <c7x.h>
#include <cstddef>
#include <cstdint>

namespace TISP {
namespace SuperNode {

#define NUM_IN_CHANNELS (2)
#define NUM_OUT_CHANNELS (1)

//! [supernode example]

template <typename dTypeIn, typename dTypeOut>
i2d_to_o2dMatMul<dTypeIn, dTypeOut>::i2d_to_o2dMatMul(std::unique_ptr<opVec> &&_myOpVec,
                                                      dTypeIn                 *_pInA,
                                                      dTypeIn                 *_pInB,
                                                      dTypeOut                *_pOut,
                                                      dTypeIn                 *_pInBlockA,
                                                      dTypeIn                 *_pInBlockB,
                                                      dTypeOut                *_pOutBlock,
                                                      size_t                   _widthA,
                                                      size_t                   _heightA,
                                                      size_t                   _widthB,
                                                      size_t                   _heightB,
                                                      size_t                   _blockWidthA,
                                                      size_t                   _blockHeightA,
                                                      size_t                   _blockWidthB,
                                                      size_t                   _blockHeightB,
                                                      size_t                   _inPitchA,
                                                      size_t                   _inPitchB,
                                                      size_t                   _outPitch,
                                                      size_t                   _numChannels)
    : BaseSuperNode(_numChannels, NUM_IN_CHANNELS, NUM_OUT_CHANNELS), myOpVec(std::move(_myOpVec)), pInA(_pInA),
      pInB(_pInB), pOut(_pOut), pInBlockA(_pInBlockA), pInBlockB(_pInBlockB), pOutBlock(_pOutBlock),
      blockWidthA(_blockWidthA), blockHeightA(_blockHeightA), blockWidthB(_blockWidthB), blockHeightB(_blockHeightB),
      widthA(_widthA), heightA(_heightA), widthB(_widthB), heightB(_heightB)

{

   /***********************/
   /* Configure input DMA */
   /***********************/

   // set transfer dimenssion structure
   dmaUtils<dTypeIn>::dmaAutoIncSetupXferPropIn2D(_widthA, _heightA, _blockWidthA, _blockHeightA, _inPitchA, _outPitch,
                                                  &transferDimInA);
   // assign channel number
   _chIdIn[0] = 0;

   dmaUtils<dTypeIn>::dmaAutoIncSetupXferPropIn2D(_widthB, _heightB, _blockWidthB, _blockHeightB, _inPitchB, _outPitch,
                                                  &transferDimInB);
   _chIdIn[1] = 1;

   /***********************/
   /* Configure output DMA */
   /***********************/

   // set transfer dimenssion structure
   dmaUtils<dTypeOut>::dmaAutoIncSetupXferPropOut2D(_widthB, _heightA, _blockWidthB, _blockHeightA, _inPitchB,
                                                    _outPitch, &transferDimOut);
   // assign channel number
   _chIdOut[0] = 2;

   offsetA   = blockWidthA * blockHeightA;
   offsetB   = blockWidthB * blockHeightB;
   offsetOut = blockWidthB * blockHeightA;

   pingPongFlagInA = 1;
   pingPongFlagInB = 1;
   pingPongFlagOut = 1;

   _numBytesInA = heightA * _inPitchA;
   _numBytesInB = heightB * _inPitchB;
   _numBytesOut = heightB * _outPitch;
}
//! [supernode example]

template <typename dTypeIn, typename dTypeOut> void i2d_to_o2dMatMul<dTypeIn, dTypeOut>::exec()
{
   // Reset node counter at the beginning of SuperNode execution
   _nodesAlreadyCounted = false;

   uint32_t transferSize = DMAUTILSAUTOINC3D_SYNC_2D;

   // configure input channel
   uint32_t retVal = dmaUtils<dTypeIn>::configure_channel(
       _dmaUtilsContext, _chIdIn[0], _pTrMemCh, static_cast<uint8_t *>(static_cast<void *>(pInA)),
       static_cast<uint8_t *>(static_cast<void *>(pInBlockA)), transferSize, &transferDimInA);

   if (retVal != UDMA_SOK) {
      (void) printf("[Error] UDMA init failed!!\n");
   }

   retVal = dmaUtils<dTypeIn>::configure_channel(
       _dmaUtilsContext, _chIdIn[1], _pTrMemCh, static_cast<uint8_t *>(static_cast<void *>(pInB)),
       static_cast<uint8_t *>(static_cast<void *>(pInBlockB)), transferSize, &transferDimInB);

   if (retVal != UDMA_SOK) {
      (void) printf("[Error] UDMA init failed!!\n");
   }

   // configure output channel
   retVal = dmaUtils<dTypeOut>::configure_channel(
       _dmaUtilsContext, _chIdOut[0], _pTrMemCh, static_cast<uint8_t *>(static_cast<void *>(pOutBlock)),
       static_cast<uint8_t *>(static_cast<void *>(pOut)), transferSize, &transferDimOut);

   if (retVal != UDMA_SOK) {
      (void) printf("[Error] UDMA init failed!!\n");
   }

   SetAddr_t addrIn;
   SetAddr_t addrOut;
   size_t    length = myOpVec->size();

   dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdIn, 0);
   dmaUtils<float>::wait_single(_dmaUtilsContext, _chIdIn, 0);

   for (size_t blockIdx_A = 0; blockIdx_A < heightA / blockHeightA; blockIdx_A++) {
      if (blockIdx_A != 0) {
         dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdOut, 0);
      }

      if (blockIdx_A != heightA / blockHeightA - 1) {
         dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdIn, 0);
      }

      pingPongFlagInA ^= 1;
      addrIn.src[0] = static_cast<void *>(&pInBlockA[offsetA * pingPongFlagInA]); // NOLINT(MISRA.PTR.ARITH)

      retVal = dmaUtils<dTypeIn>::configure_channel(
          _dmaUtilsContext, _chIdIn[1], _pTrMemCh, static_cast<uint8_t *>(static_cast<void *>(pInB)),
          static_cast<uint8_t *>(static_cast<void *>(pInBlockB)), transferSize, &transferDimInB);

      if (retVal != UDMA_SOK) {
         (void) printf("[Error] UDMA init failed!!\n");
      }

      pingPongFlagInB = 1;
      pingPongFlagOut = 1;

      dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdIn, 1);
      dmaUtils<float>::wait_single(_dmaUtilsContext, _chIdIn, 1);

      for (size_t blockIdx_B = 0; blockIdx_B < widthB / blockWidthB; blockIdx_B++) {

         if (blockIdx_B != 0) {
            dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdOut, 0);
         }

         if (blockIdx_B != widthB / blockWidthB - 1) {
            dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdIn, 1);
         }

         pingPongFlagInB ^= 1;
         pingPongFlagOut ^= 1;

         addrIn.src[1]  = static_cast<void *>(&pInBlockB[offsetB * pingPongFlagInB]);   // NOLINT(MISRA.PTR.ARITH)
         addrOut.dst[0] = static_cast<void *>(&pOutBlock[offsetOut * pingPongFlagOut]); // NOLINT(MISRA.PTR.ARITH)

         (*myOpVec)[0]->setAddr(addrIn, kAddrModeIn);
         (*myOpVec)[length - 1]->setAddr(addrOut, kAddrModeOut);

         // Execute with profiling
         for (auto const &op : *myOpVec) {
            NodeProfiler::startNode();
            op->exec();
            NodeProfiler::stopNode();
         }

         if (blockIdx_B != widthB / blockWidthB - 1) {
            dmaUtils<float>::wait_single(_dmaUtilsContext, _chIdIn, 1);
         }

         if (blockIdx_B != 0) {
            dmaUtils<float>::wait_single(_dmaUtilsContext, _chIdOut, 0);
         }
      }

      if (blockIdx_A != heightA / blockHeightA - 1) {
         dmaUtils<float>::wait_single(_dmaUtilsContext, _chIdIn, 0);
      }
   }

   dmaUtils<float>::trigger_single(_dmaUtilsContext, _chIdOut, 0);

   // Need to wait for last out transfer
   dmaUtils<float>::wait_single(_dmaUtilsContext, _chIdOut, 0);

#if !defined(HOST_EMULATION)
   (void) CacheP_inv(pOut, _numBytesOut, CacheP_TYPE_ALL);
#endif

   // Update the node count based on the number of nodes in this supernode
   NodeProfiler::setStartNodeId(NodeProfiler::getStartNodeId() + myOpVec->size());

   return;
}

template class i2d_to_o2dMatMul<float, float>;

} // namespace SuperNode
} // namespace TISP
