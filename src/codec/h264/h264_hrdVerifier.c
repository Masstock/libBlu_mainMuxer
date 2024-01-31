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
 * Conversion is usable for debugging with _dTimeH264HrdVerifierContext() function.
 */

static int _outputCpbStatisticsHeader(
  FILE * fd
)
{
  if (lbc_fprintf(fd, "initial time;;duration;fullness;\n") < 0)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to write to CPB statistics file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  return 0;
}

static int _outputDpbStatisticsHeader(
  FILE * fd
)
{
  if (lbc_fprintf(fd, "WIP - Currently used for debug purposes;\n") < 0)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to write to CPB statistics file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  // TODO
  return 0;
}

static int _initH264HrdVerifierDebug(
  H264HrdVerifierDebug * dst,
  LibbluESSettingsOptions * options
)
{
  memset(dst, 0, sizeof(H264HrdVerifierDebug));

  /* CPB stats */
  if (NULL != options->hrdCpbStatsFilepath) {
    /* Open debug output file: */
    if (NULL == (dst->cpb_fd = lbc_fopen(options->hrdCpbStatsFilepath, "w")))
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Unable to open CPB statistics output file '%s', %s (errno %d).\n",
        options->hrdCpbStatsFilepath,
        strerror(errno),
        errno
      );

    if (_outputCpbStatisticsHeader(dst->cpb_fd) < 0)
      return -1;
  }

  /* DPB stats */
  if (NULL != options->hrdDpbStatsFilepath) {
    /* Open debug output file: */
    if (NULL == (dst->dpb_fd = lbc_fopen(options->hrdDpbStatsFilepath, "w")))
      LIBBLU_H264_HRDV_ERROR_RETURN(
        "Unable to open DPB statistics output file '%s', %s (errno %d).\n",
        options->hrdDpbStatsFilepath,
        strerror(errno),
        errno
      );

    if (_outputDpbStatisticsHeader(dst->dpb_fd) < 0)
      return -1;
  }

  return 0;
}

static void _cleanH264HrdVerifierDebug(
  H264HrdVerifierDebug debug
)
{
  if (NULL != debug.cpb_fd)
    fclose(debug.cpb_fd);
  if (NULL != debug.dpb_fd)
    fclose(debug.dpb_fd);
}

#if 0
static int _initH264HrdVerifierOptions(
  H264HrdVerifierOptions * dst,
  LibbluESSettingsOptions options
)
{
  lbc * string;

  string = lookupIniFile(options.conf_hdl, "HRD.CRASHONERROR");
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
#endif

#define WARN_COUNT_CHECK(warn, name)                                          \
  (((warn)->name) < (warn)->max_amount || 0 == (warn)->max_amount)

#define WARN_COUNT_CHECK_INC(warn, name)                                      \
  (((warn)->name)++ < (warn)->max_amount || 0 == (warn)->max_amount)

static unsigned _selectSchedSelIdx(
  const H264SPSDataParameters * sps
)
{
  /* By default select the last SchedSelIdx */
  return sps->vui_parameters.nal_hrd_parameters.cpb_cnt_minus1;
}

static int _checkInitConstaints(
  H264HrdVerifierWarnings * warn,
  const H264SPSDataParameters * sps,
  const H264ConstraintsParam * cnstr,
  unsigned SchedSelIdx
)
{
  const H264VuiParameters * vui = &sps->vui_parameters;

  /* Check parameters compliance: */
  /* TODO: Low Delay not supported */

  if (
    isBaselineMainExtendedH264ProfileIdcValue(sps->profile_idc)
    || isHighH264ProfileIdcValue(sps->profile_idc)
  ) {
    int64_t BitRate = BitRateH264HrdParameters(vui->nal_hrd_parameters, SchedSelIdx);
    int64_t CpbSize = CpbSizeH264HrdParameters(vui->nal_hrd_parameters, SchedSelIdx);

    assert(0 < cnstr->cpbBrNalFactor);

    /* ITU-T Rec. H.264 A.3.1.j) / A.3.3.g) */
    if (cnstr->MaxBR * cnstr->cpbBrNalFactor < BitRate) {

      const char * name, * coeff;
      if (isBaselineMainExtendedH264ProfileIdcValue(sps->profile_idc))
        name = "A.3.1.j)", coeff = "1200";
      else // isHighH264ProfileIdcValue
        name = "A.3.3.g)", coeff = "cpbBrNalFactor";

      LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
        WARN_COUNT_CHECK_INC(warn, A_3_1_j__A_3_3_g),
        "ITU-T Rec. H.264 %s constraint is not satisfied "
        "(%s * MaxBR = %u b/s < NAL HRD BitRate[%u] = %u b/s).\n",
        name, coeff,
        cnstr->MaxBR * cnstr->cpbBrNalFactor,
        SchedSelIdx,
        BitRate
      );
    }

    if (H264_BDAV_MAX_BITRATE < BitRate) {
      LIBBLU_H264_HRDV_BD_FAIL_WCOND_RETURN(
        WARN_COUNT_CHECK_INC(warn, bdav_maxbr),
        "Bitrate value exceed BDAV limits "
        "(%u b/s < NAL HRD BitRate[%u] = %u b/s).\n",
        H264_BDAV_MAX_BITRATE,
        SchedSelIdx,
        BitRate
      );
    }

    if (cnstr->MaxCPB * cnstr->cpbBrNalFactor < CpbSize) {
      const char * name, * coeff;

      if (isBaselineMainExtendedH264ProfileIdcValue(sps->profile_idc))
        name = "A.3.1.j)", coeff = "1200";
      else
        name = "A.3.3.g)", coeff = "cpbBrNalFactor";

      LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
        WARN_COUNT_CHECK_INC(warn, A_3_1_j__A_3_3_g),
        "ITU-T Rec. H.264 %s constraint is not satisfied "
        "(%s * MaxCPB = %u bits < NAL HRD CpbSize[%u] = %u bits).\n",
        name,
        coeff,
        cnstr->MaxCPB * cnstr->cpbBrNalFactor,
        SchedSelIdx,
        CpbSize
      );
    }

    if (H264_BDAV_MAX_CPB_SIZE < CpbSize) {
      LIBBLU_H264_HRDV_BD_FAIL_WCOND_RETURN(
        WARN_COUNT_CHECK_INC(warn, bdav_cpbsize),
        "Bitrate value exceed BDAV limits "
        "(%u bits < NAL HRD CpbSize[%u] = %u bits).\n",
        H264_BDAV_MAX_CPB_SIZE,
        SchedSelIdx,
        CpbSize
      );
    }

    /* ITU-T Rec. H.264 A.3.1.i) / A.3.3.h) */
    if (vui->vcl_hrd_parameters_present_flag) {
      int64_t BitRate = BitRateH264HrdParameters(vui->vcl_hrd_parameters, SchedSelIdx);
      int64_t CpbSize = CpbSizeH264HrdParameters(vui->vcl_hrd_parameters, SchedSelIdx);

      assert(0 < cnstr->cpbBrVclFactor);

      if (cnstr->MaxBR * cnstr->cpbBrVclFactor < BitRate) {

        const char * name, * coeff;
        if (isBaselineMainExtendedH264ProfileIdcValue(sps->profile_idc))
          name = "A.3.1.i)", coeff = "1000";
        else
          name = "A.3.3.h)", coeff = "cpbBrVclFactor";

        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_1_i__A_3_3_h),
          "ITU-T Rec. H.264 %s constraint is not satisfied"
          "(%s * MaxBR = %u b/s < VCL HRD BitRate[%u] = %u b/s).\n",
          name, coeff,
          cnstr->MaxBR * cnstr->cpbBrVclFactor,
          SchedSelIdx,
          BitRate
        );
      }

      if (cnstr->MaxCPB * cnstr->cpbBrVclFactor < CpbSize) {
        const char * name, * coeff;

        if (isBaselineMainExtendedH264ProfileIdcValue(sps->profile_idc))
          name = "A.3.1.i)", coeff = "1000";
        else
          name = "A.3.3.h)", coeff = "cpbBrVclFactor";

        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_1_i__A_3_3_h),
          "ITU-T Rec. H.264 %s constraint is not satisfied"
          "(%s * MaxCPB = %u b/s < VCL HRD CpbSize[%u] = %u b/s).\n",
          name, coeff,
          cnstr->MaxCPB * cnstr->cpbBrVclFactor,
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
  const H264HrdParameters * hrd_parameters,
  unsigned SchedSelIdx,
  double second
)
{
  return BitRateH264HrdParameters(*hrd_parameters, SchedSelIdx) / second;
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
  const H264ConstraintsParam * cnstr,
  const H264SPSDataParameters * sps
)
{
  return MIN(
    cnstr->MaxDpbMbs / (sps->PicWidthInMbs * sps->FrameHeightInMbs),
    16
  );
}

