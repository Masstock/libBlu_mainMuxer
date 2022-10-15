#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "igs_parser.h"

/** \~english
 * \brief Test if filepath finishes with #IGS_COMPILER_FILE_EXT string.
 *
 * \param filepath Input filepath to test.
 * \return true Filepath ends with #IGS_COMPILER_FILE_EXT string.
 * \return false Filepath does not end with #IGS_COMPILER_FILE_EXT string.
 */
static bool isIgsCompilerFile_ext(
  const lbc * filepath
)
{
  const lbc * filepathExt;
  size_t filepathSize;

  filepathSize = lbc_strlen(filepath);
  if (filepathSize < IGS_COMPILER_FILE_EXT_SIZE)
    return false;
  filepathExt = &filepath[filepathSize - IGS_COMPILER_FILE_EXT_SIZE];

  return lbc_equal(filepathExt, IGS_COMPILER_FILE_EXT);
}

/** \~english
 * \brief Test if file starts with #IGS_COMPILER_FILE_MAGIC magic string.
 *
 * \param filepath Input filepath to test.
 * \return true The file starts with #IGS_COMPILER_FILE_MAGIC magic.
 * \return false The file does not start with #IGS_COMPILER_FILE_MAGIC magic.
 */
static bool isIgsCompilerFile_magic(
  const lbc * filepath
)
{
  FILE * fd;
  char magic[IGS_COMPILER_FILE_MAGIC_SIZE];
  bool ret;

  ret = false;
  if (NULL == (fd = lbc_fopen(filepath, "rb")))
    goto free_return;

  if (fread(magic, IGS_COMPILER_FILE_MAGIC_SIZE, 1, fd) <= 0)
    goto free_return;

  ret = (
    0 == memcmp(
      magic,
      IGS_COMPILER_FILE_MAGIC,
      IGS_COMPILER_FILE_MAGIC_SIZE
    )
  );

free_return:
  if (NULL != fd)
    fclose(fd);

  errno = 0; /* Reset if error */
  return ret;
}

bool isIgsCompilerFile(
  const lbc * filepath
)
{
  assert(NULL != filepath);

  /* Check by filename extension (and by file header if unsuccessful) */
  return
    isIgsCompilerFile_ext(filepath)
    || isIgsCompilerFile_magic(filepath)
  ;
}

uint64_t computeIgsOdsDecodeDuration(
  HdmvODataParameters param
)
{
  return computeHdmvOdsDecodeDuration(param, HDMV_STREAM_TYPE_IGS);
}

uint64_t computeIgsOdsTransferDuration(
  HdmvODataParameters param,
  uint64_t decodeDuration
)
{
  /* decodeDuration can be equal to 0 (in this case,
  decodeDuration is reprocessed). */
  /** Compute TRANSFER_DURATION of ODS.
   *
   * Equation performed is:
   *
   *   TRANSFER_DURATION(n) =
   *     DECODE_DURATION(n) * 9
   *
   * TODO: Same remark as computeIgsOdsDecodeDuration().
   */

  if (!decodeDuration)
    return 9 * computeIgsOdsDecodeDuration(param);
  return 9 * decodeDuration;
}

