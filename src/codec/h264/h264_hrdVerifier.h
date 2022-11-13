/** \~english
 * \file h264_hrdVerifier.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams Hypothetical Reference Decoder conformance
 * verification module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__HRD_VERIFIER_H__
#define __LIBBLU_MUXER__CODECS__H264__HRD_VERIFIER_H__

#include "../../util.h"
#include "../../elementaryStreamOptions.h"
#include "../common/esParsingSettings.h"

#include "h264_error.h"
#include "h264_data.h"

#define HRD_VERIFIER_VERBOSE_LEVEL 1

/* NOTE ABOUT TIME VALUES:
 * t values in ITU-T H.264 are express in seconds.
 * Here t values are express in time_scale * 90000 tick units
 * for convenience.
 * That means some time-base values are modified to apply this
 * clock scale and cannot be interpreted as original ones.
 * Conversion is usable for debugging with convertTimeH264HrdVerifierContext() function.
 */

#define H264_HRD_VERIFIER_NAME  lbc_str("HRD-Verifier")
#define H264_CPB_HRD_MSG_NAME   lbc_str("HRD-Verifier CPB")
#define H264_DPB_HRD_MSG_NAME   lbc_str("HRD-Verifier DPB")
#define H264_HRD_VERIFIER_PREFIX H264_HRD_VERIFIER_NAME lbc_str(": ")
#define H264_CPB_HRD_MSG_PREFIX  H264_CPB_HRD_MSG_NAME  lbc_str(": ")
#define H264_DPB_HRD_MSG_PREFIX  H264_DPB_HRD_MSG_NAME  lbc_str(": ")

#define H264_90KHZ_CLOCK  SUB_CLOCK_90KHZ

#define ELECARD_STREAMEYE_COMPARISON  false

static inline bool checkH264CpbHrdVerifierAvailablity(
  const H264CurrentProgressParam * curState,
  LibbluESSettingsOptions options
)
{
  assert(NULL != curState);

  if (curState->stillPictureTolerance)
    return false; /* Disabled for still pictures */

  if (options.discardSei) {
    LIBBLU_INFO(
      H264_HRD_VERIFIER_PREFIX
      "Compliance tests disabled by SEI discarding.\n"
    );
    return false; /* Disabled */
  }

  if (options.forceRebuildSei) {
    LIBBLU_INFO(
      H264_HRD_VERIFIER_PREFIX
      "Compliance tests disabled by SEI rebuilding.\n"
    );
    return false; /* Disabled */
  }

  return true; /* Available */
}

typedef enum {
  H264_NOT_USED_AS_REFERENCE        = 0x0,
  H264_USED_AS_SHORT_TERM_REFERENCE = 0x1,
  H264_USED_AS_LONG_TERM_REFERENCE  = 0x2
} H264DpbHrdRefUsage;

typedef struct {
  int64_t frameDisplayNum;
  unsigned frame_num;
  bool field_pic_flag;
  bool bottom_field_flag;
  bool IdrPicFlag;

  uint64_t dpb_output_delay;
  H264DpbHrdRefUsage usage;
  H264MemMngmntCtrlOpBlkPtr memMngmntCtrlOperations; /* NULL: No op. */

  /* Computed values: */
  unsigned longTermFrameIdx;
} H264DpbHrdPicInfos; /* Used when decoded picture enters DPB. */

typedef struct {
  unsigned AUIdx; /* For debug output. */

  size_t length; /* In bits. */

  uint64_t removalTime; /* t_r in c ticks. */

  H264DpbHrdPicInfos picInfos;
} H264CpbHrdAU;

#define H264_MAX_AU_IN_CPB                       1024 /* pow(2) ! */
#define H264_AU_CPB_MOD_MASK (H264_MAX_AU_IN_CPB - 1)

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
  bool cpb;
  bool dpb;
  bool fileOutput;
} H264HrdVerifierDebugFlags;

static inline void setFromOptionsH264HrdVerifierDebugFlags(
  H264HrdVerifierDebugFlags * dst,
  const LibbluESSettingsOptions * options
)
{
  *dst = (H264HrdVerifierDebugFlags) {
    .cpb = options->echoHrdCpb,
    .dpb = options->echoHrdDpb,
    .fileOutput = (NULL != options->echoHrdLogFilepath)
  };
}

