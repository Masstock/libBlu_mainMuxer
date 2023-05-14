#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_xll_util.h"

void cleanDtsPbrSmoothingStats(
  DtsPbrSmoothingStats stats
)
{
  free(stats.targetFrmSize);
}

int saveFrameSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats,
  size_t size
)
{
  unsigned newLength;
  size_t * newArray;

  assert(NULL != stats);
  assert(0 < stats->framesPerSec); /* FPS shall be initialized */

  if (stats->nbAllocatedFrames <= stats->nbUsedFrames) {
    newLength = GROW_ALLOCATION(stats->nbAllocatedFrames, (unsigned) 1 << 12);

    if (newLength <= stats->nbUsedFrames)
      LIBBLU_DTS_ERROR_RETURN(
        "Too many DTS XLL frames for PBR smoothing process.\n"
      );

    newArray = (size_t *) realloc(
      stats->targetFrmSize,
      newLength * sizeof(size_t)
    );
    if (NULL == newArray)
      LIBBLU_DTS_ERROR_RETURN(
        "Memory allocation error.\n"
      );

    stats->targetFrmSize = newArray;
    stats->nbAllocatedFrames = newLength;
  }

  stats->targetFrmSize[stats->nbUsedFrames++] = size;

  return 0;
}

int getFrameTargetSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats,
  unsigned frameIdx,
  size_t * size
)
{
  assert(NULL != stats);
  assert(NULL != size);

  if (stats->nbUsedFrames <= frameIdx)
    LIBBLU_DTS_XLL_ERROR_RETURN(
      "PBR smoothing failure, unknown frame index %u "
      "(%u frames registered).\n",
      frameIdx,
      stats->nbUsedFrames
    );

  *size = stats->targetFrmSize[frameIdx];

  return 0;
}

int performComputationDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats
)
{
  unsigned idx;

  /* float timestamp, timePerFrame; */
  size_t bufferedSize, maxSize, curSize, resSize;
  size_t bufferSize, maxBufferedSize;
  bool adjustmentRequired;

  bool fakePbr;

  /* timestamp = 0; */
  /* timePerFrame = 1.0 / stats->framesPerSec; */
  bufferedSize = maxBufferedSize = 0;
  adjustmentRequired = false;

  fakePbr = !isInitPbrFileHandler();

  LIBBLU_DTS_DEBUG_PBR(
    "Performing PBR smoothing computation on parsed frame sizes.\n"
  );
  LIBBLU_DTS_DEBUG_PBR(
    " -> Fake mode: %s.\n",
    BOOL_INFO(fakePbr)
  );

  if (!fakePbr) {
    bufferSize = stats->maxPbrBufferSize;

    LIBBLU_DTS_DEBUG_PBR(
      " -> Defined buffer size: %zu bytes (0: Disabled).\n",
      bufferSize
    );

    if (!stats->averageFrameSizeComputed) {
      size_t avgSize;

      if (getAvgSizePbrFileHandler(&avgSize) < 0)
        return -1;
      stats->averageFrameSize = avgSize;
      stats->averageFrameSizeComputed = true;
    }

    LIBBLU_DTS_DEBUG_PBR(
      " -> Average frame size: %zu bytes.\n",
      stats->averageFrameSize
    );
  }
  else
    bufferSize = 0;

  LIBBLU_DTS_DEBUG_PBR("Compute frames sizes:\n");
  for (idx = stats->nbUsedFrames; 0 < idx; idx--) {
    curSize = stats->targetFrmSize[idx-1] + bufferedSize;

    if (fakePbr) {
      maxSize = 3200;
    }
    else {
      /* BUG: PBR smoothing using dtspbr file values need more research. */
#if 0
      if (getMaxSizePbrFileHandler((unsigned) (timestamp * 1000), &maxSize) < 0)
        return -1;
#else
      maxSize = stats->averageFrameSize;
#endif
    }

    resSize = MIN(curSize, maxSize);
    if (bufferSize && bufferSize < curSize && resSize < curSize - bufferSize) {
      adjustmentRequired = true;
      resSize = curSize - bufferSize;
    }
    bufferedSize = curSize - resSize;

    LIBBLU_DTS_DEBUG_PBR(
      " Frame %u: %zu bytes -> %zu bytes (%zu bytes buffered).\n",
      idx-1, stats->targetFrmSize[idx-1], resSize, bufferedSize
    );

    stats->targetFrmSize[idx-1] = resSize;

    maxBufferedSize = MAX(maxBufferedSize, bufferedSize);

    /* timestamp += timePerFrame; */
  }

  if (0 < bufferedSize)
    LIBBLU_DTS_XLL_ERROR_RETURN(
      "PBR smoothing process error, unable to smooth enough data "
      "(unable to place %zu bytes).\n",
      bufferedSize
    );

  LIBBLU_DTS_DEBUG_PBR("Summary:\n");
  LIBBLU_DTS_DEBUG_PBR(" -> Max buffered size: %zu bytes.\n", maxBufferedSize);
  if (adjustmentRequired)
    LIBBLU_DTS_DEBUG_PBR(
      "  -> Adjustment done to avoid buffer overflow.\n"
    );

  return 0;
}

