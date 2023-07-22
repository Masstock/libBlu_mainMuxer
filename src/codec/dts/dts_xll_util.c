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
  unsigned size
)
{
  assert(NULL != stats);
  assert(0 < stats->framesPerSec); /* FPS shall be initialized */

  if (stats->nbAllocatedFrames <= stats->nbUsedFrames) {
    unsigned new_len = GROW_ALLOCATION(stats->nbAllocatedFrames, 4096);

    if (lb_mul_overflow(new_len, sizeof(unsigned)))
      LIBBLU_DTS_ERROR_RETURN(
        "Too many DTS XLL frames for PBR smoothing process.\n"
      );

    unsigned * new_array = realloc(
      stats->targetFrmSize,
      new_len * sizeof(unsigned)
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

int _getFrameTargetSizeDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats,
  unsigned frame_idx,
  uint32_t * size
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

static int _performComputationDtsPbrSmoothing(
  DtsPbrSmoothingStats * stats
)
{
  bool fake_pbr = !isInitPbrFileHandler();

  LIBBLU_DTS_DEBUG_PBR(
    "Performing Peak Bit-Rate Smoothing computation on parsed frame sizes.\n"
  );
  LIBBLU_DTS_DEBUG_PBR(
    " -> Fake mode: %s.\n",
    BOOL_INFO(fake_pbr)
  );

  uint32_t pbrs_buf_size;
  if (!fake_pbr) {
    pbrs_buf_size = stats->max_pbrs_buf_size;

    LIBBLU_DTS_DEBUG_PBR(
      " -> Defined buffer size: %" PRIu32 " bytes (0: Disabled).\n",
      pbrs_buf_size
    );

    if (!stats->avg_frame_size_set) {
      uint32_t avg_size;

      if (getAvgSizePbrFileHandler(&avg_size) < 0)
        return -1;
      stats->avg_frame_size     = avg_size;
      stats->avg_frame_size_set = true;
    }

    LIBBLU_DTS_DEBUG_PBR(
      " -> Average frame size: %zu bytes.\n",
      stats->avg_frame_size
    );
  }
  else
    pbrs_buf_size = 0;

  LIBBLU_DTS_DEBUG_PBR("Compute frames sizes:\n");
  bool adj_required = false;
  uint32_t accltd_size = 0; // Amount of future bytes to accumulate
  uint32_t max_accltd_size = 0; // Peak amount of bytes accumulated

  for (unsigned idx = stats->nbUsedFrames; 0 < idx; idx--) {
    /* Compute amount of bytes accumulated at current frame decoding */
    uint32_t cur_accltd_size = stats->targetFrmSize[idx-1] + accltd_size;

    /* Compute frame maximum size */
    uint32_t max_frame_size;
    if (fake_pbr) {
      max_frame_size = 3200;
    }
    else {
      /* TODO: PBR smoothing using dtspbr file values need more research. */
#if 0
      if (getMaxSizePbrFileHandler((unsigned) (timestamp * 1000), &maxSize) < 0)
        return -1;
#else
      max_frame_size = stats->avg_frame_size;
#endif
    }

    /* Compute resulting targetted frame size. */
    uint32_t res_frm_size = MIN(cur_accltd_size, max_frame_size);
    bool cur_adj_required = false;

    if (
      0 < pbrs_buf_size // PBRS buffer size is not zero
      && pbrs_buf_size < cur_accltd_size // Accumulated bytes might overflow PBRS buffer
      && res_frm_size < cur_accltd_size - pbrs_buf_size // Exceeding bytes overflows
                                                          // max target frame size
    ) {
      // Accumulated bytes will overflow PBRS buffer if accumulated any longer
      // Targeted frame size can't be maintain anymore, adjust current frame
      // max size to contain exceeding bytes to avoid this overflow.
      res_frm_size = cur_accltd_size - pbrs_buf_size;
      adj_required = cur_adj_required = true;
    }

    /* Compute amount of bytes to be accumulated in previous frames */
    accltd_size = cur_accltd_size - res_frm_size;

    if (0 < accltd_size) {
      LIBBLU_DTS_DEBUG_PBR(
        " Frame %u: %" PRIu32 " bytes -> %" PRIu32 " bytes "
        "(%" PRIu32 " bytes buffered)%s.\n",
        idx-1,
        stats->targetFrmSize[idx-1],
        res_frm_size,
        accltd_size,
        cur_adj_required ? " # Overflow avoiding" : ""
      );
    }

    stats->targetFrmSize[idx-1] = res_frm_size; // Save resulting frame size.
    max_accltd_size = MAX(max_accltd_size, accltd_size);
  }

  /* Remaining accumulated bytes, failing of PBRS */
  if (0 < accltd_size)
    LIBBLU_DTS_XLL_ERROR_RETURN(
      "PBR smoothing process error, unable to smooth enough data "
      "(unable to place %zu bytes).\n",
      accltd_size
    );

  LIBBLU_DTS_DEBUG_PBR("Summary:\n");
  LIBBLU_DTS_DEBUG_PBR(
    " -> Max buffered size: %" PRIu32 " bytes.\n",
    max_accltd_size
  );
  if (adj_required)
    LIBBLU_DTS_DEBUG_PBR(
      "  -> Adjustment done to avoid buffer overflow.\n"
    );

  return 0;
}

int initDtsXllFrameContext(
  DtsXllFrameContext * ctx,
  DcaAudioAssetDescParameters asset,
  const DcaExtSSHeaderParameters * ext_ss_hdr,
  DtshdFileHandler * dtshd
)
{
  assert(NULL != ctx);

  DcaAudioAssetDescDecNDParameters * dec_nav_data = &asset.dec_nav_data;
  DcaAudioAssetExSSXllParameters * xll_param = &dec_nav_data->coding_components.ExSSXLL;

  /* Update decoder PBRS buffer size according to header : */
  if (dec_nav_data->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    if (xll_param->bExSSXLLSyncPresent) {
      size_t buf_size = xll_param->nuPeakBRCntrlBuffSzkB << 10; // KiB

      if (DTS_XLL_MAX_PBR_BUFFER_SIZE < buf_size)
        LIBBLU_DTS_ERROR_RETURN(
          "DTS-HDMA PBR buffer size exceed 240 kBytes (%zu kBytes).\n",
          xll_param->nuPeakBRCntrlBuffSzkB
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

    ctx->pbrSmoothing.max_pbrs_buf_size = pbrs_buf_size;
  }

  if (ext_ss_hdr->bStaticFieldsPresent) {
    float ExSSFrameDuration = getExSSFrameDurationDcaExtRefClockCode(
      &ext_ss_hdr->static_fields
    );
    assert(0.f <= ExSSFrameDuration);
    ctx->pbrSmoothing.framesPerSec = ExSSFrameDuration;
  }

  return 0;
}

static int _updateDtsXllPbrBufferSize(
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
  DcaAudioAssetExSSXllParameters asset,
  size_t assetLength
)
{
  assert(!ctx->initializedPbrUnpack);

  if (_updateDtsXllPbrBufferSize(ctx, asset.nuExSSXLLFsize + ctx->pbrBufferUsedSize) < 0)
    return -1;

  uint8_t * buf_ptr = &ctx->pbrBuffer[ctx->pbrBufferUsedSize];
  if (readBytes(dtsInput, buf_ptr, asset.nuExSSXLLFsize) < 0)
    return -1;
  ctx->pbrBufferUsedSize += asset.nuExSSXLLFsize;

  size_t nbBufFrames = getNbEntriesCircularBuffer(&ctx->pendingFrames);
  DcaXllPbrFramePtr prev_frame = NULL;
  if (0 < nbBufFrames)
    prev_frame = getEntryCircularBuffer(&ctx->pendingFrames, nbBufFrames - 1);

  uint64_t xll_off  = (uint64_t) tellPos(dtsInput);
  uint32_t xll_size = asset.nuExSSXLLFsize;

  if (asset.bExSSXLLSyncPresent && 0 < asset.nuExSSXLLSyncOffset) {
    uint32_t xll_sw_off = asset.nuExSSXLLSyncOffset;

    /**
     * Sync word present,
     */

    if (xll_size <= xll_sw_off)
      LIBBLU_DTS_ERROR_RETURN(
        "Out of asset XLL sync word offset "
        "(%" PRIu32 " <= %" PRIu32 " bytes).\n",
        xll_size, xll_sw_off
      );

    if (NULL == prev_frame)
      LIBBLU_DTS_ERROR_RETURN(
        "Presence of garbage out-of-sync DTS-HDMA audio data "
        "of %zu bytes at start.\n",
        xll_sw_off
      );

    /* Collect if present data for last frame */
    prev_frame->size += xll_sw_off;
    if (addDcaXllFrameSFPosition(&prev_frame->pos, xll_off, xll_sw_off) < 0)
      LIBBLU_DTS_ERROR_RETURN("Unable to add XLL frame, source stream is too hashed.\n");

    LIBBLU_DEBUG_COM(
      " Previous frame %u data: %" PRIu32 " bytes.\n",
      prev_frame->number,
      xll_sw_off
    );

    uint64_t cur_frm_off  = xll_off  + xll_sw_off;
    uint32_t cur_frm_size = xll_size - xll_sw_off;

    /* Add frame to pending */
    DcaXllPbrFramePtr frame = newEntryCircularBuffer(
      &ctx->pendingFrames,
      sizeof(DcaXllPbrFrame)
    );
    if (NULL == frame)
      return -1;
    *frame = (DcaXllPbrFrame) {
      .number   = ctx->nbParsedPbrFrames++,
      .size     = cur_frm_size,
      .pbrDelay = asset.nuInitLLDecDlyFrames
    };

    if (addDcaXllFrameSFPosition(&frame->pos, cur_frm_off, cur_frm_size) < 0)
      LIBBLU_DTS_ERROR_RETURN("Unable to add XLL frame, source stream is too hashed.\n");

    LIBBLU_DEBUG_COM(
      " Current frame %u data: %" PRIu32 " bytes.\n",
      frame->number,
      frame->size
    );
  }
  else if (NULL != prev_frame) {
    prev_frame->size += xll_size;
    if (addDcaXllFrameSFPosition(&prev_frame->pos, xll_off, xll_size) < 0)
      LIBBLU_DTS_ERROR_RETURN("Unable to add XLL frame, source stream is too hashed.\n");
  }

  if (xll_size < assetLength) {
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

int _alignDwordDtsXllPbr(
  DtsXllFrameContext * ctx
)
{
  unsigned padding_size = (~(ctx->pbrBufferParsedSize - 1)) & 0x3;
  return unpackBytesDtsXllPbr(ctx, NULL, padding_size);
}

int completeUnpackDtsXllPbr(
  DtsXllFrameContext * ctx
)
{
  assert(ctx->initializedPbrUnpack);

  /* Align to 32-bits boundary */
  if (_alignDwordDtsXllPbr(ctx) < 0)
    return -1;

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

    if (0 == dec_frame->position.nbSourceOffsets) {
      if (popCircularBuffer(&ctx->decodedFramesOff, NULL) < 0)
        LIBBLU_DTS_XLL_ERROR_RETURN("Broken decoded frames offsets FIFO.\n");
    }
    else {
      assert(requestedFrameSize == 0);
      assert(0 < dec_frame->position.sourceOffsets[0].size);
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
  for (unsigned i = 0; i < position.nbSourceOffsets; i++) {
    lbc_printf(
      "%-*c- 0x%08" PRIX64 ", length: %" PRIu32 " bytes.\n",
      indent, ' ',
      position.sourceOffsets[i].offset,
      position.sourceOffsets[i].size
    );
  }
}

void substractDtsXllFrameOriginalPosition(
  DtsXllFrameParameters * frame,
  DcaXllFrameSFPosition * pbrFramePosition
)
{
  DcaXllFrameSFPosition dstXllFramePosition = (DcaXllFrameSFPosition) {0};
  size_t remainingFrameSize = frame->comHdr.frameSize;

  int ret = collectFrameDataDcaXllFrameSFPosition(
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
  size_t indexConsumedLength, remainingFrameSize;
  int consumedIndexes;

  assert(NULL != src);
  assert(NULL != dataAmount);

  DcaXllFrameSFPosition genFrame = (DcaXllFrameSFPosition) {0};
  if (NULL != dst)
    genFrame = *dst;

  remainingFrameSize = *dataAmount;
  consumedIndexes = 0;

  for (unsigned i = 0; i < src->nbSourceOffsets; i++) {
    /* While remaining decoded frame data to collect from PBR frame */
    if (!remainingFrameSize)
      break;

    DcaXllFrameSFPositionIndex * srcIdx = &src->sourceOffsets[i];

    assert(0 < srcIdx->size);
    indexConsumedLength = MIN(remainingFrameSize, srcIdx->size);

    if (addDcaXllFrameSFPosition(&genFrame, srcIdx->offset, indexConsumedLength) < 0)
      LIBBLU_DTS_XLL_ERROR_RETURN("Too fragmented XLL asset.\n");

    srcIdx->offset += indexConsumedLength;
    srcIdx->size -= indexConsumedLength;
    remainingFrameSize -= indexConsumedLength;
    if (0 == srcIdx->size)
      consumedIndexes++;
    else
      assert(!remainingFrameSize);
  }

  /* Update remaining data in PBR frame */
  src->nbSourceOffsets -= consumedIndexes;
  for (unsigned i = 0; i < src->nbSourceOffsets; i++) {
    src->sourceOffsets[i] = src->sourceOffsets[consumedIndexes+i];
  }

  *dataAmount = remainingFrameSize;
  if (NULL != dst)
    *dst = genFrame;

  return 0;
}

int getRelativeOffsetDcaXllFrameSFPosition(
  const DcaXllFrameSFPosition frame,
  uint64_t abs_offset,
  uint64_t * rel_offset
)
{
  uint64_t offset = 0;

  for (unsigned i = 0; i < frame.nbSourceOffsets; i++) {
    const DcaXllFrameSFPositionIndex * index = &frame.sourceOffsets[i];
    uint64_t end_off = index->offset + index->size;

    if (index->offset <= abs_offset && abs_offset < end_off) {
      if (NULL != rel_offset)
        *rel_offset = offset + (abs_offset - index->offset);
      return 0;
    }

    offset += index->size;
  }

  return -1;
}

int getFrameTargetSizeDtsXllPbr(
  DtsXllFrameContext * ctx,
  unsigned frame_idx,
  uint32_t * size
)
{
  assert(NULL != ctx);

  return _getFrameTargetSizeDtsPbrSmoothing(
    &ctx->pbrSmoothing,
    frame_idx,
    size
  );
}

int performComputationDtsXllPbr(
  DtsXllFrameContext * ctx
)
{
  return _performComputationDtsPbrSmoothing(
    &ctx->pbrSmoothing
  );
}