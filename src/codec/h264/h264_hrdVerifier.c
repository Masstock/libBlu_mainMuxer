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

/* NOTE ABOUT TIME VALUES:
 * t values in ITU-T H.264 are express in seconds.
 * Here t values are express in time_scale * 90000 tick units
 * for convenience.
 * That means some time-base values are modified to apply this
 * clock scale and cannot be interpreted as original ones.
 * Conversion is usable for debugging with _convertTimeH264HrdVerifierContext() function.
 */

static int _initH264HrdVerifierDebug(
  H264HrdVerifierDebug * dst,
  const LibbluESSettingsOptions * options
)
{
  bool useFileOutput = (NULL != options->echoHrdLogFilepath);

  *dst = (H264HrdVerifierDebug) {
    .cpb = options->echoHrdCpb,
    .dpb = options->echoHrdDpb,
    .fileOutput = useFileOutput
  };

  /* Debug: */
  if (useFileOutput) {
    /* Open debug output file: */
    if (NULL == (dst->fd = lbc_fopen(options->echoHrdLogFilepath, "w")))
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Unable to open debugging output file '%s', %s (errno %d).\n",
        options->echoHrdLogFilepath,
        strerror(errno),
        errno
      );
  }

  return 0;
}

static void _cleanH264HrdVerifierDebug(
  H264HrdVerifierDebug debug
)
{
  if (debug.fileOutput)
    fclose(debug.fd);
}

static int _initH264HrdVerifierOptions(
  H264HrdVerifierOptions * dst,
  const LibbluESSettingsOptions * options
)
{
  lbc * string;

  string = lookupIniFile(options->confHandle, "HRD.CRASHONERROR");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'crashOnError' in section "
        "[HRD] of INI file.\n"
      );
    dst->abortOnError = value;
  }

  return 0;
}

static unsigned _selectSchedSelIdx(
  const H264SPSDataParameters * sps
)
{
  /* By default select the last SchedSelIdx */
  return sps->vui_parameters.nal_hrd_parameters.cpb_cnt_minus1;
}

static int _checkInitConstaints(
  const H264SPSDataParameters * sps,
  const H264ConstraintsParam * constraints,
  unsigned SchedSelIdx
)
{
  const H264VuiParameters * vui = &sps->vui_parameters;

  /* Check parameters compliance: */
  /* TODO: Low Delay not supported */

  if (
    H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(sps->profile_idc)

    || sps->profile_idc == H264_PROFILE_HIGH
    || sps->profile_idc == H264_PROFILE_HIGH_10
    || sps->profile_idc == H264_PROFILE_HIGH_422
    || sps->profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
    || sps->profile_idc == H264_PROFILE_CAVLC_444_INTRA
  ) {
    unsigned BitRate = BitRateH264HrdParameters(vui->nal_hrd_parameters, SchedSelIdx);
    unsigned CpbSize = CpbSizeH264HrdParameters(vui->nal_hrd_parameters, SchedSelIdx);

    assert(0 < constraints->cpbBrNalFactor);

    /* Rec. ITU-T H.264 A.3.1.j) */
    /* Rec. ITU-T H.264 A.3.3.g) */
    if (constraints->MaxBR * constraints->cpbBrNalFactor < BitRate) {
      char * name, * coeff;

      if (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(sps->profile_idc))
        name = "A.3.1.j)", coeff = "1200";
      else
        name = "A.3.3.g)", coeff = "cpbBrNalFactor";

      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Rec. ITU-T H.264 %s constraint is not satisfied "
        "(%s * MaxBR = %u b/s < NAL HRD BitRate[%u] = %u b/s).\n",
        name,
        coeff,
        constraints->MaxBR * constraints->cpbBrNalFactor,
        SchedSelIdx,
        BitRate
      );
    }

    if (H264_BDAV_MAX_BITRATE < BitRate) {
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Bitrate value exceed BDAV limits "
        "(%u b/s < NAL HRD BitRate[%u] = %u b/s).\n",
        H264_BDAV_MAX_BITRATE,
        SchedSelIdx,
        BitRate
      );
    }


    if (constraints->MaxCPB * constraints->cpbBrNalFactor < CpbSize) {
      char * name, * coeff;

      if (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(sps->profile_idc))
        name = "A.3.1.j)", coeff = "1200";
      else
        name = "A.3.3.g)", coeff = "cpbBrNalFactor";

      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Rec. ITU-T H.264 %s constraint is not satisfied "
        "(%s * MaxCPB = %u bits < NAL HRD CpbSize[%u] = %u bits).\n",
        name,
        coeff,
        constraints->MaxCPB * constraints->cpbBrNalFactor,
        SchedSelIdx,
        CpbSize
      );
    }

    if (H264_BDAV_MAX_CPB_SIZE < CpbSize) {
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Bitrate value exceed BDAV limits "
        "(%u bits < NAL HRD CpbSize[%u] = %u bits).\n",
        H264_BDAV_MAX_CPB_SIZE,
        SchedSelIdx,
        CpbSize
      );
    }

    /* Rec. ITU-T H.264 A.3.1.i) */
    /* Rec. ITU-T H.264 A.3.3.h) */
    if (vui->vcl_hrd_parameters_present_flag) {
      unsigned BitRate = BitRateH264HrdParameters(vui->vcl_hrd_parameters, SchedSelIdx);
      unsigned CpbSize = CpbSizeH264HrdParameters(vui->vcl_hrd_parameters, SchedSelIdx);

      assert(0 < constraints->cpbBrVclFactor);

      if (constraints->MaxBR * constraints->cpbBrVclFactor < BitRate) {
        char * name, * coeff;

        if (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(sps->profile_idc))
          name = "A.3.1.i)", coeff = "1000";
        else
          name = "A.3.3.h)", coeff = "cpbBrVclFactor";

        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 %s constraint is not satisfied"
          "(%s * MaxBR = %u b/s < VCL HRD BitRate[%u] = %u b/s).\n",
          name,
          coeff,
          constraints->MaxBR * constraints->cpbBrVclFactor,
          SchedSelIdx,
          BitRate
        );
      }

      if (constraints->MaxCPB * constraints->cpbBrVclFactor < CpbSize) {
        char * name, * coeff;

        if (H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(sps->profile_idc))
          name = "A.3.1.i)", coeff = "1000";
        else
          name = "A.3.3.h)", coeff = "cpbBrVclFactor";

        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 %s constraint is not satisfied"
          "(%s * MaxCPB = %u b/s < VCL HRD CpbSize[%u] = %u b/s).\n",
          name,
          coeff,
          constraints->MaxCPB * constraints->cpbBrVclFactor,
          SchedSelIdx,
          CpbSize
        );
      }
    }
  }

  return 0;
}

