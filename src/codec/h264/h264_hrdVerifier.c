#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "h264_hrdVerifier.h"
#include "../../util/errorCodesVa.h"

H264HrdVerifierContextPtr createH264HrdVerifierContext(
  const LibbluESSettingsOptions * options,
  const H264SPSDataParameters * spsData,
  const H264ConstraintsParam * constraints
)
{
  /* Only operating at SchedSelIdx = cpbCntMinus1 */
  H264HrdVerifierContextPtr ctx;

  const H264VuiParameters * vuiParam;
  const H264HrdParameters * nalHrdParam;
  const H264HrdParameters * vclHrdParam;

  unsigned SchedSelIdx;
  unsigned MaxBR, MaxCPB, MaxDpbMbs;
  unsigned maxNalBitrate, maxNalCpbBufSize;
  unsigned maxVclBitrate, maxVclCpbBufSize;

  assert(NULL != spsData);

  /* Check for required parameters: */
#if 0
  if (!param->sequenceParametersSetPresent) {
    LIBBLU_H264_HRDV_ERROR_NRETURN(
      "Missing SPS to initiate HRD verifier.\n"
    );
  }
#endif

  if (!spsData->vuiParametersPresent)
    LIBBLU_H264_HRDV_ERROR_NRETURN(
      "Missing SPS VUI data to initiate HRD verifier.\n"
    );
  vuiParam = &spsData->vuiParameters;

  if (!vuiParam->timingInfoPresent)
    LIBBLU_H264_HRDV_ERROR_NRETURN(
      "Missing SPS VUI timing info data to initiate HRD verifier.\n"
    );

  if (!vuiParam->nalHrdParamPresent)
    LIBBLU_H264_HRDV_ERROR_NRETURN(
      "Missing SPS VUI NAL HRD data to initiate HRD verifier.\n"
    );
  nalHrdParam = &vuiParam->nalHrdParam;

  /* Only the last SchedSelIdx is used for tests : */
  SchedSelIdx = nalHrdParam->cpbCntMinus1;

  /* Check parameters compliance: */
  if (vuiParam->lowDelayHrd)
    LIBBLU_H264_HRDV_ERROR_NRETURN(
      "HRD Verifier does not currently support Low Delay mode.\n"
    );

  assert(0 < constraints->MaxBR);
  assert(0 < constraints->MaxCPB);
  assert(0 < constraints->MaxDpbMbs);

  MaxDpbMbs = constraints->MaxDpbMbs;

  if (
    H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)

    || spsData->profileIdc == H264_PROFILE_HIGH
    || spsData->profileIdc == H264_PROFILE_HIGH_10
    || spsData->profileIdc == H264_PROFILE_HIGH_422
    || spsData->profileIdc == H264_PROFILE_HIGH_444_PREDICTIVE
    || spsData->profileIdc == H264_PROFILE_CAVLC_444_INTRA
  ) {
    assert(0 < constraints->cpbBrVclFactor);
    assert(0 < constraints->cpbBrNalFactor);

    MaxBR = constraints->MaxBR;
    maxNalBitrate = MaxBR * constraints->cpbBrNalFactor;
    maxVclBitrate = MaxBR * constraints->cpbBrVclFactor;

    MaxCPB = constraints->MaxCPB;
    maxNalCpbBufSize = MaxCPB * constraints->cpbBrNalFactor;
    maxVclCpbBufSize = MaxCPB * constraints->cpbBrVclFactor;

    /* Rec. ITU-T H.264 A.3.1.j) */
    /* Rec. ITU-T H.264 A.3.3.g) */
    if (maxNalBitrate < nalHrdParam->schedSel[SchedSelIdx].bitRate)
      LIBBLU_H264_HRDV_ERROR_NRETURN(
        "Rec. ITU-T H.264 %s constraint is not satisfied "
        "(%s * MaxBR = %u b/s < NAL HRD BitRate[%u] = %u b/s).\n",
        (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
          "A.3.1.j)"
        :
          "A.3.3.g)",
        (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
          "1200"
        :
          "cpbBrNalFactor",
        maxNalBitrate,
        SchedSelIdx,
        nalHrdParam->schedSel[SchedSelIdx].bitRate
      );

    if (H264_BDAV_MAX_BITRATE < nalHrdParam->schedSel[SchedSelIdx].bitRate)
      LIBBLU_H264_HRDV_ERROR_NRETURN(
        "Bitrate value exceed BDAV limits "
        "(%u b/s < NAL HRD BitRate[%u] = %u b/s).\n",
        (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
          "1200"
        :
          "cpbBrNalFactor",
        H264_BDAV_MAX_BITRATE,
        SchedSelIdx,
        nalHrdParam->schedSel[SchedSelIdx].bitRate
      );

    if (maxNalCpbBufSize < nalHrdParam->schedSel[SchedSelIdx].cpbSize) {
      LIBBLU_H264_HRDV_ERROR_NRETURN(
        "Rec. ITU-T H.264 %s constraint is not satisfied "
        "(%s * MaxCPB = %u bits < NAL HRD CpbSize[%u] = %u bits).\n",
        (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
          "A.3.1.j)"
        :
          "A.3.3.g)",
        (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
          "1200"
        :
          "cpbBrNalFactor",
        maxNalCpbBufSize,
        SchedSelIdx,
        nalHrdParam->schedSel[SchedSelIdx].cpbSize
      );
    }

    if (H264_BDAV_MAX_CPB_SIZE < nalHrdParam->schedSel[SchedSelIdx].cpbSize)
      LIBBLU_H264_HRDV_ERROR_NRETURN(
        "Bitrate value exceed BDAV limits "
        "(%u bits < NAL HRD CpbSize[%u] = %u bits).\n",
        H264_BDAV_MAX_CPB_SIZE,
        SchedSelIdx,
        nalHrdParam->schedSel[SchedSelIdx].cpbSize
      );

    /* Rec. ITU-T H.264 A.3.1.i) */
    /* Rec. ITU-T H.264 A.3.3.h) */
    if (vuiParam->vclHrdParamPresent) {
      vclHrdParam = &vuiParam->vclHrdParam;

      if (maxVclBitrate < vclHrdParam->schedSel[SchedSelIdx].bitRate) {
        LIBBLU_H264_HRDV_ERROR_NRETURN(
          "Rec. ITU-T H.264 %s constraint is not satisfied"
          "(%s * MaxBR = %u b/s < VCL HRD BitRate[%u] = %u b/s).\n",
          (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
            "A.3.1.i)"
          :
            "A.3.3.h)",
          (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
            "1000"
          :
            "cpbBrVclFactor",
          maxVclBitrate,
          SchedSelIdx,
          vclHrdParam->schedSel[SchedSelIdx].bitRate
        );
      }

      if (maxVclCpbBufSize < vclHrdParam->schedSel[SchedSelIdx].cpbSize) {
        LIBBLU_H264_HRDV_ERROR_NRETURN(
          "Rec. ITU-T H.264 %s constraint is not satisfied"
          "(%s * MaxCPB = %u b/s < VCL HRD CpbSize[%u] = %u b/s).\n",
          (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
            "A.3.1.i)"
          :
            "A.3.3.h)",
          (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(spsData->profileIdc)) ?
            "1000"
          :
            "cpbBrVclFactor",
          maxVclCpbBufSize,
          SchedSelIdx,
          vclHrdParam->schedSel[SchedSelIdx].cpbSize
        );
      }
    }
  }

  /* Allocate the context: */
  ctx = (H264HrdVerifierContextPtr) calloc(
    1, sizeof(H264HrdVerifierContext)
  );
  if (NULL == ctx)
    LIBBLU_H264_HRDV_ERROR_FRETURN("Memory allocation error.\n");

  /* Prepare FIFO: */
  assert((H264_MAX_AU_IN_CPB & (H264_MAX_AU_IN_CPB - 1)) == 0);
    /* Assert H264_MAX_AU_IN_CPB is a pow 2 */
  ctx->AUInCpbHeap = ctx->AUInCpb; /* Place head on first cell. */

  assert((H264_MAX_DPB_SIZE & (H264_MAX_DPB_SIZE - 1)) == 0);
    /* Assert H264_MAX_DPB_SIZE is a pow 2 */
  ctx->picInDpbHeap = ctx->picInDpb; /* Place head on first cell. */

  /* Define timing values: */
  ctx->second = (double) H264_90KHZ_CLOCK * vuiParam->timingInfo.timeScale;
  ctx->tick90 = (uint64_t) vuiParam->timingInfo.timeScale;
  ctx->numUnitsInTick = (uint64_t) vuiParam->timingInfo.numUnitsInTick;
  ctx->clockTick = H264_90KHZ_CLOCK * ctx->numUnitsInTick;
  ctx->bitrate =
    ((double) nalHrdParam->schedSel[SchedSelIdx].bitRate)
    / ctx->second
  ;
  ctx->cbr = nalHrdParam->schedSel[SchedSelIdx].cbrFlag;
  ctx->cpbSize = (size_t) nalHrdParam->schedSel[SchedSelIdx].cpbSize;
  ctx->dpbSize = MIN(
    MaxDpbMbs / (spsData->PicWidthInMbs * spsData->FrameHeightInMbs),
    16
  );
  ctx->maxNumRefFrames = spsData->maxNumRefFrames;
  ctx->MaxFrameNum = spsData->MaxFrameNum;

  setFromOptionsH264HrdVerifierDebugFlags(&ctx->hrdDebugFlags, options);

  /* Debug: */
  if (NULL != options->echoHrdLogFilepath) {
    assert(ctx->hrdDebugFlags.fileOutput);

    /* Open debug output file: */
    if (NULL == (ctx->hrdDebugFd = lbc_fopen(options->echoHrdLogFilepath, "w")))
      LIBBLU_H264_HRDV_ERROR_FRETURN(
        "Unable to open debugging output file '%s', %s (errno %d).\n",
        options->echoHrdLogFilepath,
        strerror(errno),
        errno
      );
  }

  return ctx;

free_return:
  destroyH264HrdVerifierContext(ctx);
  return NULL;
}

void destroyH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
)
{
  if (NULL == ctx)
    return;

  if (ctx->hrdDebugFlags.fileOutput)
    fclose(ctx->hrdDebugFd);
  free(ctx);
}

void echoDebugH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  LibbluStatus status,
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);

  if (ctx->hrdDebugFlags.fileOutput) {
    va_list args_copy;

    va_copy(args_copy, args);
    lbc_vfprintf(ctx->hrdDebugFd, format, args_copy);
    va_end(args_copy);
  }

  echoMessageVa(status, format, args);
  va_end(args);
}

double convertTimeH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t time
)
{
  assert(NULL != ctx);

  return ((double) time) / ctx->second;
}

int addAUToCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  size_t length,
  uint64_t removalTime,
  unsigned AUIdx,
  H264DpbHrdPicInfos picInfos
)
{
  unsigned newCellIndex;
  H264CpbHrdAU * newCell;

  assert(NULL != ctx);

  if (ctx->nbAUInCpb == H264_MAX_AU_IN_CPB)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Too many access units in the CPB (%u).\n",
      H264_MAX_AU_IN_CPB
    );

  /* Get the next cell index in the FIFO using a modulo (and increase nb) */
  newCellIndex = (
    ((ctx->AUInCpbHeap - ctx->AUInCpb) + ctx->nbAUInCpb++)
    & H264_AU_CPB_MOD_MASK
  );

  newCell = ctx->AUInCpb + newCellIndex;
  newCell->AUIdx = AUIdx;
  newCell->length = length;
  newCell->removalTime = removalTime;
  newCell->picInfos = picInfos;

  return 0; /* OK */
}

int popAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
)
{
  unsigned newHeadCellIdx;

  assert(NULL != ctx);

  if (!ctx->nbAUInCpb)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "No access unit to pop from the CPB, the FIFO is empty.\n"
    );

  newHeadCellIdx =
    (ctx->AUInCpbHeap - ctx->AUInCpb + 1)
    & H264_AU_CPB_MOD_MASK
  ;
  ctx->AUInCpbHeap = ctx->AUInCpb + newHeadCellIdx;
  ctx->nbAUInCpb--;

  return 0; /* OK */
}

