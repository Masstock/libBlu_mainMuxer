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
 * \todo Complete rework of DPB management.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__HRD_VERIFIER_H__
#define __LIBBLU_MUXER__CODECS__H264__HRD_VERIFIER_H__

#include "../../elementaryStreamOptions.h"
#include "../../ini/iniHandler.h"
#include "../../util.h"
#include "../common/esParsingSettings.h"

#include "h264_error.h"
#include "h264_data.h"
#include "h264_util.h"

#define HRD_VERIFIER_VERBOSE_LEVEL 1

#define H264_HRD_VERIFIER_NAME  lbc_str("HRD-Verifier")
#define H264_CPB_HRD_MSG_NAME   lbc_str("HRD-Verifier CPB")
#define H264_DPB_HRD_MSG_NAME   lbc_str("HRD-Verifier DPB")
#define H264_HRD_VERIFIER_PREFIX H264_HRD_VERIFIER_NAME lbc_str(": ")
#define H264_CPB_HRD_MSG_PREFIX  H264_CPB_HRD_MSG_NAME  lbc_str(": ")
#define H264_DPB_HRD_MSG_PREFIX  H264_DPB_HRD_MSG_NAME  lbc_str(": ")

static inline bool checkH264CpbHrdVerifierAvailablity(
  const H264CurrentProgressParam * cur_state,
  const LibbluESSettingsOptions * options,
  const H264SPSDataParameters * sps
)
{

  if (cur_state->stillPictureTolerance) // TODO: Check still pictures.
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by use of still pictures.\n");

  if (options->discardSei)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by SEI discarding.\n");
  if (options->forceRebuildSei)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by SEI rebuilding.\n");

  if (!sps->vui_parameters_present_flag)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing VUI parameters.\n");
  if (!sps->vui_parameters.nal_hrd_parameters_present_flag)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing NAL HRD parameters.\n");
  if (sps->vui_parameters.low_delay_hrd_flag)  // TODO: Low Delay not supported
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by use of Low Delay.\n");
  if (!sps->vui_parameters.timing_info_present_flag)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing timing info in VUI parameters.\n");

  return true; /* Available */
}

typedef struct {
  unsigned max_amount;

  unsigned A_3_1_j__A_3_3_g;  /**< ITU-T Rec. H.264 A.3.1.j) / A.3.3.g)      */
  unsigned A_3_1_i__A_3_3_h;  /**< ITU-T Rec. H.264 A.3.1.i) / A.3.3.h)      */
  unsigned C_15__C_16_ceil;   /**< ITU-T Rec. H.264 C-15/C-16 ceil equation  */
  unsigned C_16_floor;        /**< ITU-T Rec. H.264 C-16 floor equation      */
  unsigned C_3_2;             /**< ITU-T Rec. H.264 C.3.2      */
  unsigned C_3_3;             /**< ITU-T Rec. H.264 C.3.3      */
  unsigned C_3_5;             /**< ITU-T Rec. H.264 C.3.5      */
  unsigned A_3_1_C;           /**< ITU-T Rec. H.264 A.3.1.c)      */
  unsigned A_3_1_D;           /**< ITU-T Rec. H.264 A.3.1.d)      */
  unsigned A_3_1_A__A_3_2_A;  /**< ITU-T Rec. H.264 A.3.1.a) / A.3.2.a)      */
  unsigned A_3_3_B;           /**< ITU-T Rec. H.264 A.3.3.b)      */
  unsigned A_3_3_A;           /**< ITU-T Rec. H.264 A.3.3.a)      */

  unsigned invalid_memmng;

  unsigned bdav_maxbr;
  unsigned bdav_cpbsize;

  unsigned dpb_au_overflow;   /**< More AU in DPB than supported. */
} H264HrdVerifierWarnings;

typedef struct {
  FILE * cpb_fd;
  FILE * dpb_fd;
} H264HrdVerifierDebug;