/** \~english
 * \brief
 *
 * \param hrd_parameters
 * \param SchedSelIdx
 * \param second Duration of one second.
 * \return double
 */
static double _computeBitratePerTick(
  H264HrdParameters hrd_parameters,
  unsigned SchedSelIdx,
  double second
)
{
  double BitRate = (double) BitRateH264HrdParameters(hrd_parameters, SchedSelIdx);

  return BitRate / second;
}

/** \~english
 * \brief
 *
 * \param constraints
 * \param sps
 * \return unsigned
 *
 * Equation from [1] A.3.2.f)
 */
static unsigned _computeMaxDpbFrames(
  const H264ConstraintsParam * constraints,
  const H264SPSDataParameters * sps
)
{
  return MIN(
    constraints->MaxDpbMbs / (sps->PicWidthInMbs * sps->FrameHeightInMbs),
    16
  );
}

H264HrdVerifierContextPtr createH264HrdVerifierContext(
  const LibbluESSettingsOptions * options,
  const H264SPSDataParameters * sps,
  const H264ConstraintsParam * constraints
)
{
  /* Only operating at SchedSelIdx = cpb_cnt_minus1 */
  H264HrdVerifierContextPtr ctx;
  unsigned SchedSelIdx;

  H264VuiParameters vui;
  H264HrdParameters nal_hrd_parameters;

  assert(sps->vui_parameters_present_flag);
  assert(sps->vui_parameters.nal_hrd_parameters_present_flag);
  assert(!sps->vui_parameters.low_delay_hrd_flag);
  assert(sps->vui_parameters.timing_info_present_flag);

  assert(0 < constraints->MaxBR);
  assert(0 < constraints->MaxCPB);
  assert(0 < constraints->MaxDpbMbs);

  vui = sps->vui_parameters;
  nal_hrd_parameters = vui.nal_hrd_parameters;

  /* Select the SchedSelIdx used for initial tests : */
  SchedSelIdx = _selectSchedSelIdx(sps);
  if (_checkInitConstaints(sps, constraints, SchedSelIdx) < 0)
    return NULL;

  /* Allocate the context: */
  ctx = (H264HrdVerifierContextPtr) calloc(1, sizeof(H264HrdVerifierContext));
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
  ctx->second = (double) H264_90KHZ_CLOCK * vui.time_scale;
  ctx->c90 = vui.time_scale;
  ctx->num_units_in_tick = vui.num_units_in_tick;
  ctx->t_c = H264_90KHZ_CLOCK * ctx->num_units_in_tick;
  ctx->bitrate = _computeBitratePerTick(nal_hrd_parameters, SchedSelIdx, ctx->second);
  ctx->cbr = cbr_flagH264HrdParameters(nal_hrd_parameters, SchedSelIdx);
  ctx->cpbSize = CpbSizeH264HrdParameters(nal_hrd_parameters, SchedSelIdx);
  ctx->dpbSize = _computeMaxDpbFrames(constraints, sps);
  ctx->max_num_ref_frames = sps->max_num_ref_frames;
  ctx->MaxFrameNum = sps->MaxFrameNum;

  if (_initH264HrdVerifierDebug(&ctx->debug, options) < 0)
    goto free_return;
  defaultH264HrdVerifierOptions(&ctx->options);
  if (_initH264HrdVerifierOptions(&ctx->options, options) < 0)
    goto free_return;

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

  _cleanH264HrdVerifierDebug(ctx->debug);
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

  if (ctx->debug.fileOutput) {
    va_list args_copy;

    va_copy(args_copy, args);
    lbc_vfprintf(ctx->debug.fd, format, args_copy);
    va_end(args_copy);
  }

  echoMessageVa(status, format, args);
  va_end(args);
}

/** \~english
 * \brief Convert a time value in c ticks to seconds.
 *
 * \param ctx Used HRD context.
 * \param time Time value to convert in c ticks.
 * \return double Associated time value in seconds.
 */
static double _convertTimeH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t time
)
{
  assert(NULL != ctx);

  return ((double) time) / ctx->second;
}

/** \~english
 * \brief Declare a new Access Unit with given parameters into the CPB FIFO.
 *
 * \param ctx Destination HRD context.
 * \param length Size of the access unit in bits.
 * \param removalTime Removal time of the access unit (t_r) in c ticks.
 * \param AUIdx Index of the access unit.
 * \param picInfos Picture informations associated with access unit.
 * \return int Upon successfull adding, a zero value is returned. Otherwise,
 * a negative value is returned.
 */
