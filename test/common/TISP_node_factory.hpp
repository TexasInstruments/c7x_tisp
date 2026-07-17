// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once
#include "TISP_virtualOp.hpp" // Include the base class header
#include <TISP_ErrorCtxt.hpp>
#include <audiolib.h>
#include <dsplib.h>
#include <mathlib.h>
#include <memory>

namespace TISP {
namespace TEST {

/**
 * @brief Enumeration of supported data types for node operations.
 * @details This enum class provides a type-safe representation of all
 * supported numeric data types that can be used with TISP nodes.
 */
enum class DataType {
   kDouble, /**< Double-precision floating point */
   kFloat,  /**< Single-precision floating point */
   kUint64, /**< 64-bit unsigned integer */
   kInt64,  /**< 64-bit signed integer */
   kUint32, /**< 32-bit unsigned integer */
   kInt32,  /**< 32-bit signed integer */
   kUint16, /**< 16-bit unsigned integer */
   kInt16,  /**< 16-bit signed integer */
   kUint8,  /**< 8-bit unsigned integer */
   kInt8    /**< 8-bit signed integer */
};

/**
 * @brief Converts DSPLIB data type constants to DataType enum values.
 * @details This helper function maps from DSPLIB's runtime type constants
 * to the corresponding DataType enum value used in the TISP test framework.
 *
 * @param dsplibType The DSPLIB data type constant to convert.
 * @return DataType The corresponding TISP DataType enum value.
 *
 * @note This function is used to convert DSPLIB data type constants to
 * TISP DataType enum values, which are used to create nodes of the correct
 * type.
 */
inline DataType DSPLIBTypeToTISPType(uint32_t dsplibType)
{
   switch (dsplibType) {
   case DSPLIB_FLOAT32:
      return DataType::kFloat;
   case DSPLIB_FLOAT64:
      return DataType::kDouble;
   case DSPLIB_INT8:
      return DataType::kInt8;
   case DSPLIB_UINT8:
      return DataType::kUint8;
   case DSPLIB_INT16:
      return DataType::kInt16;
   case DSPLIB_UINT16:
      return DataType::kUint16;
   case DSPLIB_INT32:
      return DataType::kInt32;
   case DSPLIB_UINT32:
      return DataType::kUint32;
   case DSPLIB_INT64:
      return DataType::kInt64;
   case DSPLIB_UINT64:
      return DataType::kUint64;
   default:
      return DataType::kFloat;
   }
}

/**
 * @brief Converts AUDIOLIB data type constants to DataType enum values.
 * @details This helper function maps from AUDIOLIB's runtime type constants
 * to the corresponding DataType enum value used in the TISP test framework.
 *
 * @param dsplibType The AUDIOLIB data type constant to convert.
 * @return DataType The corresponding TISP DataType enum value.
 *
 * @note This function is used to convert AUDIOLIB data type constants to
 * TISP DataType enum values, which are used to create nodes of the correct
 * type.
 */
inline DataType AUDIOLIBTypeToTISPType(uint32_t dsplibType)
{
   switch (dsplibType) {
   case AUDIOLIB_FLOAT32:
      return DataType::kFloat;
   case AUDIOLIB_FLOAT64:
      return DataType::kDouble;
   case AUDIOLIB_INT8:
      return DataType::kInt8;
   case AUDIOLIB_UINT8:
      return DataType::kUint8;
   case AUDIOLIB_INT16:
      return DataType::kInt16;
   case AUDIOLIB_UINT16:
      return DataType::kUint16;
   case AUDIOLIB_INT32:
      return DataType::kInt32;
   case AUDIOLIB_UINT32:
      return DataType::kUint32;
   case AUDIOLIB_INT64:
      return DataType::kInt64;
   case AUDIOLIB_UINT64:
      return DataType::kUint64;
   default:
      return DataType::kFloat;
   }
}

/**
 * @brief Converts MATHLIB data type constants to DataType enum values.
 * @details This helper function maps from MATHLIB's runtime type constants
 * to the corresponding DataType enum value used in the TISP test framework.
 *
 * @param mathlibType The MATHLIB data type constant to convert.
 * @return DataType The corresponding TISP DataType enum value.
 *
 * @note This function is used to convert MATHLIB data type constants to
 * TISP DataType enum values, which are used to create nodes of the correct
 * type.
 */

inline DataType MATHLIBTypeToTISPType(uint32_t mathlibType)
{
   switch (mathlibType) {
   case MATHLIB_FLOAT32:
      return DataType::kFloat;
   case MATHLIB_FLOAT64:
      return DataType::kDouble;
   default:
      return DataType::kFloat;
   }
}

/**
 * @brief Factory class for creating node instances with runtime type selection.
 * @details The NodeFactory class provides a static method to create any type of node.
 * It instantiates the appropriate template specialization based on the provided
 * DataType parameter, allowing for runtime type selection while maintaining
 * type safety and performance.
 */

class NodeFactory {
 public:
   /**
    * @brief Creates a node of the specified type based on runtime data type information.
    * @details This template method creates a node of the specified NodeType class,
    * instantiating it with the appropriate data type based on the runtime DataType
    * parameter. It handles the casting of void pointers to the correct type.
    *
    * @example
    * @code{.cpp}
    * auto k0 = TISP::TEST::NodeFactory::create<TISP::DSPLIB::Deinterleave>(
    *           TISP::TEST::DSPLIBTypeToTISPType(currPrm.dataType), pIn.get(), pOut.get(),
    *           static_cast<int32_t>(widthIn), static_cast<int32_t>(heightIn), static_cast<int32_t>(strideIn),
    *           static_cast<int32_t>(strideOut));
    * @endcode
    *
    * @tparam NodeType The template class to instantiate (must derive from TISP::VirtualOp).
    * @tparam Args Additional argument types for the node constructor.
    *
    * @param dataType The runtime data type to use for node instantiation.
    * @param pIn Pointer to input data (will be cast to appropriate type).
    * @param pOut Pointer to output data (will be cast to appropriate type).
    * @param args Additional arguments to forward to the node constructor.
    *
    * @return std::unique_ptr<TISP::VirtualOp> A unique pointer to the created node.
    *         Returns nullptr if an unsupported data type is provided.
    */