static void _initH264HrdVerifierWarnings(
  H264HrdVerifierWarnings * warnings,
  LibbluESSettingsOptions * options
)
{
  memset(warnings, 0, sizeof(H264HrdVerifierWarnings));

  const IniFileContext conf_hdl = options->conf_hdl;

  lbc * string = lookupIniFile(conf_hdl, "HRD.MAXNUMBEROFWARNINGS");
  if (NULL != string) {
    warnings->max_amount = lbc_strtoul(string, NULL, 10);
    errno = 0; // Ignore errors
  }
}

H264HrdVerifierContextPtr createH264HrdVerifierContext(
  LibbluESSettingsOptions * options,
  const H264SPSDataParameters * sps,
  const H264ConstraintsParam * cnstr
)
{
  assert(sps->vui_parameters_present_flag);

  const H264VuiParameters * vui = &sps->vui_parameters;
  assert(vui->nal_hrd_parameters_present_flag);
  const H264HrdParameters * nal_hrd_parameters = &vui->nal_hrd_parameters;
  assert(!vui->low_delay_hrd_flag);
  assert(vui->timing_info_present_flag);

  assert(0 < cnstr->MaxBR);
  assert(0 < cnstr->MaxCPB);
  assert(0 < cnstr->MaxDpbMbs);

  H264HrdVerifierWarnings warnings;
  _initH264HrdVerifierWarnings(&warnings, options);

  /* Select the SchedSelIdx used for initial tests : */
  // TODO: Currently only checking last SchedSelIdx
  unsigned SchedSelIdx = _selectSchedSelIdx(sps);
  if (_checkInitConstaints(&warnings, sps, cnstr, SchedSelIdx) < 0)
    return NULL;

  /* Allocate the context: */
  H264HrdVerifierContextPtr ctx;
  if (NULL == (ctx = calloc(1, sizeof(H264HrdVerifierContext))))
    LIBBLU_H264_HRDV_ERROR_FRETURN("Memory allocation error.\n");
  ctx->warnings = warnings;

  /* Prepare FIFO: */
  assert((H264_MAX_AU_IN_CPB & (H264_MAX_AU_IN_CPB - 1)) == 0);
    /* Assert H264_MAX_AU_IN_CPB is a pow 2 */
  ctx->cpb_content_heap = ctx->cpb_content; /* Place head on first cell. */

  assert((H264_MAX_DPB_SIZE & (H264_MAX_DPB_SIZE - 1)) == 0);
    /* Assert H264_MAX_DPB_SIZE is a pow 2 */
  ctx->dpb_content_heap = ctx->dpb_content; /* Place head on first cell. */

  /* Define timing values: */
  ctx->second = 90000.0 * vui->time_scale;
  ctx->c90 = vui->time_scale;
  ctx->num_units_in_tick = vui->num_units_in_tick;
  ctx->t_c = 90000 * ctx->num_units_in_tick;
  ctx->bitrate = _computeBitratePerTick(nal_hrd_parameters, SchedSelIdx, ctx->second);
  ctx->cpb_size = CpbSizeH264HrdParameters(*nal_hrd_parameters, SchedSelIdx);
  ctx->dpb_size = _computeMaxDpbFrames(cnstr, sps);
  ctx->max_num_ref_frames = sps->max_num_ref_frames;
  ctx->MaxFrameNum = sps->MaxFrameNum;

  if (_initH264HrdVerifierDebug(&ctx->debug, options) < 0)
    goto free_return;
#if 0
  defaultH264HrdVerifierOptions(&ctx->options);
  if (_initH264HrdVerifierOptions(&ctx->options, options) < 0)
    goto free_return;
#endif

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
  (void) ctx;

  va_start(args, format);
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
static double _dTimeH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t time
)
{
  return ((double) time) / ctx->second;
}

static unsigned _msTimeH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t time
)
{
  double timeInSec = _dTimeH264HrdVerifierContext(ctx, time);
  return (unsigned) round(timeInSec * 1000);
}

static uint64_t _uTimeH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t time,
  int exp
)
{
  double timeInSec = _dTimeH264HrdVerifierContext(ctx, time);
  return (uint64_t) round(timeInSec * pow(10, exp));
}

/* ### CPB Optional statistics output : #################################### */

#define TIME_EXP  7

static int _outputCpbStatistics(
  const H264HrdVerifierContextPtr ctx,
  uint64_t initial_arrival_time,
  uint64_t transfer_duration
)
{
  if (NULL == ctx->debug.cpb_fd)
    return 0;

  lbc initial_arrival_time_str[STRT_H_M_S_MS_LEN];
  str_time(
    initial_arrival_time_str,
    STRT_H_M_S_MS_LEN,
    STRT_H_M_S_MS,
    initial_arrival_time * MAIN_CLOCK_27MHZ / (ctx->c90 * 90000)
  );

  int ret = lbc_fprintf(
    ctx->debug.cpb_fd,
    "%" PRIu64 ";%" PRI_LBCS ";%" PRId64 ";%" PRIu64 ";\n",
    _uTimeH264HrdVerifierContext(ctx, initial_arrival_time, TIME_EXP),
    initial_arrival_time_str,
    _uTimeH264HrdVerifierContext(ctx, transfer_duration, TIME_EXP),
    ctx->cpb_occupancy
  );

  if (ret < 0)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to write to CPB statistics file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;
}

/* ######################################################################### */

/** \~english
 * \brief Declare a new Access Unit with given parameters into the CPB FIFO.
 *
 * \param ctx Destination HRD context.
 * \param size Size of the access unit in bits.
 * \param removal_time Removal time of the access unit (t_r) in c ticks.
 * \param AU_idx Index of the access unit.
 * \param infos Picture informations associated with access unit.
 * \return int Upon successfull adding, a zero value is returned. Otherwise,
 * a negative value is returned.
 */