DtsXllFrameContextPtr createDtsXllFrameContext(
  void
)
{
  DtsXllFrameContextPtr ctx;

  ctx = (DtsXllFrameContextPtr) malloc(sizeof(DtsXllFrameContext));
  if (NULL == ctx)
    LIBBLU_DTS_ERROR_FRETURN("Memory allocation error.\n");

  ctx->pbrBuffer = NULL;
  ctx->pbrBufferSize = 0;
  ctx->pbrBufferActiveSize = 0;
  ctx->pbrBufferUsedSize = 0;
  ctx->maxPbrBufferUsedSize = 0;

  ctx->initializedPbrUnpack = false;

  resetCrcContext(&ctx->crcCtx);

  ctx->pendingFrames = createCircularBuffer(sizeof(DcaXllPbrFrame));
  ctx->nbParsedPbrFrames = 0;

  ctx->decodedFramesOff = createCircularBuffer(sizeof(DtsXllDecodedFrame));
  ctx->nbDecodedFrames = 0;
  ctx->nbSlicedFrames = 0;

  ctx->pbrSmoothing = INIT_DTS_PBR_SMOOTHING_STATS();

  if (NULL == ctx->pendingFrames)
    goto free_return;

  return ctx;

free_return:
  destroyDtsXllFrameContext(ctx);
  return NULL;
}

void destroyDtsXllFrameContext(DtsXllFrameContextPtr ctx)
{
  if (NULL == ctx)
    return;

  destroyCircularBuffer(ctx->pendingFrames);
  destroyCircularBuffer(ctx->decodedFramesOff);
  free(ctx->pbrBuffer);
  cleanDtsPbrSmoothingStats(ctx->pbrSmoothing);
  free(ctx);
}

int initDtsXllFrameContext(
  DtsXllFrameContextPtr * ctxPtr,
  DcaAudioAssetDescriptorParameters asset,
  DcaExtSSHeaderParameters extSSHdr
)
{
  DtsXllFrameContextPtr ctx;
  size_t pbrBufSize;

  DcaAudioAssetDescriptorDecoderNavDataParameters * decNavData;
  DcaAudioAssetSSXllParameters * xllParam;

  assert(NULL != ctxPtr);

  decNavData = &asset.decNavData;
  xllParam = &decNavData->codingComponents.extSSXll;

  /* Create XLL decoder context if inexistant */
  if (NULL == *ctxPtr) {
    if (NULL == (ctx = createDtsXllFrameContext()))
      return -1;
    *ctxPtr = ctx;
  }
  else
    ctx = *ctxPtr;

  /* Update max PBR buffer size settings. */
  if (decNavData->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    if (xllParam->syncWordPresent) {
      pbrBufSize = xllParam->peakBitRateSmoothingBufSize << 10; /* KiB -> B */

      if (DTS_XLL_MAX_PBR_BUFFER_SIZE < pbrBufSize)
        LIBBLU_DTS_ERROR_RETURN(
          "DTS-HDMA PBR buffer size exceed 240 kBytes (%zu kBytes).\n",
          xllParam->peakBitRateSmoothingBufSize
        );
      UPDATE_PBR_BUF_SIZE(ctx, pbrBufSize);
    }
  }

  if (extSSHdr.staticFieldsPresent) {
    SET_FPS_DTS_PBR_SMOOTHING_STATS(
      ctx->pbrSmoothing,
      1.0 / extSSHdr.staticFields.frameDuration
    );
  }

  return 0;
}

