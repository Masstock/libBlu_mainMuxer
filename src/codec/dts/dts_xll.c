#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "dts_xll.h"

int decodeDtsXllCommonHeader(
  DtsXllFrameContextPtr ctx,
  DtsXllCommonHeader * param
)
{
  size_t startOffset;
  uint32_t value;

  uint16_t headerCrcResult;

  startOffset = tellPosDtsXllPbr(ctx);

  /* [v32 SYNCXLL] */
  if (unpackBytesDtsXllPbr(ctx, &value, 4) < 0)
    return -1;

  if (value != DTS_SYNCWORD_XLL)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DTS XLL Sync Word in Extension Substream Coding Component "
      "(0x%08" PRIX32 " parsed, expect 0x%08" PRIX32 ").\n",
      value, DTS_SYNCWORD_XLL
    );

  if (initCrc(DTS_XLL_CRC_CTX(ctx), DCA_EXT_SS_CRC_PARAM(), DCA_EXT_SS_CRC_INITIAL_V) < 0)
    return -1;

  /* [u4 nVersion] */
  if (unpackBitsDtsXllPbr(ctx, &value, 4) < 0)
    return -1;
  param->version = value + 1;

  if (DTS_XLL_MAX_VERSION < value)
    LIBBLU_DTS_ERROR_RETURN(
      "Unsupported DTS XLL stream syntax version number %u in Extension "
      "Substream Coding Component.\n",
      param->version
    );

  /* [u8 nHeaderSize] */
  if (unpackBitsDtsXllPbr(ctx, &value, 8) < 0)
    return -1;
  param->headerSize = value + 1;

  /* [u5 nBits4FrameFsize] */
  if (unpackBitsDtsXllPbr(ctx, &value, 5) < 0)
    return -1;
  param->frameSizeFieldLength = value + 1;

  /* [u<nBits4FrameFsize> nLLFrameSize] */
  if (unpackBitsDtsXllPbr(ctx, &value, param->frameSizeFieldLength) < 0)
    return -1;
  param->frameSize = value + 1;

  /* [u4 nNumChSetsInFrame] */
  if (unpackBitsDtsXllPbr(ctx, &value, 4) < 0)
    return -1;
  param->nbChSetsPerFrame = value + 1;

  /* [u4 nSegmentsInFrame] */
  if (unpackBitsDtsXllPbr(ctx, &value, 4) < 0)
    return -1;
  param->nbSegmentsInFrameCode = value;
  param->nbSegmentsInFrame = 1 << value;

  /* [u4 nSmplInSeg] */
  if (unpackBitsDtsXllPbr(ctx, &value, 4) < 0)
    return -1;
  param->nbSamplesPerSegmentCode = value;
  param->nbSamplesPerSegment = 1 << value;

  /* [u5 nBits4SSize] */
  if (unpackBitsDtsXllPbr(ctx, &value, 5) < 0)
    return -1;
  param->nbBitsSegmentSizeField = value + 1;

  /* [u2 nBandDataCRCEn] */
  if (unpackBitsDtsXllPbr(ctx, &value, 2) < 0)
    return -1;
  param->crc16Pres = value;

  /* [b1 bScalableLSBs] */
  if (unpackBitsDtsXllPbr(ctx, &value, 1) < 0)
    return -1;
  param->scalableLSBs = value;

  /* [u5 nBits4ChMask] */
  if (unpackBitsDtsXllPbr(ctx, &value, 5) < 0)
    return -1;
  param->nbBitsChMaskField = value + 1;

  if (param->scalableLSBs) {
    /* [u4 nuFixedLSBWidth] */
    if (unpackBitsDtsXllPbr(ctx, &value, 4) < 0)
      return -1;
    param->fixedLsbWidth = value;
  }

  /* [vn Reserved] */
  if (param->headerSize - 2 < tellPosDtsXllPbr(ctx) - startOffset)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DTS XLL frame Common Header size in Extension Substream.\n"
    );
  param->reservedFieldSize =
    param->headerSize - (tellPosDtsXllPbr(ctx) - startOffset) - 2
  ;

  if (skipBytesDtsXllPbr(ctx, param->reservedFieldSize) < 0)
    return -1;

  /* [vn ByteAlign] */
  param->paddingBits = ctx->remainingBits;
  alignByteDtsXllPbr(ctx);

  if (endCrc(DTS_XLL_CRC_CTX(ctx), &value) < 0)
    return -1;
  headerCrcResult = value & 0xFFFF;

  /* [u16 nCRC16Header] */
  if (unpackBitsDtsXllPbr(ctx, &value, 16) < 0)
    return -1;
  param->headerCrc = value;

  if (param->headerCrc != headerCrcResult)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS XLL frame Common Header CRC check failure.\n"
    );

  return 0;
}