H264CpbHrdAU * getOldestAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
)
{
  if (!ctx->nbAUInCpb)
    return NULL; /* No AU currently in FIFO. */
  return ctx->AUInCpbHeap;
}

H264CpbHrdAU * getNewestAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
)
{
  unsigned newestAUIdx;

  assert(NULL != ctx);

  if (!ctx->nbAUInCpb)
    return NULL; /* No AU currently in FIFO. */

  newestAUIdx =
    (ctx->AUInCpbHeap - ctx->AUInCpb + ctx->nbAUInCpb)
    & H264_AU_CPB_MOD_MASK
  ;

  return ctx->AUInCpb + newestAUIdx;
}

int addDecodedPictureToH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU * pic,
  uint64_t outputTime
)
{
  unsigned i;
  H264DpbHrdPicInfos * picInfos;

  H264DpbHrdPic * newCell, * picture;

  assert(NULL != ctx);
  assert(NULL != pic);

  picInfos = &pic->picInfos;

  if (ctx->nbPicInDpb == H264_MAX_DPB_SIZE)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to append decoded picture, too many pictures in DPB.\n"
    );

  newCell = &ctx->picInDpb[
    ((ctx->picInDpbHeap - ctx->picInDpb) + ctx->nbPicInDpb)
    & H264_DPB_MOD_MASK
  ];
  newCell->AUIdx = pic->AUIdx;
  newCell->frameDisplayNum = picInfos->frameDisplayNum;
  newCell->frameNum = picInfos->frameNum;
  newCell->outputTime = outputTime;
  newCell->usage = picInfos->usage;

  switch (picInfos->usage) {
    case H264_USED_AS_LONG_TERM_REFERENCE:
      for (i = 0; i < ctx->nbPicInDpb; i++) {
        if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
          return -1; /* Unable to get i-index picture, broken FIFO. */

        if (
          H264_USED_AS_LONG_TERM_REFERENCE == picture->usage
          && picture->longTermFrameIdx == picInfos->longTermFrameIdx
        ) {
          /* NOTE: Shall NEVER happen, means broken longTermFrameIdx managment. */
          LIBBLU_H264_HRDV_ERROR_RETURN(
            "LongTermFrameIdx = %u is already used in DPB.\n",
            picInfos->longTermFrameIdx
          );
        }
      }
      newCell->longTermFrameIdx = picInfos->longTermFrameIdx;

      ctx->numLongTerm++;
      break;

    case H264_USED_AS_SHORT_TERM_REFERENCE:
      ctx->numShortTerm++;
      break;

    case H264_NOT_USED_AS_REFERENCE:
      break; /* Non-used as reference picture, no counter to update rather than nbPicInDpb. */
  }

  ctx->nbPicInDpb++;
  return 0; /* OK */
}

