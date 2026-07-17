// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_virtualOp.hpp"

#if defined(HOST_EMULATION)
#include <drivers/dmautils/udma_standalone/udma.h>
#endif

#include <drivers/dmautils/dmautils.h>
#include <drivers/dmautils/src/dmautils_autoincrement_3d_priv.h>

#include <vector>

namespace TISP {
namespace SuperNode {

// Define a constant for maximum source addresses for a given node
constexpr int kMaxDmaChannels = 4;

/**
 *  @ingroup SUPERNODE
 */

/**@{*/

/**
 *  \class BaseSuperNode
 *  @brief Provides the base class for Supernode. Contains member functions to create dma context and allocate memory
 *         required to store DRU-related parameters.
 */

class BaseSuperNode {

 protected:
   int32_t  _chIdIn[kMaxDmaChannels];  // Input channel IDs
   int32_t  _chIdOut[kMaxDmaChannels]; // Output channel IDs
   uint8_t *_dmaUtilsContext;
   uint8_t *_pTrMemCh[kMaxDmaChannels];

   // Profiling tracking
   bool _nodesAlreadyCounted = false;

   size_t _numChannels;
   size_t _numInChannels;
   size_t _numOutChannels;

   /* Udma_DrvHandle appDrvHandle; */

#if defined(HOST_EMULATION)
   Udma_DrvObj _udmaDrvObj;
#else
   Udma_DrvObjectInt _udmaDrvObj;
#endif

   size_t _numBytesIn;  // needed for cache wb
   size_t _numBytesOut; // needed for cache inv

   /**
    * @brief BaseSuperNode constructor that is called to create the supernode with appropriate parameters. Typically,
    * used by classes derived from the @ref SuperNode as part of their constructor c
    * @param [in] numChannels    : Total number of DMA channels to be used by the supernode
    * @param [in] numInChannels  : Total number of input DMA channels
    * @param [in] numOutChannels : Total number of output DMA channels
    */
   BaseSuperNode(size_t _numChannels, size_t _numInChannels, size_t _numOutChannels);

 public:
   /**
   @brief This function is implemented by classes derived from @ref TISP::SuperNode
 */

   virtual void exec() = 0;

   /**
     @brief This function returns a reference to the operator vector (opVec) contained within the supernode.
     This is used for profiling and other operations that need access to the individual nodes.
     * @return Reference to the operator vector.
   */
   virtual const opVec &getOpVec() const = 0;

   /**
     @brief This function returns the block count for the supernode.
     This is used for profiling and other operations that need to know how many blocks were processed.
     * @return The block count.
   */
   virtual size_t getBlockCount() const = 0;

   /**
     @brief This function executes a given supernode. It contains the boiler plate template for  block-based DMA and
     compute flow. The function calls the @ref execute::graph function which in turn executes the correct compute flow.
     For data movement, appropriate APIs are called around the @ref execute::graph function. Typical execute case of
     most superNodes variants (derived classes)
     * @return None.
   */

   void execDefault(std::unique_ptr<opVec> &myOpVec);