   template <template <typename...> class NodeType, typename... Args>
   static std::unique_ptr<TISP::VirtualOp> create(DataType dataType, void* pIn, void* pOut, Args&&... args)
   {
      switch (dataType) {
      case DataType::kFloat:
         return std::make_unique<NodeType<float>>((float*) pIn, (float*) pOut, std::forward<Args>(args)...);
      case DataType::kDouble:
         return std::make_unique<NodeType<double>>((double*) pIn, (double*) pOut, std::forward<Args>(args)...);
      case DataType::kInt8:
         return std::make_unique<NodeType<int8_t>>((int8_t*) pIn, (int8_t*) pOut, std::forward<Args>(args)...);
      case DataType::kUint8:
         return std::make_unique<NodeType<uint8_t>>((uint8_t*) pIn, (uint8_t*) pOut, std::forward<Args>(args)...);
      case DataType::kInt16:
         return std::make_unique<NodeType<int16_t>>((int16_t*) pIn, (int16_t*) pOut, std::forward<Args>(args)...);
      case DataType::kUint16:
         return std::make_unique<NodeType<uint16_t>>((uint16_t*) pIn, (uint16_t*) pOut, std::forward<Args>(args)...);
      case DataType::kInt32:
         return std::make_unique<NodeType<int32_t>>((int32_t*) pIn, (int32_t*) pOut, std::forward<Args>(args)...);
      case DataType::kUint32:
         return std::make_unique<NodeType<uint32_t>>((uint32_t*) pIn, (uint32_t*) pOut, std::forward<Args>(args)...);
      case DataType::kInt64:
         return std::make_unique<NodeType<int64_t>>((int64_t*) pIn, (int64_t*) pOut, std::forward<Args>(args)...);
      case DataType::kUint64:
         return std::make_unique<NodeType<uint64_t>>((uint64_t*) pIn, (uint64_t*) pOut, std::forward<Args>(args)...);

      default:
         // Simple null pointer return for unsupported types
         // Error handling can be added later
         return std::unique_ptr<TISP::VirtualOp>(nullptr);
      }
   }

   template <template <typename...> class NodeType, typename... Args>
   static std::unique_ptr<TISP::VirtualOp>
   createA(DataType dataType, void* pIn0, void* pIn1, void* pOut, Args&&... args)
   {
      TISP::ErrorCtxt errorCtx;
      switch (dataType) {
      case DataType::kFloat:
         return std::make_unique<NodeType<float>>((float*) pIn0, (float*) pIn1, (float*) pOut,
                                                  std::forward<Args>(args)...);
      case DataType::kDouble:
         return std::make_unique<NodeType<double>>((double*) pIn0, (double*) pIn1, (double*) pOut,
                                                   std::forward<Args>(args)...);
      default:
         // Simple null pointer return for unsupported types
         // Error handling can be added later
         errorCtx.setInvalidParameterError("TISP_createKernelObject", "unsupported data type for kernel");
         return std::unique_ptr<TISP::VirtualOp>(nullptr);
      }
   }