int setDecodedPictureAsRefUnusedInH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned idx
)
{
  H264DpbHrdPic * picture;

  assert(NULL != ctx);

  if (!ctx->nbPicInDpb)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to set a decoded reference picture as unused, "
      "empty DPB.\n"
    );

  if (ctx->nbPicInDpb <= idx)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to set a decoded reference picture as unused, "
      "out of range index %u.\n",
      idx
    );

  picture = &ctx->picInDpb[
    (ctx->picInDpbHeap - ctx->picInDpb + idx)
    & H264_DPB_MOD_MASK
  ];

  switch (picture->usage) {
    case H264_USED_AS_LONG_TERM_REFERENCE:
      ctx->numLongTerm--;
      break;

    case H264_USED_AS_SHORT_TERM_REFERENCE:
      ctx->numShortTerm--;
      break;

    case H264_NOT_USED_AS_REFERENCE:
      break; /* Non-used as reference picture, no counter to update rather than nbPicInDpb. */
  }

  picture->usage = H264_NOT_USED_AS_REFERENCE;

  return 0;
}

int popDecodedPictureFromH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned idx
)
{
  unsigned i, array_abs_idx;

  assert(NULL != ctx);

  if (!ctx->nbPicInDpb)
    LIBBLU_H264_HRDV_ERROR_RETURN("No DPB pic to pop, empty FIFO.\n");

  if (ctx->nbPicInDpb <= idx)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to pop picture, out of array DPB FIFO idx %u.\n",
      idx
    );

  array_abs_idx = (ctx->picInDpbHeap - ctx->picInDpb + idx);
  switch (ctx->picInDpb[array_abs_idx & H264_DPB_MOD_MASK].usage) {
    case H264_USED_AS_LONG_TERM_REFERENCE:
      ctx->numLongTerm--;
      break;

    case H264_USED_AS_SHORT_TERM_REFERENCE:
      ctx->numShortTerm--;
      break;

    case H264_NOT_USED_AS_REFERENCE:
      break; /* Non-used as reference picture, no counter to update rather than nbPicInDpb. */
  }

  if (idx == 0) {
    /* Pop top, advance new top */
    array_abs_idx = ctx->picInDpbHeap - ctx->picInDpb + 1;

    ctx->picInDpbHeap = &ctx->picInDpb[array_abs_idx & H264_DPB_MOD_MASK];
  }
  else {
    /* Pop inside array, move every following picture */
    for (i = 0; i < ctx->nbPicInDpb - idx - 1; i++) {
      array_abs_idx = ctx->picInDpbHeap - ctx->picInDpb + idx + i;

      ctx->picInDpb[array_abs_idx & H264_DPB_MOD_MASK] =
        ctx->picInDpb[(array_abs_idx + 1) & H264_DPB_MOD_MASK]
      ;
    }
  }

  ctx->nbPicInDpb--;
  return 0; /* OK */
}

H264DpbHrdPic * getDecodedPictureFromH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned idx
)
{
  unsigned array_abs_idx;

  assert(NULL != ctx);

  if (ctx->nbPicInDpb <= idx)
    LIBBLU_H264_HRDV_ERROR_NRETURN(
      "Unable to get picture, Out of array DPB FIFO idx %u.\n",
      idx
    );

  array_abs_idx = ctx->picInDpbHeap - ctx->picInDpb + idx;

  return &ctx->picInDpb[array_abs_idx & H264_DPB_MOD_MASK];
}

void printDPBStatusH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  unsigned i;
  char * sep;

  ECHO_DEBUG_DPB_HRDV_CTX(ctx, " -> DPB content:");

  sep = "";
  for (i = 0; i < ctx->nbPicInDpb; i++) {
    H264DpbHrdPic * pic;

    if (NULL == (pic = getDecodedPictureFromH264HrdContext(ctx, i)))
      return;

    ECHO_DEBUG_NH_DPB_HRDV_CTX(
      ctx, "%s[%u: %u/%u]",
      sep, i,
      pic->AUIdx,
      pic->frameDisplayNum
    );

    sep = ", ";
  }

  if (!i)
    ECHO_DEBUG_NH_DPB_HRDV_CTX(ctx, "*empty*");
  ECHO_DEBUG_NH_DPB_HRDV_CTX(ctx, ".\n");
}

#if 0
int writeH264HrdContextDebug(
  H264HrdVerifierContextPtr ctx,
  double time,
  size_t value
)
{
  assert(NULL != ctx);

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, "%f %zu\n",
    time,
    value
  );

  return 0;
}
#endif

int manageSlidingWindowProcessDPBH264Context(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum
)
{
  /* 8.2.5.3 Sliding window decoded reference picture marking process */
  unsigned i, FrameNumWrap, minFrameNumWrap;
  H264DpbHrdPic * picture;

  unsigned oldestFrame;
  bool oldestFrameInit;

  if (MAX(ctx->maxNumRefFrames, 1) <= ctx->numShortTerm + ctx->numLongTerm) {
    if (!ctx->numShortTerm)
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "DPB reference pictures shall no be only used as long-term reference "
        "(Not enought space available).\n"
      );

    oldestFrameInit = false;
    minFrameNumWrap = 0;
    oldestFrame = 0;

    for (i = 0; i < ctx->nbPicInDpb; i++) {
      if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
        return -1; /* Unable to get i-index picture, broken FIFO. */

      if (picture->usage == H264_USED_AS_SHORT_TERM_REFERENCE) {
        if (frameNum < picture->frameNum)
          FrameNumWrap = picture->frameNum - ctx->MaxFrameNum;
        else
          FrameNumWrap = picture->frameNum;

        if (!oldestFrameInit)
          oldestFrame = i, minFrameNumWrap = FrameNumWrap, oldestFrameInit = true;
        else if (FrameNumWrap < minFrameNumWrap)
          oldestFrame = i, minFrameNumWrap = FrameNumWrap;
      }
    }
    assert(oldestFrameInit);
      /* Shall be at least one 'short-term reference' picture,
        or numShortTerm is broken. */

    if (setDecodedPictureAsRefUnusedInH264HrdContext(ctx, oldestFrame) < 0)
      return -1;
  }

  return 0;
}

