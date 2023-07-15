#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_xll_util.h"

int saveFrameSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats,
  size_t size
)
{
  assert(NULL != stats);
  assert(0 < stats->framesPerSec); /* FPS shall be initialized */

  if (stats->nbAllocatedFrames <= stats->nbUsedFrames) {
    unsigned new_len = GROW_ALLOCATION(stats->nbAllocatedFrames, 4096);

    if (lb_mul_overflow(new_len, sizeof(size_t)))
      LIBBLU_DTS_ERROR_RETURN(
        "Too many DTS XLL frames for PBR smoothing process.\n"
      );

    size_t * new_array = (size_t *) realloc(
      stats->targetFrmSize,
      new_len * sizeof(size_t)
    );
    if (NULL == new_array)
      LIBBLU_DTS_ERROR_RETURN(
        "Memory allocation error.\n"
      );

    stats->targetFrmSize = new_array;
    stats->nbAllocatedFrames = new_len;
  }

  stats->targetFrmSize[stats->nbUsedFrames++] = size;

  return 0;
}

int getFrameTargetSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats,
  unsigned frame_idx,
  size_t * size
)
{
  assert(NULL != stats);
  assert(NULL != size);

  if (stats->nbUsedFrames <= frame_idx)
    LIBBLU_DTS_XLL_ERROR_RETURN(
      "PBR smoothing failure, unknown frame index %u "
      "(%u frames registered).\n",
      frame_idx,
      stats->nbUsedFrames
    );

  *size = stats->targetFrmSize[frame_idx];

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

    if (0 < bufferedSize)
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

#if 0

DtsXllFrameContext * createDtsXllFrameContext(
  void
)
{
  DtsXllFrameContext * ctx;

  ctx = (DtsXllFrameContext *) malloc(sizeof(DtsXllFrameContext));
  if (NULL == ctx)
    LIBBLU_DTS_ERROR_FRETURN("Memory allocation error.\n");

  ctx->pbrBuffer = NULL;
  ctx->pbrBufferSize = 0;
  ctx->pbrBufferActiveSize = 0;
  ctx->pbrBufferUsedSize = 0;
  ctx->maxPbrBufferUsedSize = 0;

  ctx->initializedPbrUnpack = false;

  resetCrcContext(&ctx->crcCtx);

  ctx->pendingFrames = createCircularBuffer();
  ctx->nbParsedPbrFrames = 0;

  ctx->decodedFramesOff = createCircularBuffer();
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

void destroyDtsXllFrameContext(DtsXllFrameContext * ctx)
{
  if (NULL == ctx)
    return;

  destroyCircularBuffer(ctx->pendingFrames);
  destroyCircularBuffer(ctx->decodedFramesOff);
  free(ctx->pbrBuffer);
  _cleanDtsPbrSmoothingStats(ctx->pbrSmoothing);
  free(ctx);
}

#endif

int initDtsXllFrameContext(
  DtsXllFrameContext * ctx,
  DcaAudioAssetDescParameters asset,
  DcaExtSSHeaderParameters extSSHdr,
  DtshdFileHandler * dtshd
)
{
  assert(NULL != ctx);

  DcaAudioAssetDescDecNDParameters * dec_nav_data = &asset.decNavData;
  DcaAudioAssetSSXllParameters * xll_param = &dec_nav_data->codingComponents.extSSXll;

  /* Update decoder PBRS buffer size according to header : */
  if (dec_nav_data->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    if (xll_param->syncWordPresent) {
      size_t buf_size = xll_param->peakBitRateSmoothingBufSize << 10; // KiB

      if (DTS_XLL_MAX_PBR_BUFFER_SIZE < buf_size)
        LIBBLU_DTS_ERROR_RETURN(
          "DTS-HDMA PBR buffer size exceed 240 kBytes (%zu kBytes).\n",
          xll_param->peakBitRateSmoothingBufSize
        );

      ctx->pbrBufferActiveSize = buf_size;
    }
  }

  if (canBePerformedPBRSDtshdFileHandler(dtshd)) {
    size_t pbrs_buf_size = getPBRSmoothingBufSizeKiBDtshdFileHandler(dtshd) << 10;

    if (0 == pbrs_buf_size || DTS_XLL_MAX_PBR_BUFFER_SIZE < pbrs_buf_size)
      LIBBLU_DTS_ERROR_RETURN(
        "Invalid XLL PBRS buffer size in DTS-HD file header (%zu bytes).\n",
        pbrs_buf_size
      );

    ctx->pbrSmoothing.maxPbrBufferSize = pbrs_buf_size;
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
  DtsXllFrameContext * ctx,
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
  DtsXllFrameContext * ctx,
  DcaAudioAssetSSXllParameters asset,
  size_t assetLength
)
{
  assert(!ctx->initializedPbrUnpack);

  if (updateDtsXllPbrBufferSize(ctx, asset.size + ctx->pbrBufferUsedSize) < 0)
    return -1;

  /* lbc_printf("Frame size: %zu bytes, asset length: %zu bytes.\n", asset.size, assetLength); */

  int64_t asset_off;
  if ((asset_off = tellPos(dtsInput)) < 0)
    return -1;
  uint8_t * buf_ptr = &ctx->pbrBuffer[ctx->pbrBufferUsedSize];

  if (readBytes(dtsInput, buf_ptr, asset.size) < 0)
    return -1;
  ctx->pbrBufferUsedSize += asset.size;

  size_t nbBufFrames = getNbEntriesCircularBuffer(&ctx->pendingFrames);

  DcaXllPbrFramePtr prev_frame = NULL;
  if (0 < nbBufFrames)
    prev_frame = getEntryCircularBuffer(&ctx->pendingFrames, nbBufFrames - 1);

  if (asset.syncWordPresent) {
    if (asset.size <= asset.nbBytesOffXllSync)
      LIBBLU_DTS_ERROR_RETURN(
        "Out of asset DTS-HDMA sync word offset (%zu <= %zu bytes).\n",
        asset.size, asset.nbBytesOffXllSync
      );

    /* Collect if present data for last frame */
    if (NULL != prev_frame) {
      prev_frame->size += asset.nbBytesOffXllSync;

      ADD_DTS_XLL_FRAME_SF_POS(
        prev_frame->pos,
        asset_off,
        asset.nbBytesOffXllSync,
        LIBBLU_DTS_ERROR_RETURN(
          "Unable to add DTS-HDMA frame, source stream is too hashed.\n"
        )
      );

      LIBBLU_DEBUG_COM(
        " Previous frame %u data: %zu bytes.\n",
        prev_frame->number,
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
    DcaXllPbrFramePtr frame = newEntryCircularBuffer(
      &ctx->pendingFrames,
      sizeof(DcaXllPbrFrame)
    );
    if (NULL == frame)
      return -1;

    frame->number = ctx->nbParsedPbrFrames++;
    frame->size = asset.size - asset.nbBytesOffXllSync;
    frame->pos = INIT_DTS_XLL_FRAME_SF_POS();
    frame->pbrDelay = asset.initialXllDecodingDelayInFrames;

    ADD_DTS_XLL_FRAME_SF_POS(
      frame->pos,
      asset_off + asset.nbBytesOffXllSync,
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
  else if (NULL != prev_frame) {
    prev_frame->size += asset.size;
    ADD_DTS_XLL_FRAME_SF_POS(
      prev_frame->pos,
      asset_off,
      asset.size,
      LIBBLU_DTS_ERROR_RETURN(
        "Unable to add DTS-HDMA frame, source stream is too hashed.\n"
      )
    );
  }

  if (asset.size < assetLength) {
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
    frame->size = assetLength - asset.size;
    frame->pos = INIT_DTS_XLL_FRAME_SF_POS();
    frame->pbrDelay = 1;
    frame->pbrDelayInUse = false;

    ADD_DTS_XLL_FRAME_SF_POS(
      frame->pos,
      assetOffset + asset.size,
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
  DtsXllFrameContext * ctx
)
{
  assert(0 < ctx->pbrBufferUsedSize);

  ctx->pbrBufferParsedSize = 0;
  ctx->remainingBits = 0;
  ctx->initializedPbrUnpack = true;
  return 0;
}

int completeUnpackDtsXllPbr(
  DtsXllFrameContext * ctx
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
  DtsXllFrameContext * ctx,
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
      if (ctx->crcCtx.in_use)
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
  DtsXllFrameContext * ctx,
  size_t length
)
{
  return unpackBitsDtsXllPbr(ctx, NULL, length);
}

void alignByteDtsXllPbr(
  DtsXllFrameContext * ctx
)
{
  ctx->remainingBits = 0;
}

int align32BitsDtsXllPbr(
  DtsXllFrameContext * ctx
)
{
  size_t alignment;

  if (4 == (alignment = 4 - (ctx->pbrBufferParsedSize & 0x3)))
    return -1;
  return unpackBytesDtsXllPbr(ctx, NULL, alignment);
}

int unpackByteDtsXllPbr(
  DtsXllFrameContext * ctx,
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
  if (ctx->crcCtx.in_use)
    applySingleByteCrcContext(&ctx->crcCtx, ctx->pbrBuffer[ctx->pbrBufferParsedSize]);

  ctx->pbrBufferParsedSize++;

  return 0;
}

int unpackBytesDtsXllPbr(
  DtsXllFrameContext * ctx,
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
  DtsXllFrameContext * ctx,
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
  DtsXllFrameContext * ctx
)
{
  return ctx->pbrBufferParsedSize;
}

int registerDecodedFrameOffsetsDtsXllFrameContext(
  DtsXllFrameContext * ctx,
  const DcaXllFrameSFPosition framePosition
)
{
  DtsXllDecodedFrame * entry;

  /* Create a new entry */
  entry = (DtsXllDecodedFrame *) newEntryCircularBuffer(
    &ctx->decodedFramesOff,
    sizeof(DtsXllDecodedFrame)
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
  DtsXllFrameContext * ctx,
  size_t requestedFrameSize,
  DcaXllFrameSFPosition * builded_frame_pos_ret,
  int * sync_word_off_idx_ret,
  unsigned * init_dec_delay_ret
)
{
  DcaXllFrameSFPosition builded_frame_pos = (DcaXllFrameSFPosition) {0};

  int sync_word_off_idx = -1;
  unsigned init_dec_delay = 0;

  for (int index = 0; 0 < requestedFrameSize; index++) {
    DtsXllDecodedFrame * dec_frame = getEntryCircularBuffer(
      &ctx->decodedFramesOff,
      0
    );
    if (NULL == dec_frame)
      LIBBLU_DTS_XLL_ERROR_RETURN(
        "Requested frame size is greater than the amount of buffered data.\n"
      );

    if (!dec_frame->unpackingStarted) {
      if (sync_word_off_idx < 0) {
        sync_word_off_idx = builded_frame_pos.nbSourceOffsets;
        init_dec_delay = dec_frame->number - ctx->nbSlicedFrames;
      }
    }

    int ret = collectFrameDataDcaXllFrameSFPosition(
      &dec_frame->position,
      &requestedFrameSize,
      &builded_frame_pos
    );
    if (ret < 0)
      return -1;

    if (IS_EMPTY_XLL_FRAME_SF_POS(dec_frame->position)) {
      if (popCircularBuffer(&ctx->decodedFramesOff, NULL) < 0)
        LIBBLU_DTS_XLL_ERROR_RETURN("Broken decoded frames offsets FIFO.\n");
    }
    else {
      assert(requestedFrameSize == 0);
      assert(0 < dec_frame->position.sourceOffsets[0].len);
      dec_frame->unpackingStarted = true;
    }
  }

  if (NULL != builded_frame_pos_ret)
    *builded_frame_pos_ret = builded_frame_pos;
  if (NULL != sync_word_off_idx_ret)
    *sync_word_off_idx_ret = sync_word_off_idx;
  if (NULL != init_dec_delay_ret)
    *init_dec_delay_ret = init_dec_delay;

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

    DcaXllFrameSFPositionIndex * srcIdx = &src->sourceOffsets[i];

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
  size_t abs_offset,
  size_t * rel_offset
)
{
  size_t offset = 0;
  for (int i = 0; i < frame.nbSourceOffsets; i++) {
    const DcaXllFrameSFPositionIndex * index = &frame.sourceOffsets[i];
    size_t indexEndOffset = index->off + index->len;

    if (index->off <= abs_offset && abs_offset < indexEndOffset) {
      if (NULL != rel_offset)
        *rel_offset = offset + (abs_offset - index->off);
      return 0;
    }

    offset += index->len;
  }

  return -1;
}

int getFrameTargetSizeDtsXllPbr(
  DtsXllFrameContext * ctx,
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
  DtsXllFrameContext * ctx
)
{
  return performComputationDtsPbrSmoothing(
    &ctx->pbrSmoothing
  );
}