int updateDtsXllPbrBufferSize(
  DtsXllFrameContextPtr ctx,
  size_t newSize
)
{
  uint8_t * buf;

  if (ctx->pbrBufferSize < newSize) {
    /* Need realloc */
    if (NULL == (buf = (uint8_t *) realloc(ctx->pbrBuffer, newSize)))
      LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

    ctx->pbrBuffer = buf;
    ctx->pbrBufferSize = newSize;
  }

  return 0;
}

int parseDtsXllToPbrBuffer(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContextPtr ctx,
  DcaAudioAssetSSXllParameters asset,
  size_t assetLength
)
{
  size_t nbBufFrames, frameSize;
  int64_t assetOffset;
  uint8_t * pbrDst;

  DcaXllPbrFramePtr frame;

  assert(!ctx->initializedPbrUnpack);

  frameSize = asset.size;
  if (updateDtsXllPbrBufferSize(ctx, frameSize + ctx->pbrBufferUsedSize) < 0)
    return -1;

  /* lbc_printf("Frame size: %zu bytes, asset length: %zu bytes.\n", frameSize, assetLength); */

  pbrDst = ctx->pbrBuffer + ctx->pbrBufferUsedSize;
  assetOffset = tellPos(dtsInput);
  if (readBytes(dtsInput, pbrDst, frameSize) < 0)
    return -1;
  ctx->pbrBufferUsedSize += frameSize;

  if (0 < (nbBufFrames = getNbEntriesCircularBuffer(ctx->pendingFrames)))
    frame = getEntryCircularBuffer(ctx->pendingFrames, nbBufFrames - 1);
  else
    frame = NULL;

  if (asset.syncWordPresent) {
    if (frameSize <= asset.nbBytesOffXllSync)
      LIBBLU_DTS_ERROR_RETURN(
        "Out of asset DTS-HDMA sync word offset (%zu <= %zu bytes).\n",
        frameSize, asset.nbBytesOffXllSync
      );

    /* Collect if present data for last frame */
    if (NULL != frame) {
      frame->size += asset.nbBytesOffXllSync;

      ADD_DTS_XLL_FRAME_SF_POS(
        frame->pos,
        assetOffset,
        asset.nbBytesOffXllSync,
        LIBBLU_DTS_ERROR_RETURN(
          "Unable to add DTS-HDMA frame, source stream is too hashed.\n"
        )
      );

      LIBBLU_DEBUG_COM(
        " Previous frame %u data: %zu bytes.\n",
        frame->number,
        asset.nbBytesOffXllSync
      );
    }
    else if (0 < asset.nbBytesOffXllSync)
      LIBBLU_DTS_ERROR_RETURN(
        "Presence of garbage out-of-sync DTS-HDMA audio data "
        "of %zu bytes at start.\n",
        asset.nbBytesOffXllSync
      );

    /* Add frame to pending */
    frame = (DcaXllPbrFramePtr) newEntryCircularBuffer(ctx->pendingFrames);
    if (NULL == frame)
      return -1;

    frame->number = ctx->nbParsedPbrFrames++;
    frame->size = frameSize - asset.nbBytesOffXllSync;
    frame->pos = INIT_DTS_XLL_FRAME_SF_POS();
    frame->pbrDelay = asset.initialXllDecodingDelayInFrames;

    ADD_DTS_XLL_FRAME_SF_POS(
      frame->pos,
      assetOffset + asset.nbBytesOffXllSync,
      frame->size,
      LIBBLU_DTS_ERROR_RETURN(
        "Unable to add DTS-HDMA frame, source stream is too hashed.\n"
      )
    );

    LIBBLU_DEBUG_COM(
      " Current frame %u data: %zu bytes.\n",
      frame->number,
      frame->size
    );
  }
  else if (NULL != frame) {
    frame->size += frameSize;
    ADD_DTS_XLL_FRAME_SF_POS(
      frame->pos,
      assetOffset,
      frameSize,
      LIBBLU_DTS_ERROR_RETURN(
        "Unable to add DTS-HDMA frame, source stream is too hashed.\n"
      )
    );
  }

  if (frameSize < assetLength) {
#if 1
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected data in asset after the end of the XLL component.\n"
    );
#else
    /* Add extra data to a newer frame */
    frame = (DcaXllPbrFramePtr) newEntryCircularBuffer(ctx->pendingFrames);
    if (NULL == frame)
      return -1;

    frame->number = ctx->nbParsedPbrFrames++;
    frame->size = assetLength - frameSize;
    frame->pos = INIT_DTS_XLL_FRAME_SF_POS();
    frame->pbrDelay = 1;
    frame->pbrDelayInUse = false;

    ADD_DTS_XLL_FRAME_SF_POS(
      frame->pos,
      assetOffset + frameSize,
      frame->size,
      LIBBLU_DTS_ERROR_RETURN(
        "Unable to add DTS-HDMA frame, too many hashed source.\n"
      )
    );

    LIBBLU_DEBUG_COM(
      " Extra frame %u data: %zu bytes.\n",
      frame->number,
      frame->size
    );
#endif
  }

  return 0;
}