int applyDecodedReferencePictureMarkingProcessDPBH264Context(
  H264HrdVerifierContextPtr ctx,
  H264DpbHrdPicInfos * infos
)
{
  /* 8.2.5.1 Sequence of operations for decoded reference picture marking process */

  assert(NULL != ctx);
  assert(NULL != infos);

  assert(ctx->numLongTerm + ctx->numShortTerm <= ctx->nbPicInDpb);

  if (infos->idrPic) {
    if (markAllDecodedPicturesAsUnusedH264HrdContext(ctx) < 0)
      return -1;

    if (infos->usage != H264_USED_AS_LONG_TERM_REFERENCE)
      ctx->maxLongTermFrameIdx = -1;
    else {
      assert(infos->usage == H264_USED_AS_SHORT_TERM_REFERENCE);

      infos->longTermFrameIdx = 0;
      ctx->maxLongTermFrameIdx = 0;
    }
  }
  else {
    if (NULL != infos->memMngmntCtrlOperations) {
      /* Invoke 8.2.5.4 */
      /* TODO */

      assert(0);
    }
    else {
      /* Check if adaptive_ref_pic_marking_mode_flag can be set to 0b0. */
      /* => MAX(max_num_ref_frames, 1) < numLongTerm */
      if (MAX(ctx->maxNumRefFrames, 1) <= ctx->numLongTerm) {
        LIBBLU_H264_HRDV_ERROR(
          "Parameter 'adaptive_ref_pic_marking_mode_flag' shall be defined "
          "to 0b1 (Max(max_num_ref_frames, 1) <= numLongTerm).\n"
        );
        LIBBLU_H264_HRDV_ERROR_RETURN(
          " -> Too many long-term pictures in DPB, causing an overflow "
          "(%u over %u max ref frames).\n",
          ctx->numLongTerm,
          MAX(ctx->maxNumRefFrames, 1)
        );
      }

      /* Invoke 8.2.5.3 */
      if (manageSlidingWindowProcessDPBH264Context(ctx, infos->frameNum) < 0)
        return -1;
    }
  }

  if (MAX(ctx->maxNumRefFrames, 1) < ctx->numShortTerm + ctx->numLongTerm) {
    LIBBLU_H264_HRDV_ERROR(
      "DPB reference pictures shall be less than "
      "Max(max_num_ref_frames, 1).\n"
    );
    LIBBLU_H264_HRDV_ERROR_RETURN(
      " => Currently: %u (Max: %u).\n",
      ctx->numShortTerm + ctx->numLongTerm,
      MAX(ctx->maxNumRefFrames, 1)
    );
  }

  return 0;
}

int updateDPBH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t currentTime
)
{
  /* Remove unused pictures (as reference) from DPB if their release time as been reached. */
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb;) {
    if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (
      H264_NOT_USED_AS_REFERENCE == picture->usage
      && picture->outputTime <= currentTime
    ) {
      if (popDecodedPictureFromH264HrdContext(ctx, i) < 0)
        return -1;
    }
    else
      i++;
  }

  return 0; /* OK */
}

void clearDPBH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  assert(NULL != ctx);

  ctx->nbPicInDpb = 0;
  ctx->numShortTerm = 0;
  ctx->numLongTerm = 0;
}

int markAllDecodedPicturesAsUnusedH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    picture->usage = H264_NOT_USED_AS_REFERENCE;
  }
  ctx->numShortTerm = 0;
  ctx->numLongTerm = 0;

  return 0;
}

int markShortTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum
)
{
  /* memory_management_control_operation == 0x1 usage. */
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frameNum == frameNum) {
      if (H264_USED_AS_SHORT_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "frame_num %u index does not correspond to a "
          "'short-term reference' picture.\n",
          frameNum
        );

      assert(0 < ctx->numShortTerm);

      picture->usage = H264_NOT_USED_AS_REFERENCE;
      ctx->numShortTerm--;
      return 0;
    }
  }

  LIBBLU_H264_HRDV_ERROR_RETURN(
    "Unable to mark short term reference picture num %u, "
    "unknown frame number.\n",
    frameNum
  );
}

int markLongTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum
)
{
  /* memory_management_control_operation == 0x2 usage. */
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frameNum == frameNum) {
      if (H264_USED_AS_LONG_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          H264_DPB_HRD_MSG_PREFIX
          "frame_num %u index does not correspond to a "
          "'long-term reference' picture.\n",
          frameNum
        );

      assert(0 < ctx->numLongTerm);

      picture->usage = H264_NOT_USED_AS_REFERENCE;
      ctx->numLongTerm--;
      return 0;
    }
  }

  LIBBLU_H264_HRDV_ERROR_RETURN(
    "Unable to mark long term reference picture num %u, "
    "unknown frame number.\n",
    frameNum
  );
}

int markShortTermRefPictureAsUsedForLongTermReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum,
  unsigned longTermFrameIdx
)
{
  /* memory_management_control_operation == 0x3 usage. */
  H264DpbHrdPic * picture, * subPicture;
  unsigned i;

  assert(NULL != ctx);

  if (
    ctx->maxLongTermFrameIdx < 0 ||
    (unsigned) ctx->maxLongTermFrameIdx <= longTermFrameIdx
  ) {
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Specified LongTermFrameIdx (=%u) exceeds MaxLongTermFrameIdx (=%d).\n",
      longTermFrameIdx,
      ctx->maxLongTermFrameIdx
    );
  }

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frameNum == frameNum) {
      if (H264_USED_AS_SHORT_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "frame_num %u index does not correspond to a "
          "'long-term reference' picture.\n",
          frameNum
        );

      for (i = 0; i < ctx->nbPicInDpb; i++) {
        /* Clearing previously used LongTermFrameIdx */
        if (NULL == (subPicture = getDecodedPictureFromH264HrdContext(ctx, i)))
          return -1; /* Unable to get i-index picture, broken FIFO. */

        if (
          H264_USED_AS_LONG_TERM_REFERENCE == subPicture->usage
          && subPicture->longTermFrameIdx == longTermFrameIdx
        ) {
          assert(0 < ctx->numLongTerm);

          subPicture->usage = H264_NOT_USED_AS_REFERENCE;
          ctx->numLongTerm--;
          break;
        }
      }

      assert(0 < ctx->numShortTerm);

      picture->usage = H264_USED_AS_LONG_TERM_REFERENCE;
      picture->longTermFrameIdx = longTermFrameIdx;
      ctx->numLongTerm++;
      ctx->numShortTerm--;
      return 0;
    }
  }

  LIBBLU_H264_HRDV_ERROR_RETURN(
    "Unable to mark short term reference picture num %u, "
    "unknown frame number.\n",
    frameNum
  );
}