int computeIgsEpochInitializeDuration(
  uint64_t * res,
  HdmvVDParameters videoDesc,
  HdmvSequencePtr odsSequences
)
{
  /** Compute INITIALIZATION_DURATION of ICS (Used as time reference for
   * whole Epoch).
   *
   * Equation performed is:
   *
   *   INITIALIZATION_DURATION = MAX(EPOCH_DECODING_DURATION, PLANE_CLEAR_TIME)
   *
   * with:
   *  EPOCH_DECODING_DURATION : Equation described below.
   *  PLANE_CLEAR_TIME        : "
   *
   *   EPOCH_DECODING_DURATION = 0
   *   for (i = 0; i < nb_ODS; i++) // For each ODS in epoch
   *     EPOCH_DECODING_DURATION += DECODE_DURATION(i)
   *     if (i != nb_ODS-1) // Not the last ODS in epoch
   *       EPOCH_DECODING_DURATION += TRANSFER_DURATION(i)
   *
   * with:
   *  nb_ODS : Number of ODS in ICS current epoch.
   *
   *   PLANE_CLEAR_TIME =
   *     ceil(90000 * 8 * (video_width * video_height) / 128000000)
   * <=> ceil(9 * (video_width * video_height) / 1600)
   *
   * with:
   *  90000        : PTS/DTS clock frequency (90kHz);
   *  128000000    : Pixel Decoding Rate (Rd = 128 Mb/s).
   *  video_width  : video_width parameter parsed from ICS's
   *                 Video_descriptor();
   *  video_height : video_height parameter parsed from ICS's
   *                 Video_descriptor().
   *
   * TODO: Same remark as computeIgsOdsDecodeDuration().
   */
  uint64_t videoSize, planeClearTime, epochDecodingDuration, decodeDuration;
  HdmvSequencePtr seq;

  videoSize = videoDesc.video_width * videoDesc.video_height;
  assert(0 != videoSize);

  if (!videoSize)
    planeClearTime = 0;
  else
    planeClearTime = 1 + ((9 * videoSize - 1) / 1600); /* Ceil */

  epochDecodingDuration = 0;
  for (seq = odsSequences; NULL != seq; seq = seq->nextSequence) {
    if (HDMV_SEGMENT_TYPE_ODS != seq->type)
      LIBBLU_HDMV_IGS_ERROR_RETURN(
        "Unexpected sequence type of %s(s) (0x%02X), in ODS list.\n",
        HdmvSegmentTypeStr(seq->type),
        seq->type
      );

    /* DECODE_DURATION() */
    decodeDuration = computeIgsOdsDecodeDuration(seq->data.objectData);
    epochDecodingDuration += decodeDuration;

    /* TRANSFER_DURATION() */
    if (NULL != seq->nextSequence)
      epochDecodingDuration += computeIgsOdsTransferDuration(
        seq->data.objectData, decodeDuration
      );
  }

  if (NULL != res)
    *res = MAX(planeClearTime, epochDecodingDuration);
  return 0;
}