int initUnpackDtsXllPbr(
  DtsXllFrameContextPtr ctx
)
{
  assert(0 < ctx->pbrBufferUsedSize);

  ctx->pbrBufferParsedSize = 0;
  ctx->remainingBits = 0;
  ctx->initializedPbrUnpack = true;
  return 0;
}

int completeUnpackDtsXllPbr(
  DtsXllFrameContextPtr ctx
)
{
  assert(ctx->initializedPbrUnpack);

  /* Align to 32-bits boundary */
  if (ctx->pbrBufferParsedSize & 0x3) {
    if (align32BitsDtsXllPbr(ctx) < 0)
      return -1;
  }

  memmove(
    ctx->pbrBuffer,
    ctx->pbrBuffer + ctx->pbrBufferParsedSize,
    ctx->pbrBufferUsedSize - ctx->pbrBufferParsedSize
  );
  ctx->pbrBufferUsedSize -= ctx->pbrBufferParsedSize;
  ctx->initializedPbrUnpack = false;

  return 0;
}

int unpackBitsDtsXllPbr(
  DtsXllFrameContextPtr ctx,
  uint32_t * value,
  size_t length
)
{
  assert(ctx->initializedPbrUnpack);
  assert(length <= 32 * 8);

  if (NULL != value)
    *value = 0;

  while (0 < length) {
    if (!ctx->remainingBits) {
      if (ctx->pbrBufferUsedSize <= ctx->pbrBufferParsedSize)
        LIBBLU_DTS_ERROR_RETURN(
          "Unable to unpack DTS-HDMA frame, prematurate end of data "
          "(%zu bytes parsed).\n",
          ctx->pbrBufferUsedSize
        );

      ctx->byteBuf = ctx->pbrBuffer[ctx->pbrBufferParsedSize++];
      ctx->remainingBits = 8;

      /* Applying CRC calculation if in use : */
      if (IN_USE_DTS_XLL_CRC(ctx))
        applySingleByteCrcContext(&ctx->crcCtx, ctx->byteBuf);
    }

    if (NULL != value)
      *value |= ((ctx->byteBuf >> (--ctx->remainingBits)) & 0x1) << (--length);
    else {
      ctx->remainingBits--;
      length--;
    }
  }

  return 0;
}

