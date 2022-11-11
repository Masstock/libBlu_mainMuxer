#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "pgs_parser.h"

#if 0
uint64_t computePgsOdsDecodeDuration(
  HdmvODParameters param
)
{
  return computeHdmvOdsDecodeDuration(param, HDMV_STREAM_TYPE_PGS);
}

uint64_t computePgsCompositionObjectDecodeDuration(
  uint16_t object_id_ref,
  HdmvSequencePtr odsSequences
)
{
  HdmvSequencePtr ods;
  uint16_t odsId;

  for (ods = odsSequences; NULL != ods; ods = ods->nextSequence) {
    /* Looking ODS with object_id == object_id_ref */
    odsId = ods->segments->param.data.odsParam.object_descriptor.object_id;
    if (odsId == object_id_ref)
      break;
  }

  assert(NULL != ods); /* Unreferenced object_id_ref */

  if (NULL == ods)
    return 0;
  return computePgsOdsDecodeDuration(ods->data.objectDefinition);
}

uint64_t computePgsWindowDecodeDuration(
  uint8_t window_id_ref,
  HdmvWDParameters winDef
)
{
  /**
   * 90000 * 8 * window_width * window_height / 256000000
   */
  uint64_t windowSize;
  uint8_t i;

  windowSize = 0;
  for (i = 0; i < winDef.number_of_windows; i++) {
    if (window_id_ref == winDef.windows[i]->window_id) {
      windowSize =
        winDef.windows[i]->window_width
        * winDef.windows[i]->window_height
      ;
      break;
    }
  }

  return DIV_ROUND_UP(9 * windowSize, 3200);
}

uint64_t computePgsDisplaySetInitializeDuration(
  HdmvCompositionState compoState,
  HdmvPCParameters compo,
  HdmvWDParameters winDef,
  HdmvVDParameters videoDesc,
  HdmvSequencePtr odsSequences
)
{
  /**
   *
   *
   */
  uint64_t planeInitDur, dsDecodeDur, odsDecodeDur;

  /* PLANE_INITIALIZATION_TIME */
  if (compoState == HDMV_COMPO_STATE_EPOCH_START) {
    uint64_t videoSize = videoDesc.video_width * videoDesc.video_height;

    planeInitDur = 9 * videoSize / 3200; /* 90000 * 8 * videoSize / 256000000 */
  }
  else {
    unsigned i, j;

    planeInitDur = 0;
    for (i = 0; i < winDef.number_of_windows; i++) {
      bool referenced = false;

      for (j = 0; j < compo.number_of_composition_objects; j++) {
        uint8_t window_id_ref = compo.composition_objects[j]->window_id_ref;

        if (winDef.windows[i]->window_id == window_id_ref) {
          referenced = true;
          break;
        }
      }

      if (!referenced) {
        /* Window is empty (to be cleared) */
        uint64_t windowSize =
          winDef.windows[i]->window_width
          * winDef.windows[i]->window_height
        ;
        planeInitDur += DIV_ROUND_UP(9 * windowSize, 3200);
          /* 90000 * 8 * windowSize / 256000000 */
      }
    }
  }

  /* DECODE_DURATION(DS) */
  dsDecodeDur = planeInitDur;
  odsDecodeDur = 0;
  if (compo.number_of_composition_objects == 2) {
    uint8_t compoObj0WinIdRef = compo.composition_objects[0]->window_id_ref;
    uint16_t compoObj0ObjIdRef = compo.composition_objects[0]->object_id_ref;
    uint8_t compoObj1WinIdRef = compo.composition_objects[1]->window_id_ref;
    uint16_t compoObj1ObjIdRef = compo.composition_objects[1]->object_id_ref;

    /* WAIT OBJ[0] decoding */
    odsDecodeDur += computePgsCompositionObjectDecodeDuration(
      compoObj0ObjIdRef,
      odsSequences
    );
    dsDecodeDur = MAX(dsDecodeDur, odsDecodeDur); /* WAIT */

    if (compoObj0WinIdRef == compoObj1WinIdRef) {
      /* WAIT OBJ[1] decoding */
      odsDecodeDur += computePgsCompositionObjectDecodeDuration(
        compoObj1ObjIdRef,
        odsSequences
      );
      dsDecodeDur = MAX(dsDecodeDur, odsDecodeDur); /* WAIT */

      /* DECODE_DURATION(WIN_0) */
      dsDecodeDur += computePgsWindowDecodeDuration(
        compoObj0WinIdRef,
        winDef
      );
    }
    else {
      /* DECODE_DURATION(WIN_0) */
      dsDecodeDur += computePgsWindowDecodeDuration(
        compoObj0WinIdRef,
        winDef
      );

      /* WAIT OBJ[1] decoding */
      odsDecodeDur += computePgsCompositionObjectDecodeDuration(
        compoObj1ObjIdRef,
        odsSequences
      );
      dsDecodeDur = MAX(dsDecodeDur, odsDecodeDur); /* WAIT */

      /* DECODE_DURATION(WIN_1) */
      dsDecodeDur += computePgsWindowDecodeDuration(
        compoObj1WinIdRef,
        winDef
      );
    }

  }
  else if (compo.number_of_composition_objects == 1) {
    uint8_t compoObj0WinIdRef = compo.composition_objects[0]->window_id_ref;
    uint16_t compoObj0ObjIdRef = compo.composition_objects[0]->object_id_ref;

    /* WAIT OBJ[0] decoding */
    odsDecodeDur += computePgsCompositionObjectDecodeDuration(
      compoObj0ObjIdRef,
      odsSequences
    );
    dsDecodeDur = MAX(dsDecodeDur, odsDecodeDur); /* WAIT */

    /* DECODE_DURATION(WIN_0) */
    dsDecodeDur += computePgsWindowDecodeDuration(
      compoObj0WinIdRef,
      winDef
    );
  }

  return dsDecodeDur;
}