static int _addAUToCPBH264HrdVerifierContext(
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

  if (H264_MAX_AU_IN_CPB <= ctx->nbAUInCpb)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Too many access units in the CPB (%u).\n",
      H264_MAX_AU_IN_CPB
    );

  /* Get the next cell index in the FIFO using a modulo (and increase nb) */
  newCellIndex = (
    ((ctx->AUInCpbHeap - ctx->AUInCpb) + ctx->nbAUInCpb++)
    & H264_AU_CPB_MOD_MASK
  );

  newCell = &ctx->AUInCpb[newCellIndex];
  newCell->AUIdx = AUIdx;
  newCell->length = length;
  newCell->removalTime = removalTime;
  newCell->picInfos = picInfos;

  return 0; /* OK */
}

/** \~english
 * \brief Remove the oldest declared Access Unit from the CPB FIFO.
 *
 * \param ctx Used HRD context.
 * \return int Upon successfull poping, a zero value is returned. Otherwise,
 * a negative value is returned.
 */
static int _popAUFromCPBH264HrdVerifierContext(
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

  ctx->AUInCpbHeap = &ctx->AUInCpb[newHeadCellIdx];
  ctx->nbAUInCpb--;

  return 0; /* OK */
}

static H264CpbHrdAU * _getOldestAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
)
{
  if (!ctx->nbAUInCpb)
    return NULL; /* No AU currently in FIFO. */
  return ctx->AUInCpbHeap;
}

#if 0
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
#endif

/** \~english
 * \brief Get Decoded Picture pointer from DPB by index.
 *
 * \param ctx
 * \param idx
 * \return H264DpbHrdPic*
 */
static H264DpbHrdPic * _getDPByIdxH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned idx
)
{
  unsigned array_abs_idx = ctx->picInDpbHeap - ctx->picInDpb + idx;

  return &ctx->picInDpb[array_abs_idx & H264_DPB_MOD_MASK];
}

static H264DpbHrdPic * _initDpbPicH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU cpbPicture,
  uint64_t outputTime
)
{
  H264DpbHrdPic * last = _getDPByIdxH264HrdContext(ctx, ctx->nbPicInDpb);

  *last = (H264DpbHrdPic) {
    .AUIdx = cpbPicture.AUIdx,
    .frameDisplayNum = cpbPicture.picInfos.frameDisplayNum,
    .frame_num = cpbPicture.picInfos.frameDisplayNum,
    .field_pic_flag = cpbPicture.picInfos.field_pic_flag,
    .bottom_field_flag = cpbPicture.picInfos.bottom_field_flag,
    .outputTime = outputTime,
    .usage = cpbPicture.picInfos.usage
  };

  return last;
}

static int _addDecodedPictureToH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU cpbPicture,
  uint64_t outputTime
)
{
  H264DpbHrdPicInfos picInfos = cpbPicture.picInfos;
  H264DpbHrdPic * newPicture;
  unsigned i;

  assert(NULL != ctx);

  if (H264_MAX_DPB_SIZE <= ctx->nbPicInDpb)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to append decoded picture, too many pictures in DPB.\n"
    );

  newPicture = _initDpbPicH264HrdVerifierContext(ctx, cpbPicture, outputTime);

  switch (picInfos.usage) {
    case H264_USED_AS_LONG_TERM_REFERENCE:
      for (i = 0; i < ctx->nbPicInDpb; i++) {
        H264DpbHrdPic * picture;

        if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
          return -1; /* Unable to get i-index picture, broken FIFO. */

        if (
          H264_USED_AS_LONG_TERM_REFERENCE == picture->usage
          && picture->longTermFrameIdx == picInfos.longTermFrameIdx
        ) {
          /* NOTE: Shall NEVER happen, means broken longTermFrameIdx
            management. */
          LIBBLU_H264_HRDV_ERROR_RETURN(
            "LongTermFrameIdx = %u is already used in DPB.\n",
            picInfos.longTermFrameIdx
          );
        }
      }
      newPicture->longTermFrameIdx = picInfos.longTermFrameIdx;
      ctx->numLongTerm++;
      break;

    case H264_USED_AS_SHORT_TERM_REFERENCE:
      ctx->numShortTerm++;
      break;

    case H264_NOT_USED_AS_REFERENCE:
      break; /* Non-used as reference picture, no counter to update rather than
        nbPicInDpb. */
  }

  ctx->nbPicInDpb++;
  return 0; /* OK */
}

static int _setDecodedPictureAsRefUnusedInH264HrdContext(
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
      break; /* Non-used as reference picture, no counter to update rather than
        nbPicInDpb. */
  }

  picture->usage = H264_NOT_USED_AS_REFERENCE;

  return 0;
}

