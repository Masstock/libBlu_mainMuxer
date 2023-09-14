#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "dts_xll.h"

#define READ_BITS(d, br, s, e)                                                \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readLibbluBitReader(br, &_val, s) < 0)                                \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

#define READ_BITS_PO(d, br, s, e)                                             \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readLibbluBitReader(br, &_val, s) < 0)                                \
      e;                                                                      \
    *(d) = _val + 1;                                                          \
  } while (0)

static int _checkHeaderCrc(
  LibbluBitReader * br,
  const DtsXllCommonHeader * ch
)
{
  CrcContext crc_ctx;
  initCrcContext(&crc_ctx, DCA_EXT_SS_CRC_PARAM, DCA_EXT_SS_CRC_INITIAL_V);
  applyTableCrcContext(&crc_ctx, &br->buf[4], ch->nHeaderSize - 6);

  if (ch->nCRC16Header != bswap16(completeCrcContext(&crc_ctx)))
    LIBBLU_DTS_ERROR_RETURN("DTS XLL frame Common Header CRC check failure.\n");

  return 0;
}

static int _parseXllCommonHeader(
  LibbluBitReader * br,
  DtsXllCommonHeader * param
)
{

  /* [v32 SYNCXLL] */
  uint32_t SYNCXLL;
  READ_BITS(&SYNCXLL, br, 32, return -1);

  if (DCA_SYNCXLL != SYNCXLL)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DTS XLL Sync Word in Extension Substream Coding Component "
      "(0x%08" PRIX32 " parsed, expect 0x%08" PRIX32 ").\n",
      SYNCXLL, DCA_SYNCXLL
    );

  /* [u4 nVersion] */
  READ_BITS_PO(&param->nVersion, br, 4, return -1);

  if (DTS_XLL_MAX_VERSION < param->nVersion)
    LIBBLU_DTS_ERROR_RETURN(
      "Too high XLL stream syntax version identification %u.\n",
      param->nVersion
    );

  /* [u8 nHeaderSize] */
  READ_BITS_PO(&param->nHeaderSize, br, 8, return -1);

  /* Check header size */
  unsigned header_rem_bits = param->nHeaderSize << 3;
  if (header_rem_bits < DTS_XLL_MIN_HEADER_SIZE)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS XLL Common Header size, "
      "shorter than minimal required size (%u < %u bits).\n",
      param->nHeaderSize << 3,
      DTS_XLL_MIN_HEADER_SIZE
    );
  header_rem_bits -= DTS_XLL_MIN_HEADER_SIZE;

  /* [u5 nBits4FrameFsize] */
  READ_BITS_PO(&param->nBits4FrameFsize, br, 5, return -1);

  if (header_rem_bits < param->nBits4FrameFsize)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS XLL Common Header size, "
      "shorter than minimal required size (%u < %u bits).\n",
      param->nHeaderSize << 3,
      DTS_XLL_MIN_HEADER_SIZE + param->nBits4FrameFsize
    );
  header_rem_bits -= param->nBits4FrameFsize;

  /* [u<nBits4FrameFsize> nLLFrameSize] */
  READ_BITS_PO(&param->nLLFrameSize, br, param->nBits4FrameFsize, return -1);

  /* [u4 nNumChSetsInFrame] */
  READ_BITS_PO(&param->nNumChSetsInFrame, br, 4, return -1);

  /* [u4 nSegmentsInFrame] */
  READ_BITS(&param->nSegmentsInFrame_code, br, 4, return -1);
  param->nSegmentsInFrame = 1u << param->nSegmentsInFrame_code;

  /* [u4 nSmplInSeg] */
  READ_BITS(&param->nSmplInSeg_code, br, 4, return -1);
  param->nSmplInSeg = 1u << param->nSmplInSeg_code;

  /* [u5 nBits4SSize] */
  READ_BITS_PO(&param->nBits4SSize, br, 5, return -1);

  /* [u2 nBandDataCRCEn] */
  READ_BITS(&param->nBandDataCRCEn, br, 2, return -1);

  /* [b1 bScalableLSBs] */
  READ_BITS(&param->bScalableLSBs, br, 1, return -1);

  /* [u5 nBits4ChMask] */
  READ_BITS_PO(&param->nBits4ChMask, br, 5, return -1);

  if (param->bScalableLSBs) {
    if (header_rem_bits < 4)
      LIBBLU_DTS_ERROR_RETURN(
        "Invalid DTS XLL Common Header size, "
        "shorter than minimal required size (%u < %u bits).\n",
        param->nHeaderSize << 3,
        DTS_XLL_MIN_HEADER_SIZE + param->nBits4FrameFsize + 4
      );
    header_rem_bits -= 4;

    /* [u4 nuFixedLSBWidth] */
    READ_BITS(&param->nuFixedLSBWidth, br, 4, return -1);
  }

  assert(header_rem_bits <= remainingBitsLibbluBitReader(br));

  /* [vn Reserved/ByteAlign] */
  param->Reserved_size = header_rem_bits >> 3;
  skipLibbluBitReader(br, header_rem_bits);

  /* [u16 nCRC16Header] */
  READ_BITS(&param->nCRC16Header, br, 16, return -1);

  /* Check header CRC */
  if (_checkHeaderCrc(br, param) < 0)
    return -1;

  return 0;
}