int defineMaxLongTermFrameIdxH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  int maxLongTermFrameIdx
)
{
  /* memory_management_control_operation == 0x4 usage. */
  H264DpbHrdPic * picture;
  unsigned i;

  bool update;

  assert(NULL != ctx);

  if (maxLongTermFrameIdx < ctx->maxLongTermFrameIdx)
    update = true;
  ctx->maxLongTermFrameIdx = maxLongTermFrameIdx;

  if (update) {
    /* Mark all long-term reference pictures with
     * MaxLongTermFrameIdx < LongTermFrameIdx as
     * 'unused for reference'.
     */

    for (i = 0; i < ctx->nbPicInDpb; i++) {
      if (NULL == (picture = getDecodedPictureFromH264HrdContext(ctx, i)))
        return -1; /* Unable to get i-index picture, broken FIFO. */

      if (
        picture->usage == H264_USED_AS_LONG_TERM_REFERENCE
        && (
          maxLongTermFrameIdx < 0
          || (unsigned) maxLongTermFrameIdx < picture->longTermFrameIdx
        )
      ) {
        assert(0 < ctx->numLongTerm);

        picture->usage = H264_NOT_USED_AS_REFERENCE;
        ctx->numLongTerm--;
      }
    }
  }

  return 0;
}

int clearAllReferencePicturesH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  /* memory_management_control_operation == 0x5 usage. */

  assert(NULL != ctx);

  if (markAllDecodedPicturesAsUnusedH264HrdContext(ctx) < 0)
    return -1;

  ctx->maxLongTermFrameIdx = -1;

  return 0;
}

int checkH264CpbHrdConformanceTests(
  H264ConstraintsParam * constraints,
  H264HrdVerifierContextPtr ctx,
  size_t AUlength,
  bool AUIsNewBufferingPeriod,
  H264HrdBufferingPeriodParameters * AUNalHrdParam,
  H264SPSDataParameters * AUSpsData,
  H264SliceHeaderParameters * AUSliceHeader,
  uint64_t Tr_n, unsigned AUNbSlices
)
{
  /* Rec. ITU-T H.264 - Annex C.3 Bitstream conformance */

  uint64_t initialCpbRemovalDelay;
  uint64_t Tf_nMinusOne, Tr_nMinusOne;

  double delta_tg90; /* H.264 C-14 equation. */
  unsigned maxMBPS; /* H.264 Table A-1 linked to levelIdc. */
  double fR; /* H.264 C.3.4. A.3.1/2.a) */
  double minCpbRemovalDelay; /* H.264 C.3.4. A.3.1/2.a) Max(PicSizeInMbs ÷ MaxMBPS, fR) result */
  size_t maxAULength;
  unsigned sliceRate; /* H.264 A.3.3.a) */
  unsigned maxNbSlices; /* H.264 A.3.3.a) */

  assert(NULL != ctx);
  assert(NULL != AUNalHrdParam);
  assert(NULL != AUSpsData);
  assert(NULL != AUSliceHeader);

  initialCpbRemovalDelay = AUNalHrdParam->initialCpbRemovalDelay; /* initial_cpb_removal_delay */

  if (60 <= AUSpsData->levelIdc && AUSpsData->levelIdc <= 62)
    fR = 1.0 / 300.0;
  else
    fR = (AUSliceHeader->fieldPic) ? 1.0 / 344.0 : 1.0 / 172.0;

  if (0 < ctx->nbProcessedAU) {

    Tf_nMinusOne = ctx->clockTime; /* At every process, clockTime stops on AU final arrival time. */
    Tr_nMinusOne = ctx->nMinusOneAUParameters.removalTime;

    /* H.264 C.3.1. */
    if (
      AUIsNewBufferingPeriod
      && (
        ctx->nMinusOneAUParameters.initialCpbRemovalDelay
          == initialCpbRemovalDelay
      )
    ) {
      /* C-15/C-16 equation check. */
      /* No check if new buffering period introduce different buffering from last period. */

      delta_tg90 = convertTimeH264HrdVerifierContext(ctx, Tr_n - Tf_nMinusOne) * H264_90KHZ_CLOCK;

      /* H.264 C-15/C-16 ceil equation conformance : */
      if (ceil(delta_tg90) < initialCpbRemovalDelay)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 %s equation is not satisfied "
          "(delta_tg90 = %.0f < initial_cpb_removal_delay = %" PRIu64 ").\n",
          (ctx->cbr) ? "C-16" : "C-15",
          ceil(delta_tg90),
          initialCpbRemovalDelay
        );

      /* H.264 C-16 floor equation conformance : */
      if (ctx->cbr && initialCpbRemovalDelay < floor(delta_tg90))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 C-16 equation is not satisfied "
          "(initial_cpb_removal_delay = %" PRIu64 " < delta_tg90 = %.0f).\n",
          initialCpbRemovalDelay, floor(delta_tg90)
        );
    }

    /* NOTE: A.3.1, A.3.2 and A.3.3 conformance verifications are undue by C.3.4 constraint. */

    /* H.264 A.3.1.a) and A.3.2.a) */
    if (0 == (maxMBPS = getH264MaxMBPS(ctx->nMinusOneAUParameters.levelIdc)))
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Unable to find a MaxMBPS value for levelIdc = 0x%" PRIx8 ".\n",
        ctx->nMinusOneAUParameters.levelIdc
      );

    minCpbRemovalDelay = MAX(
      (double) ctx->nMinusOneAUParameters.picSizeInMbs / maxMBPS,
      fR
    );

    if ((double) (Tr_n - Tr_nMinusOne) < minCpbRemovalDelay * ctx->second)
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Rec. ITU-T H.264 %s constraint is not satisfied "
        "(t_r,n(n) - t_t(n-1) = %f < Max(PicSizeInMbs / MaxMBPS, fR) = %f).\n",
        (H264_PROFILE_IS_HIGH(AUSpsData->profileIdc)) ? "4.3.2.a)" : "4.3.1.a)",
        convertTimeH264HrdVerifierContext(ctx, Tr_n - Tr_nMinusOne),
        minCpbRemovalDelay
      );

    /* H.264 A.3.1.d) and A.3.3.j) */
    if (
      AUSpsData->profileIdc == H264_PROFILE_BASELINE
      || AUSpsData->profileIdc == H264_PROFILE_MAIN
      || AUSpsData->profileIdc == H264_PROFILE_EXTENDED

      || AUSpsData->profileIdc == H264_PROFILE_HIGH
    ) {
      assert(0 != AUSliceHeader->picSizeInMbs);

      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for levelIdc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.levelIdc
        );

      maxAULength = (size_t) ABS(
        384.0
        * (double) maxMBPS
        * convertTimeH264HrdVerifierContext(ctx, Tr_n - Tr_nMinusOne)
        / (double) getH264MinCR(AUSpsData->levelIdc)
        * 8.0
      );
      /* NOTE: This formula miss the part induced by low_delay flag. */

      if (maxAULength < AUlength)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 A.3.1.d) constraint is not satisfied "
          "(MaxNumBytesInNALunit = %zu < NumBytesInNALunit = %zu).\n",
          maxAULength / 8,
          AUlength / 8
        );
    }

    /* H.264 A.3.3.b) */
    if (
      AUSpsData->profileIdc == H264_PROFILE_MAIN
      || AUSpsData->profileIdc == H264_PROFILE_HIGH
      || AUSpsData->profileIdc == H264_PROFILE_HIGH_10
      || AUSpsData->profileIdc == H264_PROFILE_HIGH_422
      || AUSpsData->profileIdc == H264_PROFILE_HIGH_444_PREDICTIVE
      || AUSpsData->profileIdc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for levelIdc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.levelIdc
        );

      if (0 == (sliceRate = constraints->SliceRate))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a SliceRate value for levelIdc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.levelIdc
        );

      maxNbSlices = (unsigned) (
        (double) maxMBPS
        * convertTimeH264HrdVerifierContext(ctx, Tr_n - Tr_nMinusOne)
        / (double) sliceRate
      );

      if (maxNbSlices < AUNbSlices)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 A.3.3.b) constraint is not satisfied "
          "(maxNbSlices = %u < nbSlices = %u).\n",
          maxNbSlices,
          AUNbSlices
        );
    }
  }
  else {
    /* Access Unit 0 */

    /* H.264 A.3.1.c) and A.3.3.i) */
    if (
      AUSpsData->profileIdc == H264_PROFILE_BASELINE
      || AUSpsData->profileIdc == H264_PROFILE_MAIN
      || AUSpsData->profileIdc == H264_PROFILE_EXTENDED

      || AUSpsData->profileIdc == H264_PROFILE_HIGH
    ) {
      assert(0 != AUSliceHeader->picSizeInMbs);

      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for levelIdc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.levelIdc
        );

      maxAULength = (size_t) ABS(
        384.0
        * MAX((double) AUSliceHeader->picSizeInMbs, fR * maxMBPS)
        / (double) getH264MinCR(AUSpsData->levelIdc)
        * 8.0
      );
      /* NOTE: This formula miss the part induce by low_delay flag. */

      if (maxAULength < AUlength)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          H264_HRD_VERIFIER_NAME "Rec. ITU-T H.264 A.3.1.c) constraint is not satisfied "
          "(MaxNumBytesInFirstAU = %zu < NumBytesInFirstAU = %zu).\n",
          maxAULength / 8,
          AUlength / 8
        );
    }

    /* H.264 A.3.3.a) */
    if (
      AUSpsData->profileIdc == H264_PROFILE_MAIN
      || AUSpsData->profileIdc == H264_PROFILE_HIGH
      || AUSpsData->profileIdc == H264_PROFILE_HIGH_10
      || AUSpsData->profileIdc == H264_PROFILE_HIGH_422
      || AUSpsData->profileIdc == H264_PROFILE_HIGH_444_PREDICTIVE
      || AUSpsData->profileIdc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for levelIdc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.levelIdc
        );

      if (0 == (sliceRate = constraints->SliceRate))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a SliceRate value for levelIdc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.levelIdc
        );

      maxNbSlices = (unsigned) ceil(
        MAX((double) AUSliceHeader->picSizeInMbs, fR * maxMBPS)
        / sliceRate
      );

      if (maxNbSlices < AUNbSlices)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 A.3.3.a) constraint is not satisfied "
          "(maxNbSlices = %u < nbSlices = %u).\n",
          maxNbSlices,
          AUNbSlices
        );
    }
  }

  return 0; /* OK */
}