int decodeDtsXllChannelSetSubHeader(
  DtsXllFrameContextPtr ctx,
  DtsXllChannelSetSubHeader * param
)
{
  uint32_t value;

  /* [u10 nChSetHeaderSize] */
  if (unpackBitsDtsXllPbr(ctx, &value, 10) < 0)
    return -1;
  param->chSetHeaderSize = value + 1;

  /* [u4 nChSetLLChannel] */
  if (unpackBitsDtsXllPbr(ctx, &value, 4) < 0)
    return -1;
  param->nbChannels = value + 1;

  if (DTS_XLL_MAX_CH_NB < param->nbChannels)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many channels defined in XLL Frame ChSet Header (%u < %u).\n",
      DTS_XLL_MAX_CH_NB,
      param->nbChannels
    );

  /* [u<nChSetLLChannel> nResidualChEncode] */
  if (unpackBitsDtsXllPbr(ctx, &value, param->nbChannels) < 0)
    return -1;
  param->residualChType = value;

  /* TODO */

  alignByteDtsXllPbr(ctx);
  return 0;
}

int decodeDtsXllFrame(
  DtsXllFrameContextPtr ctx,
  DcaXllFrameSFPosition * pbrFramePosition
)
{
  int ret;

  DtsXllFrameParameters frame;
  unsigned chSet;

  if (initUnpackDtsXllPbr(ctx) < 0)
    return -1;

  LIBBLU_DTS_DEBUG_XLL(
    " === DTS Lossless Extension (XLL) Frame %u ===\n",
    ctx->nbDecodedFrames
  );

  LIBBLU_DTS_DEBUG_XLL("  XLL Header:\n");
  LIBBLU_DTS_DEBUG_XLL("   Common Header:\n");

  if (decodeDtsXllCommonHeader(ctx, &frame.comHdr) < 0)
    return -1;

  if (checkDtsXllCommonHeader(frame.comHdr) < 0)
    return -1;

  for (chSet = 0; chSet < frame.comHdr.nbChSetsPerFrame; chSet++) {
    LIBBLU_DTS_DEBUG_XLL("   Channel Set Sub-Header %u:\n", chSet);

    if (decodeDtsXllChannelSetSubHeader(ctx, frame.subHdr + chSet) < 0)
      return -1;

    if (checkDtsXllChannelSetSubHeader(frame.subHdr[chSet]) < 0)
      return -1;

    break; /* TODO */
  }

  ret = skipBytesDtsXllPbr(
    ctx,
    frame.comHdr.frameSize - ctx->pbrBufferParsedSize
  );
  if (ret < 0)
    return -1;

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
    printDcaXllFrameSFPositionIndexes(frame.originalPosition, 3);

  return completeUnpackDtsXllPbr(ctx);
}

int parseDtsXllFrame(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContextPtr ctx,
  DcaAudioAssetDescriptorParameters asset,
  size_t assetLength
)
{
  size_t i, bufferedFramesNb, decodedDataLength;

  DcaXllPbrFramePtr frame;

  DcaAudioAssetSSXllParameters xllAsset =
    asset.decNavData.codingComponents.extSSXll
  ;

  /* Reduce decoding delay of PBR pending frames */
  bufferedFramesNb = getNbEntriesCircularBuffer(ctx->pendingFrames);
  for (i = 0; i < bufferedFramesNb; i++) {
    frame = (DcaXllPbrFramePtr) getEntryCircularBuffer(ctx->pendingFrames, i);
    assert(NULL != frame);

    if (0 < frame->pbrDelay)
      frame->pbrDelay--;
  }

  /* Copy asset payload to PBR buffer */
  if (parseDtsXllToPbrBuffer(dtsInput, ctx, xllAsset, assetLength) < 0)
    return -1;

  /* Decode PBR buffer ready frame */
  if (!getNbEntriesCircularBuffer(ctx->pendingFrames))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS PBR buffering underflow, no frame to decode.\n"
    );

  frame = (DcaXllPbrFramePtr) getEntryCircularBuffer(ctx->pendingFrames, 0);
  assert(NULL != frame);
  if (0 < frame->pbrDelay)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS PBR buffering underflow, unexpected delay.\n"
    );

  /* Frame waiting time elapsed, decode frame */
  LIBBLU_DTS_DEBUG_XLL(
    " Decoding XLL frame from PBR frame %u (of at most %zu bytes), "
    "stored at:\n",
    frame->number,
    frame->size
  );
  if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_XLL))
    printDcaXllFrameSFPositionIndexes(frame->pos, 2);

  if (decodeDtsXllFrame(ctx, &frame->pos) < 0)
    return -1;
  decodedDataLength = ctx->pbrBufferParsedSize;

  if (saveFrameSizeDtsPbrSmoothing(&ctx->pbrSmoothing, frame->size) < 0)
    return -1;

  if (frame->size <= decodedDataLength) {
    assert(frame->size == decodedDataLength);

    if (popCircularBuffer(ctx->pendingFrames, NULL) < 0)
      LIBBLU_DTS_ERROR_RETURN("Broken DTS-HDMA pending frames FIFO.\n");
  }
  else {
    /* Next nasted DTS XLL frame */
    frame->size -= decodedDataLength;
    frame->pbrDelay = 0;
    frame->number = ctx->nbParsedPbrFrames++;
  }

  if (ctx->pbrBufferActiveSize < ctx->pbrBufferUsedSize)
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HDMA PBR buffer overflow (%zu < %zu bytes).\n",
      ctx->pbrBufferActiveSize,
      ctx->pbrBufferUsedSize
    );
  /* Update max level: */
  ctx->maxPbrBufferUsedSize = MAX(
    ctx->maxPbrBufferUsedSize,
    ctx->pbrBufferUsedSize
  );

  return 0;
}