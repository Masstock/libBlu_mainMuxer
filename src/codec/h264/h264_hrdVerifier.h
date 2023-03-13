/** \~english
 * \file h264_hrdVerifier.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams Hypothetical Reference Decoder conformance
 * verification module.
 *
 * \bug Issues with interlaced streams.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__HRD_VERIFIER_H__
#define __LIBBLU_MUXER__CODECS__H264__HRD_VERIFIER_H__

#include "../../elementaryStreamOptions.h"
#include "../../ini/iniHandler.h"
#include "../../util.h"
#include "../common/esParsingSettings.h"

#include "h264_error.h"
#include "h264_data.h"

#define HRD_VERIFIER_VERBOSE_LEVEL 1

#define H264_HRD_VERIFIER_NAME  lbc_str("HRD-Verifier")
#define H264_CPB_HRD_MSG_NAME   lbc_str("HRD-Verifier CPB")
#define H264_DPB_HRD_MSG_NAME   lbc_str("HRD-Verifier DPB")
#define H264_HRD_VERIFIER_PREFIX H264_HRD_VERIFIER_NAME lbc_str(": ")
#define H264_CPB_HRD_MSG_PREFIX  H264_CPB_HRD_MSG_NAME  lbc_str(": ")
#define H264_DPB_HRD_MSG_PREFIX  H264_DPB_HRD_MSG_NAME  lbc_str(": ")

#define H264_90KHZ_CLOCK  SUB_CLOCK_90KHZ

#define H264_HRD_DISABLE_C_3_2  false

static inline bool checkH264CpbHrdVerifierAvailablity(
  H264CurrentProgressParam currentState,
  LibbluESSettingsOptions options,
  H264SPSDataParameters sps
)
{

  if (currentState.stillPictureTolerance) // TODO: Check still pictures.
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by use of still pictures.\n");

  if (options.discardSei)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by SEI discarding.\n");
  if (options.forceRebuildSei)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by SEI rebuilding.\n");

  if (!sps.vui_parameters_present_flag)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing VUI parameters.\n");
  if (!sps.vui_parameters.nal_hrd_parameters_present_flag)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing NAL HRD parameters.\n");
  if (sps.vui_parameters.low_delay_hrd_flag)  // TODO: Low Delay not supported
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by use of Low Delay.\n");
  if (!sps.vui_parameters.timing_info_present_flag)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing timing info in VUI parameters.\n");

  return true; /* Available */
}

typedef enum {
  H264_NOT_USED_AS_REFERENCE         = 0x0,
  H264_USED_AS_SHORT_TERM_REFERENCE  = 0x1,
  H264_USED_AS_LONG_TERM_REFERENCE   = 0x2
} H264DpbHrdRefUsage;

typedef struct {
  int64_t frameDisplayNum;
  unsigned frame_num;
  bool field_pic_flag;
  bool bottom_field_flag;
  bool IdrPicFlag;

  uint64_t dpb_output_delay;
  H264DpbHrdRefUsage usage;
  H264MemMngmntCtrlOpBlk MemMngmntCtrlOp[H264_MAX_SUPPORTED_MEM_MGMNT_CTRL_OPS];
  unsigned nbMemMngmntCtrlOp; /* 0: No op. */

  /* Computed values: */
  unsigned longTermFrameIdx;
} H264DpbHrdPicInfos; /* Used when decoded picture enters DPB. */

typedef struct {
  unsigned AUIdx; /* For debug output. */

  int64_t size; /* In bits. */

  uint64_t removalTime; /* t_r in c ticks. */

  H264DpbHrdPicInfos picInfos;
} H264CpbHrdAU;

#define H264_MAX_AU_IN_CPB  1024 /* pow(2) ! */
#define H264_AU_CPB_MOD_MASK  (H264_MAX_AU_IN_CPB - 1)

typedef struct {
  unsigned AUIdx; /* For debug output. */

  int64_t frameDisplayNum; /* Derivated from picOrderCnt */
  unsigned frame_num; /* SliceHeader() frame_num */
  bool field_pic_flag;
  bool bottom_field_flag;

  uint64_t outputTime; /* t_o,dpb in c ticks. */
  H264DpbHrdRefUsage usage;

  unsigned longTermFrameIdx;
} H264DpbHrdPic;

typedef struct {
  FILE * cpbFd;
  FILE * dpbFd;
} H264HrdVerifierDebug;

typedef struct {
  bool abortOnError;
} H264HrdVerifierOptions;

static inline void defaultH264HrdVerifierOptions(
  H264HrdVerifierOptions * dst
)
{
  *dst = (H264HrdVerifierOptions) {
    .abortOnError = false
  };
}