int skipBitsDtsXllPbr(
  DtsXllFrameContextPtr ctx,
  size_t length
)
{
  return unpackBitsDtsXllPbr(ctx, NULL, length);
}

void alignByteDtsXllPbr(
  DtsXllFrameContextPtr ctx
)
{
  ctx->remainingBits = 0;
}

int align32BitsDtsXllPbr(
  DtsXllFrameContextPtr ctx
)
{
  size_t alignment;

  if (4 == (alignment = 4 - (ctx->pbrBufferParsedSize & 0x3)))
    return -1;
  return unpackBytesDtsXllPbr(ctx, NULL, alignment);
}

int unpackByteDtsXllPbr(
  DtsXllFrameContextPtr ctx,
  uint8_t * byte
)
{
  assert(ctx->initializedPbrUnpack);

  if (ctx->pbrBufferUsedSize <= ctx->pbrBufferParsedSize)
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to unpack DTS-HDMA frame, prematurate end of data "
      "(%zu bytes parsed).\n",
      ctx->pbrBufferUsedSize
    );

  if (ctx->remainingBits) {
    if (ctx->remainingBits == 8) {
      if (NULL != byte)
        *byte = ctx->byteBuf;
      ctx->remainingBits = 0;
      return 0;
    }

    LIBBLU_DTS_ERROR_RETURN(
      "Attempt to read from DTS-HDMA PBR buffer at byte level without "
      "binary alignmnent.\n"
    );
  }

  if (NULL != byte)
    *byte = ctx->pbrBuffer[ctx->pbrBufferParsedSize];

  /* Applying CRC calculation if in use : */
  if (IN_USE_DTS_XLL_CRC(ctx))
    applySingleByteCrcContext(&ctx->crcCtx, ctx->pbrBuffer[ctx->pbrBufferParsedSize]);

  ctx->pbrBufferParsedSize++;

  return 0;
}

int unpackBytesDtsXllPbr(
  DtsXllFrameContextPtr ctx,
  uint32_t * value,
  size_t length
)
{
  uint8_t byte;

  assert(length <= 4);

  if (NULL != value)
    *value = 0;

  while (0 < length) {
    if (unpackByteDtsXllPbr(ctx, &byte) < 0)
      return -1;
    if (NULL != value)
      *value |= byte << (8 * (--length));
    else
      length--;
  }

  return 0;
}

int skipBytesDtsXllPbr(
  DtsXllFrameContextPtr ctx,
  size_t length
)
{
  while (0 < length--) {
    if (unpackByteDtsXllPbr(ctx, NULL) < 0)
      LIBBLU_DTS_ERROR_RETURN(
        "Unable to parse the end of DTS XLL frame, missing %zu bytes.\n",
        length + 1
      );
  }

  return 0;
}

size_t tellPosDtsXllPbr(
  DtsXllFrameContextPtr ctx
)
{
  return ctx->pbrBufferParsedSize;
}

int registerDecodedFrameOffsetsDtsXllFrameContext(
  DtsXllFrameContextPtr ctx,
  const DcaXllFrameSFPosition framePosition
)
{
  DtsXllDecodedFrame * entry;

  /* Create a new entry */
  entry = (DtsXllDecodedFrame *) newEntryCircularBuffer(
    ctx->decodedFramesOff
  );
  if (NULL == entry)
    return -1;

  /* Save data on created entry */
  entry->position = framePosition;
  entry->number = ctx->nbDecodedFrames;
  entry->unpackingStarted = false;

  ctx->nbDecodedFrames++;
  return 0;
}

