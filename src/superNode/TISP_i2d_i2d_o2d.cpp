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
I2dI2dToO2d<dTypeIn, dTypeOut>::I2dI2dToO2d(std::unique_ptr<opVec> &&_myOpVec,
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
                                            size_t                   _widthOut,
                                            size_t                   _heightOut,
                                            size_t                   _blockWidthA,
                                            size_t                   _blockHeightA,
                                            size_t                   _blockWidthB,
                                            size_t                   _blockHeightB,
                                            size_t                   _blockWidthOut,
                                            size_t                   _blockHeightOut,
                                            size_t                   _inPitchA,
                                            size_t                   _inPitchB,
                                            size_t                   _outPitch,
                                            size_t                   _numChannels)
    : BaseSuperNode(_numChannels, NUM_IN_CHANNELS, NUM_OUT_CHANNELS), myOpVec(std::move(_myOpVec)), pInA(_pInA),
      pInB(_pInB), pOut(_pOut), pInBlockA(_pInBlockA), pInBlockB(_pInBlockB), pOutBlock(_pOutBlock),
      blockWidthA(_blockWidthA), blockHeightA(_blockHeightA), blockWidthB(_blockWidthB), blockHeightB(_blockHeightB),
      blockWidthOut(_blockWidthOut), blockHeightOut(_blockHeightOut), widthA(_widthA), heightA(_heightA),
      widthB(_widthB), heightB(_heightB), widthOut(_widthOut), heightOut(_heightOut)

{

   /***********************/
   /* Configure input DMA */
   /***********************/

   // set transfer dimenssion structure
   dmaUtils<dTypeIn>::dmaAutoIncSetupXferPropIn2D(_widthA, _heightA, _blockWidthA, _blockHeightA, _inPitchA, _outPitch,
                                                  &transferDimInA);
   // assign channel number
   _chIdIn[0] = 0;

   // Regular configuration for multi-element input
   dmaUtils<dTypeIn>::dmaAutoIncSetupXferPropIn2D(_widthB, _heightB, _blockWidthB, _blockHeightB, _inPitchB, _outPitch,
                                                  &transferDimInB);

   _chIdIn[1] = 1;

   /***********************/
   /* Configure output DMA */
   /***********************/

   // set transfer dimenssion structure
   dmaUtils<dTypeOut>::dmaAutoIncSetupXferPropOut2D(_widthOut, _heightOut, _blockWidthOut, _blockHeightOut, _inPitchA,
                                                    _outPitch, &transferDimOut);
   // assign channel number
   _chIdOut[0] = 2;

   offsetA   = blockWidthA * blockHeightA;
   offsetB   = blockWidthB * blockHeightB;
   offsetOut = blockWidthOut * blockHeightOut;

   pingPongFlagInA = 0;
   pingPongFlagInB = 0;
   pingPongFlagOut = 0;

   _numBytesInA = heightA * _inPitchA;
   _numBytesInB = heightB * _inPitchB;
   _numBytesOut = heightOut * _outPitch;
}
//! [supernode example]

template <typename dTypeIn, typename dTypeOut> void I2dI2dToO2d<dTypeIn, dTypeOut>::exec()
{

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

   uint32_t blockIdx     = 0;
   uint32_t firstTrigger = 0;

   SetAddr_t addrIn;
   SetAddr_t addrOut;
   size_t    length = myOpVec->size();

   _blockCount = dmaUtils<float>::trigger(_dmaUtilsContext, _chIdIn, _numInChannels);
   _blockCount += 1;

   // Wait for previous transfer of in
   dmaUtils<float>::wait(_dmaUtilsContext, _chIdIn, _numInChannels);
   while (1) {
      // Now trigger the output DMA transfer (if needed)
      if (firstTrigger != 0) {
         blockIdx = dmaUtils<float>::trigger(_dmaUtilsContext, _chIdOut, _numOutChannels);
      }
      if (blockIdx != 1) {
         dmaUtils<float>::trigger(_dmaUtilsContext, _chIdIn, _numInChannels);
      }
      // Execute the kernel operation
      execute::graph(myOpVec);

      // Trigger input DMA first, before any output actions
      // Toggle the ping-pong flags to alternate between blocks
      pingPongFlagInA ^= 1;
      pingPongFlagInB ^= 1;
      pingPongFlagOut ^= 1;

      // Set addresses for the next blocks based on ping-pong flags
      addrIn.src[0]  = static_cast<void *>(&pInBlockA[offsetA * pingPongFlagInA]);   // NOLINT(MISRA.PTR.ARITH)
      addrIn.src[1]  = static_cast<void *>(&pInBlockB[offsetB * pingPongFlagInB]);   // NOLINT(MISRA.PTR.ARITH)
      addrOut.dst[0] = static_cast<void *>(&pOutBlock[offsetOut * pingPongFlagOut]); // NOLINT(MISRA.PTR.ARITH)

      (*myOpVec)[0]->setAddr(addrIn, kAddrModeIn);
      (*myOpVec)[length - 1]->setAddr(addrOut, kAddrModeOut);
      // Wait for the next input DMA transfer (if necessary)
      if (blockIdx != 1) {
         dmaUtils<float>::wait(_dmaUtilsContext, _chIdIn, _numInChannels);
      }

      // Wait for the output DMA transfer to complete before continuing to the next iteration
      if (firstTrigger != 0) {
         dmaUtils<float>::wait(_dmaUtilsContext, _chIdOut, _numOutChannels);
      }
      else {
         firstTrigger = 1;
      }
      if (blockIdx == 1) {
         break;
      }
   }
   dmaUtils<float>::trigger(_dmaUtilsContext, _chIdOut, _numOutChannels);

   // Need to wait for last out transfer
   dmaUtils<float>::wait(_dmaUtilsContext, _chIdOut, _numOutChannels);

#if !defined(HOST_EMULATION)
   (void) CacheP_inv(pOut, _numBytesOut, CacheP_TYPE_ALL);
#endif

   return;
}

} // namespace SuperNode
} // namespace TISP

// Explicit template instantiations (replacing macro to satisfy MISRA.DEFINE.NOPARS)
template class TISP::SuperNode::I2dI2dToO2d<float, float>;
template class TISP::SuperNode::I2dI2dToO2d<double, double>;
template class TISP::SuperNode::I2dI2dToO2d<int16_t, int16_t>;
template class TISP::SuperNode::I2dI2dToO2d<uint16_t, uint16_t>;
template class TISP::SuperNode::I2dI2dToO2d<int32_t, int32_t>;
template class TISP::SuperNode::I2dI2dToO2d<uint32_t, uint32_t>;
template class TISP::SuperNode::I2dI2dToO2d<uint8_t, uint8_t>;
template class TISP::SuperNode::I2dI2dToO2d<uint64_t, uint64_t>;
