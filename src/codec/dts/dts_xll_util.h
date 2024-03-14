/** \~english
 * \file dts_xll_util.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams XLL utilities module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__XLL_UTIL_H__
#define __LIBBLU_MUXER__CODECS__DTS__XLL_UTIL_H__

#include "../../util.h"

#include "dts_error.h"
#include "dts_pbr_file.h"
#include "dts_data.h"

typedef struct {
  uint64_t timestamp;
  uint32_t target_frame_size;
} DtsPbrSmoothingStatsFrame;

/** \~english
 * \brief PBR smoothing process statistics.
 *
 * Used during PBR smoothing process.
 */
typedef struct {
  DtsPbrSmoothingStatsFrame *frames;
  unsigned nb_alloc_frames;
  unsigned last_frame_idx;

  uint32_t avg_frame_size;
  bool avg_frame_size_set;

  uint32_t max_pbrs_buf_size;
} DtsPbrSmoothingStats;

int saveFrameSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats *stats,
  unsigned audio_frame_idx,
  uint64_t timestamp,
  uint32_t size
);

typedef struct {
  DcaXllFrameSFPosition position;
  unsigned number;
  bool unpack_started;
} DtsXllDecodedFrame;

/** \~english
 * \brief DTS XLL extension substream parsing handle.
 *
 * A difference is made between regrouped XLL coding components from DTS
 * Extension Substream, called Peak Bit-Rate Smoothed (PBRS) frames and XLL
 * Lossless frames.
 *
 * A PBRS frame is referenced by XLL coding component parameters within Audio
 * Asset header of the DTS Extension Substream. Containing XLL coding component
 * payload from previous indicated XLL Sync (or first payload byte if no
 * previous XLL Sync) to last byte prior to next XLL Sync (or the last payload
 * byte if no following XLL Sync).
 * A PBRS frame contains one XLL Lossless frame, followed by any number of fol-
 * lowing XLL Lossless frames (zero included).
 *
 * The following diagram shows possible Extension Substream payload (headers
 * removed) containing shared PBR frame :
 *
 * [ExtSS frame #0 XLL Coding Component 0 <PBRSF #0... ]
 * [ExtSS frame #1 XLL Coding Component 0 ...End of PBRSF #0> <PBRSF #1... ]
 * ...
 *
 * With start offset (from XLL Coding Component within Audio Asset data first
 * byte) of "PBRSF #1" indicated by associated Audio Asset header
 * 'nuExSSXLLSyncOffset' field.
 *
 * And the first PBRS frame of previous diagram may be composed as shown in
 * following diagram:
 *
 * [PBRS Frame 0 <XLL Lossless frame #0> <XLL Lossless frame #1>]
 *
 * With "XLL Lossless frame #1" start offset at "XLL Lossless frame #0" end
 * plus observed 32-bit bistream boundary alignment.
 *
 * This difference is used since at high level, without parsing XLL content
 * from PBRS buffer, it is impossible to identify presence of multiple
 * XLL Lossless frames in one XLL Coding Component.
 *
 *
 * PBR buffer usage is only compared to Audio Asset header specified size at
 * end of ExtSS parsing. Since according to DTS specs, it may be set to zero
 * if all frames from bitstreams are instantly decoded. In this manner, we
 * only check if decoding delayed XLL frames cause overflow.
 */
typedef struct {
  uint8_t *pbrs_buf;          /**< PBRS buffer used to re-assemble XLL
    frames prior to decoding.                                                */
  uint32_t pbrs_buf_alloc_size;  /**< PBR buffer allocated size in bytes.      */
  uint32_t pbr_buf_size;         /**< PBR buffer active max size according to
    audio asset parameters. If this value is exceed, an error is returned.   */
  uint32_t pbr_buf_used_size;    /**< PBR buffer current usage level in bytes. */

  /* PBR buffer unpacking related */
  bool initializedPbrUnpack;   /**< Is PBR buffer data unpacking is active.  */
  LibbluBitReader bit_reader;
  // uint32_t pbrBufferParsedSize;

  // uint8_t byteBuf;
  // unsigned remainingBits;

  CrcContext crcCtx;

  CircularBuffer pendingFrames;  /**< FIFO of #DcaXllPbrFrame, pending
    frames to decode in PBR buffer.                                          */
  unsigned nbParsedPbrFrames;       /**< Current number of parsed PBR
    frames.                                                                  */

  CircularBuffer decodedFramesOff;  /**< FIFO of
    #DtsXllDecodedFrame, decoded frames source file offsets.                 */
  unsigned nbDecodedFrames;         /**< Current number of decoded XLL
    frames.                                                                  */
  unsigned nbSlicedFrames;

  /* PBR smoothing process related */
  DtsPbrSmoothingStats pbrSmoothing;

  uint32_t pbr_buf_peak_size;  /**< Maximal PBR buffer usage level at end of
    ExtSS parsing in bytes.                                                  */
} DtsXllFrameContext;