static int _addAUToCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  size_t size,
  uint64_t removal_time,
  unsigned AU_idx,
  H264DpbHrdPicInfos infos
)
{
  assert(NULL != ctx);

  if (H264_MAX_AU_IN_CPB <= ctx->nb_au_cpb_content)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Too many AUs in CPB, unable to continue HRD verification, "
      "input bitstream is too much broken.\n"
    );

  /* Get the next cell index in the FIFO using a modulo (and increase nb) */
  unsigned newCellIndex = (
    ((ctx->cpb_content_heap - ctx->cpb_content) + ctx->nb_au_cpb_content++)
    & H264_AU_CPB_MOD_MASK
  );

  H264CpbHrdAU * newCell = &ctx->cpb_content[newCellIndex];
  newCell->AU_idx = AU_idx;
  newCell->size = size;
  newCell->removal_time = removal_time;
  newCell->infos = infos;

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
  assert(NULL != ctx);

  if (!ctx->nb_au_cpb_content)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "No access unit to pop from the CPB, the FIFO is empty.\n"
    );

  unsigned newHeadCellIdx =
    (ctx->cpb_content_heap - ctx->cpb_content + 1)
    & H264_AU_CPB_MOD_MASK
  ;

  ctx->cpb_content_heap = &ctx->cpb_content[newHeadCellIdx];
  ctx->nb_au_cpb_content--;

  return 0; /* OK */
}

static H264CpbHrdAU * _getOldestAccessUnitFromCPB(
  H264HrdVerifierContextPtr ctx
)
{
  if (!ctx->nb_au_cpb_content)
    return NULL; /* No AU currently in FIFO. */
  return ctx->cpb_content_heap;
}

#if 0
H264CpbHrdAU * getNewestAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
)
{
  unsigned newestAUIdx;

  assert(NULL != ctx);

  if (!ctx->nb_au_cpb_content)
    return NULL; /* No AU currently in FIFO. */

  newestAUIdx =
    (ctx->cpb_content_heap - ctx->cpb_content + ctx->nb_au_cpb_content)
    & H264_AU_CPB_MOD_MASK
  ;

  return ctx->cpb_content + newestAUIdx;
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
  unsigned array_abs_idx = ctx->dpb_content_heap - ctx->dpb_content + idx;

  return &ctx->dpb_content[array_abs_idx & H264_DPB_MOD_MASK];
}

static H264DpbHrdPic * _initDpbPicH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU au_from_cpb,
  uint64_t output_time
)
{
  H264DpbHrdPic * last = _getDPByIdxH264HrdContext(ctx, ctx->nb_au_dpb_content);

  *last = (H264DpbHrdPic) {
    .AU_idx = au_from_cpb.AU_idx,
    .frame_display_num = au_from_cpb.infos.frame_display_num,
    .frame_num = au_from_cpb.infos.frame_display_num,
    .field_pic_flag = au_from_cpb.infos.field_pic_flag,
    .bottom_field_flag = au_from_cpb.infos.bottom_field_flag,
    .output_time = output_time,
    .usage = au_from_cpb.infos.usage
  };

  return last;
}

static int _addDecodedPictureToH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU au_from_cpb,
  uint64_t output_time
)
{
  assert(NULL != ctx);

  if (H264_MAX_DPB_SIZE <= ctx->nb_au_dpb_content) {
    LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
      WARN_COUNT_CHECK_INC(&ctx->warnings, dpb_au_overflow),
      "More pictures in DPB than supported (%u).\n",
      H264_MAX_DPB_SIZE
    );
    return 0; // If explode on fail disabled.
  }

  H264DpbHrdPic * newPicture = _initDpbPicH264HrdVerifierContext(
    ctx, au_from_cpb, output_time
  );

  H264DpbHrdPicInfos infos = au_from_cpb.infos;

  switch (infos.usage) {
  case H264_USED_AS_LONG_TERM_REFERENCE:
    for (unsigned i = 0; i < ctx->nb_au_dpb_content; i++) {
      const H264DpbHrdPic * picture = _getDPByIdxH264HrdContext(ctx, i);

      if (NULL == picture)
        return -1; /* Unable to get i-index picture, broken FIFO. */

      if (
        H264_USED_AS_LONG_TERM_REFERENCE == picture->usage
        && picture->LongTermFrameIdx == infos.LongTermFrameIdx
      ) {
        /* NOTE: Shall NEVER happen, means broken longTermFrameIdx
          management. */
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "LongTermFrameIdx = %u is already used in DPB.\n",
          infos.LongTermFrameIdx
        );
      }
    }
    newPicture->LongTermFrameIdx = infos.LongTermFrameIdx;
    ctx->numLongTerm++;
    break;

  case H264_USED_AS_SHORT_TERM_REFERENCE:
    ctx->numShortTerm++;
    break;

  case H264_NOT_USED_AS_REFERENCE:
    break; /* Non-used as reference picture, no counter to update rather than
      nb_au_dpb_content. */
  }

  ctx->nb_au_dpb_content++;
  return 0; /* OK */
}

static int _setDecodedPictureAsRefUnusedInH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned idx
)
{
  assert(NULL != ctx);

  if (!ctx->nb_au_dpb_content)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to set a decoded reference picture as unused, "
      "empty DPB.\n"
    );

  if (ctx->nb_au_dpb_content <= idx)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "Unable to set a decoded reference picture as unused, "
      "out of range index %u.\n",
      idx
    );

  H264DpbHrdPic * pic = &ctx->dpb_content[
    (ctx->dpb_content_heap - ctx->dpb_content + idx)
    & H264_DPB_MOD_MASK
  ];

  switch (pic->usage) {
  case H264_USED_AS_LONG_TERM_REFERENCE: ctx->numLongTerm--; break;
  case H264_USED_AS_SHORT_TERM_REFERENCE: ctx->numShortTerm--; break;
  case H264_NOT_USED_AS_REFERENCE: break;
    // Non-used as reference picture,
    // no counter to update rather than nb_au_dpb_content.
  }

  pic->usage = H264_NOT_USED_AS_REFERENCE;
  return 0;
}

static int _popDecodedPictureFromH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned idx
)
{
  assert(NULL != ctx);

  if (!ctx->nb_au_dpb_content)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "No picture to pop from DPB, empty FIFO.\n"
    );

  if (ctx->nb_au_dpb_content <= idx)
    LIBBLU_H264_HRDV_ERROR_RETURN(
      "No picture to pop from DPB, out of range index %u.\n",
      idx
    );

  unsigned array_abs_idx = (ctx->dpb_content_heap - ctx->dpb_content + idx);
  switch (ctx->dpb_content[array_abs_idx & H264_DPB_MOD_MASK].usage) {
  case H264_USED_AS_LONG_TERM_REFERENCE:
    ctx->numLongTerm--;
    break;

  case H264_USED_AS_SHORT_TERM_REFERENCE:
    ctx->numShortTerm--;
    break;

  case H264_NOT_USED_AS_REFERENCE:
    break; /* Non-used as reference picture, no counter to update rather than
      nb_au_dpb_content. */
  }

  if (idx == 0) {
    /* Pop top, advance new top */
    array_abs_idx = ctx->dpb_content_heap - ctx->dpb_content + 1;

    ctx->dpb_content_heap = &ctx->dpb_content[array_abs_idx & H264_DPB_MOD_MASK];
  }
  else {
    /* Pop inside array, move every following picture */
    for (unsigned i = 0; i < ctx->nb_au_dpb_content - idx - 1; i++) {
      array_abs_idx = ctx->dpb_content_heap - ctx->dpb_content + idx + i;

      ctx->dpb_content[array_abs_idx & H264_DPB_MOD_MASK] =
        ctx->dpb_content[(array_abs_idx + 1) & H264_DPB_MOD_MASK]
      ;
    }
  }

  ctx->nb_au_dpb_content--;
  return 0; /* OK */
}