static int _popDecodedPictureFromH264HrdContext(
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
      break; /* Non-used as reference picture, no counter to update rather than
        nbPicInDpb. */
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

static void _printDPBStatusH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  unsigned i;
  char * sep;

  ECHO_DEBUG_DPB_HRDV_CTX(ctx, " -> DPB content:");

  sep = "";
  for (i = 0; i < ctx->nbPicInDpb; i++) {
    H264DpbHrdPic * pic;

    if (NULL == (pic = _getDPByIdxH264HrdContext(ctx, i)))
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

  if (MAX(ctx->max_num_ref_frames, 1) <= ctx->numShortTerm + ctx->numLongTerm) {
    if (!ctx->numShortTerm)
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "DPB reference pictures shall no be only used as long-term reference "
        "(Not enought space available).\n"
      );

    oldestFrameInit = false;
    minFrameNumWrap = 0;
    oldestFrame = 0;

    for (i = 0; i < ctx->nbPicInDpb; i++) {
      if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
        return -1; /* Unable to get i-index picture, broken FIFO. */

      if (picture->usage == H264_USED_AS_SHORT_TERM_REFERENCE) {
        if (frameNum < picture->frame_num)
          FrameNumWrap = picture->frame_num - ctx->MaxFrameNum;
        else
          FrameNumWrap = picture->frame_num;

        if (!oldestFrameInit)
          oldestFrame = i, minFrameNumWrap = FrameNumWrap, oldestFrameInit = true;
        else if (FrameNumWrap < minFrameNumWrap)
          oldestFrame = i, minFrameNumWrap = FrameNumWrap;
      }
    }
    assert(oldestFrameInit);
      /* Shall be at least one 'short-term reference' picture,
        or numShortTerm is broken. */

    if (_setDecodedPictureAsRefUnusedInH264HrdContext(ctx, oldestFrame) < 0)
      return -1;
  }

  return 0;
}

static int _markAllDecodedPicturesAsUnusedH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    picture->usage = H264_NOT_USED_AS_REFERENCE;
  }
  ctx->numShortTerm = 0;
  ctx->numLongTerm = 0;

  return 0;
}


static unsigned _computePicNum(
  H264DpbHrdPicInfos * picInfos
)
{
  if (picInfos->field_pic_flag)
    return 2 * picInfos->frame_num + 1;
  return picInfos->frame_num;
}

/** \~english
 * \brief Mark
 *
 * \param ctx
 * \param difference_of_pic_nums_minus1
 * \param picInfos
 * \return int
 */
static int _markShortTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned difference_of_pic_nums_minus1,
  H264DpbHrdPicInfos * picInfos
)
{
  /**
   * 8.2.5.4.1
   * Marking process of a short-term reference picture as
   * "unused for reference"
   *
   * memory_management_control_operation == 0x1 usage.
   */
  // FIXME: Computed picNumX does not seem accurate.
  unsigned picNumX;
  unsigned i;

  assert(NULL != ctx);

  picNumX = _computePicNum(picInfos) - (difference_of_pic_nums_minus1 + 1);

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    H264DpbHrdPic * picture;

    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frame_num == picNumX) {
      if (H264_USED_AS_SHORT_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "picNumX %u index does not correspond to a "
          "'short-term reference' picture.\n",
          picNumX
        );

      assert(0 < ctx->numShortTerm);

      picture->usage = H264_NOT_USED_AS_REFERENCE;
      ctx->numShortTerm--;
    }
  }

  return 0;
}

#if 0
static int _markLongTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum
)
{
  /* memory_management_control_operation == 0x2 usage. */
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb; i++) {
    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frame_num == frameNum) {
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

static int _markShortTermRefPictureAsUsedForLongTermReferenceH264HrdContext(
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
    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frame_num == frameNum) {
      if (H264_USED_AS_SHORT_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "frame_num %u index does not correspond to a "
          "'long-term reference' picture.\n",
          frameNum
        );

      for (i = 0; i < ctx->nbPicInDpb; i++) {
        /* Clearing previously used LongTermFrameIdx */
        if (NULL == (subPicture = _getDPByIdxH264HrdContext(ctx, i)))
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
#endif

int applyDecodedReferencePictureMarkingProcessDPBH264Context(
  H264HrdVerifierContextPtr ctx,
  H264DpbHrdPicInfos * infos
)
{
  /* 8.2.5.1 Sequence of operations for decoded reference picture marking
    process */

  assert(NULL != ctx);
  assert(NULL != infos);

  assert(ctx->numLongTerm + ctx->numShortTerm <= ctx->nbPicInDpb);

  if (infos->IdrPicFlag) {
    if (_markAllDecodedPicturesAsUnusedH264HrdContext(ctx) < 0)
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
    if (0 < infos->nbMemMngmntCtrlOp) {
      /* Invoke 8.2.5.4 */
      unsigned i;

      for (i = 0; i < infos->nbMemMngmntCtrlOp; i++) {
        const H264MemMngmntCtrlOpBlk * opBlk = &infos->MemMngmntCtrlOp[i];
        int ret = 0;

        switch (opBlk->operation) {
          case H264_MEM_MGMNT_CTRL_OP_END:
            break;
          case H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED:
            ret = _markShortTermRefPictureAsUnusedForReferenceH264HrdContext(
              ctx,
              opBlk->difference_of_pic_nums_minus1,
              infos
            );
            break;

          default:
            LIBBLU_TODO();
        }
        if (ret < 0)
          return -1;
      }
    }
    else {
      /* Check if adaptive_ref_pic_marking_mode_flag can be set to 0b0. */
      /* => MAX(max_num_ref_frames, 1) < numLongTerm */
      if (MAX(ctx->max_num_ref_frames, 1) <= ctx->numLongTerm) {
        LIBBLU_H264_HRDV_ERROR(
          "Parameter 'adaptive_ref_pic_marking_mode_flag' shall be defined "
          "to 0b1 (Max(max_num_ref_frames, 1) <= numLongTerm).\n"
        );
        LIBBLU_H264_HRDV_ERROR_RETURN(
          " -> Too many long-term pictures in DPB, causing an overflow "
          "(%u over %u max ref frames).\n",
          ctx->numLongTerm,
          MAX(ctx->max_num_ref_frames, 1)
        );
      }

      /* Invoke 8.2.5.3 */
      if (manageSlidingWindowProcessDPBH264Context(ctx, infos->frame_num) < 0)
        return -1;
    }
  }

  if (MAX(ctx->max_num_ref_frames, 1) < ctx->numShortTerm + ctx->numLongTerm) {
    LIBBLU_H264_HRDV_ERROR(
      "DPB reference pictures shall be less than "
      "Max(max_num_ref_frames, 1).\n"
    );
    LIBBLU_H264_HRDV_ERROR_RETURN(
      " => Currently: %u (Max: %u).\n",
      ctx->numShortTerm + ctx->numLongTerm,
      MAX(ctx->max_num_ref_frames, 1)
    );
  }

  return 0;
}

static int _updateDPBH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t currentTime
)
{
  /* Remove unused pictures (as reference) from DPB if their release time as
    been reached. */
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nbPicInDpb;) {
    H264DpbHrdPic * picture;

    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (
      H264_NOT_USED_AS_REFERENCE == picture->usage
      && picture->outputTime <= currentTime
    ) {
      if (_popDecodedPictureFromH264HrdContext(ctx, i) < 0)
        return -1;
    }
    else
      i++;
  }

  return 0; /* OK */
}

#if 0
void clearDPBH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  assert(NULL != ctx);

  ctx->nbPicInDpb = 0;
  ctx->numShortTerm = 0;
  ctx->numLongTerm = 0;
}
#endif