uint64_t computePgsWindowClearingDuration(
  HdmvWDParameters winDef
)
{
  HdmvWindowInfoParameters * win;
  uint8_t i;

  uint64_t duration = 0;

  for (i = 0; i < winDef.number_of_windows; i++) {
    win = winDef.windows[i];
    duration += DIV_ROUND_UP(9 * win->window_width * win->window_height, 3200);
  }

  return duration;
}

int processPgsDisplaySetTimingValues(HdmvSegmentsContextPtr ctx)
{
  HdmvSequencePtr seq;
  uint64_t clockRef, initializationDuration, clearingDuration, decodeDuration;
  unsigned i;

  if (!ctx->curNbPcsSegments)
    LIBBLU_HDMV_PGS_ERROR_RETURN(
      "Missing of Presentation Composition Segment in one epoch.\n"
    );

  LIBBLU_DEBUG_COM(" Calculation of time values:\n");
  clockRef = 0;
  initializationDuration = 0;
  clearingDuration = 0;
  decodeDuration = 0;

  if (ctx->forceRetiming) {
    initializationDuration = computePgsDisplaySetInitializeDuration(
      ctx->compoState,
      ctx->segRefs.pcs->segments->param.data.pcsParam.presentation_composition,
      ctx->segRefs.wds->segments->param.data.wdsParam.window_definition,
      ctx->videoDesc,
      ctx->segRefs.ods
    );

    LIBBLU_DEBUG_COM(
      "  Initialization duration of Display Set: %" PRIu64 ".\n",
      initializationDuration
    );

    if (!initializationDuration)
      LIBBLU_WARNING("No delay epoch founded (empty ?).\n");

    LIBBLU_DEBUG_COM("  Applying time values to segments:\n");

    ctx->referenceStartClock = initializationDuration;

    LIBBLU_DEBUG_COM(
      "   Using initialization duration as ref clock (%" PRIu64 ").\n",
      initializationDuration
    );
  }
  else {
    LIBBLU_DEBUG_COM(
      "  NOTE: Using input file headers time values.\n"
    );
  }

  LIBBLU_DEBUG_COM("   - PCS:\n");
  for (
    seq = ctx->segRefs.pcs, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_PCS);

    if (i == 0)
      clockRef = seq->segments->param.pts; /* Collect Subtitle PTS */

    if (ctx->forceRetiming) {
      seq->dts = clockRef - initializationDuration; /* 0 */
      seq->pts = clockRef;
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_DEBUG_COM(
      "    - PCS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 " "
      "TIME=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts, seq->pts - seq->dts
    );
  }

  LIBBLU_DEBUG_COM("   - WDS:\n");
  for (
    seq = ctx->segRefs.wds, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_WDS);

    if (ctx->forceRetiming) {
      /* Warning, will be false if nbWin > 1 */
      assert(i <= 1);

      clearingDuration = computePgsWindowClearingDuration(
        seq->segments->param.data.wdsParam.window_definition
      );

      assert(clearingDuration <= clockRef);
      clearingDuration = MIN(clearingDuration, clockRef);

      seq->dts = clockRef - initializationDuration; /* 0 */
      seq->pts = clockRef - clearingDuration;
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_DEBUG_COM(
      "    - WDS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );

    if (ctx->forceRetiming) {
      LIBBLU_DEBUG_COM(
        "       => CLEARING_DURATION=%" PRIu64 ".\n",
        clearingDuration
      );
    }
  }

  LIBBLU_DEBUG_COM("   - PDS:\n");
  for (
    seq = ctx->segRefs.pds, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_PDS);

    if (ctx->forceRetiming)
      seq->dts = seq->pts = clockRef - initializationDuration;
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_DEBUG_COM(
      "    - PDS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );
  }

  LIBBLU_DEBUG_COM("   - ODS:\n");
  for (
    seq = ctx->segRefs.ods, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_ODS);

    if (ctx->forceRetiming) {
      decodeDuration = computePgsOdsDecodeDuration(
        seq->data.objectDefinition
      );

      seq->dts = clockRef - initializationDuration;

      clockRef += decodeDuration; /* Adding ODS decoding delay. */

      seq->pts = clockRef - initializationDuration;
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_DEBUG_COM(
      "    - ODS #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );

    if (ctx->forceRetiming) {
      LIBBLU_DEBUG_COM(
        "       => DECODE_DURATION=%" PRIu64 ".\n",
        decodeDuration
      );
    }
  }

  LIBBLU_DEBUG_COM("   - END:\n");
  for (
    seq = ctx->segRefs.end, i = 0;
    NULL != seq;
    seq = seq->nextSequence, i++
  ) {
    /* Debug check for broken lists : */
    assert(seq->type == HDMV_SEGMENT_TYPE_END);

    if (ctx->forceRetiming) {
      seq->dts = seq->pts = clockRef - initializationDuration; /* 0 */
    }
    else {
      assert(NULL != seq->segments);
      seq->dts = seq->segments->param.dts;
      seq->pts = seq->segments->param.pts;
    }

    LIBBLU_DEBUG_COM(
      "    - END #%u: DTS=%016" PRIu64 " PTS=%016" PRIu64 ".\n",
      i, seq->dts, seq->pts
    );
  }

  return 0;
}

int registerPgsDisplaySet(HdmvSegmentsContextPtr ctx)
{
  HdmvSequencePtr seq;
  HdmvSegmentPtr seg;
  uint64_t pts, dts;
  unsigned i;

  const unsigned segTypesNb = 5;
  const HdmvSequencePtr segTypes[] = {
    ctx->segRefs.pcs,
    ctx->segRefs.wds,
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

int processCompletePgsDisplaySet(HdmvSegmentsContextPtr ctx)
{
  assert(NULL != ctx);

  LIBBLU_DEBUG_COM("Processing complete display set...\n");

  LIBBLU_DEBUG_COM(" Current number of segments by type:\n");
  LIBBLU_DEBUG_COM("  - PCS: %u.\n", ctx->curNbPcsSegments);
  LIBBLU_DEBUG_COM("  - WDS: %u.\n", ctx->curNbWdsSegments);
  LIBBLU_DEBUG_COM("  - PDS: %u.\n", ctx->curNbPdsSegments);
  LIBBLU_DEBUG_COM("  - ODS: %u.\n", ctx->curNbOdsSegments);
  LIBBLU_DEBUG_COM("  - END: %u.\n", ctx->curNbEndSegments);

  /* Compute decoding duration: */
  if (processPgsDisplaySetTimingValues(ctx) < 0)
    return -1;

  LIBBLU_DEBUG_COM(" Registering %u segments...\n", ctx->curEpochNbSegments);
  if (registerPgsDisplaySet(ctx) < 0) {
    LIBBLU_DEBUG_COM_NO_HEADER("Failed !\n");
    return -1;
  }
  LIBBLU_DEBUG_COM_NO_HEADER(" Done.\n");

  LIBBLU_DEBUG_COM(" Clearing all buffers... ");
  resetHdmvContext(ctx);
  LIBBLU_DEBUG_COM_NO_HEADER("Done.\n");

  LIBBLU_DEBUG_COM("Processing done, switching to next epoch (or completing parsing).\n");
  return 0; /* OK */
}

int parsePgsSupHeader(BitstreamReaderPtr pgsInput, HdmvSegmentsContextPtr ctx)
{
  uint32_t value;

  assert(NULL != pgsInput);
  assert(NULL != ctx);

  LIBBLU_DEBUG_COM("0x%08" PRIX64 ": SUP Header.\n", tellPos(pgsInput));

  /* [u16 format_identifier] */
  if (readValueBigEndian(pgsInput, 2, &value) < 0)
    return -1;

  if (value != IGS_SUP_WORD)
    LIBBLU_HDMV_PGS_ERROR_RETURN(
      "Unknown SUP magic word, expect 0x%04X.\n",
      IGS_SUP_WORD
    );

  LIBBLU_DEBUG_COM(
    " format_identifier: \"%c%c\" (0x%04x).\n",
    (char) ((value >> 8) & 0xFF), (char) (value & 0xFF),
    value
  );

  /* [d32 pts] */
  if (readValueBigEndian(pgsInput, 4, &value) < 0)
    return -1;
  ctx->curSegProperties.pts = (int32_t) value;

  LIBBLU_DEBUG_COM(
    " pts: %" PRId32 " (0x%08" PRIX32 ").\n",
    ctx->curSegProperties.pts,
    ctx->curSegProperties.pts
  );

  /* [d32 dts] */
  if (readValueBigEndian(pgsInput, 4, &value) < 0)
    return -1;
  ctx->curSegProperties.dts = (int32_t) value;

  LIBBLU_DEBUG_COM(
    " dts: %" PRId32 " (0x%08" PRIX32 ").\n",
    ctx->curSegProperties.dts,
    ctx->curSegProperties.dts
  );

  return 0;
}

bool nextUint8IsPgsSegmentType(BitstreamReaderPtr pgsInput)
{
  uint8_t value = nextUint8(pgsInput);

  return
    value == HDMV_SEGMENT_TYPE_PDS
    || value == HDMV_SEGMENT_TYPE_ODS
    || value == HDMV_SEGMENT_TYPE_PCS
    || value == HDMV_SEGMENT_TYPE_WDS
    || value == HDMV_SEGMENT_TYPE_END
  ;
}

#if 0
int parsePgsSegment(
  BitstreamReaderPtr input,
  HdmvSegmentsContextPtr ctx
)
{
  HdmvSegmentParameters seg;

  assert(NULL != input);
  assert(NULL != ctx);

  /* Segment_descriptor() */
  if (parseHdmvSegmentDescriptor(input, &seg) < 0)
    return -1;

  if (ctx->totalNbSegments == 0) {
    /* Initializing reference start clock with first segment DTS. */
    /* This value is used as offset to allow negative time values (pre-buffering) */

    if (ctx->curSegProperties.dts < 0) {
      ctx->referenceStartClock = ctx->curSegProperties.dts;
    }
    else
      ctx->referenceStartClock = 0 /* ctx->curSegProperties.dts */;

    LIBBLU_DEBUG_COM(
      "  Used reference start clock: %" PRId64 ".\n",
      ctx->referenceStartClock
    );
  }

  if (!ctx->forceRetiming) {
    /* Copying time values. */
    LIBBLU_DEBUG_COM(" Relative segment timing values (parsed from SUP header):\n");

    /* DTS */
    if (ctx->curSegProperties.dts != 0) {
      if (ctx->curSegProperties.dts < ctx->referenceStartClock) {
        seg.dts = 0;
        LIBBLU_WARNING(
          "A segment in the middle of bitstream as a lower DTS value "
          "than initial one (time values will broke).\n"
        );
      }
      else {
        seg.dts = ctx->curSegProperties.dts - ctx->referenceStartClock;
        LIBBLU_DEBUG_COM("  DTS: %" PRIu64 ".\n", seg.dts);
      }
    }
    else {
      seg.dts = 0;
      LIBBLU_DEBUG_COM("  DTS: Not transmited.\n");
    }
  }

  /* PTS */
  if (ctx->curSegProperties.pts < ctx->referenceStartClock) {
    seg.pts = 0;
    LIBBLU_WARNING(
      "A segment in the middle of bitstream as a lower PTS value "
      "than initial one (time values will broke).\n"
    );
  }
  else {
    seg.pts = ctx->curSegProperties.pts - ctx->referenceStartClock;
    LIBBLU_DEBUG_COM("  PTS: %" PRIu64 ".\n", seg.pts);
  }

  ctx->lastClock = seg.pts;

  switch (seg.type) {
    case HDMV_SEGMENT_TYPE_PCS:
      /* Presentation Composition Segment */
      if (decodeHdmvPcsSegment(input, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_WDS:
      /* Window Definition Segment */
      if (decodeHdmvWdsSegment(input, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_PDS:
      /* Palette Definition Segment */
      if (decodeHdmvPdsSegment(input, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_ODS:
      /* Object Definition Segment */
      if (decodeHdmvOdsSegment(input, ctx, seg) < 0)
        return -1;
      break;

    case HDMV_SEGMENT_TYPE_END:
      /* End Segment */
      if (decodeHdmvEndSegment(ctx, seg) < 0)
        return -1;

      LIBBLU_DEBUG_COM(
        "End of epoch of %u segments.\n",
        ctx->curEpochNbSegments
      );

      return processCompletePgsDisplaySet(ctx);

    default:
      LIBBLU_HDMV_PGS_ERROR_RETURN(
        "Unexpected segment type: 0x%" PRIx8 ".\n",
        seg.type
      );
  }

  ctx->curEpochNbSegments++;
  ctx->totalNbSegments++;

  return 0;
}
#endif
#endif

int analyzePgs(
  LibbluESParsingSettings * settings
)
{
#if 0
  EsmsFileHeaderPtr pgsInfos = NULL;
  unsigned pgsSourceFileIdx;

  BitstreamReaderPtr pgsInput = NULL;
  BitstreamWriterPtr essOutput = NULL;

  HdmvSegmentsContextPtr pgsContext = NULL;

  if (NULL == (pgsInfos = createEsmsFileHandler(ES_HDMV, settings->options, FMT_SPEC_INFOS_NONE)))
    goto free_return;

  if (appendSourceFileEsms(pgsInfos, settings->esFilepath, &pgsSourceFileIdx) < 0)
    goto free_return;

  if (NULL == (essOutput = createBitstreamWriter(settings->scriptFilepath, (size_t) WRITE_BUFFER_LEN)))
    goto free_return;

  if (NULL == (pgsContext = createHdmvSegmentsContext(HDMV_STREAM_TYPE_PGS, essOutput, pgsInfos, pgsSourceFileIdx)))
    goto free_return;
  pgsContext->forceRetiming = true;

  if (NULL == (pgsInput = createBitstreamReader(settings->esFilepath, (size_t) READ_BUFFER_LEN)))
    goto free_return;

  /* Used as Place Holder (empty header) : */
  if (writeEsmsHeader(essOutput) < 0)
    goto free_return;
#endif

  HdmvContextPtr ctx;

  if (NULL == (ctx = createHdmvContext(settings, NULL, HDMV_STREAM_TYPE_PGS)))
    return -1;

  while (!isEofHdmvContext(ctx)) {
    /* Progress bar : */
    printFileParsingProgressionBar(inputHdmvContext(ctx));

#if 0
    if (!pgsContext->rawStreamInputMode) {
      if (parsePgsSupHeader(pgsInput, pgsContext) < 0)
        goto free_return;
    }

    if (nextUint8IsPgsSegmentType(pgsInput)) {
      /* Presentation_graphic_stream_segment() */
      if (parsePgsSegment(pgsInput, pgsContext) < 0)
        goto free_return;
      continue;
    }

    LIBBLU_HDMV_PGS_ERROR_RETURN(
      "Unknown header type word: 0x%" PRIX8 " at 0x%" PRIx64 ".\n",
      nextUint8(pgsInput), tellPos(pgsInput)
    );
#endif

    if (parseHdmvSegment(ctx) < 0)
      goto free_return;
  }

  /* Process remaining segments: */
  if (completeDisplaySetHdmvContext(ctx) < 0)
    return -1;

  lbc_printf(" === Parsing finished with success. ===              \n");

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: HDMV/PGS Subtitles format.        \n");
  lbc_printf("Total number of segments by type:        \n");
  printContentHdmvContext(ctx);
  lbc_printf("=======================================================================================\n");

  if (closeHdmvContext(ctx) < 0)
    goto free_return;
  destroyHdmvContext(ctx);
  return 0;

#if 0
  closeBitstreamReader(pgsInput);
  pgsInput = NULL;

  /* [u8 endMarker] */
  if (writeByte(essOutput, ESMS_SCRIPT_END_MARKER) < 0)
    goto free_return;

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: HDMV/PGS Subtitles format.             \n");
  lbc_printf("Total number of segments by type:        \n");
  lbc_printf(" - PCS: %u.\n", pgsContext->nbPcsSegments   );
  lbc_printf(" - WDS: %u.\n", pgsContext->nbWdsSegments   );
  lbc_printf(" - PDS: %u.\n", pgsContext->nbPdsSegments   );
  lbc_printf(" - ODS: %u.\n", pgsContext->nbOdsSegments   );
  lbc_printf(" - END: %u.\n", pgsContext->nbEndSegments   );
  lbc_printf(" TOTAL: %u.\n", pgsContext->totalNbSegments );
  lbc_printf("=======================================================================================\n");

  pgsInfos->ptsRef = ABS(pgsContext->referenceStartClock) * 300;
  pgsInfos->bitrate = HDMV_RX_BITRATE;
  pgsInfos->endPts = pgsContext->lastClock * 300;
  pgsInfos->prop.codingType = STREAM_CODING_TYPE_PG;

  destroyHdmvContext(pgsContext);
  pgsContext = NULL;

  if (addEsmsFileEnd(essOutput, pgsInfos) < 0)
    goto free_return;
  closeBitstreamWriter(essOutput);
  essOutput = NULL;

  if (updateEsmsHeader(settings->scriptFilepath, pgsInfos) < 0)
    goto free_return;

  destroyEsmsFileHandler(pgsInfos);
#endif

  destroyHdmvContext(ctx);
  return 0;

free_return:
  destroyHdmvContext(ctx);

  return -1;
}