static inline void cleanDtsXllFrameContext(
  DtsXllFrameContext ctx
)
{
  free(ctx.pbrs_buf);
  cleanCircularBuffer(ctx.pendingFrames);
  cleanCircularBuffer(ctx.decodedFramesOff);
  free(ctx.pbrSmoothing.frames);
}

int initDtsXllFrameContext(
  DtsXllFrameContext *ctx,
  DcaAudioAssetDescParameters asset,
  DtshdFileHandler *dtshd
);

int parseDtsXllToPbrBuffer(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContext *ctx,
  DcaAudioAssetExSSXllParameters asset,
  uint32_t asset_size
);

LibbluBitReader *initUnpackDtsXllPbr(
  DtsXllFrameContext *ctx
);

int completeUnpackDtsXllPbr(
  DtsXllFrameContext *ctx
);

#if 0

int unpackBitsDtsXllPbr(
  DtsXllFrameContext *ctx,
  uint32_t *value,
  size_t length
);

int skipBitsDtsXllPbr(
  DtsXllFrameContext *ctx,
  size_t length
);

void alignByteDtsXllPbr(
  DtsXllFrameContext *ctx
);

int unpackByteDtsXllPbr(
  DtsXllFrameContext *ctx,
  uint8_t *byte
);

int unpackBytesDtsXllPbr(
  DtsXllFrameContext *ctx,
  uint32_t *value,
  size_t length
);

int skipBytesDtsXllPbr(
  DtsXllFrameContext *ctx,
  size_t length
);

uint32_t tellPosDtsXllPbr(
  DtsXllFrameContext *ctx
);

#endif

int registerDecodedFrameOffsetsDtsXllFrameContext(
  DtsXllFrameContext *ctx,
  const DcaXllFrameSFPosition framePosition
);

/** \~english
 * \brief Produce a PBR frame of requested size using slices of decoded frames.
 *
 * \param ctx DTS XLL handler context.
 * \param req_frame_size Requested builded PBR frame size in bytes.
 * \param builded_frame_pos_ret Generated PBR frame source file offsets.
 * \param sync_word_off_idx_ret If produced PBR frame contain the start of a
 * decoded frame (starting with a sync word), its entry index is set. If no
 * sync word is in produced PBR frame, the value is set to -1. This parameter
 * may be set to NULL to disable this functionnality.
 * \param init_dec_delay_ret If produced PBR frame contain the start
 * of a decoded frame (starting with a sync word), its delay in decoded
 * frames is returned using this pointer. Otherwise, value is untouched.
 * This parameter may be set to NULL to disable this functionnality.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * If requested size is greater than the amount of decoded data, an error is
 * returned.
 */
int substractPbrFrameSliceDtsXllFrameContext(
  DtsXllFrameContext *ctx,
  uint32_t req_frame_size,
  DcaXllFrameSFPosition *builded_frame_pos_ret,
  int *sync_word_off_idx_ret,
  unsigned *init_dec_delay_ret
);

void debugPrintDcaXllFrameSFPositionIndexes(
  DcaXllFrameSFPosition position,
  unsigned indent
);

void substractDtsXllFrameOriginalPosition(
  DtsXllFrameParameters *frame,
  DcaXllFrameSFPosition *pbrFramePosition
);

int collectFrameDataDcaXllFrameSFPosition(
  DcaXllFrameSFPosition *src,
  uint32_t *req_frame_size,
  DcaXllFrameSFPosition *dst
);

int getRelativeOffsetDcaXllFrameSFPosition(
  const DcaXllFrameSFPosition frame,
  int64_t abs_offset,
  uint32_t *rel_offset
);

/** \~english
 * \brief Fetch the target XLL frame size according to PBR smoothing process.
 *
 * \param ctx
 * \param frameIdx
 * \param size
 * \return int
 */
int getFrameTargetSizeDtsXllPbr(
  DtsXllFrameContext *ctx,
  unsigned audio_frame_idx,
  unsigned *size
);

int performComputationDtsXllPbr(
  DtsXllFrameContext *ctx
);

#endif