static void _printDPBStatusH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  unsigned i;
  char * sep;

  ECHO_DEBUG_DPB_HRDV_CTX(ctx, " -> DPB content:");

  sep = " ";
  for (i = 0; i < ctx->nb_au_dpb_content; i++) {
    H264DpbHrdPic * pic;

    if (NULL == (pic = _getDPByIdxH264HrdContext(ctx, i)))
      return;

    ECHO_DEBUG_NH_DPB_HRDV_CTX(
      ctx, "%s[%u: %u/%u]",
      sep, i,
      pic->AU_idx,
      pic->frame_display_num
    );

    sep = ", ";
  }

  if (!i)
    ECHO_DEBUG_NH_DPB_HRDV_CTX(ctx, "*empty*");
  ECHO_DEBUG_NH_DPB_HRDV_CTX(ctx, ".\n");
}

/* ### DPB Optional statistics output : #################################### */

static int _outputDpbStatistics(
  const H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU * au_from_cpb
)
{
  if (NULL == ctx->debug.dpb_fd)
    return 0;

  uint64_t clockTimeTicks = _uTimeH264HrdVerifierContext(
    ctx, ctx->clock_time, TIME_EXP
  );

  lbc clockTimeExpr[STRT_H_M_S_MS_LEN];
  str_time(
    clockTimeExpr, STRT_H_M_S_MS_LEN, STRT_H_M_S_MS,
    ctx->clock_time * MAIN_CLOCK_27MHZ / (ctx->c90 * 90000)
  );

  int ret = lbc_fprintf(
    ctx->debug.dpb_fd,
    "%u;%" PRIu64 ";%" PRI_LBCS ";",
    au_from_cpb->AU_idx,
    clockTimeTicks,
    clockTimeExpr
  );
  if (ret < 0)
    goto free_return;

  for (unsigned idx = 0; idx < ctx->nb_au_dpb_content; idx++) {
    const H264DpbHrdPic * picture = _getDPByIdxH264HrdContext(ctx, idx);

    assert(NULL != picture);

    if (lbc_fprintf(ctx->debug.dpb_fd, "%u;", picture->AU_idx) < 0)
      goto free_return;
  }

  if (lbc_fprintf(ctx->debug.dpb_fd, ";\n") < 0)
    goto free_return;

  return 0;

free_return:
  LIBBLU_H264_HRDV_ERROR_RETURN(
    "Unable to write to CPB statistics file, %s (errno: %d).\n",
    strerror(errno),
    errno
  );
}

/* ######################################################################### */

static int _manageSlidingWindowProcessDPBH264Context(
  H264HrdVerifierContextPtr ctx,
  unsigned frame_num
)
{
  /* 8.2.5.3 Sliding window decoded reference picture marking process */

  if (MAX(ctx->max_num_ref_frames, 1) <= ctx->numShortTerm + ctx->numLongTerm) {
    if (!ctx->numShortTerm)
      LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
        WARN_COUNT_CHECK_INC(&ctx->warnings, invalid_memmng),
        "DPB reference pictures shall no be only used as long-term reference "
        "(Not enought space available).\n"
      );

    bool oldest_frame_init = false;
    unsigned minFrameNumWrap = 0;
    unsigned oldest_frame_idx = 0;

    for (unsigned i = 0; i < ctx->nb_au_dpb_content; i++) {
      H264DpbHrdPic * picture;

      if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
        return -1; /* Unable to get i-index picture, broken FIFO. */

      if (picture->usage == H264_USED_AS_SHORT_TERM_REFERENCE) {
        unsigned FrameNumWrap;
        if (frame_num < picture->frame_num)
          FrameNumWrap = picture->frame_num - ctx->MaxFrameNum;
        else
          FrameNumWrap = picture->frame_num;

        if (!oldest_frame_init) {
          oldest_frame_idx = i;
          minFrameNumWrap = FrameNumWrap;
          oldest_frame_init = true;
        }
        else if (FrameNumWrap < minFrameNumWrap) {
          oldest_frame_idx = i;
          minFrameNumWrap = FrameNumWrap;
        }
      }
    }

    assert(oldest_frame_init);
      // Shall be at least one 'short-term reference' picture,
      // or numShortTerm is broken.

    if (_setDecodedPictureAsRefUnusedInH264HrdContext(ctx, oldest_frame_idx) < 0)
      return -1;
  }

  return 0;
}

static int _markAllDecodedPicturesAsUnusedH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  assert(NULL != ctx);

  for (unsigned i = 0; i < ctx->nb_au_dpb_content; i++) {
    H264DpbHrdPic * picture;

    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    picture->usage = H264_NOT_USED_AS_REFERENCE;
  }
  ctx->numShortTerm = 0;
  ctx->numLongTerm = 0;

  return 0;
}


static unsigned _computePicNum(
  H264DpbHrdPicInfos * infos
)
{
  if (infos->field_pic_flag)
    return 2 * infos->frame_num + 1;
  return infos->frame_num;
}

/** \~english
 * \brief Mark
 *
 * \param ctx
 * \param difference_of_pic_nums_minus1
 * \param infos
 * \return int
 */
