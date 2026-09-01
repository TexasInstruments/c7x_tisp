// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

// Define this macro to disable storing error messages in memory-constrained systems
//#define TISP_MINIMAL_ERROR_MESSAGES

#include <cstdint>

#ifndef TISP_MINIMAL_ERROR_MESSAGES
#include <cstdio>
#include <cstring>
#endif

namespace TISP {

/**
 * @brief Error codes for DSP operations
 * @ingroup errorHandling
 */
enum class ErrorCode : int32_t {
   kNone               = 0, ///< No error
   kNullPointer        = 1, ///< Null pointer error
   kInvalidParameter   = 2, ///< Invalid parameter error
   kMemory             = 3, ///< Memory allocation error
   kLibrary            = 4, ///< Library operation error
   kInvalidNode        = 5, ///< Invalid node error
   kInvalidObjectId    = 6, ///< Invalid object/node ID in opVec
   kInvalidSignalChain = 7, ///< Invalid signal chain/graph structure
   kDataProbeBusy      = 8  ///< Resource is already in use and was not disturbed
};

/**
 * @brief Error context for node and supernode creation
 * @ingroup errorHandling
 *
 * This class provides a mechanism for reporting and handling errors during
 * the graph setup phase without using exceptions. It is designed to be used
 * as an output parameter in constructors and factory methods.
 */
class GraphSetupErrorCtxt {
 public:
   /**
    * @brief Constructs an error context with success status
    */
   GraphSetupErrorCtxt() : status(ErrorCode::kNone), libraryStatusCode(0)
   {
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      errorMessage[0] = '\0';
#endif
   }

#ifndef TISP_MINIMAL_ERROR_MESSAGES
   /**
    * @brief Helper method to safely copy error messages
    *
    * @param message Error description to copy
    */
 private:
   void copyErrorMessage(const char* message)
   {
      strncpy(errorMessage, message, sizeof(errorMessage) - 1);
      errorMessage[sizeof(errorMessage) - 1] = '\0';
   }

 public:
#endif