#define H264_MAX_DPB_SIZE  32 /* pow(2) ! */
#define H264_DPB_MOD_MASK  (H264_MAX_DPB_SIZE - 1)

typedef struct {
  double second; /* c = time_scale * 90000. */
  uint64_t c90; /* c90 = time_scale */
  uint64_t num_units_in_tick;
  uint64_t t_c; /* t_c = num_units_in_tick * 90000. */

  double bitrate; /* In bits per c tick. */
  bool cbr; /* true: CRB stream, false: VBR stream. */
  int64_t cpbSize; /* In bits. */
  unsigned dpbSize; /* In frames. */

  uint64_t clockTime; /* Current time on t_c Hz clock */

  int64_t cpbBitsOccupancy; /* Current CPB state in bits. */

  /* Access Units currently in CPB: */
  H264CpbHrdAU AUInCpb[H264_MAX_AU_IN_CPB]; /* Circular buffer list (FIFO). */
  H264CpbHrdAU * AUInCpbHeap; /* Oldest AU added. */
  unsigned nbAUInCpb; /* Nb of AU currently in CPB. */

  /* Frames currently in DPB: */
  H264DpbHrdPic picInDpb[H264_MAX_DPB_SIZE]; /* Circular buffer list (FIFO). */
  H264DpbHrdPic * picInDpbHeap; /* Oldest decoded picture added. */
  unsigned nbPicInDpb; /* Nb of pictures currently in DPB. */

  unsigned numShortTerm;
  unsigned numLongTerm;

  unsigned max_num_ref_frames;
  unsigned MaxFrameNum;

  /* Evolution saved parameters: */
  unsigned nbProcessedAU; /* Total current number of processed AU by HRD Verifier. */
  uint64_t nominalRemovalTimeFirstAU; /* t_r(n_b) */
  uint64_t removalTimeAU; /* Current AU CPB removal time in #MAIN_CLOCK_27MHZ ticks */
  uint64_t outputTimeAU; /* Current AU DPB removal time in #MAIN_CLOCK_27MHZ ticks */
  int64_t pesNbAlreadyProcessedBytes;

  int maxLongTermFrameIdx;

  struct {
    unsigned frame_num;

    unsigned PicSizeInMbs;
    uint8_t level_idc;

    uint64_t removalTime;

    uint64_t initial_cpb_removal_delay;
    uint64_t initial_cpb_removal_delay_offset;
  } nMinusOneAUParameters;

  H264HrdVerifierDebug debug;
  H264HrdVerifierOptions options;
} H264HrdVerifierContext, *H264HrdVerifierContextPtr;

H264HrdVerifierContextPtr createH264HrdVerifierContext(
  LibbluESSettingsOptions options,
  const H264SPSDataParameters * spsData,
  const H264ConstraintsParam * constraints
);

void destroyH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
);

void echoDebugH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  LibbluStatus status,
  const lbc * format,
  ...
);

#define ECHO_DEBUG_CPB_HRDV_CTX(ctx, format, ...)                             \
  echoDebugH264HrdVerifierContext(                                            \
    ctx, LIBBLU_DEBUG_H264_HRD_CPB,                                           \
    "H.264 HRD Verifier CPB: " lbc_str(format), ##__VA_ARGS__                 \
  )

#define ECHO_DEBUG_NH_CPB_HRDV_CTX(ctx, format, ...)                          \
  echoDebugH264HrdVerifierContext(                                            \
    ctx, LIBBLU_DEBUG_H264_HRD_CPB, lbc_str(format), ##__VA_ARGS__            \
  )

#define ECHO_DEBUG_DPB_HRDV_CTX(ctx, format, ...)                             \
  echoDebugH264HrdVerifierContext(                                            \
    ctx, LIBBLU_DEBUG_H264_HRD_DPB,                                           \
    "H.264 HRD Verifier DPB: " lbc_str(format), ##__VA_ARGS__                 \
  )

#define ECHO_DEBUG_NH_DPB_HRDV_CTX(ctx, format, ...)                          \
  echoDebugH264HrdVerifierContext(                                            \
    ctx, LIBBLU_DEBUG_H264_HRD_DPB, lbc_str(format), ##__VA_ARGS__            \
  )

int processAUH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264CurrentProgressParam * curState,
  const H264SPSDataParameters * spsData,
  const H264SliceHeaderParameters * sliceHeader,
  const H264SeiPicTiming * picTimingSei,
  const H264SeiBufferingPeriod * bufPeriodSei,
  const H264ConstraintsParam * constraints,
  const bool isNewBufferingPeriod,
  const int64_t accessUnitSize
);

#endif