   /**
    * @brief Creates a node of the specified type based on runtime data type information.
    * @details This template method creates a node of the specified NodeType class,
    * instantiating it with the appropriate data type based on the runtime DataType
    * parameter. It handles the casting of void pointers to the correct type.
    *
    * @example
    * @code{.cpp}
    * auto k0 = TISP::TEST::NodeFactory::create<TISP::MATHLIB::Sin>(
    *           TISP::TEST::MATHLIBTypeToTISPType(currPrm.dataType), pIn.get(), pOut.get(),
    *           static_cast<int32_t>(widthIn), static_cast<int32_t>(heightIn), static_cast<int32_t>(strideIn),
    *           static_cast<int32_t>(strideOut));
    * @endcode
    *
    * @tparam NodeType The template class to instantiate (must derive from TISP::VirtualOp).
    * @tparam Args Additional argument types for the node constructor.
    *
    * @param dataType The runtime data type to use for node instantiation.
    * @param pIn Pointer to input data (will be cast to appropriate type).
    * @param pIn1 Pointer to second input data (will be cast to appropriate type).
    * @param pOut Pointer to output data (will be cast to appropriate type).
    * @param args Additional arguments to forward to the node constructor.
    *
    * @return std::unique_ptr<TISP::VirtualOp> A unique pointer to the created node.
    *         Returns nullptr if an unsupported data type is provided.
    */

   template <template <typename...> class NodeType>
   static std::unique_ptr<TISP::VirtualOp>
   create(DataType dataType, size_t size, void* pIn, void* pOut, ErrorCtxt& errorCtx)
   {
      switch (dataType) {
      case DataType::kFloat:
         return std::make_unique<NodeType<float>>(size, (float*) pIn, (float*) pOut, errorCtx);
      case DataType::kDouble:
         return std::make_unique<NodeType<double>>(size, (double*) pIn, (double*) pOut, errorCtx);
      default:
         // Simple null pointer return for unsupported types
         // Error handling can be added later
         errorCtx.setInvalidParameterError("TISP_createKernelObject", "unsupported data type for kernel");
         return std::unique_ptr<TISP::VirtualOp>(nullptr);
      }
   }

   template <template <typename...> class NodeType>
   static std::unique_ptr<TISP::VirtualOp> create(DataType dataType, size_t size, void* pIn, void* pIn1, void* pOut)
   {
      switch (dataType) {
      case DataType::kFloat:
         return std::make_unique<NodeType<float>>(size, (float*) pIn, (float*) pIn1, (float*) pOut);
      case DataType::kDouble:
         return std::make_unique<NodeType<double>>(size, (double*) pIn, (double*) pIn1, (double*) pOut);
      default:
         // Simple null pointer return for unsupported types
         // Error handling can be added later
         return std::unique_ptr<TISP::VirtualOp>(nullptr);
      }
   }

   /**
    * @brief Creates a node with different input and output data types.
    * @details This overload allows creating nodes where the input and output
    * data types differ, such as in type conversion operations.
    *
    * @tparam NodeType The template class to instantiate (must derive from TISP::VirtualOp).
    * @tparam Args Additional argument types for the node constructor.
    *
    * @param inDataType The runtime data type for input data.
    * @param outDataType The runtime data type for output data.
    * @param pIn Pointer to input data (will be cast to appropriate type).
    * @param pOut Pointer to output data (will be cast to appropriate type).
    * @param args Additional arguments to forward to the node constructor.
    *
    * @return std::unique_ptr<TISP::VirtualOp> A unique pointer to the created node.
    *         Returns nullptr if the implementation is not available or types are unsupported.
    */
   template <template <typename, typename> class NodeType, typename... Args>
   static std::unique_ptr<TISP::VirtualOp>
   create(DataType inDataType, DataType outDataType, void* pIn, void* pOut, Args&&... args)
   {
      // Implementation would handle mixed types
      // For now, return nullptr to indicate this isn't implemented yet
      return std::unique_ptr<TISP::VirtualOp>(nullptr);
   }
};

} // namespace TEST
} // namespace TISP