int defineMaxLongTermFrameIdxH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  int maxLongTermFrameIdx
)
{
  /* memory_management_control_operation == 0x4 usage. */
  bool update;

  assert(NULL != ctx);

  if (maxLongTermFrameIdx < ctx->maxLongTermFrameIdx)
    update = true;
  ctx->maxLongTermFrameIdx = maxLongTermFrameIdx;

  if (update) {
    unsigned i;
    /* Mark all long-term reference pictures with
     * MaxLongTermFrameIdx < LongTermFrameIdx as
     * 'unused for reference'.
     */

    for (i = 0; i < ctx->nbPicInDpb; i++) {
      H264DpbHrdPic * picture;

      if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
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

  if (_markAllDecodedPicturesAsUnusedH264HrdContext(ctx) < 0)
    return -1;

  ctx->maxLongTermFrameIdx = -1;

  return 0;
}

static int _checkH264CpbHrdConformanceTests(
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

  uint64_t initial_cpb_removal_delay;
  /* uint64_t Tf_nMinusOne, Tr_nMinusOne; */

 /*  double delta_tg90; */
  unsigned maxMBPS; /* H.264 Table A-1 linked to level_idc. */
  double fR; /* H.264 C.3.4. A.3.1/2.a) */
  double minCpbRemovalDelay; /* H.264 C.3.4. A.3.1/2.a) Max(PicSizeInMbs ÷ MaxMBPS, fR) result */
  size_t maxAULength;
  unsigned sliceRate; /* H.264 A.3.3.a) */
  unsigned maxNbSlices; /* H.264 A.3.3.a) */

  assert(NULL != ctx);
  assert(NULL != AUNalHrdParam);
  assert(NULL != AUSpsData);
  assert(NULL != AUSliceHeader);

  initial_cpb_removal_delay = AUNalHrdParam->initial_cpb_removal_delay;
    /* initial_cpb_removal_delay */

  if (60 <= AUSpsData->level_idc && AUSpsData->level_idc <= 62)
    fR = 1.0 / 300.0;
  else
    fR = (AUSliceHeader->field_pic_flag) ? 1.0 / 344.0 : 1.0 / 172.0;

  if (0 < ctx->nbProcessedAU) {
    uint64_t Tf_nMinusOne = ctx->clockTime;
    uint64_t Tr_nMinusOne = ctx->nMinusOneAUParameters.removalTime;
    /* At every process, clockTime stops on AU final arrival time. */

    /* H.264 C.3.1. */
    if (
      AUIsNewBufferingPeriod
      && (
        ctx->nMinusOneAUParameters.initial_cpb_removal_delay
          == initial_cpb_removal_delay
      )
    ) {
      /* C-15/C-16 equation check. */
      /* No check if new buffering period introduce different buffering from
        last period. */

      /* H.264 C-14 equation. */
      double delta_tg90 =
        _convertTimeH264HrdVerifierContext(ctx, Tr_n - Tf_nMinusOne)
        * H264_90KHZ_CLOCK
      ;

      /* H.264 C-15/C-16 ceil equation conformance : */
      if (ceil(delta_tg90) < initial_cpb_removal_delay)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 %s equation is not satisfied "
          "(delta_tg90 = %.0f < initial_cpb_removal_delay = %" PRIu64 ").\n",
          (ctx->cbr) ? "C-16" : "C-15",
          ceil(delta_tg90),
          initial_cpb_removal_delay
        );

      /* H.264 C-16 floor equation conformance : */
      if (ctx->cbr && initial_cpb_removal_delay < floor(delta_tg90))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Rec. ITU-T H.264 C-16 equation is not satisfied "
          "(initial_cpb_removal_delay = %" PRIu64 " < delta_tg90 = %.0f).\n",
          initial_cpb_removal_delay, floor(delta_tg90)
        );
    }

    /* NOTE: A.3.1, A.3.2 and A.3.3 conformance verifications are undue by C.3.4 constraint. */

    /* H.264 A.3.1.a) and A.3.2.a) */
    if (0 == (maxMBPS = getH264MaxMBPS(ctx->nMinusOneAUParameters.level_idc)))
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Unable to find a MaxMBPS value for level_idc = 0x%" PRIx8 ".\n",
        ctx->nMinusOneAUParameters.level_idc
      );

    minCpbRemovalDelay = MAX(
      (double) ctx->nMinusOneAUParameters.PicSizeInMbs / maxMBPS,
      fR
    );

    if ((double) (Tr_n - Tr_nMinusOne) < minCpbRemovalDelay * ctx->second)
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Rec. ITU-T H.264 %s constraint is not satisfied "
        "(t_r,n(n) - t_t(n-1) = %f < Max(PicSizeInMbs / MaxMBPS, fR) = %f).\n",
        (H264_PROFILE_IS_HIGH(AUSpsData->profile_idc)) ? "4.3.2.a)" : "4.3.1.a)",
        _convertTimeH264HrdVerifierContext(ctx, Tr_n - Tr_nMinusOne),
        minCpbRemovalDelay
      );

    /* H.264 A.3.1.d) and A.3.3.j) */
    if (
      AUSpsData->profile_idc == H264_PROFILE_BASELINE
      || AUSpsData->profile_idc == H264_PROFILE_MAIN
      || AUSpsData->profile_idc == H264_PROFILE_EXTENDED

      || AUSpsData->profile_idc == H264_PROFILE_HIGH
    ) {
      assert(0 != AUSliceHeader->PicSizeInMbs);

      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for level_idc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.level_idc
        );

      maxAULength = (size_t) ABS(
        384.0
        * (double) maxMBPS
        * _convertTimeH264HrdVerifierContext(ctx, Tr_n - Tr_nMinusOne)
        / (double) getH264MinCR(AUSpsData->level_idc)
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
      AUSpsData->profile_idc == H264_PROFILE_MAIN
      || AUSpsData->profile_idc == H264_PROFILE_HIGH
      || AUSpsData->profile_idc == H264_PROFILE_HIGH_10
      || AUSpsData->profile_idc == H264_PROFILE_HIGH_422
      || AUSpsData->profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
      || AUSpsData->profile_idc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for level_idc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.level_idc
        );

      if (0 == (sliceRate = constraints->SliceRate))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a SliceRate value for level_idc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.level_idc
        );

      maxNbSlices = (unsigned) (
        (double) maxMBPS
        * _convertTimeH264HrdVerifierContext(ctx, Tr_n - Tr_nMinusOne)
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
      AUSpsData->profile_idc == H264_PROFILE_BASELINE
      || AUSpsData->profile_idc == H264_PROFILE_MAIN
      || AUSpsData->profile_idc == H264_PROFILE_EXTENDED

      || AUSpsData->profile_idc == H264_PROFILE_HIGH
    ) {
      assert(0 != AUSliceHeader->PicSizeInMbs);

      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for level_idc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.level_idc
        );

      maxAULength = (size_t) ABS(
        384.0
        * MAX((double) AUSliceHeader->PicSizeInMbs, fR * maxMBPS)
        / (double) getH264MinCR(AUSpsData->level_idc)
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
      AUSpsData->profile_idc == H264_PROFILE_MAIN
      || AUSpsData->profile_idc == H264_PROFILE_HIGH
      || AUSpsData->profile_idc == H264_PROFILE_HIGH_10
      || AUSpsData->profile_idc == H264_PROFILE_HIGH_422
      || AUSpsData->profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
      || AUSpsData->profile_idc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      if (0 == (maxMBPS = constraints->MaxMBPS))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for level_idc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.level_idc
        );

      if (0 == (sliceRate = constraints->SliceRate))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a SliceRate value for level_idc = 0x%" PRIx8 ".\n",
          ctx->nMinusOneAUParameters.level_idc
        );

      maxNbSlices = (unsigned) ceil(
        MAX((double) AUSliceHeader->PicSizeInMbs, fR * maxMBPS)
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
  size_t AUlength
)
{
  H264HrdBufferingPeriodParameters * nal_hrd_parameters;
  H264DecRefPicMarking * dec_ref_pic_marking;
  H264DpbHrdPicInfos picInfos;

  uint64_t initial_cpb_removal_delay, initial_cpb_removal_delay_offset;
  uint64_t cpb_removal_delay, dpb_output_delay;

  uint64_t Tr_n, Ta_n, Tf_n, Tout_n;
  uint64_t Tf_nMinusOne;

  bool storePicFlag; /* C.2.4.2 */
  H264CpbHrdAU * cpbExtractedPic;

  assert(NULL != ctx);
  assert(NULL != spsData);
  assert(NULL != sliceHeader);
  assert(NULL != picTimingSei);

  /* Check for required parameters : */
  dec_ref_pic_marking = &sliceHeader->dec_ref_pic_marking; /* Decoded reference picture marking */

  nal_hrd_parameters = &bufPeriodSei->nal_hrd_parameters[0];

  /* Parse required parameters from signal : */
  initial_cpb_removal_delay = nal_hrd_parameters->initial_cpb_removal_delay; /* initial_cpb_removal_delay */
  initial_cpb_removal_delay_offset = nal_hrd_parameters->initial_cpb_removal_delay_offset; /* initial_cpb_removal_delay */

  cpb_removal_delay = picTimingSei->cpb_removal_delay; /* cpb_removal_delay */
  dpb_output_delay = picTimingSei->dpb_output_delay; /* dpb_output_delay */

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
    Tr_n = initial_cpb_removal_delay * ctx->c90; /* C-7 */
  else
    Tr_n = ctx->nominalRemovalTimeFirstAU + ctx->t_c * cpb_removal_delay; /* C-8 / C-9 */

  /* Since low_delay_hrd_flag is not supported, C-10 and C-11 equations are not used. */

  /* Saving CPB removal time */
  ctx->removalTimeAU = curState->auCpbRemovalTime = (uint64_t) (
    _convertTimeH264HrdVerifierContext(ctx, Tr_n)
    * MAIN_CLOCK_27MHZ
  );
  ctx->outputTimeAU = curState->auDpbOutputTime = (uint64_t) (
    _convertTimeH264HrdVerifierContext(
      ctx,
      Tr_n + ctx->t_c * dpb_output_delay
    ) * MAIN_CLOCK_27MHZ
  );

  if (isNewBufferingPeriod) /* n_b = n */
    ctx->nominalRemovalTimeFirstAU = Tr_n; /* Saving new t_r(n_b) */

  if (!ctx->cbr) {
    /* VBR case */
    uint64_t Te_n;

    /* t_e(n)/t_ai,earliest(n) - earliest initial arrival time: */
    unsigned initialCpbTotalRemovalDelay =
      ctx->c90 * (
        initial_cpb_removal_delay
        + ((!isNewBufferingPeriod) ? initial_cpb_removal_delay_offset : 0)
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

    LIBBLU_H264_DEBUG_HRD(
      "CPB Underflow, Tr_n = %" PRIu64 " Tf_n = %" PRIu64 ".\n",
      Tr_n,
      Tf_n
    );

    if (ctx->options.abortOnError) {
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Rec. ITU-T H.264 C.3.3 constraint is not satisfied "
        "(CPB Underflow, Tr_n = %fs < Tf_n = %fs "
        "on Access Unit %u, initial arrival time: %fs).\n",
        _convertTimeH264HrdVerifierContext(ctx, Tr_n),
        _convertTimeH264HrdVerifierContext(ctx, Tf_n),
        ctx->nbProcessedAU,
        _convertTimeH264HrdVerifierContext(ctx, Ta_n)
      );
    }

    LIBBLU_H264_HRDV_WARNING(
      "Rec. ITU-T H.264 C.3.3 constraint is not satisfied "
      "(CPB Underflow, Tr_n = %fs < Tf_n = %fs "
      "on Access Unit %u, initial arrival time: %fs).\n",
      _convertTimeH264HrdVerifierContext(ctx, Tr_n),
      _convertTimeH264HrdVerifierContext(ctx, Tf_n),
      ctx->nbProcessedAU,
      _convertTimeH264HrdVerifierContext(ctx, Ta_n)
    );

    Tr_n = Tf_n; // Remove at final arrival time to avoid underflow.
  }

  /* Rec. ITU-T H.264 - C.3 Bitstream conformance Checks: */
  if (
    _checkH264CpbHrdConformanceTests(
      constraints,
      ctx,
      AUlength, isNewBufferingPeriod,
      nal_hrd_parameters, spsData, sliceHeader,
      Tr_n, curState->nbSlicesInPic
    ) < 0
  )
    return -1;

  /* Remove AU with releaseClockTime reached: */
  while (
    NULL != (cpbExtractedPic = _getOldestAUFromCPBH264HrdVerifierContext(ctx))
    && cpbExtractedPic->removalTime <= Tf_n
  ) {
    size_t instantaneousAlreadyTransferedCpbBits;

    ctx->clockTime = cpbExtractedPic->removalTime;

    if (Ta_n < ctx->clockTime)
      instantaneousAlreadyTransferedCpbBits = (size_t) ABS(
        ctx->bitrate
        * (double) (ctx->clockTime - Ta_n)
      ); /* Adds bits from current AU already buffered according to bitrate. */
    else
      instantaneousAlreadyTransferedCpbBits = 0;

    if (
      ctx->cpbSize
      < ctx->cpbBitsOccupancy + instantaneousAlreadyTransferedCpbBits
    ) {
      /* cpbBitsOccupancy + number of bits from current AU already buffered */
      /* exceed CPB size. => Buffer overflow */

#if !H264_HRD_DISABLE_C_3_2
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
        _convertTimeH264HrdVerifierContext(ctx, ctx->clockTime) * 1000
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
#endif
    }

    /* Else AU can be removed safely. */
    assert(cpbExtractedPic->length <= ctx->cpbBitsOccupancy);
      /* Buffer underflow shouldn't happen here. */

    ECHO_DEBUG_CPB_HRDV_CTX(
      ctx, "Removing %zu bits from CPB "
      "(picture %u, currently removed: %zu bits) at %f ms.\n",
      cpbExtractedPic->length,
      cpbExtractedPic->AUIdx,
      MIN(ctx->cpbBitsOccupancy, cpbExtractedPic->length),
      _convertTimeH264HrdVerifierContext(ctx, ctx->clockTime) * 1000
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
    picInfos = cpbExtractedPic->picInfos;

    if (applyDecodedReferencePictureMarkingProcessDPBH264Context(ctx, &picInfos) < 0)
      return -1;

    /* C.2.3.2. Not applied. */

    ECHO_DEBUG_DPB_HRDV_CTX(
      ctx, "Adding picture %u to DPB (display: %u) at %f ms.\n",
      cpbExtractedPic->AUIdx,
      picInfos.frameDisplayNum,
      _convertTimeH264HrdVerifierContext(ctx, ctx->clockTime) * 1000
    );

    if (_updateDPBH264HrdContext(ctx, ctx->clockTime) < 0) /* Update before insertion. */
      return -1;

    Tout_n = ctx->clockTime + ctx->t_c * picInfos.dpb_output_delay;

    storePicFlag = false;
    if (picInfos.usage == H264_NOT_USED_AS_REFERENCE) {
      /* C.2.4.2 Storage of a non-reference picture into the DPB */
      storePicFlag = (Tout_n > ctx->clockTime);
    }

    if (storePicFlag) {
      if (_addDecodedPictureToH264HrdContext(ctx, *cpbExtractedPic, Tout_n) < 0)
        return -1;
    }
    else
      ECHO_DEBUG_DPB_HRDV_CTX(
        ctx, " -> Not stored in DPB (according to Rec. ITU-T H.264 C.2.4.2).\n"
      );

    _printDPBStatusH264HrdContext(ctx);

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
        _convertTimeH264HrdVerifierContext(ctx, ctx->clockTime)
      );
    }

    if (_popAUFromCPBH264HrdVerifierContext(ctx))
      return -1;
  }

  /* Adding current AU : */
  picInfos = (H264DpbHrdPicInfos) {
    .frameDisplayNum = curState->PicOrderCnt,
    .frame_num = sliceHeader->frameNum,
    .field_pic_flag = sliceHeader->field_pic_flag,
    .bottom_field_flag = sliceHeader->bottom_field_flag,
    .IdrPicFlag = (
      sliceHeader->isIdrPic
      && !dec_ref_pic_marking->no_output_of_prior_pics_flag
    ),
    .dpb_output_delay = dpb_output_delay
  };

  if (sliceHeader->isIdrPic) {
    if (dec_ref_pic_marking->long_term_reference_flag)
      picInfos.usage = H264_USED_AS_LONG_TERM_REFERENCE;
    else
      picInfos.usage = H264_USED_AS_SHORT_TERM_REFERENCE;
  }
  else {
    if (
      dec_ref_pic_marking->adaptive_ref_pic_marking_mode_flag
      && dec_ref_pic_marking->presenceOfMemManCtrlOp6
    ) {
      picInfos.usage = H264_USED_AS_LONG_TERM_REFERENCE;
    }
    else {
      if (sliceHeader->isRefPic)
        picInfos.usage = H264_USED_AS_SHORT_TERM_REFERENCE;
      else
        picInfos.usage = H264_NOT_USED_AS_REFERENCE;
    }
  }

  if (
    !sliceHeader->isIdrPic
    && dec_ref_pic_marking->adaptive_ref_pic_marking_mode_flag
  ) {
    memcpy(
      &picInfos.MemMngmntCtrlOp,
      &dec_ref_pic_marking->MemMngmntCtrlOp,
      dec_ref_pic_marking->nbMemMngmntCtrlOp * sizeof(H264MemMngmntCtrlOpBlk)
    );
    picInfos.nbMemMngmntCtrlOp = dec_ref_pic_marking->nbMemMngmntCtrlOp;
  }
  else
    picInfos.nbMemMngmntCtrlOp = 0;

  if (ctx->clockTime < Tf_n)
    ctx->clockTime = Tf_n;  /* Align clock to Tf_n for next AU process. */

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, "Adding %zu bits to CPB (picture %u) at %.4f ms (during %.4f ms).\n",
    AUlength,
    ctx->nbProcessedAU,
    _convertTimeH264HrdVerifierContext(ctx, Ta_n) * 1000,
    _convertTimeH264HrdVerifierContext(ctx, ctx->clockTime - Ta_n) * 1000
  );

  ctx->cpbBitsOccupancy += AUlength;
  if (_addAUToCPBH264HrdVerifierContext(ctx, AUlength, Tr_n, ctx->nbProcessedAU, picInfos) < 0)
    return -1;

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, " -> CPB fullness: %zu bits.\n",
    ctx->cpbBitsOccupancy
  );

  if (ctx->cpbSize < ctx->cpbBitsOccupancy) {
    /* cpbBitsOccupancy exceed CPB size. */
    /* => Buffer overflow */

#if !H264_HRD_DISABLE_C_3_2
    LIBBLU_H264_HRDV_ERROR(
      "Rec. ITU-T H.264 C.3.2 constraint is not satisfied "
      "(CPB Overflow happen, CPB size = %zu bits < %zu bits).\n",
      ctx->cpbSize, ctx->cpbBitsOccupancy
    );
    LIBBLU_H264_HRDV_ERROR_RETURN(
      " => Affect while trying to append Access Unit %u, "
      "at final arrival time: %f s.\n",
      ctx->nbProcessedAU,
      _convertTimeH264HrdVerifierContext(ctx, ctx->clockTime)
    );
#endif
  }

  /* Save constraints checks related parameters: */
  assert(0 < sliceHeader->PicSizeInMbs);
  assert(0x00 != spsData->level_idc);

  ctx->nMinusOneAUParameters.frame_num    = sliceHeader->frameNum;
  ctx->nMinusOneAUParameters.PicSizeInMbs = sliceHeader->PicSizeInMbs;
  ctx->nMinusOneAUParameters.level_idc    = spsData->level_idc;
  ctx->nMinusOneAUParameters.removalTime  = Tr_n;
  ctx->nMinusOneAUParameters.initial_cpb_removal_delay =
    initial_cpb_removal_delay;
  ctx->nMinusOneAUParameters.initial_cpb_removal_delay_offset =
    initial_cpb_removal_delay_offset;

  ctx->nbProcessedAU++;
  return 0; /* OK */
}