static int _markShortTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned diff_of_pic_nums_minus1,
  H264DpbHrdPicInfos * infos
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

  assert(NULL != ctx);

  unsigned picNumX = _computePicNum(infos) - (diff_of_pic_nums_minus1 + 1);

  for (unsigned i = 0; i < ctx->nb_au_dpb_content; i++) {
    H264DpbHrdPic * picture;

    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frame_num == picNumX) {
      if (H264_USED_AS_SHORT_TERM_REFERENCE != picture->usage) {
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "picNumX %u index does not correspond to a "
          "'short-term reference' picture.\n",
          picNumX
        );
        return 0; // If explode on fail disabled.
      }

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
  unsigned frame_num
)
{
  /* memory_management_control_operation == 0x2 usage. */
  H264DpbHrdPic * picture;
  unsigned i;

  assert(NULL != ctx);

  for (i = 0; i < ctx->nb_au_dpb_content; i++) {
    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frame_num == frame_num) {
      if (H264_USED_AS_LONG_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          H264_DPB_HRD_MSG_PREFIX
          "frame_num %u index does not correspond to a "
          "'long-term reference' picture.\n",
          frame_num
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
    frame_num
  );
}

static int _markShortTermRefPictureAsUsedForLongTermReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frame_num,
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

  for (i = 0; i < ctx->nb_au_dpb_content; i++) {
    if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (picture->frame_num == frame_num) {
      if (H264_USED_AS_SHORT_TERM_REFERENCE != picture->usage)
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "frame_num %u index does not correspond to a "
          "'long-term reference' picture.\n",
          frame_num
        );

      for (i = 0; i < ctx->nb_au_dpb_content; i++) {
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
    frame_num
  );
}
#endif

static int _applyAdaptiveMemoryControlDecodedReferencePictureMarkingProcess(
  H264HrdVerifierContextPtr ctx,
  H264DpbHrdPicInfos * infos
)
{
  /* 8.2.5.4 Adaptive memory control decoded reference picture marking
    process */

  for (unsigned i = 0; i < infos->nb_MemMngmntCtrlOp; i++) {
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

  return 0;
}

static int _applyDecodedReferencePictureMarkingProcess(
  H264HrdVerifierContextPtr ctx,
  H264DpbHrdPicInfos * infos
)
{
  /* 8.2.5.1 Sequence of operations for decoded reference picture marking
    process */

  assert(NULL != ctx);
  assert(NULL != infos);

  assert(ctx->numLongTerm + ctx->numShortTerm <= ctx->nb_au_dpb_content);

  if (infos->IdrPicFlag) {
    // 2.a. All reference pictures are marked as "unused for reference"
    if (_markAllDecodedPicturesAsUnusedH264HrdContext(ctx) < 0)
      return -1;

    if (infos->usage != H264_USED_AS_LONG_TERM_REFERENCE) {
      /**
       * If long_term_reference_flag is equal to 0, the IDR picture is marked
       * as "used for short-term reference" and MaxLongTermFrameIdx is set
       * equal to "no long-term frame indices".
       */
      ctx->MaxLongTermFrameIdx = -1;
    }
    else {
      /**
       * Otherwise (long_term_reference_flag is equal to 1), the IDR picture
       * is marked as "used for long-term reference", the LongTermFrameIdx
       * for the IDR picture is set equal to 0, and MaxLongTermFrameIdx is
       * set equal to 0.
       */
      assert(infos->usage == H264_USED_AS_SHORT_TERM_REFERENCE);
      infos->LongTermFrameIdx = 0;
      ctx->MaxLongTermFrameIdx = 0;
    }
  }
  else {
    if (0 == infos->nb_MemMngmntCtrlOp) {
      /**
       * If adaptive_ref_pic_marking_mode_flag is equal to 0, the process
       * specified in clause 8.2.5.3 is invoked.
       */

      /* Check if adaptive_ref_pic_marking_mode_flag can be set to 0b0. */
      /* => MAX(max_num_ref_frames, 1) < numLongTerm */
      if (MAX(ctx->max_num_ref_frames, 1) <= ctx->numLongTerm) {
        LIBBLU_H264_HRDV_FAIL_WCOND(
          WARN_COUNT_CHECK(&ctx->warnings, invalid_memmng),
          "Parameter 'adaptive_ref_pic_marking_mode_flag' shall be defined "
          "to 0b1 (Max(max_num_ref_frames, 1) <= numLongTerm).\n"
        );
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(&ctx->warnings, invalid_memmng),
          " -> Too many long-term pictures in DPB, causing an overflow "
          "(%u over %u max ref frames).\n",
          ctx->numLongTerm,
          MAX(ctx->max_num_ref_frames, 1)
        );
      }

      if (_manageSlidingWindowProcessDPBH264Context(ctx, infos->frame_num) < 0)
        return -1;
    }
    else {
      /**
       * Otherwise (adaptive_ref_pic_marking_mode_flag is equal to 1),
       * the process specified in clause 8.2.5.4 is invoked.
       */
      if (_applyAdaptiveMemoryControlDecodedReferencePictureMarkingProcess(ctx, infos) < 0)
        return -1;
    }
  }

  if (MAX(ctx->max_num_ref_frames, 1) < ctx->numShortTerm + ctx->numLongTerm) {
    LIBBLU_H264_HRDV_FAIL_WCOND(
      WARN_COUNT_CHECK(&ctx->warnings, invalid_memmng),
      "DPB reference pictures shall be less than "
      "Max(max_num_ref_frames, 1).\n"
    );
    LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
      WARN_COUNT_CHECK_INC(&ctx->warnings, invalid_memmng),
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

  assert(NULL != ctx);

  unsigned idx = 0;
  while (idx < ctx->nb_au_dpb_content) {
    const H264DpbHrdPic * picture = _getDPByIdxH264HrdContext(ctx, idx);

    if (NULL == picture)
      return -1; /* Unable to get i-index picture, broken FIFO. */

    if (
      H264_NOT_USED_AS_REFERENCE == picture->usage
      && picture->output_time <= currentTime
    ) {
      // Pop picture at current index.
      if (_popDecodedPictureFromH264HrdContext(ctx, idx) < 0)
        return -1;
    }
    else
      idx++; // Switch to next index.
  }

  return 0; /* OK */
}

#if 0
void clearDPBH264HrdContext(
  H264HrdVerifierContextPtr ctx
)
{
  assert(NULL != ctx);

  ctx->nb_au_dpb_content = 0;
  ctx->numShortTerm = 0;
  ctx->numLongTerm = 0;
}

int defineMaxLongTermFrameIdxH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  int MaxLongTermFrameIdx
)
{
  /* memory_management_control_operation == 0x4 usage. */
  assert(NULL != ctx);

  ctx->MaxLongTermFrameIdx = MaxLongTermFrameIdx;

  if (MaxLongTermFrameIdx < ctx->MaxLongTermFrameIdx) {
    /* Mark all long-term reference pictures with
     * MaxLongTermFrameIdx < LongTermFrameIdx as
     * 'unused for reference'.
     */

    for (unsigned i = 0; i < ctx->nb_au_dpb_content; i++) {
      H264DpbHrdPic * picture;

      if (NULL == (picture = _getDPByIdxH264HrdContext(ctx, i)))
        return -1; /* Unable to get i-index picture, broken FIFO. */

      if (
        picture->usage == H264_USED_AS_LONG_TERM_REFERENCE
        && (
          MaxLongTermFrameIdx < 0
          || (unsigned) MaxLongTermFrameIdx < picture->LongTermFrameIdx
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

#endif

static int _checkH264CpbHrdConformanceTests(
  H264HrdVerifierContextPtr ctx,
  H264ParametersHandlerPtr handle,
  const int64_t au_size,
  uint64_t Tr_n
)
{
  /* ITU-T Rec. H.264 - Annex C.3 Bitstream conformance */
  H264HrdVerifierWarnings * warn = &ctx->warnings;

  const H264SPSDataParameters * sps     = &handle->sequenceParametersSet.data;
  const H264VuiParameters * vui         = &sps->vui_parameters;
  const H264SliceHeaderParameters * sh  = &handle->slice.header;
  const H264SeiBufferingPeriod * sei_bp = &handle->sei.bufferingPeriod;
  const H264ConstraintsParam * cnstr    = &handle->constraints;

  const H264HrdVerifierAUProperties * prev_au = &ctx->prev_AU;

  unsigned SchedSelIdx = _selectSchedSelIdx(sps);
  bool first_au_in_buf_period = handle->sei.bufferingPeriodValid;
  unsigned NumSlicesAU = handle->curProgParam.nbSlicesInPic;
  int64_t NumBytesInAU = DIV_ROUND_UP(au_size, 8);

  // NAL HRD parameters
  const H264SchedSel * nalhrd = &vui->nal_hrd_parameters.schedSel[SchedSelIdx];
  const H264SeiBufferingPeriodSchedSel * nalhrd_bp = &sei_bp->nal_hrd_parameters[SchedSelIdx];

  // initial_cpb_removal_delay
  uint64_t init_cpb_rem_d     = nalhrd_bp->initial_cpb_removal_delay;
  // initial_cpb_removal_delay_offset
  uint64_t init_cpb_rem_d_off = nalhrd_bp->initial_cpb_removal_delay_offset;

  double fR;
  if (60 <= sps->level_idc && sps->level_idc <= 62)
    fR = 1.0 / 300.0;
  else if (!sh->field_pic_flag)
    fR = 1.0 / 172.0;
  else
    fR = 1.0 / 344.0;

  if (0 < ctx->nbProcessedAU) { // 0 < n
    uint64_t Taf_n_mo = prev_au->final_arrival_time; // t_af(n-1)
    uint64_t Tr_n_mo  = prev_au->removal_time; // t_r(n-1)

    assert(Taf_n_mo == ctx->clock_time);
      // At every process, clock_time shall stops on AU final arrival time.

    /* H.264 C.3.1. */
    if (
      first_au_in_buf_period
      && prev_au->initial_cpb_removal_delay == init_cpb_rem_d
      && prev_au->initial_cpb_removal_delay_offset == init_cpb_rem_d_off
    ) {
      /* ITU-T Rec. H.264 C-15/C-16 equation check. */
      /* No check if new buffering period introduce different buffering from
        last period. */

      /* H.264 C-14 equation. */
      double delta_tg90 = 90000 * _dTimeH264HrdVerifierContext(ctx, Tr_n - Taf_n_mo);

      /* H.264 C-15/C-16 ceil equation conformance : */
      double ceil_delta_tg90 = ceil(delta_tg90);
      if (ceil_delta_tg90 < init_cpb_rem_d) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, C_15__C_16_ceil),
          "ITU-T Rec. H.264 %s equation is not satisfied "
          "(ceil(delta_tg90) = %.0f "
          "< initial_cpb_removal_delay = %" PRIu64 ").\n",
          (nalhrd->cbr_flag) ? "C-16" : "C-15",
          ceil_delta_tg90,
          init_cpb_rem_d
        );
      }

      /* H.264 C-16 floor equation conformance : */
      if (nalhrd->cbr_flag) {
        double floor_delta_tg90 = floor(delta_tg90);
        if (init_cpb_rem_d < floor_delta_tg90) {
          LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
            WARN_COUNT_CHECK_INC(warn, C_16_floor),
            "ITU-T Rec. H.264 C-16 equation is not satisfied (initial_cpb_"
            "removal_delay = %" PRIu64 " < floor(delta_tg90) = %.0f, "
            "Tr,n(n) = %u ms; Taf(n-1) = %u ms).\n",
            init_cpb_rem_d,
            floor_delta_tg90,
            _msTimeH264HrdVerifierContext(ctx, Tr_n),
            _msTimeH264HrdVerifierContext(ctx, Taf_n_mo)
          );
        }
      }
    }

    /* NOTE: A.3.1, A.3.2 and A.3.3 conformance verifications are undue by C.3.4 constraint. */

    /* H.264 A.3.1.a) and A.3.2.a) */ {
      unsigned prev_au_MaxMBPS;
      if (0 == (prev_au_MaxMBPS = getH264MaxMBPS(prev_au->level_idc)))
        LIBBLU_H264_HRDV_ERROR_RETURN(
          "Unable to find a MaxMBPS value for level_idc = 0x%" PRIx8 ".\n",
          prev_au->level_idc
        );

      double minCpbRemovalDelay = MAX(1.0 * prev_au->PicSizeInMbs / prev_au_MaxMBPS, fR);
        // H.264 C.3.4. A.3.1/2.a) Max(PicSizeInMbs ÷ MaxMBPS, fR) result

      if ((1.0 * Tr_n - Tr_n_mo) < floor(minCpbRemovalDelay * ctx->second)) {

        const char * name;
        if (isHighScalableMultiviewH264ProfileIdcValue(sps->profile_idc))
          name = "A.3.2.a)";
        else
          name = "A.3.1.a)";

        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_1_A__A_3_2_A),
          "ITU-T Rec. H.264 %s constraint is not satisfied "
          "(t_r,n(n) - t_t(n-1) = %f < Max(PicSizeInMbs / MaxMBPS, fR) = %f).\n",
          name, _dTimeH264HrdVerifierContext(ctx, Tr_n - Tr_n_mo),
          minCpbRemovalDelay
        );
      }
    }

    /* H.264 A.3.1.d) and A.3.3.j) */
    if (
      sps->profile_idc == H264_PROFILE_BASELINE
      || sps->profile_idc == H264_PROFILE_MAIN
      || sps->profile_idc == H264_PROFILE_EXTENDED

      || sps->profile_idc == H264_PROFILE_HIGH
    ) {
      assert(0 != sh->PicSizeInMbs);

      int64_t MaxNumBytesInAU = (int64_t) floor(
        384.0 * cnstr->MaxMBPS
        * _dTimeH264HrdVerifierContext(ctx, Tr_n - Tr_n_mo)
        / getH264MinCR(sps->level_idc)
      );
      // TODO: This formula miss the part induced by low_delay flag.

      if (MaxNumBytesInAU < NumBytesInAU) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_1_D),
          "ITU-T Rec. H.264 A.3.1.d) constraint is not satisfied "
          "(MaxNumBytesInNALunit = %" PRId64 " "
          "< NumBytesInNALunit = %" PRId64 ").\n",
          MaxNumBytesInAU,
          NumBytesInAU
        );
      }
    }

    /* H.264 A.3.3.b) */
    if (
      sps->profile_idc == H264_PROFILE_MAIN
      || sps->profile_idc == H264_PROFILE_HIGH
      || sps->profile_idc == H264_PROFILE_HIGH_10
      || sps->profile_idc == H264_PROFILE_HIGH_422
      || sps->profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
      || sps->profile_idc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      double duration = _dTimeH264HrdVerifierContext(ctx, Tr_n - Tr_n_mo);
      double MaxNumSlices = duration * cnstr->MaxMBPS / cnstr->SliceRate;

      if (floor(MaxNumSlices) < 1.0 * NumSlicesAU) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_3_B),
          "ITU-T Rec. H.264 A.3.3.b) constraint is not satisfied "
          "(maxNbSlices = %f < nbSlices = %u).\n",
          MaxNumSlices,
          NumSlicesAU
        );
      }
    }
  }
  else {
    /* Access Unit 0 */

    /* H.264 A.3.1.c) and A.3.3.i) */
    if (
      sps->profile_idc == H264_PROFILE_BASELINE
      || sps->profile_idc == H264_PROFILE_MAIN
      || sps->profile_idc == H264_PROFILE_EXTENDED

      || sps->profile_idc == H264_PROFILE_HIGH
    ) {
      assert(0 != sh->PicSizeInMbs);

      int64_t MaxNumBytesInFirstAU = (int64_t) floor(
        384.0 * MAX(1.0 * sh->PicSizeInMbs, fR * cnstr->MaxMBPS)
        / getH264MinCR(sps->level_idc)
      );
      // TODO: This formula miss the part induce by low_delay flag.

      if (MaxNumBytesInFirstAU < NumBytesInAU) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_1_C),
          "ITU-T Rec. H.264 A.3.1.c) constraint is not satisfied "
          "(MaxNumBytesInFirstAU = %" PRId64
          " < NumBytesInFirstAU = %" PRId64 ").\n",
          MaxNumBytesInFirstAU,
          NumBytesInAU
        );
      }
    }

    /* H.264 A.3.3.a) */
    if (
      sps->profile_idc == H264_PROFILE_MAIN
      || sps->profile_idc == H264_PROFILE_HIGH
      || sps->profile_idc == H264_PROFILE_HIGH_10
      || sps->profile_idc == H264_PROFILE_HIGH_422
      || sps->profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
      || sps->profile_idc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      double MaxNumSlices = ceil(
        MAX(1.0 * sh->PicSizeInMbs, fR * cnstr->MaxMBPS)
        / cnstr->SliceRate
      );

      if (MaxNumSlices < 1.0 * NumSlicesAU) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, A_3_3_A),
          "ITU-T Rec. H.264 A.3.3.a) constraint is not satisfied "
          "(maxNbSlices = %.0f < nbSlices = %u).\n",
          MaxNumSlices,
          NumSlicesAU
        );
      }
    }
  }

  return 0; /* OK */
}