   virtual ~BaseSuperNode()
   {
      free(_dmaUtilsContext);
      for (uint32_t ch = 0; ch < _numChannels; ch++) {
         free(_pTrMemCh[ch]);
      }
      Udma_deinit((Udma_DrvHandle) (&_udmaDrvObj));
   }

#if defined(HOST_EMULATION)
   Udma_DrvHandle setUdmaDrvHandle();
#else
   Udma_DrvHandle    setUdmaDrvHandle();
#endif
};

typedef std::vector<std::unique_ptr<TISP::SuperNode::BaseSuperNode>> superNodeVec;

/** @} */

/**
 *  @ingroup  SUPERNODE
 */

/**@{*/

/**
 *  \class I2dToO2d
 *  @brief Provides the supernode style when input data is transferred in 2D form and output is transferred out in 2D
 *         form.
 */

template <typename dTypeIn, typename dTypeOut> class I2dToO2d final : public BaseSuperNode {

 private:
   std::unique_ptr<opVec> _myOpVec;

   dTypeIn  *_pIn       = nullptr;
   dTypeOut *_pOut      = nullptr;
   dTypeIn  *_pInBlock  = nullptr;
   dTypeOut *_pOutBlock = nullptr;

   DmaUtilsAutoInc3d_TransferDim _transferDimIn;  /*!< Structure to hold transfer properties of input DMA channel */
   DmaUtilsAutoInc3d_TransferDim _transferDimOut; /*!< Structure to hold transfer properties of output DMA channel */

   bool   _pingPongFlagIn;
   bool   _pingPongFlagOut;
   size_t _offset;
   size_t _blockCount = 0; // Track the block count for profiling

 public:
   /**
    * @brief Supernode constructor to create a 2D input to 2D output DMA supernode.
    * @param [in]  &myOpVec    : Operator vector with list of nodes to be executed inside the supernode
    * @param [in]  *pIn        : Input pointer in DDR space
    * @param [out] *pOut       : Output pointer in DDR space
    * @param [in]  *pInBlock   : Input pointer in L2 space
    * @param [out] *pOutBlock  : Output pointer in L2 space
    * @param [in]  width       : Width of 2D block in DDR
    * @param [in]  height      : height of 2D block in DDR
    * @param [in]  blockWidth  : Width of 2D block in L2
    * @param [in]  blockHeight : Height of 2D block in L2
    * @param [in]  inPitch     : Input pitch in DDR
    * @param [in]  outPitch    : Output pitch in DDR
    * @param [in]  numChannels : Total number of DMA channels to be used by the supernode (always 2)
    *
    */

   I2dToO2d(std::unique_ptr<opVec> &&myOpVec,
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
            size_t                   numChannels);

   void exec() override;

   /**
     @brief Implementation of the getOpVec function for I2dToO2d.
     * @return Reference to the operator vector.
   */
   const opVec &getOpVec() const override { return *_myOpVec; }

   /**
     @brief Implementation of the getBlockCount function for I2dToO2d.
     * @return The block count.
   */
   size_t getBlockCount() const override { return _blockCount; }
};

/**
 *  \class I2dToO2dG
 *  @brief Provides the supernode style when input data is transferred in 2D form and output is transferred out in 2D
 *         form.
 * @details Unlike the input, the output buffer may have different dimensions
 *          (e.g., different width, height). This allows the
 *          kernel to support cases where the dimensions of input and output data
 *          are not the same.
 */

template <typename dTypeIn, typename dTypeOut> class I2dToO2dDiffDims final : public BaseSuperNode {

 private:
   std::unique_ptr<opVec> _myOpVec;

   dTypeIn  *_pIn;
   dTypeOut *_pOut;
   dTypeIn  *_pInBlock;

   dTypeOut *_pOutBlock;

   DmaUtilsAutoInc3d_TransferDim _transferDimIn;  /*!< Structure to hold transfer properties of input DMA channel */
   DmaUtilsAutoInc3d_TransferDim _transferDimOut; /*!< Structure to hold transfer properties of output DMA channel */

   bool   _pingPongFlagIn;
   bool   _pingPongFlagOut;
   size_t _offset;
   size_t _offsetB;

   size_t _blockCount = 0; // Track the block count for profiling

 public:
   /**
    * @brief Supernode constructor to create a 2D input to 2D output DMA supernode.
    * @param [in]  &myOpVec       : Operator vector with list of nodes to be executed inside the supernode
    * @param [in]  *pIn           : Input pointer in DDR space
    * @param [out] *pOut          : Output pointer in DDR space
    * @param [in]  *pInBlock      : Input pointer in L2 space
    * @param [out] *pOutBlock     : Output pointer in L2 space
    * @param [in]  widthIn        : Width of 2D input block in DDR
    * @param [in]  heightIn       : height of 2D input block in DDR
    * @param [in]  widthOut       : Width of output 2D block in DDR
    * @param [in]  heightOut      : height of output 2D block in DDR
    * @param [in]  blockWidthIn   : Width of input 2D block in L2
    * @param [in]  blockHeightIn  : Height of input 2D block in L2
    * @param [in]  blockWidthOut  : Width of output 2D block in L2
    * @param [in]  blockHeightOut : Height of output 2D block in L2
    * @param [in]  inPitch        : Input pitch in DDR
    * @param [in]  outPitch       : Output pitch in DDR
    * @param [in]  numChannels    : Total number of DMA channels to be used by the supernode (always 2)
    *
    */

   I2dToO2dDiffDims(std::unique_ptr<opVec> &&myOpVec,
                    dTypeIn                 *pIn,
                    dTypeOut                *pOut,
                    dTypeIn                 *pInBlock,
                    dTypeOut                *pOutBlock,
                    size_t                   widthIn,
                    size_t                   heightIn,
                    size_t                   widthOut,
                    size_t                   heightOut,
                    size_t                   blockWidthIn,
                    size_t                   blockHeightIn,
                    size_t                   blockWidthOut,
                    size_t                   blockHeightOut,
                    size_t                   inPitch,
                    size_t                   outPitch,
                    size_t                   numChannels);

   void exec() override;

   /**
     @brief Implementation of the getOpVec function for I2dToO2dDiffDims.
     * @return Reference to the operator vector.
   */
   const opVec &getOpVec() const override { return *_myOpVec; }

   /**
     @brief Implementation of the getBlockCount function for I2dToO2dDiffDims.
     * @return The block count.
   */
   size_t getBlockCount() const override { return _blockCount; }
};

template <typename dTypeIn, typename dTypeOut> class i2d_to_o2dMatMul final : public BaseSuperNode {

 private:
   std::unique_ptr<opVec> myOpVec;

   dTypeIn  *pInA = nullptr;
   dTypeIn  *pInB = nullptr;
   dTypeOut *pOut = nullptr;

   dTypeIn  *pInBlockA = nullptr;
   dTypeIn  *pInBlockB = nullptr;
   dTypeOut *pOutBlock = nullptr;

   size_t blockWidthA;
   size_t blockHeightA;
   size_t blockWidthB;
   size_t blockHeightB;

   size_t widthA;
   size_t heightA;
   size_t widthB;
   size_t heightB;

   DmaUtilsAutoInc3d_TransferDim transferDimInA; /*!< Structure to hold transfer properties of input DMA channel */
   DmaUtilsAutoInc3d_TransferDim transferDimInB;
   DmaUtilsAutoInc3d_TransferDim transferDimOut; /*!< Structure to hold transfer properties of output DMA channel */

   bool   pingPongFlagInA;
   bool   pingPongFlagInB;
   bool   pingPongFlagOut;
   size_t offsetA;
   size_t offsetB;
   size_t offsetOut;

   size_t _numBytesInA; // needed for cache wb
   size_t _numBytesInB; // needed for cache wb
   size_t _numBytesOut; // needed for cache inv

   size_t _blockCount = 0; // Track the block count for profiling

 public:
   /**
    * @brief Supernode constructor to create a 2D input to 2D output DMA supernode.
    * @param [in]  &_myOpVec    : Operator vector with list of nodes to be executed inside the supernode
    * @param [in]  *_pInA        : Input pointer for matrix A in DDR space
    * @param [in]  *_pInB        : Input pointer for matrix B in DDR space
    * @param [out] *_pOut       : Output pointer in DDR space
    * @param [in]  *_pInBlockA  : Input pointer for matrix A in L2 space
    * @param [in]  *_pInBlockB  : Input pointer for matrix B in L2 space
    * @param [out] *_pOutBlock  : Output pointer in L2 space
    * @param [in]  _widthA       : Width of 2D block for matrix A in DDR
    * @param [in]  _heightA      : height of 2D block for matrix A in DDR
    * @param [in]  _widthB       : Width of 2D block for matrix B in DDR
    * @param [in]  _heightB      : height of 2D block for matrix B in DDR
    * @param [in]  _blockWidthA  : Width of 2D block for matrix A in L2
    * @param [in]  _blockHeightA : Height of 2D block for matrix A in L2
    * @param [in]  _blockWidthB  : Width of 2D block for matrix B in L2
    * @param [in]  _inPitchA     : Input pitch for matrix A in DDR
    * @param [in]  _inPitchB     : Input pitch for matrix B in DDR
    * @param [in]  _outPitch    : Output pitch in DDR
    * @param [in]  _numChannels : Total number of DMA channels to be used by the supernode (always 3)
    *
    */

   i2d_to_o2dMatMul(std::unique_ptr<opVec> &&_myOpVec,
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
                    size_t                   _numChannels);

   void exec() override;

   /**
     @brief Implementation of the getOpVec function for i2d_to_o2dMatMul.
     * @return Reference to the operator vector.
   */
   const opVec &getOpVec() const override { return *myOpVec; }

   /**
     @brief Implementation of the getBlockCount function for i2d_to_o2dMatMul.
     * @return The block count.
   */
   size_t getBlockCount() const override { return _blockCount; }
};

/** @} */

/**
 *  @ingroup  SUPERNODE
 */

/**@{*/

/**
 *  \class I2dToO2dTranspose
 *  @brief Provides the supernode style when input data is transferred in 2D form and output blocks are transferred in a
 *         transposed fashion in 2D form.
 */

template <typename dTypeIn, typename dTypeOut> class I2dToO2dTranspose final : public BaseSuperNode {
 private:
   std::unique_ptr<opVec> _myOpVec;

   dTypeIn  *_pIn       = nullptr;
   dTypeOut *_pOut      = nullptr;
   dTypeIn  *_pInBlock  = nullptr;
   dTypeOut *_pOutBlock = nullptr;

   DmaUtilsAutoInc3d_TransferDim _transferDimIn;  /*!< Structure to hold transfer properties of input DMA channel */
   DmaUtilsAutoInc3d_TransferDim _transferDimOut; /*!< Structure to hold transfer properties of output DMA channel */

   bool   _pingPongFlagIn;
   bool   _pingPongFlagOut;
   size_t _offset;

   size_t _blockCount = 0; // Track the block count for profiling

 public:
   /**
    * @brief Supernode constructor to create a 2D input to 2D output (tranposed) DMA supernode.
    * @param [in]  &myOpVec    : Operator vector with list of nodes to be executed inside the supernode
    * @param [in]  *pIn        : Input pointer in DDR space
    * @param [out] *pOut       : Output pointer in DDR space
    * @param [in]  *pInBlock   : Input pointer in L2 space
    * @param [out] *pOutBlock  : Output pointer in L2 space
    * @param [in]  width       : Width of 2D block in DDR
    * @param [in]  height      : height of 2D block in DDR
    * @param [in]  blockWidth  : Width of 2D block in L2
    * @param [in]  blockHeight : Height of 2D block in L2
    * @param [in]  inPitch     : Input pitch in DDR
    * @param [in]  outPitch    : Output pitch in DDR
    * @param [in]  numChannels : Total number of DMA channels to be used by the supernode (always 2)
    *
    */

   I2dToO2dTranspose(std::unique_ptr<opVec> &&myOpVec,
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
                     size_t                   numChannels);

   void exec() override;

   /**
     @brief Implementation of the getOpVec function for I2dToO2dTranspose.
     * @return Reference to the operator vector.
   */
   const opVec &getOpVec() const override { return *_myOpVec; }

   /**
     @brief Implementation of the getBlockCount function for I2dToO2dTranspose.
     * @return The block count.
   */
   size_t getBlockCount() const override { return _blockCount; }
};

/** @} */

/**
 *  @ingroup  SUPERNODE
 */

/**@{*/

/**
 *  \class I2dI2dToO2d
 *  @brief Provides the supernode style when 2 inputs of data is transferred in 2D form and output blocks are
 * transferred in a transposed fashion in 2D form.
 * @brief This wrapper also supports when input and output dimensions are different.
 */

template <typename dTypeIn, typename dTypeOut> class I2dI2dToO2d final : public BaseSuperNode {

 private:
   std::unique_ptr<opVec>        myOpVec;
   DmaUtilsAutoInc3d_TransferDim transferDimInA; /*!< Structure to hold transfer properties of input DMA channel */
   DmaUtilsAutoInc3d_TransferDim transferDimInB;
   DmaUtilsAutoInc3d_TransferDim transferDimOut; /*!< Structure to hold transfer properties of output DMA channel */

   dTypeIn  *pInA;
   dTypeIn  *pInB;
   dTypeIn  *pInBlockA;
   dTypeIn  *pInBlockB;
   dTypeOut *pOut;
   dTypeOut *pOutBlock;

   size_t widthA;
   size_t heightA;
   size_t widthB;
   size_t heightB;
   size_t widthOut;
   size_t heightOut;

   size_t blockWidthA;
   size_t blockHeightA;
   size_t blockWidthB;
   size_t blockHeightB;
   size_t blockWidthOut;
   size_t blockHeightOut;

   bool   pingPongFlagInA;
   bool   pingPongFlagInB;
   bool   pingPongFlagOut;
   size_t offsetA;
   size_t offsetB;
   size_t offsetOut;

   size_t _numBytesInA; // needed for cache wb
   size_t _numBytesInB; // needed for cache wb
   size_t _numBytesOut; // needed for cache inv

   size_t _blockCount = 0; // Track the block count for profiling

 public:
   /**
    * @brief Supernode constructor to create a 2D input to 2D output DMA supernode.
    * @param [in]  &_myOpVec    : Operator vector with list of nodes to be executed inside the supernode
    * @param [in]  *_pInA        : Input pointer for matrix A in DDR space
    * @param [in]  *_pInB        : Input pointer for matrix B in DDR space
    * @param [out] *_pOut       : Output pointer in DDR space
    * @param [in]  *_pInBlockA  : Input pointer for matrix A in L2 space
    * @param [in]  *_pInBlockB  : Input pointer for matrix B in L2 space
    * @param [out] *_pOutBlock  : Output pointer in L2 space
    * @param [in]  _widthA       : Width of 2D block for matrix A in DDR
    * @param [in]  _heightA      : height of 2D block for matrix A in DDR
    * @param [in]  _widthB       : Width of 2D block for matrix B in DDR
    * @param [in]  _heightB      : height of 2D block for matrix B in DDR
    * @param [in]  _widthOut       : Width of 2D block for output matrix in DDR
    * @param [in]  _heightOut      : height of 2D block for output matrix in DDR
    * @param [in]  _blockWidthA  : Width of 2D block for matrix A in L2
    * @param [in]  _blockHeightA : Height of 2D block for matrix A in L2
    * @param [in]  _blockWidthB  : Width of 2D block for matrix B in L2
    * @param [in]  _blockHeightB : Height of 2D block for matrix B in L2
    * @param [in]  _blockWidthOut  : Width of 2D block for output matrix in L2
    * @param [in]  _blockHeightOut : Height of 2D block for output matrix in L2
    * @param [in]  _inPitchA     : Input pitch for matrix A in DDR
    * @param [in]  _inPitchB     : Input pitch for matrix B in DDR
    * @param [in]  _outPitch    : Output pitch in DDR
    * @param [in]  _numChannels : Total number of DMA channels to be used by the supernode (always 3)
    *
    */

   I2dI2dToO2d(std::unique_ptr<opVec> &&_myOpVec,
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
               size_t                   _numChannels);

   void exec() override;

   /**
     @brief Implementation of the getOpVec function for I2dI2dToO2d.
     * @return Reference to the operator vector.
   */
   const opVec &getOpVec() const override { return *myOpVec; }

   /**
     @brief Implementation of the getBlockCount function for I2dI2dToO2d.
     * @return The block count.
   */
   size_t getBlockCount() const override { return _blockCount; }
};

/** @} */

/**
 *  @ingroup  SUPERNODE
 */

/**@{*/

/**
 *  \class I2dI2dToO2dO2d
 *  @brief Provides the supernode style when 2 inputs of data is transferred in 2D form and 2 output blocks are
 * transferred in a transposed fashion in 2D form.
 * @brief This wrapper also supports when input and output dimensions are different.
 */

template <typename dTypeIn, typename dTypeOut> class I2dI2dToO2dO2d final : public BaseSuperNode {

 private:
   std::unique_ptr<opVec>        myOpVec;
   DmaUtilsAutoInc3d_TransferDim transferDimInA;  /*!< Structure to hold transfer properties of inputA DMA channel */
   DmaUtilsAutoInc3d_TransferDim transferDimInB;  /*!< Structure to hold transfer properties of inputB DMA channel */
   DmaUtilsAutoInc3d_TransferDim transferDimOutA; /*!< Structure to hold transfer properties of outputA DMA channel */
   DmaUtilsAutoInc3d_TransferDim transferDimOutB; /*!< Structure to hold transfer properties of outputB DMA channel */

   dTypeIn  *pInA;
   dTypeIn  *pInB;
   dTypeOut *pOutA;
   dTypeOut *pOutB;
   dTypeIn  *pInBlockA;
   dTypeIn  *pInBlockB;
   dTypeOut *pOutBlockA;
   dTypeOut *pOutBlockB;

   size_t blockWidthA;
   size_t blockHeightA;
   size_t blockWidthB;
   size_t blockHeightB;

   size_t blockWidthOutA;
   size_t blockHeightOutA;
   size_t blockWidthOutB;
   size_t blockHeightOutB;

   size_t widthA;
   size_t heightA;
   size_t widthB;
   size_t heightB;

   size_t widthOutA;
   size_t heightOutA;
   size_t widthOutB;
   size_t heightOutB;

   bool pingPongFlagInA;
   bool pingPongFlagInB;
   bool pingPongFlagOutA;
   bool pingPongFlagOutB;

   size_t offsetA;
   size_t offsetB;
   size_t offsetOutA;
   size_t offsetOutB;

   size_t _numBytesInA;  // needed for cache wb
   size_t _numBytesInB;  // needed for cache wb
   size_t _numBytesOutA; // needed for cache inv
   size_t _numBytesOutB; // needed for cache inv

   size_t _blockCount = 0; // Track the block count for profiling

 public:
   /**
    * @brief Supernode constructor to create a 2D input to 2D output DMA supernode.
    * @param [in]  &_myOpVec     : Operator vector with list of nodes to be executed inside the supernode
    * @param [in]  *_pInA        : Input pointer for matrix A in DDR space
    * @param [in]  *_pInB        : Input pointer for matrix B in DDR space
    * @param [out] *_pOutA       : Output pointer for matrix A in DDR space
    * @param [out] *_pOutA       : Output pointer for matrix B in DDR space
    * @param [in]  *_pInBlockA   : Input pointer for matrix A in L2 space
    * @param [in]  *_pInBlockB   : Input pointer for matrix B in L2 space
    * @param [out] *_pOutBlockA  : Output pointer matrix A in L2 space
    * @param [out] *_pOutBlockB  : Output pointer matrix B in L2 space
    * @param [in]  _widthA       : Width of 2D block for matrix A in DDR
    * @param [in]  _heightA      : height of 2D block for matrix A in DDR
    * @param [in]  _widthB       : Width of 2D block for matrix B in DDR
    * @param [in]  _heightB      : height of 2D block for matrix B in DDR
    * @param [in]  _widthOutA       : Width of output 2D block for matrix A in DDR
    * @param [in]  _heightOutA      : height of output 2D block for matrix A in DDR
    * @param [in]  _widthOutB       : Width of output 2D block for matrix B in DDR
    * @param [in]  _heightOutB      : height of output 2D block for matrix B in DDR
    * @param [in]  _blockWidthA     : Width of 2D block for matrix A in L2
    * @param [in]  _blockHeightA    : Height of 2D block for matrix A in L2
    * @param [in]  _blockWidthB     : Width of 2D block for matrix B in L2
    * @param [in]  _blockHeightB    : Height of 2D block for matrix B in L2
    * @param [in]  _blockWidthOutA  : Width of output 2D block for matrix A in L2
    * @param [in]  _blockHeightOutA : Height of output 2D block for matrix A in L2
    * @param [in]  _blockWidthOutB  : Width of output 2D block for matrix B in L2
    * @param [in]  _blockHeightOutB : Height of output 2D block for matrix B in L2
    * @param [in]  _inPitchA        : Input pitch for matrix A in DDR
    * @param [in]  _inPitchB        : Input pitch for matrix B in DDR
    * @param [in]  _outPitchA       : Output pitch for matrix A in DDR
    * @param [in]  _outPitchB       : Output pitch for matrix B in DDR
    * @param [in]  _numChannels     : Total number of DMA channels to be used by the supernode (always 3)
    *
    */

   I2dI2dToO2dO2d(std::unique_ptr<opVec> &&_myOpVec,
                  dTypeIn                 *_pInA,
                  dTypeIn                 *_pInB,
                  dTypeOut                *_pOutA,
                  dTypeOut                *_pOutB,
                  dTypeIn                 *_pInBlockA,
                  dTypeIn                 *_pInBlockB,
                  dTypeOut                *_pOutBlockA,
                  dTypeOut                *_pOutBlockB,
                  size_t                   _widthA,
                  size_t                   _heightA,
                  size_t                   _widthB,
                  size_t                   _heightB,
                  size_t                   _widthOutA,
                  size_t                   _heightOutA,
                  size_t                   _widthOutB,
                  size_t                   _heightOutB,
                  size_t                   _blockWidthA,
                  size_t                   _blockHeightA,
                  size_t                   _blockWidthB,
                  size_t                   _blockHeightB,
                  size_t                   _blockWidthOutA,
                  size_t                   _blockHeightOutA,
                  size_t                   _blockWidthOutB,
                  size_t                   _blockHeightOutB,
                  size_t                   _inPitchA,
                  size_t                   _inPitchB,
                  size_t                   _outPitchA,
                  size_t                   _outPitchB,
                  size_t                   _numChannels);

   void exec() override;

   /**
     @brief Implementation of the getOpVec function for I2dI2dToO2dO2d.
     * @return Reference to the operator vector.
   */
   const opVec &getOpVec() const override { return *myOpVec; }

   /**
     @brief Implementation of the getBlockCount function for I2dI2dToO2dO2d.
     * @return The block count.
   */
   size_t getBlockCount() const override { return _blockCount; }
};

/** @} */

} // namespace SuperNode
} // namespace TISP
