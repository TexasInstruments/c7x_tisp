#ifndef TISP_DOWNWARDEXPANDER_IDAT_H
#define TISP_DOWNWARDEXPANDER_IDAT_H

#include <audiolib.h>

#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {

     uint32_t testId;
     uint32_t testPattern;
     uint32_t outputDataLocation;
     uint32_t numReps;
     uint32_t numChannels;
     uint32_t blockSize;
     uint32_t dataType;
     uint32_t pitchX;
     uint32_t pitchS;
     uint32_t pitchY;
     uint32_t bypassMode;
     uint32_t numBlocks;
     void* staticIn;
     void* staticOut;
     void* staticGainOut;
     void* staticHoldOut;
     void* threshold_dB;
     void* ratio;
     void* kneeWidth_dB;
     void* attackCoeff;
     void* releaseCoeff;
     void* holdSamples;

} TISP_downwardExpander_testParams_t;

void TISP_downwardExpander_getTestParams(TISP_downwardExpander_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif
#endif /* define TISP_DOWNWARDEXPANDER_IDAT_H */

/* ======================================================================== */
/*  End of file:  TISP_downwardExpander_idat.h                                               */
/* ======================================================================== */