#define H264_MAX_DPB_SIZE                          32 /* pow(2) ! */
#define H264_DPB_MOD_MASK     (H264_MAX_DPB_SIZE - 1)

typedef struct {
  double second; /* c = time_scale * 90000. */
  uint64_t c90; /* c90 = time_scale */
  uint64_t num_units_in_tick;
  uint64_t t_c; /* t_c = num_units_in_tick * 90000. */

  double bitrate; /* In bits per c tick. */
  bool cbr; /* true: CRB stream, false: VBR stream. */
  uint64_t cpbSize; /* In bits. */
  unsigned dpbSize; /* In frames. */

  uint64_t clockTime; /* Current time on t_c Hz clock */

  uint64_t cpbBitsOccupancy; /* Current CPB state in bits. */

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
  size_t nbProcessedBytes;

  int maxLongTermFrameIdx;

  struct {
    unsigned frame_num;

    unsigned PicSizeInMbs;
    uint8_t level_idc;

    uint64_t removalTime;

    uint64_t pts;
    uint64_t dts;

    uint64_t initial_cpb_removal_delay;
    uint64_t initial_cpb_removal_delay_offset;
  } nMinusOneAUParameters;

  H264HrdVerifierDebugFlags hrdDebugFlags;
  FILE * hrdDebugFd;
} H264HrdVerifierContext, *H264HrdVerifierContextPtr;

H264HrdVerifierContextPtr createH264HrdVerifierContext(
  const LibbluESSettingsOptions * options,
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

/** \~english
 * \brief Convert a time value in c ticks to seconds.
 *
 * \param ctx Used HRD context.
 * \param time Time value to convert in c ticks.
 * \return double Associated time value in seconds.
 */
double convertTimeH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  uint64_t time
);

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
int addAUToCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx,
  size_t length,
  uint64_t removalTime,
  unsigned AUIdx,
  H264DpbHrdPicInfos picInfos
);

/** \~english
 * \brief Remove the oldest declared Access Unit from the CPB FIFO.
 *
 * \param ctx Used HRD context.
 * \return int Upon successfull poping, a zero value is returned. Otherwise,
 * a negative value is returned.
 */
int popAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
);

H264CpbHrdAU * getOldestAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
);

H264CpbHrdAU * getNewestAUFromCPBH264HrdVerifierContext(
  H264HrdVerifierContextPtr ctx
);

int addDecodedPictureToH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  H264CpbHrdAU * pic,
  uint64_t outputTime
);

int popDecodedPictureFromH264HrdContext(H264HrdVerifierContextPtr ctx, unsigned idx);
int setDecodedPictureAsRefUnusedInH264HrdContext(H264HrdVerifierContextPtr ctx, unsigned idx);
H264DpbHrdPic * getDecodedPictureFromH264HrdContext(H264HrdVerifierContextPtr ctx, unsigned idx);

void printDPBStatusH264HrdContext(H264HrdVerifierContextPtr ctx);

int updateDPBH264HrdContext(H264HrdVerifierContextPtr ctx, uint64_t currentTime);
void clearDPBH264HrdContext(H264HrdVerifierContextPtr ctx);

int markAllDecodedPicturesAsUnusedH264HrdContext(H264HrdVerifierContextPtr ctx);

int markShortTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned difference_of_pic_nums_minus1,
  H264DpbHrdPicInfos * picInfos
);

int markLongTermRefPictureAsUnusedForReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum
);
int markShortTermRefPictureAsUsedForLongTermReferenceH264HrdContext(
  H264HrdVerifierContextPtr ctx,
  unsigned frameNum, unsigned longTermFrameIdx
);

int checkH264CpbHrdConformanceTests(
  H264ConstraintsParam * constraints,
  H264HrdVerifierContextPtr ctx,
  size_t AUlength,
  bool AUIsNewBufferingPeriod,
  H264HrdBufferingPeriodParameters * AUNalHrdParam,
  H264SPSDataParameters * AUSpsData,
  H264SliceHeaderParameters * AUSliceHeader,
  uint64_t Tr_n, unsigned AUNbSlices
);

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
);

#endif