int substractPbrFrameSliceDtsXllFrameContext(
  DtsXllFrameContextPtr ctx,
  size_t requestedFrameSize,
  DcaXllFrameSFPosition * pbrFramePosition,
  int * syncPresenceIndex,
  unsigned * syncDecodingDelay
)
{
  int ret, index, syncPresEntryIndex;
  DtsXllDecodedFrame * decodedFrame;

  DcaXllFrameSFPosition pbrFrmPos;

  pbrFrmPos = INIT_DTS_XLL_FRAME_SF_POS();

  syncPresEntryIndex = -1;
  index = 0;
  while (0 < requestedFrameSize) {
    decodedFrame = (DtsXllDecodedFrame *) getEntryCircularBuffer(
      ctx->decodedFramesOff,
      0
    );
    if (NULL == decodedFrame)
      LIBBLU_DTS_XLL_ERROR_RETURN(
        "Requested frame size is greater than the amount of buffered data.\n"
      );

    if (!decodedFrame->unpackingStarted) {
      if (syncPresEntryIndex < 0) {
        syncPresEntryIndex = pbrFrmPos.nbSourceOffsets;
        if (NULL != syncDecodingDelay) {
          /* lbc_printf("%u %u %d\n", decodedFrame->number, ctx->nbSlicedFrames, index); */
          *syncDecodingDelay = decodedFrame->number - ctx->nbSlicedFrames;
        }
      }
    }

    ret = collectFrameDataDcaXllFrameSFPosition(
      &decodedFrame->position,
      &requestedFrameSize,
      &pbrFrmPos
    );
    if (ret < 0)
      return -1;

    if (IS_EMPTY_XLL_FRAME_SF_POS(decodedFrame->position)) {
      if (popCircularBuffer(ctx->decodedFramesOff, NULL) < 0)
        LIBBLU_DTS_XLL_ERROR_RETURN("Broken decoded frames offsets FIFO.\n");
    }
    else {
      assert(requestedFrameSize == 0);
      assert(0 < decodedFrame->position.sourceOffsets[0].len);
      decodedFrame->unpackingStarted = true;
    }
    index++;
  }

  if (NULL != pbrFramePosition)
    *pbrFramePosition = pbrFrmPos;
  if (NULL != syncPresenceIndex)
    *syncPresenceIndex = syncPresEntryIndex;

  ctx->nbSlicedFrames++;

  return 0;
}

void printDcaXllFrameSFPositionIndexes(
  DcaXllFrameSFPosition position,
  unsigned indent
)
{
  int i;

  for (i = 0; i < position.nbSourceOffsets; i++) {
    lbc_printf(
      "%-*c- 0x%08" PRIX64 ", length: %lu bytes.\n",
      indent, ' ',
      position.sourceOffsets[i].off,
      (unsigned long) position.sourceOffsets[i].len
    );
  }
}

void substractDtsXllFrameOriginalPosition(
  DtsXllFrameParameters * frame,
  DcaXllFrameSFPosition * pbrFramePosition
)
{
  int ret;

  DcaXllFrameSFPosition dstXllFramePosition;
  size_t remainingFrameSize;

  dstXllFramePosition = INIT_DTS_XLL_FRAME_SF_POS();
  remainingFrameSize = frame->comHdr.frameSize;

  ret = collectFrameDataDcaXllFrameSFPosition(
    pbrFramePosition,
    &remainingFrameSize,
    &dstXllFramePosition
  );
  assert(0 <= ret);
  assert(0 == remainingFrameSize);

  if (ret < 0)
    LIBBLU_ERROR_EXIT("Exit on error.\n");

  frame->originalPosition = dstXllFramePosition;

#if 0
  consumedIndexes = 0;

  while (0 < remainingFrameSize) {
    /* While remaining decoded frame data to collect from PBR frame */

    assert (consumedIndexes < pbrFramePosition->nbSourceOffsets);
    srcIdx = pbrFramePosition->sourceOffsets + consumedIndexes;

    assert(0 < srcIdx->len);
    indexConsumedLength = MIN(remainingFrameSize, srcIdx->len);

    ADD_DTS_XLL_FRAME_SF_POS(
      dstXllFramePosition,
      srcIdx->off,
      indexConsumedLength,
      assert(0);
    );

    srcIdx->len -= indexConsumedLength;
    remainingFrameSize -= indexConsumedLength;
    if (0 == srcIdx->len)
      consumedIndexes++;
  }

  frame->originalPosition = dstXllFramePosition;

  /* Update remaining data in PBR frame */
  if (consumedIndexes < pbrFramePosition->nbSourceOffsets) {
    memmove(
      pbrFramePosition->sourceOffsets,
      pbrFramePosition->sourceOffsets + consumedIndexes,
      pbrFramePosition->nbSourceOffsets - consumedIndexes
    );
  }
  pbrFramePosition->nbSourceOffsets -= consumedIndexes;
#endif
}