typedef enum {
  H264_NOT_USED_AS_REFERENCE         = 0x0,
  H264_USED_AS_SHORT_TERM_REFERENCE  = 0x1,
  H264_USED_AS_LONG_TERM_REFERENCE   = 0x2
} H264DpbHrdRefUsage;

typedef struct {
  int64_t frame_display_num;

  unsigned frame_num;
  bool field_pic_flag;
  bool bottom_field_flag;
  bool IdrPicFlag;

  uint64_t dpb_output_delay;
  H264DpbHrdRefUsage usage;
  H264MemMngmntCtrlOpBlk MemMngmntCtrlOp[H264_MAX_SUPPORTED_MEM_MGMNT_CTRL_OPS];
  unsigned nb_MemMngmntCtrlOp; /* 0: No op. */

  /* Computed values: */
  unsigned LongTermFrameIdx;
} H264DpbHrdPicInfos; /* Used when decoded picture enters DPB. */

typedef struct {
  unsigned AU_idx; /* For debug output. */

  int64_t size; /* In bits. */

  uint64_t removal_time; /* t_r in c ticks. */

  H264DpbHrdPicInfos infos;
} H264CpbHrdAU;

#define H264_MAX_AU_IN_CPB  4096 /* pow(2) ! */
#define H264_AU_CPB_MOD_MASK  (H264_MAX_AU_IN_CPB - 1)

typedef struct {
  unsigned AU_idx; /* For debug output. */
  int64_t frame_display_num; /* Derivated from picOrderCnt */

  unsigned frame_num; /* SliceHeader() frame_num */
  bool field_pic_flag;
  bool bottom_field_flag;

  uint64_t output_time; /* t_o,dpb in c ticks. */
  H264DpbHrdRefUsage usage;

  unsigned LongTermFrameIdx;
} H264DpbHrdPic;

#define H264_MAX_DPB_SIZE  (1u << 5) /* pow(2) ! */
#define H264_DPB_MOD_MASK  (H264_MAX_DPB_SIZE - 1)

typedef struct {
  unsigned frame_num;

  unsigned PicSizeInMbs;
  uint8_t level_idc;

  uint64_t final_arrival_time;
  uint64_t removal_time;

  uint64_t initial_cpb_removal_delay;
  uint64_t initial_cpb_removal_delay_offset;
} H264HrdVerifierAUProperties;

typedef struct {
  double second; /* c = time_scale * 90000. */
  uint64_t c90; /* c90 = time_scale */
  uint64_t num_units_in_tick;
  uint64_t t_c; /* t_c = num_units_in_tick * 90000. */

  double bitrate; /* In bits per c tick. */
  int64_t cpb_size; /* In bits. */
  unsigned dpb_size; /* In frames. */

  uint64_t clock_time; /* Current time on t_c Hz clock */

  int64_t cpb_occupancy; /* Current CPB state in bits. */

  /* Access Units currently in CPB: */
  H264CpbHrdAU cpb_content[H264_MAX_AU_IN_CPB]; /* Circular buffer list (FIFO). */
  H264CpbHrdAU * cpb_content_heap; /* Oldest AU added. */
  unsigned nb_au_cpb_content; /* Nb of AU currently in CPB. */

  /* Frames currently in DPB: */
  H264DpbHrdPic dpb_content[H264_MAX_DPB_SIZE]; /* Circular buffer list (FIFO). */
  H264DpbHrdPic * dpb_content_heap; /* Oldest decoded picture added. */
  unsigned nb_au_dpb_content; /* Nb of pictures currently in DPB. */

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

  int MaxLongTermFrameIdx;  /**< If -1, means "no long-term frame indices".  */

  H264HrdVerifierAUProperties prev_AU;

  H264HrdVerifierWarnings warnings;
  H264HrdVerifierDebug debug;
} H264HrdVerifierContext, *H264HrdVerifierContextPtr;

H264HrdVerifierContextPtr createH264HrdVerifierContext(
  LibbluESSettingsOptions * options,
  const H264SPSDataParameters * sps,
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
  H264ParametersHandlerPtr handle,
  int64_t accessUnitSize
);

#endif