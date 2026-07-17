// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_superNode_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_util.hpp>
#include <cstddef>
#include <cstdint>

namespace TISP {
namespace SuperNode {

#define NUM_IN_CHANNELS (1)
#define NUM_OUT_CHANNELS (1)

//! [supernode example]

template <typename dTypeIn, typename dTypeOut>
I2dToO2d<dTypeIn, dTypeOut>::I2dToO2d(std::unique_ptr<opVec> &&myOpVec,
                                      dTypeIn                 *pIn,
                                      dTypeOut                *pOut,
                                      dTypeIn                 *pInBlock,
                                      dTypeOut                *pOutBlock,
                                      size_t                   width,
                                      size_t                   height,
                                      size_t                   blockWidth,
                                      size_t                   blockHeight,
                                      size_t                   inPitch,
                                      size_t                   outPitch,
                                      size_t                   numChannels)
    : BaseSuperNode(numChannels, NUM_IN_CHANNELS, NUM_OUT_CHANNELS), _myOpVec(std::move(myOpVec)), _pIn(pIn),
      _pOut(pOut), _pInBlock(pInBlock), _pOutBlock(pOutBlock)

{

   /***********************/
   /* Configure input DMA */
   /***********************/

   // set transfer dimenssion structure
   dmaUtils<dTypeIn>::dmaAutoIncSetupXferPropIn2D(width, height, blockWidth, blockHeight, inPitch, outPitch,
                                                  &_transferDimIn);
   // assign channel number
   _chIdIn[0] = 0;

   /***********************/
   /* Configure output DMA */
   /***********************/

   // set transfer dimenssion structure
   dmaUtils<dTypeOut>::dmaAutoIncSetupXferPropOut2D(width, height, blockWidth, blockHeight, inPitch, outPitch,
                                                    &_transferDimOut);
   // assign channel number
   _chIdOut[0] = 1;

   _offset          = blockWidth * blockHeight;
   _pingPongFlagIn  = 0;
   _pingPongFlagOut = 0;

   _numBytesIn  = height * inPitch;
   _numBytesOut = height * outPitch;
}
//! [supernode example]

template <typename dTypeIn, typename dTypeOut> void I2dToO2d<dTypeIn, dTypeOut>::exec()
{

   uint32_t transferSize = DMAUTILSAUTOINC3D_SYNC_2D;

   // configure input channel
   uint32_t retVal = dmaUtils<dTypeIn>::configure_channel(
       _dmaUtilsContext, _chIdIn[0], _pTrMemCh, static_cast<uint8_t *>(static_cast<void *>(_pIn)),
       static_cast<uint8_t *>(static_cast<void *>(_pInBlock)), transferSize, &_transferDimIn);

   if (retVal != UDMA_SOK) {
      (void) printf("[Error] UDMA init failed!!\n");
   }

   // configure output channel
   retVal = dmaUtils<dTypeOut>::configure_channel(
       _dmaUtilsContext, _chIdOut[0], _pTrMemCh, static_cast<uint8_t *>(static_cast<void *>(_pOutBlock)),
       static_cast<uint8_t *>(static_cast<void *>(_pOut)), transferSize, &_transferDimOut);

   if (retVal != UDMA_SOK) {
      (void) printf("[Error] UDMA init failed!!\n");
   }

   uint32_t blockIdx     = 0;
   uint32_t firstTrigger = 0;

   // DMA trigger for pipe-up, out transfer is dummy and handled inside DMA
   // utility; trigger and wait are datatype independent so using float

   _blockCount = dmaUtils<float>::trigger(_dmaUtilsContext, _chIdIn, _numInChannels);
   _blockCount += 1;

   // Wait for previous transfer of in
   dmaUtils<float>::wait(_dmaUtilsContext, _chIdIn, _numInChannels);

   SetAddr_t addrIn;
   SetAddr_t addrOut;
   size_t    length      = _myOpVec->size();
   uint32_t  startNodeId = NodeProfiler::getStartNodeId();

   while (1) {

      if (firstTrigger != 0) {

         blockIdx = dmaUtils<float>::trigger(_dmaUtilsContext, _chIdOut, _numOutChannels);
      }

      // DMA trigger for next in buffer
      if (blockIdx != 1) {
         dmaUtils<float>::trigger(_dmaUtilsContext, _chIdIn, _numInChannels);
      }

      // Get the current node count to use as starting ID
      NodeProfiler::setStartNodeId(startNodeId);

      // Execute with profiling, using the current node count as starting ID
      execute::graph(_myOpVec);

      _pingPongFlagIn ^= 1;
      _pingPongFlagOut ^= 1;

      addrIn.src[0]  = static_cast<void *>(&_pInBlock[_offset * _pingPongFlagIn]);
      addrOut.dst[0] = static_cast<void *>(&_pOutBlock[_offset * _pingPongFlagOut]);

      (*_myOpVec)[0]->setAddr(addrIn, kAddrModeIn);
      (*_myOpVec)[length - 1]->setAddr(addrOut, kAddrModeOut);

      if (blockIdx != 1) {
         dmaUtils<float>::wait(_dmaUtilsContext, _chIdIn, _numInChannels);
      }

      // Wait for previous transfer out
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
   (void) CacheP_inv(_pOut, _numBytesOut, CacheP_TYPE_ALL);
#endif

   // Update the node count based on the number of nodes in this supernode
   NodeProfiler::setStartNodeId(startNodeId + _myOpVec->size());

   return;
}

} // namespace SuperNode
} // namespace TISP

// Explicit template instantiations (replacing macros to satisfy MISRA.DEFINE.NOPARS)
template class TISP::SuperNode::I2dToO2d<float, float>;
template class TISP::SuperNode::I2dToO2d<double, double>;
template class TISP::SuperNode::I2dToO2d<int8_t, int8_t>;
template class TISP::SuperNode::I2dToO2d<int16_t, int16_t>;
template class TISP::SuperNode::I2dToO2d<int32_t, int32_t>;
template class TISP::SuperNode::I2dToO2d<int64_t, int64_t>;
template class TISP::SuperNode::I2dToO2d<uint8_t, uint8_t>;
template class TISP::SuperNode::I2dToO2d<uint16_t, uint16_t>;
template class TISP::SuperNode::I2dToO2d<uint32_t, uint32_t>;
template class TISP::SuperNode::I2dToO2d<uint64_t, uint64_t>;
template class TISP::SuperNode::I2dToO2d<float, int16_t>;
template class TISP::SuperNode::I2dToO2d<float, int32_t>;
template class TISP::SuperNode::I2dToO2d<double, int32_t>;
template class TISP::SuperNode::I2dToO2d<double, int16_t>;
template class TISP::SuperNode::I2dToO2d<int16_t, float>;
template class TISP::SuperNode::I2dToO2d<int16_t, double>;
template class TISP::SuperNode::I2dToO2d<int32_t, float>;
template class TISP::SuperNode::I2dToO2d<int32_t, double>;