static int _parseXllChannelSetSubHeader(
  LibbluBitReader * br,
  DtsXllChannelSetSubHeader * param
)
{

  /* [u10 nChSetHeaderSize] */
  READ_BITS_PO(&param->nChSetHeaderSize, br, 10, return -1);

  /* [u4 nChSetLLChannel] */
  READ_BITS_PO(&param->nChSetLLChannel, br, 4, return -1);

  if (DTS_XLL_MAX_CH_NB < param->nChSetLLChannel)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many channels defined in XLL Frame ChSet Header (%u < %u).\n",
      DTS_XLL_MAX_CH_NB,
      param->nChSetLLChannel
    );

  /* [u<nChSetLLChannel> nResidualChEncode] */
  READ_BITS(&param->nResidualChEncode, br, param->nChSetLLChannel, return -1);

  /* TODO */

  if (padByteLibbluBitReader(br) < 0)
    return -1;

  return 0;
}

int decodeDtsXllFrame(
  DtsXllFrameContext * ctx,
  DcaXllFrameSFPosition * pbrFramePosition
)
{
  int ret;

  DtsXllFrameParameters frame;
  unsigned chSet;

  LibbluBitReader * br = initUnpackDtsXllPbr(ctx);

  LIBBLU_DTS_DEBUG_XLL(
    " === DTS Lossless Extension (XLL) Frame %u ===\n",
    ctx->nbDecodedFrames
  );

  LIBBLU_DTS_DEBUG_XLL("  XLL Header:\n");
  LIBBLU_DTS_DEBUG_XLL("   Common Header:\n");

  if (_parseXllCommonHeader(br, &frame.comHdr) < 0)
    return -1;

  if (checkDtsXllCommonHeader(frame.comHdr) < 0)
    return -1;

  for (chSet = 0; chSet < frame.comHdr.nNumChSetsInFrame; chSet++) {
    LIBBLU_DTS_DEBUG_XLL("   Channel Set Sub-Header %u:\n", chSet);

    if (_parseXllChannelSetSubHeader(br, &frame.subHdr[chSet]) < 0)
      return -1;

    if (checkDtsXllChannelSetSubHeader(frame.subHdr[chSet]) < 0)
      return -1;

    break; /* TODO */
  }

#if 0
  ret = skipBytesDtsXllPbr(
    ctx,
    frame.comHdr.nLLFrameSize - (offsetLibbluBitReader(br) >> 3)
  );
  if (ret < 0)
    return -1;
#else
  skipLibbluBitReader(br, (frame.comHdr.nLLFrameSize << 3) - offsetLibbluBitReader(br));
#endif

  /* Manage PBR buffer content and register decoded frame offset : */
  substractDtsXllFrameOriginalPosition(&frame, pbrFramePosition);

  ret = registerDecodedFrameOffsetsDtsXllFrameContext(
    ctx,
    frame.originalPosition
  );
  if (ret < 0)
    return -1;

  LIBBLU_DTS_DEBUG_XLL("  Source file position(s):\n");
  if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_XLL))
    debugPrintDcaXllFrameSFPositionIndexes(frame.originalPosition, 3);

  return completeUnpackDtsXllPbr(ctx);
}

int parseDtsXllFrame(
  BitstreamReaderPtr bs,
  DtsXllFrameContext * ctx,
  uint32_t asset_length,
  const DcaAudioAssetDescDecNDParameters * asset_decnav
)
{
  DcaAudioAssetExSSXllParameters asset_xll = asset_decnav->coding_components.ExSSXLL;

  /* Reduce decoding delay of PBR pending frames */
  size_t bufferedFramesNb = nbEntriesCircularBuffer(&ctx->pendingFrames);
  for (size_t i = 0; i < bufferedFramesNb; i++) {
    DcaXllPbrFramePtr frame = getCircularBuffer(&ctx->pendingFrames, i);
    assert(NULL != frame);

    if (0 < frame->pbrDelay)
      frame->pbrDelay--;
  }

  /* Copy asset payload to PBR buffer */
  if (parseDtsXllToPbrBuffer(bs, ctx, asset_xll, asset_length) < 0)
    return -1;

  /* Decode PBR buffer ready frame */
  if (!nbEntriesCircularBuffer(&ctx->pendingFrames))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS PBR buffering underflow, no frame to decode.\n"
    );

  DcaXllPbrFramePtr frame = getCircularBuffer(&ctx->pendingFrames, 0);
  assert(NULL != frame);

  if (0 < frame->pbrDelay)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS PBR buffering underflow, unexpected delay.\n"
    );

  /* Frame waiting time elapsed, decode frame */
  LIBBLU_DTS_DEBUG_XLL(
    " Decoding XLL frame from PBR frame %u (of at most %u bytes), "
    "stored at:\n",
    frame->number,
    frame->size
  );
  if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_XLL))
    debugPrintDcaXllFrameSFPositionIndexes(frame->pos, 2);

  if (decodeDtsXllFrame(ctx, &frame->pos) < 0)
    return -1;
  unsigned decoded_data_size = offsetLibbluBitReader(&ctx->bit_reader) >> 3;

  if (saveFrameSizeDtsPbrSmoothing(&ctx->pbrSmoothing, frame->size) < 0)
    return -1;

  if (frame->size <= decoded_data_size) {
    assert(frame->size == decoded_data_size);

    if (popCircularBuffer(&ctx->pendingFrames, NULL) < 0)
      LIBBLU_DTS_ERROR_RETURN("Broken DTS-HDMA pending frames FIFO.\n");
  }
  else {
    /* Next nasted DTS XLL frame */
    frame->size -= decoded_data_size;
    frame->pbrDelay = 0;
    frame->number = ctx->nbParsedPbrFrames++;
  }

  if (ctx->pbr_buf_size < ctx->pbr_buf_used_size)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HDMA PBR buffer overflow (%zu < %zu bytes).\n",
      ctx->pbr_buf_size,
      ctx->pbr_buf_used_size
    );
  /* Update max level: */
  ctx->pbr_buf_peak_size = MAX(
    ctx->pbr_buf_peak_size,
    ctx->pbr_buf_used_size
  );

  return 0;
}