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

/** \~english
 * \brief PBR smoothing process statistics.
 *
 * Used during PBR smoothing process.
 */
typedef struct {
  uint32_t * targetFrmSize; /* TODO: Add frames offsets #DcaXllFrameSFPosition */
  unsigned nbAllocatedFrames;
  unsigned nbUsedFrames;

  float framesPerSec;

  uint32_t avg_frame_size;
  bool avg_frame_size_set;

  uint32_t max_pbrs_buf_size;
} DtsPbrSmoothingStats;

int saveFrameSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats,
  unsigned size
);

typedef struct {
  DcaXllFrameSFPosition position;
  unsigned number;
  bool unpackingStarted;
} DtsXllDecodedFrame;

/** \~english
 * \brief DTS XLL extension substream parsing handle.
 *
 * A difference is made between PBR frames and current XLL frames. A PBR frame
 * is referenced by Extension Substream XLL parameters in Audio Asset, with
 * its start offset at start of Encoded Audio Asset Data if XLL sync word
 * is set as present (or otherwise 'nuExSSXLLSyncOffset' if non zero)
 * and end at start of the next PBR frame. Meaning a PBR frame may contain
 * one or more XLL frames (the first one identified by audio asset header,
 * other nasted at the end of the first one, unsignaled by audio asset header).
 *
 * The following diagram shows possible Extension Substream payload (headers
 * removed) containing shared PBR frame :
 *
 * [ExtSS frame #0 Audio Asset Data 0 <PBR frame #0... ]
 * [ExtSS frame #1 Audio Asset Data 0 ...End of PBR frame #0> <PBR frame #1... ]
 * ...
 *
 * With start offset (from container Encoded Asset Data start offset) of
 * "PBR frame #1" indicated by 'nuExSSXLLSyncOffset' of linked Audio Asset
 * header.
 *
 * And the first PBR frame of previous diagram may be composed as shown in
 * following diagram:
 *
 * [PBR frame 0 <XLL frame #0> <XLL frame #1>]
 *
 * With "XLL frame #1" start offset at "XLL frame #0" end plus 32-bit bistream
 * boundary alignment.
 *
 * This difference is used since at high level, without parsing XLL frame
 * content from PBR buffer, its impossible to identify presence of multiple
 * XLL frames in one Audio Asset Data.
 *
 *
 * PBR buffer usage is only compared to Audio Asset header specified size at
 * end of ExtSS parsing. Since according to DTS specs, it may be set to zero
 * if all frames from bitstreams are instantly decoded. In this manner, we
 * only check if decoding delayed XLL frames cause overflow.
 */
typedef struct {
  uint8_t * pbrBuffer;         /**< PBR buffer used to re-assemble XLL
    frames prior to decoding.                                                */
  size_t pbrBufferSize;        /**< PBR buffer allocated size in bytes.      */
  size_t pbrBufferActiveSize;  /**< PBR buffer active max size according to
    audio asset parameters. If this value is exceed, an error is returned.   */
  size_t pbrBufferUsedSize;    /**< PBR buffer current usage level in bytes. */
  size_t maxPbrBufferUsedSize; /**< Maximal PBR buffer usage level at end
    of ExtSS parsing in bytes.                                               */

  /* PBR buffer unpacking related */
  bool initializedPbrUnpack;   /**< Is PBR buffer data unpacking is active.  */
  size_t pbrBufferParsedSize;

  uint8_t byteBuf;
  unsigned remainingBits;

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
} DtsXllFrameContext;

static inline void cleanDtsXllFrameContext(
  DtsXllFrameContext ctx
)
{
  free(ctx.pbrBuffer);
  cleanCircularBuffer(ctx.pendingFrames);
  cleanCircularBuffer(ctx.decodedFramesOff);
  free(ctx.pbrSmoothing.targetFrmSize);
}

int initDtsXllFrameContext(
  DtsXllFrameContext * ctx,
  DcaAudioAssetDescParameters asset,
  const DcaExtSSHeaderParameters * ext_ss_hdr,
  DtshdFileHandler * dtshd
);

int parseDtsXllToPbrBuffer(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContext * ctx,
  DcaAudioAssetExSSXllParameters asset,
  size_t assetLength
);

int initUnpackDtsXllPbr(
  DtsXllFrameContext * ctx
);

int completeUnpackDtsXllPbr(
  DtsXllFrameContext * ctx
);

int unpackBitsDtsXllPbr(
  DtsXllFrameContext * ctx,
  uint32_t * value,
  size_t length
);

int skipBitsDtsXllPbr(
  DtsXllFrameContext * ctx,
  size_t length
);

void alignByteDtsXllPbr(
  DtsXllFrameContext * ctx
);

int unpackByteDtsXllPbr(
  DtsXllFrameContext * ctx,
  uint8_t * byte
);

int unpackBytesDtsXllPbr(
  DtsXllFrameContext * ctx,
  uint32_t * value,
  size_t length
);

int skipBytesDtsXllPbr(
  DtsXllFrameContext * ctx,
  size_t length
);

size_t tellPosDtsXllPbr(
  DtsXllFrameContext * ctx
);

int registerDecodedFrameOffsetsDtsXllFrameContext(
  DtsXllFrameContext * ctx,
  const DcaXllFrameSFPosition framePosition
);

/** \~english
 * \brief Produce a PBR frame of requested size using slices of decoded frames.
 *
 * \param ctx DTS XLL handler context.
 * \param requestedFrameSize Requested builded PBR frame size in bytes.
 * \param pbrFramePosition Generated PBR frame source file offsets.
 * \param syncPresenceIndex If produced PBR frame contain the start of a
 * decoded frame (starting with a sync word), its entry index is set. If no
 * sync word is in produced PBR frame, the value is set to -1. This parameter
 * may be set to NULL to disable this functionnality.
 * \param syncDecodingDelay If produced PBR frame contain the start
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
  DtsXllFrameContext * ctx,
  size_t requestedFrameSize,
  DcaXllFrameSFPosition * pbrFramePosition,
  int * syncPresenceIndex,
  unsigned * syncDecodingDelay
);

void printDcaXllFrameSFPositionIndexes(
  DcaXllFrameSFPosition position,
  unsigned indent
);

void substractDtsXllFrameOriginalPosition(
  DtsXllFrameParameters * frame,
  DcaXllFrameSFPosition * pbrFramePosition
);

int collectFrameDataDcaXllFrameSFPosition(
  DcaXllFrameSFPosition * src,
  size_t * dataAmount,
  DcaXllFrameSFPosition * dst
);

int getRelativeOffsetDcaXllFrameSFPosition(
  const DcaXllFrameSFPosition frame,
  uint64_t abs_offset,
  uint64_t * rel_offset
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
  DtsXllFrameContext * ctx,
  unsigned frameIdx,
  unsigned * size
);

int performComputationDtsXllPbr(
  DtsXllFrameContext * ctx
);

#endif