int processIgsEpochTimingValues(HdmvSegmentsContextPtr ctx)
{
  HdmvSequencePtr seq;
  uint64_t clockRef, initDur, decodeDur, transfDur;
  unsigned i;
  int ret;

  if (!ctx->curNbIcsSegments)
    LIBBLU_HDMV_IGS_ERROR_RETURN(
      "Missing of Interactive Composition Segment in one epoch.\n"
    );

  LIBBLU_HDMV_IGS_DEBUG(" Calculation of time values:\n");
  initDur = decodeDur = transfDur = 0;

  if (ctx->forceRetiming) {
    ret = computeIgsEpochInitializeDuration(&initDur, ctx->videoDesc, ctx->segRefs.ods);
    if (ret < 0)
      return -1;

    LIBBLU_HDMV_IGS_DEBUG(
      "  Initialization duration of epoch: %" PRIu64 ".\n",
      initDur
    );

    if (!initDur)
      LIBBLU_WARNING("No delay epoch founded (empty ?).\n");

    LIBBLU_HDMV_IGS_DEBUG("  Applying time values to segments:\n");

    ctx->referenceStartClock = initDur;

    LIBBLU_HDMV_IGS_DEBUG(
      "   Using initialization duration as ref clock (%" PRIu64 ").\n",
      initDur
    );
  }
  else {
    LIBBLU_HDMV_IGS_DEBUG(
      "  NOTE: Using input file headers time values.\n"
    );
  }

  LIBBLU_HDMV_IGS_DEBUG("   - IGS:\n");
  for (
    seq = ctx->segRefs.ics, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_ICS);

    if (ctx->forceRetiming) {
      seq->dts = 0;
      seq->pts = initDur;
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_HDMV_IGS_DEBUG(
      "    - IGS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );
  }

  LIBBLU_HDMV_IGS_DEBUG("   - PDS:\n");
  for (
    seq = ctx->segRefs.pds, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_PDS);

    if (ctx->forceRetiming) {
      seq->dts = 0;
      seq->pts = 0;
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_HDMV_IGS_DEBUG(
      "    - PDS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );
  }

  LIBBLU_HDMV_IGS_DEBUG("   - ODS:\n");
  clockRef = 0;
  for (
    seq = ctx->segRefs.ods, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_ODS);

    if (ctx->forceRetiming) {
      decodeDur = computeIgsOdsDecodeDuration(
        seq->data.objectData
      );
      transfDur = computeIgsOdsTransferDuration(
        seq->data.objectData, decodeDur
      );

      seq->dts = clockRef;

      clockRef += decodeDur; /* Adding ODS decoding delay. */

      seq->pts = clockRef;

      /* Adding ODS transfer delay (only if ODS is not the last one) */
      if (NULL != seq->nextSequence)
        clockRef += transfDur;

      /* Debug check initDur shall always be the larger : */
      assert(clockRef <= initDur);
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_HDMV_IGS_DEBUG(
      "    - ODS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );

    if (ctx->forceRetiming) {
      LIBBLU_HDMV_IGS_DEBUG(
        "       => DECODE_DURATION=%" PRIu64
        " TRANSFER_DURATION=%" PRIu64 ".\n",
        decodeDur, transfDur
      );
      if (NULL == seq->nextSequence)
        LIBBLU_HDMV_IGS_DEBUG("     -> TRANSFER_DURATION is not used.\n");
    }
  }

  LIBBLU_HDMV_IGS_DEBUG("   - END:\n");
  for (
    seq = ctx->segRefs.end, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_END);

    if (ctx->forceRetiming) {
      seq->dts = seq->pts = initDur;
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_HDMV_IGS_DEBUG(
      "    - END #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );
  }

  return 0;
}

int registerIgsEpoch(HdmvSegmentsContextPtr ctx)
{
  HdmvSequencePtr seq;
  HdmvSegmentPtr seg;
  uint64_t pts, dts;
  unsigned i;

  const unsigned segTypesNb = 4;
  const HdmvSequencePtr segTypes[] = {
    ctx->segRefs.ics,
    ctx->segRefs.pds,
    ctx->segRefs.ods,
    ctx->segRefs.end
  };

  for (i = 0; i < segTypesNb; i++) {
    for (seq = segTypes[i]; NULL != seq; seq = seq->nextSequence) {
      pts = seq->pts * 300;
      dts = seq->dts * 300;

      for (seg = seq->segments; NULL != seg; seg = seg->nextSegment) {
        if (insertHdmvSegment(ctx, seg->param, pts, dts) < 0)
          return -1;
      }
    }
  }

  return 0;
}

int processCompleteIgsEpoch(HdmvSegmentsContextPtr ctx)
{
  assert(NULL != ctx);

  LIBBLU_HDMV_IGS_DEBUG("Processing complete epoch...\n");

  LIBBLU_HDMV_IGS_DEBUG(" Current number of segments by type:\n");
  LIBBLU_HDMV_IGS_DEBUG("  - IGS: %u.\n", ctx->curNbIcsSegments   );
  LIBBLU_HDMV_IGS_DEBUG("  - PDS: %u.\n", ctx->curNbPdsSegments   );
  LIBBLU_HDMV_IGS_DEBUG("  - ODS: %u.\n", ctx->curNbOdsSegments   );
  LIBBLU_HDMV_IGS_DEBUG("  - END: %u.\n", ctx->curNbEndSegments   );

  /* Compute decoding duration: */
  if (processIgsEpochTimingValues(ctx) < 0)
    return -1;

  LIBBLU_HDMV_IGS_DEBUG(" Registering %u segments...\n", ctx->curEpochNbSegments);
  if (registerIgsEpoch(ctx) < 0) {
    LIBBLU_DEBUG_COM_NO_HEADER("Failed !\n");
    return -1;
  }
  LIBBLU_HDMV_IGS_DEBUG(" Done.\n");

  LIBBLU_HDMV_IGS_DEBUG(" Clearing all buffers... ");
  resetHdmvContext(ctx);
  LIBBLU_DEBUG_COM_NO_HEADER("Done.\n");

  LIBBLU_HDMV_IGS_DEBUG("Processing done, switching to next epoch (or completing parsing).\n");
  return 0; /* OK */
}

int parseIgsMnuHeader(BitstreamReaderPtr igsInput, HdmvSegmentsContextPtr ctx)
{
  uint32_t value;

  assert(NULL != igsInput);
  assert(NULL != ctx);

  LIBBLU_HDMV_IGS_DEBUG("0x%08" PRIX64 ": MNU Header.\n");

  /* [u16 format_identifier] */
  if (readValueBigEndian(igsInput, 2, &value) < 0)
    return -1;

  if (value != IGS_MNU_WORD)
    LIBBLU_HDMV_IGS_ERROR_RETURN(
      "Unexpected 'format_identifier' not equal to 0x%04X in MNU.\n",
      IGS_MNU_WORD
    );

  LIBBLU_HDMV_IGS_DEBUG(
    " format_identifier: \"%c%c\" (0x%04x).\n",
    (char) ((value >> 8) & 0xFF), (char) (value & 0xFF),
    value
  );

  /* [u32 pts] */
  if (readValueBigEndian(igsInput, 4, &value) < 0)
    return -1;
  ctx->curSegProperties.pts = value;

  LIBBLU_HDMV_IGS_DEBUG(
    " pts: %" PRIu32 " (0x%08" PRIX32 ").\n",
    ctx->curSegProperties.pts,
    ctx->curSegProperties.pts
  );

  /* [u32 dts] */
  if (readValueBigEndian(igsInput, 4, &value) < 0)
    return -1;
  ctx->curSegProperties.dts = value;

  LIBBLU_HDMV_IGS_DEBUG(
    " dts: %" PRIu32 " (0x%08" PRIX32 ").\n",
    ctx->curSegProperties.dts,
    ctx->curSegProperties.dts
  );

  return 0;
}

bool nextUint8IsIgsSegmentType(BitstreamReaderPtr igsInput)
{
  uint8_t value = nextUint8(igsInput);

  return
    value == HDMV_SEGMENT_TYPE_PDS ||
    value == HDMV_SEGMENT_TYPE_ODS ||
    value == HDMV_SEGMENT_TYPE_ICS ||
    value == HDMV_SEGMENT_TYPE_END
  ;
}

int parseIgsSegment(BitstreamReaderPtr igsInput, HdmvSegmentsContextPtr ctx)
{
  HdmvSegmentParameters seg;

  assert(NULL != igsInput);
  assert(NULL != ctx);

  /* Segment_descriptor() */
  if (parseHdmvSegmentDescriptor(igsInput, &seg) < 0)
    return -1;

  if (!ctx->forceRetiming) {
    /* Copying time values. */
    LIBBLU_HDMV_IGS_DEBUG(" Relative segment timing values (parsed from MNU header):\n");

    if (ctx->totalNbSegments == 0) {
      /* Initializing reference start clock with first segment DTS. */
      /* This value is used as offset to allow negative time values (pre-buffering) */
      ctx->referenceStartClock = ctx->curSegProperties.dts;

      LIBBLU_HDMV_IGS_DEBUG(
        "  Used reference start clock: %" PRIu64 ".\n",
        ctx->referenceStartClock
      );
    }

    /* DTS */
    if (ctx->curSegProperties.dts != 0) {
      if (ctx->curSegProperties.dts < ctx->referenceStartClock) {
        seg.dts = 0;
        LIBBLU_WARNING(
          "A segment in the middle of bitstream as a lower DTS value "
          "than initial one (time values will broke).\n"
        );
      }
      else
        seg.dts = ctx->curSegProperties.dts - ctx->referenceStartClock;
    }
    else {
      seg.dts = 0;
      LIBBLU_HDMV_IGS_DEBUG("  DTS: Not transmited.\n");
    }

    /* PTS */
    if (ctx->curSegProperties.pts < ctx->referenceStartClock) {
      seg.pts = 0;
      LIBBLU_WARNING(
        "A segment in the middle of bitstream as a lower PTS value "
        "than initial one (time values will broke).\n"
      );
    }
    else
      seg.pts = ctx->curSegProperties.pts - ctx->referenceStartClock;
  }

  switch (seg.type) {
    case HDMV_SEGMENT_TYPE_ICS:
      /* Interactive Composition Segment */
      if (decodeHdmvIcsSegment(igsInput, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_PDS:
      /* Palette Definition Segment */
      if (decodeHdmvPdsSegment(igsInput, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_ODS:
      /* Object Definition Segment */
      if (decodeHdmvOdsSegment(igsInput, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_END:
      /* End Segment */
      if (decodeHdmvEndSegment(ctx, seg) < 0)
        return -1;

      LIBBLU_HDMV_IGS_DEBUG(
        "End of epoch of %u segments.\n",
        ctx->curEpochNbSegments
      );

      return processCompleteIgsEpoch(ctx);

    default:
      assert(seg.type != HDMV_SEGMENT_TYPE_ERROR);

      LIBBLU_HDMV_IGS_ERROR_RETURN(
        "Unexpected segment type %s (0x%02X).\n",
        HdmvSegmentTypeStr(seg.type),
        seg.type
      );
  }

  ctx->curEpochNbSegments++;
  ctx->totalNbSegments++;

  return 0;
}

int analyzeIgs(
  LibbluESParsingSettings * settings
)
{
  EsmsFileHeaderPtr igsInfos = NULL;
  unsigned igsSourceFileIdx;

  BitstreamReaderPtr igsInput = NULL;
  BitstreamWriterPtr essOutput = NULL;

  HdmvSegmentsContextPtr igsContext = NULL;

  lbc * inputFilenameDup;
  bool igsCompilerFileMode;

  igsCompilerFileMode = isIgsCompilerFile(settings->esFilepath);

  if (igsCompilerFileMode) {
#if !defined(DISABLE_IGS_COMPILER)
    int ret;

    LIBBLU_HDMV_IGS_DEBUG("Processing input file as Igs Compiler file.\n");

    if (processIgsCompiler(settings->esFilepath, settings->options.confHandle) < 0)
      return -1;

    ret = lbc_asprintf(
      &inputFilenameDup,
      "%" PRI_LBCS "%s",
      settings->esFilepath,
      HDMV_IGS_COMPL_OUTPUT_EXT
    );
    if (ret < 0)
      LIBBLU_HDMV_IGS_ERROR_RETURN(
        "Unable to set Igs Compiler output filename, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

#else
    LIBBLU_ERROR("Igs Compiler is not available in this program build !\n");
    return -1;
#endif
  }
  else {
    if (NULL == (inputFilenameDup = lbc_strdup(settings->esFilepath)))
      LIBBLU_HDMV_IGS_ERROR_RETURN("Memory allocation error.\n");
  }

  if (NULL == (igsInfos = createEsmsFileHandler(ES_HDMV, settings->options, FMT_SPEC_INFOS_NONE)))
    goto free_return;

  if (appendSourceFileEsms(igsInfos, inputFilenameDup, &igsSourceFileIdx) < 0)
    goto free_return;

  if (NULL == (essOutput = createBitstreamWriter(settings->scriptFilepath, (size_t) WRITE_BUFFER_LEN)))
    goto free_return;

  if (NULL == (igsContext = createHdmvSegmentsContext(HDMV_STREAM_TYPE_IGS, essOutput, igsInfos, igsSourceFileIdx)))
    goto free_return;

  if (NULL == (igsInput = createBitstreamReader(inputFilenameDup, (size_t) READ_BUFFER_LEN)))
    goto free_return;

  if (igsCompilerFileMode) {
    /* Add original script file to script to target changes. */
    if (appendSourceFileEsms(igsInfos, settings->esFilepath, NULL) < 0)
      goto free_return;
  }

  /* Used as Place Holder (empty header) : */
  if (writeEsmsHeader(essOutput) < 0)
    goto free_return;

  /* Guess input file is not a MNU file but raw IGS PES bitstream (.ies file). */
  /* No MNU header, expect direct segment_type field.                          */
  igsContext->rawStreamInputMode = nextUint8IsIgsSegmentType(igsInput);
  igsContext->forceRetiming |= igsContext->rawStreamInputMode;

  while (!isEof(igsInput)) {
    /* Progress bar : */
    printFileParsingProgressionBar(igsInput);

    if (!igsContext->rawStreamInputMode) {
      if (parseIgsMnuHeader(igsInput, igsContext) < 0)
        goto free_return;
    }

    if (nextUint8IsIgsSegmentType(igsInput)) {
      /* Interactive_graphic_stream_segment() */
      if (parseIgsSegment(igsInput, igsContext) < 0)
        goto free_return;
      continue;
    }

    /* else : */
    LIBBLU_HDMV_IGS_ERROR_FRETURN(
      "Unknown header type word: 0x%" PRIX8 " at 0x%" PRIx64 ".\n",
      nextUint8(igsInput), tellPos(igsInput)
    );
  }

  /* Process remaining segments: */
  if (0 < igsContext->curEpochNbSegments) {
    LIBBLU_WARNING(
      "Presence of unclosed epoch at end of bitstream (missing END segment).\n"
    );
    LIBBLU_WARNING(
      " => Composed of %u segments.\n",
      igsContext->curEpochNbSegments
    );
  }

  lbc_printf(" === Parsing finished with success. ===              \n");

  closeBitstreamReader(igsInput);
  igsInput = NULL;

  /* [u8 endMarker] */
  if (writeByte(essOutput, ESMS_SCRIPT_END_MARKER) < 0)
    goto free_return;

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: HDMV/IGS Menu format.             \n");
  lbc_printf("Total number of segments by type:        \n");
  lbc_printf(" - IGS: %u.\n", igsContext->nbIcsSegments   );
  lbc_printf(" - PDS: %u.\n", igsContext->nbPdsSegments   );
  lbc_printf(" - ODS: %u.\n", igsContext->nbOdsSegments   );
  lbc_printf(" - END: %u.\n", igsContext->nbEndSegments   );
  lbc_printf(" TOTAL: %u.\n", igsContext->totalNbSegments );
  lbc_printf("=======================================================================================\n");

  igsInfos->ptsRef = igsContext->referenceStartClock * 300;
  igsInfos->bitrate = HDMV_RX_BITRATE;
  igsInfos->endPts = igsContext->referenceStartClock * 600;
  igsInfos->prop.codingType = STREAM_CODING_TYPE_IG;

  destroyHdmvContext(igsContext);
  igsContext = NULL;

  if (addEsmsFileEnd(essOutput, igsInfos) < 0)
    goto free_return;
  closeBitstreamWriter(essOutput);
  essOutput = NULL;

  if (updateEsmsHeader(settings->scriptFilepath, igsInfos) < 0)
    goto free_return;

  free(inputFilenameDup);
  destroyEsmsFileHandler(igsInfos);
  return 0;

free_return:
  free(inputFilenameDup);
  destroyHdmvContext(igsContext);
  closeBitstreamReader(igsInput);
  closeBitstreamWriter(essOutput);
  destroyEsmsFileHandler(igsInfos);

  return -1;
}