int processAUH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264SPSDataParameters * spsData,
  H264SliceHeaderParameters * sliceHeader,
  H264SeiPicTiming * picTimingSei,
  H264SeiBufferingPeriod * bufPeriodSei,
  H264CurrentProgressParam * curState,
  H264ConstraintsParam * constraints,
  bool isNewBufferingPeriod,
  size_t AUlength,
  bool doubleFrameTiming
)
{
  H264HrdBufferingPeriodParameters * nalHrdParam;
  ;
  H264DecRefPicMarking * decRefPicMarking;
  H264DpbHrdPicInfos picInfos;

  uint64_t initialCpbRemovalDelay, initialCpbRemovalDelayOff, initialCpbTotalRemovalDelay;
  uint64_t cpbRemovalDelay, dpbOutputDelay;

  uint64_t Tr_n, Te_n, Ta_n, Tf_n, Tout_n;
  uint64_t Tf_nMinusOne;

  size_t instantaneousAlreadyTransferedCpbBits;

  bool storePicFlag; /* C.2.4.2 */
  H264CpbHrdAU * cpbExtractedPic;

  assert(NULL != ctx);
  assert(NULL != spsData);
  assert(NULL != sliceHeader);
  assert(NULL != picTimingSei);

  /* Check for required parameters : */
  decRefPicMarking = &sliceHeader->decRefPicMarking; /* Decoded reference picture marking */

  nalHrdParam = &bufPeriodSei->nalHrdParam[0];

  /* Parse required parameters from signal : */
  initialCpbRemovalDelay = nalHrdParam->initialCpbRemovalDelay; /* initial_cpb_removal_delay */
  initialCpbRemovalDelayOff = nalHrdParam->initialCpbRemovalDelayOff; /* initial_cpb_removal_delay */

  cpbRemovalDelay = picTimingSei->cpbRemovalDelay; /* cpb_removal_delay */
  dpbOutputDelay = picTimingSei->dpbOutputDelay; /* dpb_output_delay */

  /* A new buffering period is defined as an AU with buffering_period SEI message: */
  /* isNewBufferingPeriod = param->sei.bufferingPeriodValid; */

  Tf_nMinusOne = ctx->clockTime; /* At every process, clockTime stops on AU final arrival time. */

#if ELECARD_STREAMEYE_COMPARISON
  /* StreamEye counts two times SPS and PPS NALUs, so for debug, manually add size of such NALUs in bits. */
  LIBBLU_DEBUG_COM(
    "CPB HRD: Don't forget to update SPS and PPS sizes in "
    "processAUH264HrdContext() function for StreamEye comparison.\n"
  );

  if (ctx->nbProcessedAU == 0)
    AUlength += 520; /* For '00019_track2.avc' test file (SPS: 57 bytes + PPS: 8 bytes = 65 bytes = 520 bits). */
#endif

  /* Compute AU timing values: */

  /* t_r(n) - removal time: */
  if (ctx->nbProcessedAU == 0)
    Tr_n = initialCpbRemovalDelay * ctx->tick90; /* C-7 */
  else
    Tr_n = ctx->nominalRemovalTimeFirstAU + ctx->clockTick * cpbRemovalDelay; /* C-8 / C-9 */

  /* Since low_delay_hrd_flag is not supported, C-10 and C-11 equations are not used. */

  /* Saving CPB removal time */
  ctx->removalTimeAU = curState->auCpbRemovalTime = (uint64_t) (
    convertTimeH264HrdVerifierContext(ctx, Tr_n)
    * MAIN_CLOCK_27MHZ
  );
  ctx->outputTimeAU = curState->auDpbOutputTime = (uint64_t) (
    convertTimeH264HrdVerifierContext(
      ctx,
      Tr_n + ctx->clockTick * dpbOutputDelay
    ) * MAIN_CLOCK_27MHZ
  );

  if (isNewBufferingPeriod) /* n_b = n */
    ctx->nominalRemovalTimeFirstAU = Tr_n; /* Saving new t_r(n_b) */

  if (!ctx->cbr) {
    /* VBR case */

    /* t_e(n)/t_ai,earliest(n) - earliest initial arrival time: */
    initialCpbTotalRemovalDelay =
      ctx->tick90 * (
        initialCpbRemovalDelay
        + ((!isNewBufferingPeriod) ? initialCpbRemovalDelayOff : 0)
      )
    ;

    if (initialCpbTotalRemovalDelay < Tr_n)
      Te_n = Tr_n - initialCpbTotalRemovalDelay;
    else
      Te_n = 0; /* Rounding to 0, avoid negative unsigned error. */

    /* t_a(n)/t_ai(n) - initial arrival time: */
    Ta_n = MAX(Tf_nMinusOne, Te_n);
  }
  else
    Ta_n = Tf_nMinusOne; /* CBR case */

  /* t_f(n)/t_af(n) - final arrival time: */
  Tf_n = Ta_n + (uint64_t) ABS(AUlength / ctx->bitrate);

  if (Tr_n < Tf_n) {
    /**
     * Final arrival time is bigger than removal time, only a part of AU will
     * be currently in CPB at removal time.
     *  => Buffer underflow will happen
     */

    LIBBLU_H264_HRDV_ERROR(
      "Rec. ITU-T H.264 C.3.3 constraint is not satisfied "
      "(CPB Underflow will happen, Tr_n = %f s < Tf_n = %f s).\n",
      convertTimeH264HrdVerifierContext(ctx, Tr_n),
      convertTimeH264HrdVerifierContext(ctx, Tf_n)
    );
    LIBBLU_H264_HRDV_ERROR_RETURN(
      " => Affect Access Unit %u, with initial arrival time: %f s.\n",
      ctx->nbProcessedAU,
      convertTimeH264HrdVerifierContext(ctx, Ta_n)
    );
  }

  /* Rec. ITU-T H.264 - C.3 Bitstream conformance Checks: */
  if (
    checkH264CpbHrdConformanceTests(
      constraints,
      ctx,
      AUlength, isNewBufferingPeriod,
      nalHrdParam, spsData, sliceHeader,
      Tr_n, curState->curNbSlices
    ) < 0
  )
    return -1;

  /* Remove AU with releaseClockTime reached: */
  while (
    NULL != (cpbExtractedPic = getOldestAUFromCPBH264HrdVerifierContext(ctx))
    && cpbExtractedPic->removalTime <= Tf_n
  ) {
    ctx->clockTime = cpbExtractedPic->removalTime;

    if (Ta_n < ctx->clockTime)
      instantaneousAlreadyTransferedCpbBits =
        (size_t) ABS(
          ctx->bitrate
          * (double) (ctx->clockTime - Ta_n)
        )
      ; /* Adds bits from current AU already buffered according to bitrate. */
    else
      instantaneousAlreadyTransferedCpbBits = 0;

    if (
      ctx->cpbSize
      < ctx->cpbBitsOccupancy + instantaneousAlreadyTransferedCpbBits
    ) {
      /* cpbBitsOccupancy + number of bits from current AU already buffered */
      /* exceed CPB size. => Buffer overflow */

      LIBBLU_H264_HRDV_ERROR(
        "Rec. ITU-T H.264 C.3.2 constraint is not satisfied "
        "(CPB Overflow happen, CPB size = %zu bits < %zu bits).\n",
        ctx->cpbSize,
        ctx->cpbBitsOccupancy + instantaneousAlreadyTransferedCpbBits
      );
      LIBBLU_H264_HRDV_ERROR(
        " => Affect while trying to remove Access Unit %u, "
        "at removal time: %f ms.\n",
        cpbExtractedPic->AUIdx,
        convertTimeH264HrdVerifierContext(ctx, ctx->clockTime) * 1000
      );

      if (Ta_n < ctx->clockTime) {
        LIBBLU_H264_HRDV_ERROR_RETURN(
          " => Access Unit %u was in transfer "
          "(%zu bits already in CPB over %zu total bits).\n",
          ctx->nbProcessedAU,
          (size_t) ABS(ctx->bitrate * (double) (ctx->clockTime - Ta_n)),
          AUlength
        );
      }

      if (0 < ctx->nbProcessedAU) {
        LIBBLU_H264_HRDV_ERROR_RETURN(
          " => Happen during final arrival time of Access Unit %u "
          "and initial arrival time of Access Unit %u interval.\n",
          ctx->nbProcessedAU - 1,
          ctx->nbProcessedAU
        );
      }

      return -1;
    }

    /* Else AU can be removed safely. */
    assert(cpbExtractedPic->length < ctx->cpbBitsOccupancy);
      /* Buffer underflow shouldn't happen here. */

    ECHO_DEBUG_CPB_HRDV_CTX(
      ctx, "Removing %zu bits from CPB "
      "(picture %u, currently removed: %zu bits) at %f ms.\n",
      cpbExtractedPic->length,
      cpbExtractedPic->AUIdx,
      MIN(ctx->cpbBitsOccupancy, cpbExtractedPic->length),
      convertTimeH264HrdVerifierContext(ctx, ctx->clockTime) * 1000
    );

    ctx->cpbBitsOccupancy -= MIN(
      ctx->cpbBitsOccupancy,
      cpbExtractedPic->length
    );

    ECHO_DEBUG_CPB_HRDV_CTX(
      ctx, " -> CPB fullness: %zu bits.\n",
      ctx->cpbBitsOccupancy + instantaneousAlreadyTransferedCpbBits
    );

    /* Transfer decoded picture to DPB: */
    cpbExtractedPic = cpbExtractedPic;
    picInfos = cpbExtractedPic->picInfos;

    if (applyDecodedReferencePictureMarkingProcessDPBH264Context(ctx, &picInfos) < 0)
      return -1;
    closeH264MemoryManagementControlOperations(picInfos.memMngmntCtrlOperations); /* TODO */
    picInfos.memMngmntCtrlOperations = NULL;

    /* C.2.3.2. Not applied. */

    ECHO_DEBUG_DPB_HRDV_CTX(
      ctx, "Adding picture %u to DPB (display: %u) at %f ms.\n",
      cpbExtractedPic->AUIdx,
      picInfos.frameDisplayNum,
      convertTimeH264HrdVerifierContext(ctx, ctx->clockTime) * 1000
    );

    if (updateDPBH264HrdContext(ctx, ctx->clockTime) < 0) /* Update before insertion. */
      return -1;

    Tout_n = ctx->clockTime + ctx->clockTick * picInfos.dpbOutputDelay;

    storePicFlag = false;
    if (picInfos.usage == H264_NOT_USED_AS_REFERENCE) {
      /* C.2.4.2 Storage of a non-reference picture into the DPB */
      storePicFlag = (Tout_n > ctx->clockTime);
    }

    if (storePicFlag) {
      if (addDecodedPictureToH264HrdContext(ctx, cpbExtractedPic, Tout_n) < 0)
        return -1;
    }
    else
      ECHO_DEBUG_DPB_HRDV_CTX(
        ctx, " -> Not stored in DPB (according to Rec. ITU-T H.264 C.2.4.2).\n"
      );

    printDPBStatusH264HrdContext(ctx);

    if (ctx->dpbSize < ctx->nbPicInDpb) {
      LIBBLU_ERROR(
        H264_DPB_HRD_MSG_PREFIX
        "Rec. ITU-T H.264 C.3.5 constraint is not satisfied "
        "(DPB Overflow happen, DPB size = %u pictures < %u pictures).\n",
        ctx->dpbSize, ctx->nbPicInDpb
      );
      LIBBLU_ERROR(
        H264_DPB_HRD_MSG_NAME
        " => Affect while trying to add Access Unit %u, at removal time: %f s.\n",
        cpbExtractedPic->AUIdx,
        convertTimeH264HrdVerifierContext(ctx, ctx->clockTime)
      );
    }

    if (popAUFromCPBH264HrdVerifierContext(ctx))
      return -1;
  }

  /* Adding current AU : */
  picInfos.frameDisplayNum =
    curState->picOrderCntAU / ((doubleFrameTiming) ? 1 : 2)
  ;
  picInfos.frameNum = sliceHeader->frameNum;
  picInfos.idrPic = sliceHeader->IdrPicFlag && !decRefPicMarking->noOutputOfPriorPics;
  picInfos.dpbOutputDelay = dpbOutputDelay;

  if (sliceHeader->IdrPicFlag) {
    if (decRefPicMarking->longTermReference)
      picInfos.usage = H264_USED_AS_LONG_TERM_REFERENCE;
    else
      picInfos.usage = H264_USED_AS_SHORT_TERM_REFERENCE;
  }
  else {
    if (
      decRefPicMarking->adaptativeRefPicMarkingMode
      && decRefPicMarking->presenceOfMemManCtrlOp6
    ) {
      picInfos.usage = H264_USED_AS_LONG_TERM_REFERENCE;
    }
    else {
      if (sliceHeader->refPic)
        picInfos.usage = H264_USED_AS_SHORT_TERM_REFERENCE;
      else
        picInfos.usage = H264_NOT_USED_AS_REFERENCE;
    }
  }

  if (
    !sliceHeader->IdrPicFlag
    && decRefPicMarking->adaptativeRefPicMarkingMode
  ) {
    picInfos.memMngmntCtrlOperations = copyH264MemoryManagementControlOperations(
      decRefPicMarking->MemMngmntCtrlOp
    );
    if (NULL == picInfos.memMngmntCtrlOperations)
      return -1;
  }
  else
    picInfos.memMngmntCtrlOperations = NULL;

  if (ctx->clockTime < Tf_n)
    ctx->clockTime = Tf_n;  /* Align clock to Tf_n for next AU process. */

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, "Adding %zu bits to CPB (picture %u) at %.4f ms (during %.4f ms).\n",
    AUlength,
    ctx->nbProcessedAU,
    convertTimeH264HrdVerifierContext(ctx, Ta_n) * 1000,
    convertTimeH264HrdVerifierContext(ctx, ctx->clockTime - Ta_n) * 1000
  );

  ctx->cpbBitsOccupancy += AUlength;
  if (addAUToCPBH264HrdVerifierContext(ctx, AUlength, Tr_n, ctx->nbProcessedAU, picInfos) < 0)
    return -1;

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, " -> CPB fullness: %zu bits.\n",
    ctx->cpbBitsOccupancy
  );

  if (ctx->cpbSize < ctx->cpbBitsOccupancy) {
    /* cpbBitsOccupancy exceed CPB size. */
    /* => Buffer overflow */

    LIBBLU_H264_HRDV_ERROR(
      "Rec. ITU-T H.264 C.3.2 constraint is not satisfied "
      "(CPB Overflow happen, CPB size = %zu bits < %zu bits).\n",
      ctx->cpbSize, ctx->cpbBitsOccupancy
    );
    LIBBLU_H264_HRDV_ERROR_RETURN(
      " => Affect while trying to append Access Unit %u, "
      "at final arrival time: %f s.\n",
      ctx->nbProcessedAU,
      convertTimeH264HrdVerifierContext(ctx, ctx->clockTime)
    );
  }

  /* Save constraints checks related parameters: */
  ctx->nMinusOneAUParameters.frameNum = sliceHeader->frameNum;
  assert(0 != sliceHeader->picSizeInMbs);
  ctx->nMinusOneAUParameters.picSizeInMbs = sliceHeader->picSizeInMbs;
  assert(0 != spsData->levelIdc);
  ctx->nMinusOneAUParameters.levelIdc = spsData->levelIdc;
  ctx->nMinusOneAUParameters.removalTime = Tr_n;
  ctx->nMinusOneAUParameters.initialCpbRemovalDelay = initialCpbRemovalDelay;
  ctx->nMinusOneAUParameters.initialCpbRemovalDelayOff = initialCpbRemovalDelayOff;

#if 0
  /* Using HRD Verifier to generate PTS and DTS according to Rec. ITU-T H.222 2.14.3.1 */
  ctx->nMinusOneAUParameters.dts = (uint64_t) ABS(
    (double) Tr_n * (MAIN_CLOCK_27MHZ / H264_90KHZ_CLOCK) / (double) ctx->tick90
  ); /* CPB removal time */
  ctx->nMinusOneAUParameters.pts = (uint64_t) ABS(
    (double) (
      (ctx->nMinusOneAUParameters.dts + ctx->clockTick * dpbOutputDelay) *
      (MAIN_CLOCK_27MHZ / H264_90KHZ_CLOCK)
    ) / (double) ctx->tick90
  ); /* DPB output time  */
#endif

  ctx->nbProcessedAU++;
  return 0; /* OK */
}