   /**
    * @brief Sets the error context to indicate a null pointer error with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    */
   void setNullPointerError(const char* functionName, const char* errorDescription) noexcept
   {
      status = ErrorCode::kNullPointer;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Sets the error context to indicate an invalid parameter error with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    */
   void setInvalidParameterError(const char* functionName, const char* errorDescription) noexcept
   {
      status = ErrorCode::kInvalidParameter;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Sets the error context to indicate a memory allocation error with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    */
   void setMemoryError(const char* functionName, const char* errorDescription) noexcept
   {
      status = ErrorCode::kMemory;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Sets the error context to indicate a library operation error with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    * @param statusCode Library-specific status code
    */
   void setLibraryError(const char* functionName, const char* errorDescription, int32_t statusCode) noexcept
   {
      status            = ErrorCode::kLibrary;
      libraryStatusCode = statusCode;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Sets the error context to indicate an invalid node error with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    */
   void setInvalidNodeError(const char* functionName, const char* errorDescription) noexcept
   {
      status = ErrorCode::kInvalidNode;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Sets the error context to indicate an invalid object/node ID in opVec with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    */
   void setInvalidObjectIdError(const char* functionName, const char* errorDescription) noexcept
   {
      status = ErrorCode::kInvalidObjectId;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Sets the error context to indicate an invalid signal chain/graph structure with formatted message
    *
    * @param functionName The name of the function that failed
    * @param errorDescription Brief description of the error
    */
   void setInvalidSignalChainError(const char* functionName, const char* errorDescription) noexcept
   {
      status = ErrorCode::kInvalidSignalChain;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      snprintf(this->errorMessage, sizeof(this->errorMessage), "%s: %s", functionName, errorDescription);
#endif
   }

   /**
    * @brief Checks if the error context indicates success
    *
    * @return true if the status is success, false otherwise
    */
   bool isSuccess() const { return status == ErrorCode::kNone; }

   /**
    * @brief Gets the error message
    *
    * @return The error message string
    */
   const char* getMessage() const
   {
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      return errorMessage;
#else
      // Return generic messages based on error code when in minimal mode
      switch (status) {
      case ErrorCode::kNone:
         return "Success";
      case ErrorCode::kNullPointer:
         return "Null pointer error";
      case ErrorCode::kInvalidParameter:
         return "Invalid parameter error";
      case ErrorCode::kMemory:
         return "Memory allocation error";
      case ErrorCode::kLibrary:
         return "Library operation error";
      case ErrorCode::kInvalidNode:
         return "Invalid node error";
      case ErrorCode::kInvalidObjectId:
         return "Invalid object ID error";
      case ErrorCode::kInvalidSignalChain:
         return "Invalid signal chain error";
      case ErrorCode::kDataProbeBusy:
         return "Resource busy error";
      default:
         return "Unknown error";
      }
#endif
   }

   /**
    * @brief Gets the error code
    *
    * @return Error code identifying the error type
    */
   ErrorCode getErrorCode() const { return status; }

   /**
    * @brief Gets the library-specific status code
    *
    * @return The status code returned by the library function
    */
   int32_t getLibraryStatusCode() const { return libraryStatusCode; }

   /**
    * @brief Resets the error context to success status
    */
   void reset()
   {
      status = ErrorCode::kNone;
#ifndef TISP_MINIMAL_ERROR_MESSAGES
      errorMessage[0] = '\0';
#endif
      libraryStatusCode = 0;
   }

   /**
    * @brief Remaps standard library error codes (0-13) for AUDIOLIB, DSPLIB, MATHLIB
    *
    * Centralizes error code mapping across all TISP libraries that share standard error codes.
    * Performs semantic mapping where possible (e.g., NULL_POINTER → setNullPointerError)
    * and falls back to generic library error reporting with the numeric status code.
    *
    * Maps codes used by: AUDIOLIB, DSPLIB, MATHLIB
    * Code 0 = SUCCESS (no error set, returns true)
    * Codes 1-12 = Various error conditions with semantic mapping
    * Codes 13+ = Library-specific errors (generic library error with code)
    *
    * @param statusCode The status code returned from an XLIB function (AUDIOLIB, DSPLIB, MATHLIB)
    * @param functionName The name of the XLIB function that failed (e.g., "AUDIOLIB_gain_init")
    *
    * @return true if statusCode == 0 (SUCCESS), false otherwise
    *
    * Example usage:
    * @code
    * AUDIOLIB_STATUS ret = AUDIOLIB_gain_init(...);
    * if (!errorCtx.remapXLIBError(ret, "AUDIOLIB_gain_init")) {
    *     // Error handling
    * }
    * @endcode
    */
   bool remapXLIBError(int statusCode, const char* functionName) noexcept
   {
      switch (statusCode) {
      case 0: // SUCCESS
         return true;

      case 1: // AUDIOLIB_ERR_FAILURE
         this->setLibraryError(functionName, "unspecified error", statusCode);
         break;

      case 2: // INVALID_VALUE
         this->setInvalidParameterError(functionName, "invalid parameter value");
         break;

      case 3: // INVALID_TYPE
         this->setInvalidParameterError(functionName, "invalid parameter type");
         break;

      case 4: // INVALID_DIMENSION
         this->setInvalidParameterError(functionName, "invalid buffer dimension");
         break;

      case 5: // NULL_POINTER
         this->setNullPointerError(functionName, "null pointer in parameters");
         break;

      case 6: // NOT_IMPLEMENTED
         this->setLibraryError(functionName, "configuration not implemented", statusCode);
         break;

      case 7: // NOT_EQUAL_WIDTH_STRIDE
         this->setInvalidParameterError(functionName, "stride not equal to width × element size");
         break;

      case 8: // NOT_ALIGNED_PTRS_STRIDES
         this->setInvalidParameterError(functionName, "pointers or strides not aligned");
         break;

      case 9: // NOT_ALIGNED_WIDTHS
         this->setInvalidParameterError(functionName, "widths not aligned");
         break;

      case 10: // BUFFER_TOO_SMALL
         this->setMemoryError(functionName, "buffer too small");
         break;

      case 11: // INVALID_ACTIVATION
         this->setInvalidParameterError(functionName, "activation incompatible with data type");
         break;

      case 12: // INVALID_SHIFT
         this->setInvalidParameterError(functionName, "requested shift amount not valid");
         break;

      default:
         this->setLibraryError(functionName, "library error", statusCode);
         break;
      }

      return false;
   }

 private:
   ErrorCode status;            ///< Error code
   int32_t   libraryStatusCode; ///< Library-specific status code

 public:
#ifndef TISP_MINIMAL_ERROR_MESSAGES
   char errorMessage[256]; ///< Error message (only included when not in minimal mode)
#endif
};

/**
 * @brief Convenience alias for GraphSetupErrorCtxt.
 * @ingroup errorHandling
 */
using ErrorCtxt = GraphSetupErrorCtxt;

} // namespace TISP