int collectFrameDataDcaXllFrameSFPosition(
  DcaXllFrameSFPosition * src,
  size_t * dataAmount,
  DcaXllFrameSFPosition * dst
)
{
  DcaXllFrameSFPosition genFrame;

  size_t indexConsumedLength, remainingFrameSize;
  int i, consumedIndexes;

#if !defined(NDEBUG)
  int64_t offset;
#endif

  DcaXllFrameSFPositionIndex * srcIdx;

  assert(NULL != src);
  assert(NULL != dataAmount);

  if (NULL == dst)
    genFrame = INIT_DTS_XLL_FRAME_SF_POS();
  else
    genFrame = *dst;

  remainingFrameSize = *dataAmount;
  consumedIndexes = 0;

  for (i = 0; i < src->nbSourceOffsets; i++) {
    /* While remaining decoded frame data to collect from PBR frame */

    if (!remainingFrameSize)
      break;

    srcIdx = src->sourceOffsets + i;

#if !defined(NDEBUG)
    if (0 < i)
      assert(offset < srcIdx->off);
    offset = srcIdx->off;
#endif

    assert(0 < srcIdx->len);
    indexConsumedLength = MIN(remainingFrameSize, srcIdx->len);

    ADD_DTS_XLL_FRAME_SF_POS(
      genFrame,
      srcIdx->off,
      indexConsumedLength,
      LIBBLU_DTS_XLL_ERROR_RETURN(
        "Too fragmented DTS PBR frame.\n"
      )
    );

    srcIdx->off += indexConsumedLength;
    srcIdx->len -= indexConsumedLength;
    remainingFrameSize -= indexConsumedLength;
    if (0 == srcIdx->len)
      consumedIndexes++;
    else
      assert(!remainingFrameSize);
  }

  /* Update remaining data in PBR frame */
  src->nbSourceOffsets -= consumedIndexes;
  for (i = 0; i < src->nbSourceOffsets; i++) {
    src->sourceOffsets[i] = src->sourceOffsets[consumedIndexes+i];
  }

  *dataAmount = remainingFrameSize;
  if (NULL != dst)
    *dst = genFrame;

  return 0;
}

int getRelativeOffsetDcaXllFrameSFPosition(
  const DcaXllFrameSFPosition frame,
  int64_t absoluteOffset,
  int64_t * relativeOffset
)
{
  int i;
  size_t offset;
  int64_t indexEndOffset;

  const DcaXllFrameSFPositionIndex * index;

  offset = 0;
  for (i = 0; i < frame.nbSourceOffsets; i++) {
    index = frame.sourceOffsets + i;
    indexEndOffset = index->off + index->len;

    if (index->off <= absoluteOffset && absoluteOffset < indexEndOffset) {
      if (NULL != relativeOffset)
        *relativeOffset = offset + (absoluteOffset - index->off);
      return 0;
    }

    offset += index->len;
  }

  return -1;
}

int getFrameTargetSizeDtsXllPbr(
  DtsXllFrameContextPtr ctx,
  unsigned frameIdx,
  size_t * size
)
{
  assert(NULL != ctx);

  return getFrameTargetSizeDtsPbrSmoothing(
    &ctx->pbrSmoothing,
    frameIdx,
    size
  );
}

int performComputationDtsXllPbr(
  DtsXllFrameContextPtr ctx
)
{
  return performComputationDtsPbrSmoothing(
    &ctx->pbrSmoothing
  );
}