int processAUH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264ParametersHandlerPtr handle,
  int64_t au_size
)
{
  H264HrdVerifierWarnings * warn = &ctx->warnings;

  const H264SPSDataParameters * sps     = &handle->sequenceParametersSet.data;
  const H264HrdParameters * hrd         = &sps->vui_parameters.nal_hrd_parameters;
  const H264SliceHeaderParameters * sh  = &handle->slice.header;
  const H264DecRefPicMarking * dec_ref_pic_marking = &sh->dec_ref_pic_marking;
  const H264SeiBufferingPeriod * bp_sei = &handle->sei.bufferingPeriod;
  const H264SeiPicTiming * pt_sei       = &handle->sei.picTiming;
  bool first_au_in_buf_period           = handle->sei.bufferingPeriodValid;

  // NAL HRD parameters
  unsigned SchedSelIdx = _selectSchedSelIdx(sps);
  const H264SchedSel * nalhrd = &hrd->schedSel[SchedSelIdx];
  const H264SeiBufferingPeriodSchedSel * nalhrd_bp = &bp_sei->nal_hrd_parameters[SchedSelIdx];

  /* Parse required parameters from signal : */
  uint64_t initial_cpb_removal_delay = nalhrd_bp->initial_cpb_removal_delay;
  uint64_t initial_cpb_removal_delay_offset = nalhrd_bp->initial_cpb_removal_delay_offset;
  uint64_t cpb_removal_delay = pt_sei->cpb_removal_delay;
  uint64_t dpb_output_delay  = pt_sei->dpb_output_delay;

  uint64_t Taf_n_mo = ctx->prev_AU.final_arrival_time;
    // At every process, clock_time stops on AU final arrival time.

  /* Compute AU timing values: */

  /* C.1.2 Timing of coded picture removal */
  uint64_t Tr_n; // t_r(n) - removal time
  if (0 == ctx->nbProcessedAU)
    Tr_n = initial_cpb_removal_delay * ctx->c90; // C-7
  else {
    Tr_n = ctx->nominalRemovalTimeFirstAU + ctx->t_c * cpb_removal_delay; /* C-8 / C-9 */
  }

  /* Since low_delay_hrd_flag is not supported, C-10 and C-11 equations are not used. */

  /* Saving CPB removal time */
  ctx->removalTimeAU = (uint64_t) ceil(_dTimeH264HrdVerifierContext(ctx, Tr_n) * MAIN_CLOCK_27MHZ);
  handle->curProgParam.auCpbRemovalTime = ctx->removalTimeAU;

  uint64_t To_dpb_n = Tr_n + ctx->t_c * dpb_output_delay;
  ctx->outputTimeAU = (uint64_t) ceil(_dTimeH264HrdVerifierContext(ctx, To_dpb_n) * MAIN_CLOCK_27MHZ);
  handle->curProgParam.auDpbOutputTime = ctx->outputTimeAU;

  if (first_au_in_buf_period) /* n_b = n */
    ctx->nominalRemovalTimeFirstAU = Tr_n; /* Saving new t_r(n_b) */

  /* C.1.1 Timing of bitstream arrival */
  uint64_t Tai_n; // t_ai(n) - initial arrival time
  if (0 == ctx->nbProcessedAU) {
    Tai_n = 0; // t_ai(0) = 0
  }
  else {
    if (nalhrd->cbr_flag) {
      Tai_n = Taf_n_mo;
    }
    else {
      /* VBR case */
      /* t_ai,earliest(n) - earliest initial arrival time: */
      uint64_t total_init_cpb_removal_delay = initial_cpb_removal_delay;
      if (!first_au_in_buf_period)
        total_init_cpb_removal_delay += initial_cpb_removal_delay_offset;
      total_init_cpb_removal_delay *= ctx->c90;

      uint64_t Tai_earliest_n = 0;
      if (total_init_cpb_removal_delay < Tr_n) // Avoid negative
        Tai_earliest_n = Tr_n - total_init_cpb_removal_delay;

      Tai_n = MAX(Taf_n_mo, Tai_earliest_n);
    }
  }

  /* t_af(n) - final arrival time: */
  uint64_t Taf_n = Tai_n + (uint64_t) ceil(au_size / ctx->bitrate);

  if (Tr_n < Taf_n) {
    /**
     * Final arrival time is bigger than removal time, only a part of AU will
     * be currently in CPB at removal time.
     *  => Buffer underflow will happen
     */

    LIBBLU_H264_DEBUG_HRD(
      "CPB Underflow, Tr_n = %" PRIu64 " Taf_n = %" PRIu64 ".\n",
      Tr_n,
      Taf_n
    );

    LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
      WARN_COUNT_CHECK_INC(&ctx->warnings, C_3_3),
      "ITU-T Rec. H.264 C.3.3 constraint is not satisfied "
      "(CPB Underflow, Tr_n = %u ms < Taf_n = %u ms "
      "on Access Unit %u, initial arrival time: %u ms).\n",
      _msTimeH264HrdVerifierContext(ctx, Tr_n),
      _msTimeH264HrdVerifierContext(ctx, Taf_n),
      ctx->nbProcessedAU,
      _msTimeH264HrdVerifierContext(ctx, Tai_n)
    );

    Tr_n = Taf_n; // Remove at final arrival time to avoid underflow.
  }

  /* ITU-T Rec. H.264 - C.3 Bitstream conformance Checks: */
  if (_checkH264CpbHrdConformanceTests(ctx, handle, au_size, Tr_n) < 0)
    return -1;

  /* Remove AU with releaseClockTime reached: */
  for (;;) {
    H264CpbHrdAU * cpb_removed_AU = _getOldestAccessUnitFromCPB(ctx);
    if (NULL == cpb_removed_AU || Taf_n < cpb_removed_AU->removal_time)
      break;

    ctx->clock_time = cpb_removed_AU->removal_time;

    int64_t already_transfered_bits = 0;
    if (Tai_n < ctx->clock_time) {
      /* Adds bits from current AU already buffered according to bitrate. */
      already_transfered_bits = ceil(ctx->bitrate * (ctx->clock_time - Tai_n));
    }

    if (ctx->cpb_size < ctx->cpb_occupancy + already_transfered_bits) {
      /* cpb_occupancy + number of bits from current AU already buffered */
      /* exceed CPB size. => Buffer overflow */

      LIBBLU_H264_HRDV_FAIL_WCOND(
        WARN_COUNT_CHECK(warn, C_3_2),
        "ITU-T Rec. H.264 C.3.2 constraint is not satisfied "
        "(CPB Overflow happen, CPB size = %" PRId64 " bits < %" PRId64 " bits).\n",
        ctx->cpb_size,
        ctx->cpb_occupancy + already_transfered_bits
      );
      LIBBLU_H264_HRDV_FAIL_WCOND(
        WARN_COUNT_CHECK(warn, C_3_2),
        " => Affect while trying to remove Access Unit %u, "
        "at removal time: %u ms.\n",
        cpb_removed_AU->AU_idx,
        _msTimeH264HrdVerifierContext(ctx, ctx->clock_time)
      );

      if (Tai_n < ctx->clock_time) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, C_3_2),
          " => Access Unit %u was in transfer "
          "(%.0f bits already in CPB over %" PRId64 " total bits).\n",
          ctx->nbProcessedAU,
          ceil(ctx->bitrate * (double) (ctx->clock_time - Tai_n)),
          au_size
        );
      }
      else if (0 < ctx->nbProcessedAU) {
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, C_3_2),
          " => Happen during final arrival time of Access Unit %u "
          "and initial arrival time of Access Unit %u interval.\n",
          ctx->nbProcessedAU - 1,
          ctx->nbProcessedAU
        );
      }
      else
        LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
          WARN_COUNT_CHECK_INC(warn, C_3_2),
          " => Happen at stream start.\n"
        );
    }

    /* Else AU can be removed safely. */
    assert(0 <= ctx->cpb_occupancy - cpb_removed_AU->size);
      /* Buffer underflow shouldn't happen here. */

    ECHO_DEBUG_CPB_HRDV_CTX(
      ctx, "Removing %" PRId64 " bits from CPB (picture %u) at %u ms.\n",
      cpb_removed_AU->size,
      cpb_removed_AU->AU_idx,
      _msTimeH264HrdVerifierContext(ctx, ctx->clock_time)
    );
    ctx->cpb_occupancy -= cpb_removed_AU->size;

    if (_outputCpbStatistics(ctx, ctx->clock_time, 0) < 0)
      return -1;

    /* Transfer decoded picture to DPB: */
    H264DpbHrdPicInfos pic_infos = cpb_removed_AU->infos;
    if (_applyDecodedReferencePictureMarkingProcess(ctx, &pic_infos) < 0)
      return -1;

    /* C.2.3.2. Not applied. */

    ECHO_DEBUG_DPB_HRDV_CTX(
      ctx, "Adding picture %u to DPB (display: %u) at %u ms.\n",
      cpb_removed_AU->AU_idx,
      pic_infos.frame_display_num,
      _msTimeH264HrdVerifierContext(ctx, ctx->clock_time)
    );

    if (_updateDPBH264HrdContext(ctx, ctx->clock_time) < 0) /* Update before insertion. */
      return -1;

    if (_outputDpbStatistics(ctx, cpb_removed_AU) < 0)
      return -1;

    uint64_t Tout_n = ctx->clock_time + ctx->c90 * pic_infos.dpb_output_delay;

    /* C.2.4.2 Storage of a non-reference picture into the DPB */
    bool StorePicFlag = (ctx->clock_time < Tout_n);
    // if (pic_infos.usage != H264_NOT_USED_AS_REFERENCE)
    //   StorePicFlag = (ctx->clock_time < Tout_n);
    // TODO: Add OutputFlag computation

    if (StorePicFlag) {
      if (_addDecodedPictureToH264HrdContext(ctx, *cpb_removed_AU, Tout_n) < 0)
        return -1;
    }
    else
      ECHO_DEBUG_DPB_HRDV_CTX(
        ctx, " -> Not stored in DPB (according to ITU-T Rec. H.264 C.2.4.2).\n"
      );

    _printDPBStatusH264HrdContext(ctx);

    if (ctx->dpb_size < ctx->nb_au_dpb_content) {
      LIBBLU_H264_HRDV_FAIL_WCOND(
        WARN_COUNT_CHECK(warn, C_3_5),
        "ITU-T Rec. H.264 C.3.5 constraint is not satisfied "
        "(DPB Overflow happen, DPB size = %u pictures < %u pictures).\n",
        ctx->dpb_size, ctx->nb_au_dpb_content
      );
      LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
        WARN_COUNT_CHECK_INC(warn, C_3_5),
        " => Affect while trying to add Access Unit %u, at removal time: %u ms.\n",
        cpb_removed_AU->AU_idx,
        _msTimeH264HrdVerifierContext(ctx, ctx->clock_time)
      );
    }

    if (_popAUFromCPBH264HrdVerifierContext(ctx))
      return -1;
  }

  /* Adding current AU : */
  H264DpbHrdPicInfos infos = (H264DpbHrdPicInfos) {
    .frame_display_num = handle->curProgParam.PicOrderCnt,
    .frame_num = sh->frame_num,
    .field_pic_flag = sh->field_pic_flag,
    .bottom_field_flag = sh->bottom_field_flag,
    .IdrPicFlag = (
      sh->isIdrPic
      && !dec_ref_pic_marking->no_output_of_prior_pics_flag
    ),
    .dpb_output_delay = dpb_output_delay
  };

  if (sh->isIdrPic) {
    if (dec_ref_pic_marking->long_term_reference_flag)
      infos.usage = H264_USED_AS_LONG_TERM_REFERENCE;
    else
      infos.usage = H264_USED_AS_SHORT_TERM_REFERENCE;
  }
  else {
    if (
      dec_ref_pic_marking->adaptive_ref_pic_marking_mode_flag
      && dec_ref_pic_marking->presenceOfMemManCtrlOp6
    ) {
      infos.usage = H264_USED_AS_LONG_TERM_REFERENCE;
    }
    else {
      if (sh->isRefPic)
        infos.usage = H264_USED_AS_SHORT_TERM_REFERENCE;
      else
        infos.usage = H264_NOT_USED_AS_REFERENCE;
    }
  }

  if (
    !sh->isIdrPic
    && dec_ref_pic_marking->adaptive_ref_pic_marking_mode_flag
  ) {
    memcpy(
      &infos.MemMngmntCtrlOp,
      &dec_ref_pic_marking->MemMngmntCtrlOp,
      dec_ref_pic_marking->nbMemMngmntCtrlOp * sizeof(H264MemMngmntCtrlOpBlk)
    );
    infos.nb_MemMngmntCtrlOp = dec_ref_pic_marking->nbMemMngmntCtrlOp;
  }
  else
    infos.nb_MemMngmntCtrlOp = 0;

  if (ctx->clock_time < Taf_n)
    ctx->clock_time = Taf_n;  /* Align clock to Taf_n for next AU process. */

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, "Adding %zu bits to CPB (picture %u) at %u ms (during %u ms).\n",
    au_size,
    ctx->nbProcessedAU,
    _msTimeH264HrdVerifierContext(ctx, Tai_n),
    _msTimeH264HrdVerifierContext(ctx, ctx->clock_time - Tai_n)
  );

  ctx->cpb_occupancy += au_size;
  if (_addAUToCPBH264HrdVerifierContext(ctx, au_size, Tr_n, ctx->nbProcessedAU, infos) < 0)
    return -1;

  ECHO_DEBUG_CPB_HRDV_CTX(
    ctx, " -> CPB fullness: %zu bits.\n",
    ctx->cpb_occupancy
  );

  if (_outputCpbStatistics(ctx, Tai_n, ctx->clock_time - Tai_n) < 0)
    return -1;

  if (ctx->cpb_size < ctx->cpb_occupancy) {
    /* cpb_occupancy exceed CPB size. */
    /* => Buffer overflow */

    LIBBLU_H264_HRDV_FAIL_WCOND(
      WARN_COUNT_CHECK(warn, C_3_2),
      "ITU-T Rec. H.264 C.3.2 constraint is not satisfied "
      "(CPB Overflow happen, CPB size = %" PRId64 " bits < %" PRId64 " bits).\n",
      ctx->cpb_size,
      ctx->cpb_occupancy
    );
    LIBBLU_H264_HRDV_FAIL_WCOND_RETURN(
      WARN_COUNT_CHECK_INC(warn, C_3_2),
      " => Affect while trying to append Access Unit %u, "
      "at final arrival time: %u ms.\n",
      ctx->nbProcessedAU,
      _msTimeH264HrdVerifierContext(ctx, ctx->clock_time)
    );
  }

  /* Save constraints checks related parameters: */
  assert(0 < sh->PicSizeInMbs);
  assert(0x00 != sps->level_idc);

  ctx->prev_AU = (H264HrdVerifierAUProperties) {
    .frame_num = sh->frame_num,
    .PicSizeInMbs = sh->PicSizeInMbs,
    .level_idc = sps->level_idc,
    .final_arrival_time = Taf_n,
    .removal_time = Tr_n,
    .initial_cpb_removal_delay = initial_cpb_removal_delay,
    .initial_cpb_removal_delay_offset = initial_cpb_removal_delay_offset
  };

  ctx->nbProcessedAU++;
  ctx->pesNbAlreadyProcessedBytes += au_size;

  return 0; /* OK */
}