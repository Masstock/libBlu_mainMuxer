#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "hdmv_parser.h"

static void _initReferenceClockFromHdmvHeader(
  HdmvContextPtr ctx,
  int32_t firstSegmentDtsValue
)
{
  int64_t first_DTS = firstSegmentDtsValue - ctx->param.initialDelay;

  /* Update reference timecode */
  int64_t reference = ctx->param.referenceTimecode;
  if (first_DTS < 0)
    reference += 0 - first_DTS;

  LIBBLU_HDMV_COM_DEBUG(
    "  Used reference start clock: %" PRId64 ".\n",
    reference
  );

  setReferenceTimecodeHdmvContext(ctx, reference);
}

static int _parseHdmvHeader(
  HdmvContextPtr ctx,
  HdmvSegmentParameters * param
)
{
  int64_t headerOffset;
  uint32_t value;

  headerOffset = tellPos(inputHdmvContext(ctx));

  LIBBLU_HDMV_PARSER_DEBUG(
    "0x%08" PRIX64 ": %s Header.\n",
    headerOffset,
    HdmvStreamTypeStr(ctx->type)
  );

  /* [u16 format_identifier] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;

  LIBBLU_HDMV_PARSER_DEBUG(
    " format_identifier: \"%c%c\" (0x%04" PRIX16 ").\n",
    (char) (value >> 8),
    (char) (value),
    value
  );

  if (!checkFormatIdentifierHdmvStreamType(value, ctx->type))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unexpected %s magic word, expect 0x%02X, "
      "got 0x%02X at offset 0x%08" PRIX64 ".\n",
      HdmvStreamTypeStr(ctx->type),
      expectedFormatIdentifierHdmvStreamType(ctx->type),
      value,
      headerOffset
    );

  /* [d32 pts] */
  if (readValueHdmvContext(ctx, 4, &value) < 0)
    return -1;
  param->header.pts = (int32_t) value;

  LIBBLU_HDMV_PARSER_DEBUG(
    " pts: %" PRId32 " (0x%08" PRIX32 ").\n",
    param->header.pts,
    param->header.pts
  );

  /* [d32 dts] */
  if (readValueHdmvContext(ctx, 4, &value) < 0)
    return -1;
  param->header.dts = (int32_t) value;

  LIBBLU_HDMV_PARSER_DEBUG(
    " dts: %" PRId32 " (0x%08" PRIX32 ").\n",
    param->header.dts,
    param->header.dts
  );

  if (
    !getTotalHdmvContextSegmentTypesCounter(ctx->globalCounters.nbSegments)
    && !ctx->param.forceRetiming
  ) {
    /**
     * Initializing reference start clock with first segment DTS.
     * This value is used as offset to allow negative time
     * values (pre-buffering).
     */
    _initReferenceClockFromHdmvHeader(
      ctx,
      param->header.dts
    );
  }

  return 0;
}

static int _parseHdmvSegmentDescriptor(
  HdmvContextPtr ctx,
  HdmvSegmentParameters * param
)
{
  uint32_t value;

  param->inputFileOffset = tellPos(inputHdmvContext(ctx));

  /* [u8 segment_type] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->type = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "0x%08" PRIX64 ": %s.\n",
    param->inputFileOffset,
    HdmvSegmentTypeStr(param->type)
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    " Segment_descriptor():\n"
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  segment_type: %s (0x%02" PRIX8 ").\n",
    HdmvSegmentTypeStr(param->type),
    param->type
  );

  /* [u16 segment_length] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  segment_length: %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
    param->length,
    param->length
  );

  return 0;
}

/* ### Palette Definition Segments : ####################################### */

static int _parseHdmvPaletteDescriptor(
  HdmvContextPtr ctx,
  HdmvPDParameters * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(" Palette_descriptor():\n");

  /* [u8 palette_id] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->palette_id = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_id: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id,
    param->palette_id
  );

  /* [u8 palette_version_number] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->palette_version_number = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_version_number: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_version_number,
    param->palette_version_number
  );

  return 0;
}

static int _parseHdmvPaletteEntry(
  HdmvContextPtr ctx,
  HdmvPaletteEntryParameters entries[static HDMV_MAX_NB_PDS_ENTRIES],
  unsigned i
)
{
  uint32_t value;

  /* [u8 palette_entry_id] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  uint8_t palette_entry_id = value & 0xFF;

  HdmvPaletteEntryParameters * entry = &entries[palette_entry_id];

  /* [u8 Y_value] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  entry->y_value = value;

  /* [u8 Cr_value] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  entry->cr_value = value;

  /* [u8 Cb_value] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  entry->cb_value = value;

  /* [u8 T_value] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  entry->t_value = value;

  entry->updated = true;

#if 0
  LIBBLU_HDMV_PAL_DEBUG(
    "   Palette_entry(%u): palette_entry_id=%03u (0x%02X), "
    "Y=%03u (0x%02X), Cr=%03u (0x%02X), Cb=%03u (0x%02X), T=%03u (0x%02X);\n",
    i,
    palette_entry_id, palette_entry_id,
    entry->y_value  , entry->y_value,
    entry->cr_value , entry->cr_value,
    entry->cb_value , entry->cb_value,
    entry->t_value  , entry->t_value
  );
#else
  (void) i;
#endif

  return 0;
}

static int _computeNumberOfPaletteEntries(
  unsigned * dst,
  size_t segmentSize
)
{
  size_t palSize;
  unsigned nbEntries;

  if (segmentSize < HDMV_SIZE_PALETTE_DESCRIPTOR)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected PDS size (smaller than 2 bytes).\n"
    );

  palSize = segmentSize - HDMV_SIZE_PALETTE_DESCRIPTOR;
  if (0 != (palSize % HDMV_SIZE_PALETTE_DEFINITION_ENTRY))
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected PDS size (not a multiple of entries size).\n"
    );

  nbEntries = palSize / HDMV_SIZE_PALETTE_DEFINITION_ENTRY;
  if (HDMV_MAX_NB_PDS_ENTRIES < nbEntries)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected PDS size (max number of entries exceeded: %u/%u).\n",
      nbEntries,
      HDMV_MAX_NB_PDS_ENTRIES
    );

  *dst = nbEntries;
  return 0;
}

static int _parseHdmvPdsSegment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvPdsSegmentParameters param;

  /* Palette_descriptor() */
  if (_parseHdmvPaletteDescriptor(ctx, &param.palette_descriptor) < 0)
    return -1;

  if (_computeNumberOfPaletteEntries(&param.number_of_palette_entries, segment.length) < 0)
    goto free_return;

  if (0 < param.number_of_palette_entries) {
    LIBBLU_HDMV_PAL_DEBUG("  Palette_entries():\n");

#if 1
    LIBBLU_HDMV_PAL_DEBUG(
      "   => %u entrie(s).\n",
      param.number_of_palette_entries
    );
#endif

    memset(param.palette_entries, 0, HDMV_MAX_NB_PDS_ENTRIES * sizeof(HdmvPaletteEntryParameters));
    for (unsigned i = 0; i < param.number_of_palette_entries; i++) {
      if (_parseHdmvPaletteEntry(ctx, param.palette_entries, i) < 0)
        goto free_return;
    }
  }

  /* Add parsed segment as a sequence in PDS sequences list. */
  HdmvSequencePtr sequence = addSegToSeqDisplaySetHdmvContext(
    ctx,
    HDMV_SEGMENT_TYPE_PDS_IDX,
    segment,
    HDMV_SEQUENCE_LOC_UNIQUE
  );
  if (NULL == sequence)
    goto free_return;
  sequence->data.pd = param;

  if (completeSeqDisplaySetHdmvContext(ctx, HDMV_SEGMENT_TYPE_PDS_IDX) < 0)
    goto free_return;

  /* Increment counters of PDS segments/sequences */
  incrementSegmentsNbHdmvContext(ctx, HDMV_SEGMENT_TYPE_PDS_IDX);

  return 0;

free_return:
  LIBBLU_HDMV_PARSER_ERROR_RETURN(
    " Error in PDS with 'palette_id' == %u ('composition_number' == %u).\n",
    param.palette_descriptor.palette_id,
    ctx->lastParsedCompositionNumber
  );
}

/* ### Object Definition Segments : ######################################## */

static int _parseHdmvObjectDescriptor(
  HdmvContextPtr ctx,
  HdmvODescParameters * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Object_descriptor():\n"
  );

  /* [u16 object_id] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->object_id = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_id: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->object_id,
    param->object_id
  );

  /* [u8 object_version_number] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->object_version_number = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_version_number: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->object_version_number,
    param->object_version_number
  );

  return 0;
}

static int _parseHdmvSequenceDescriptor(
  HdmvContextPtr ctx,
  HdmvSDParameters * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Sequence_descriptor():\n"
  );

  /* [b1 first_in_sequence] [b1 last_in_sequence] [v6 reserved] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->first_in_sequence = (value & 0x80);
  param->last_in_sequence  = (value & 0x40);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  first_in_sequence: %s.\n",
    BOOL_STR(param->first_in_sequence)
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  last_in_sequence: %s.\n",
    BOOL_STR(param->last_in_sequence)
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%02X.\n",
    value & 0x3F
  );

  return 0;
}

static int _computeSizeHdmvObjectDataFragment(
  size_t * dst,
  size_t segmentSize
)
{
  if (segmentSize < HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected ODS size (smaller than 4 bytes).\n"
    );

  *dst = segmentSize - HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER;
  return 0;
}

static int _parseHdmvObjectDataFragment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment,
  HdmvSequencePtr sequence
)
{
  size_t size;

  LIBBLU_HDMV_PARSER_DEBUG(" Object_data_fragment():\n");

  if (_computeSizeHdmvObjectDataFragment(&size, segment.length) < 0)
    return -1;

  if (parseFragmentHdmvSequence(sequence, inputHdmvContext(ctx), size) < 0)
    return -1;

  return 0;
}

static int _decodeHdmvObjectData(
  HdmvSequencePtr sequence,
  HdmvODescParameters object_descriptor
)
{
  HdmvODParameters * param = &sequence->data.od;
  uint32_t value;

  param->object_descriptor = object_descriptor;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Object_data(): /* Recomposed from object_data_fragment() */\n"
  );

  /* [u24 object_data_length] */
  if (readValueFromHdmvSequence(sequence, 3, &value) < 0)
    return -1;
  param->object_data_length = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_data_length: %" PRIu32 " bytes (0x%06" PRIX32 ").\n",
    param->object_data_length,
    param->object_data_length
  );

  /* [u16 object_width] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->object_width = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->object_width,
    param->object_width
  );

  /* [u16 object_height] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->object_height = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->object_height,
    param->object_height
  );

  return 0;
}

static int _parseHdmvOdsSegment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvOdsSegmentParameters param;
  HdmvSequenceLocation location;
  HdmvSequencePtr sequence;

  /* Object_descriptor() */
  if (_parseHdmvObjectDescriptor(ctx, &param.object_descriptor) < 0)
    return -1;

  /* Sequence_descriptor() */
  if (_parseHdmvSequenceDescriptor(ctx, &param.sequence_descriptor) < 0)
    return -1;
  location = getHdmvSequenceLocation(param.sequence_descriptor);

  /* Add parsed segment as a sequence in ODS sequences list. */
  sequence = addSegToSeqDisplaySetHdmvContext(
    ctx,
    HDMV_SEGMENT_TYPE_ODS_IDX,
    segment,
    location
  );
  if (NULL == sequence)
    goto free_return;

  /* Object_data_fragment() */
  if (_parseHdmvObjectDataFragment(ctx, segment, sequence) < 0)
    goto free_return;

  if (isLastSegmentHdmvSequenceLocation(location)) {
    /* Decode complete object data. */

    /* Object_data() */
    if (_decodeHdmvObjectData(sequence, param.object_descriptor) < 0)
      goto free_return;

    /* Mark sequence as decoded/completed. */
    if (completeSeqDisplaySetHdmvContext(ctx, HDMV_SEGMENT_TYPE_ODS_IDX) < 0)
      goto free_return;
  }

  /* Increment counters of ODS segments */
  incrementSegmentsNbHdmvContext(ctx, HDMV_SEGMENT_TYPE_ODS_IDX);

  return 0;

free_return:
  LIBBLU_HDMV_PARSER_ERROR_RETURN(
    " Error in ODS with 'object_id' == %u ('composition_number' == %u).\n",
    param.object_descriptor.object_id,
    ctx->lastParsedCompositionNumber
  );
}

/* ### Presentation Composition Segments : ################################# */

static int _parseHdmvVideoDescriptor(
  HdmvContextPtr ctx,
  HdmvVDParameters * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Video_descriptor():\n"
  );

  /* [u16 video_width] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->video_width = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  video_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->video_width,
    param->video_width
  );

  /* [u16 video_height] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->video_height = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  video_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->video_height,
    param->video_height
  );

  /* [u4 frame_rate_id] [v4 reserved] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->frame_rate = (value >> 4) & 0xF;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  frame_rate_id: 0x%" PRIX8 " (%.3f).\n",
    param->frame_rate,
    frameRateCodeToFloat(param->frame_rate)
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%" PRIX8 ".\n",
    value & 0xF
  );

  return 0;
}

static int _parseHdmvCompositionDescriptor(
  HdmvContextPtr ctx,
  HdmvCDParameters * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Composition_descriptor():\n"
  );

  /* [u16 composition_number] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->composition_number = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  composition_number: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->composition_number,
    param->composition_number
  );

  /* [u2 composition_state] [v6 reserved] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->composition_state = value >> 6;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  composition_state: %s (0x%X).\n",
    HdmvCompositionStateStr(param->composition_state),
    param->composition_state
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%02X.\n",
    value & 0x3F
  );

  ctx->lastParsedCompositionNumber = param->composition_number;

  return 0;
}

static int _parseHdmvCompositionObject(
  HdmvContextPtr ctx,
  HdmvCompositionObjectParameters * param
)
{
  uint32_t value;

  /* [u16 object_id_ref] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->object_id_ref,
    param->object_id_ref
  );

  /* [u8 window_id_ref] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->window_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id_ref,
    param->window_id_ref
  );

  /* [b1 object_cropped_flag] [b1 forced_on_flag] [v6 reserved] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->object_cropped_flag = value & 0x80;
  param->forced_on_flag      = value & 0x40;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    object_cropped_flag: %s (0b%x).\n",
    BOOL_STR(param->object_cropped_flag),
    param->object_cropped_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "    forced_on_flag: %s (0b%x).\n",
    BOOL_STR(param->forced_on_flag),
    param->forced_on_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "    reserved: 0x%02X.\n",
    value & 0x3F
  );

  /* [u16 object_horizontal_position] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->composition_object_horizontal_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    object_horizontal_position: %" PRIu16 " px (0x%02X).\n",
    param->composition_object_horizontal_position,
    param->composition_object_horizontal_position
  );

  /* [u16 object_vertical_position] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->composition_object_vertical_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    object_vertical_position: %" PRIu16 " px (0x%02X).\n",
    param->composition_object_vertical_position,
    param->composition_object_vertical_position
  );

  if (param->object_cropped_flag) {
    LIBBLU_HDMV_PARSER_DEBUG(
      "    if (object_cropped_flag == 0b1):\n"
    );

    /* [u16 object_cropping_horizontal_position] */
    if (readValueHdmvContext(ctx, 2, &value) < 0)
      return -1;
    param->object_cropping.horizontal_position = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_horizontal_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.horizontal_position,
      param->object_cropping.horizontal_position
    );

    /* [u16 object_cropping_vertical_position] */
    if (readValueHdmvContext(ctx, 2, &value) < 0)
      return -1;
    param->object_cropping.vertical_position = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.vertical_position,
      param->object_cropping.vertical_position
    );

    /* [u16 object_cropping_width] */
    if (readValueHdmvContext(ctx, 2, &value) < 0)
      return -1;
    param->object_cropping.width = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.width,
      param->object_cropping.width
    );

    /* [u16 object_cropping_height] */
    if (readValueHdmvContext(ctx, 2, &value) < 0)
      return -1;
    param->object_cropping.height = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.height,
      param->object_cropping.height
    );
  }

  return 0;
}

static int _parseHdmvPresentationComposition(
  HdmvContextPtr ctx,
  HdmvPCParameters * param
)
{
  uint32_t value;
  unsigned i;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Presentation_composition():\n"
  );

  /* [b1 palette_update_flag] [v7 reserved] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->palette_update_flag = (value >> 7);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_update_flag: %s (0b%x).\n",
    BOOL_STR(param->palette_update_flag),
    param->palette_update_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%02X.\n",
    value & 0x7F
  );

  /* [u8 palette_id_ref] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->palette_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_composition_objects] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->number_of_composition_objects = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  number_of_composition_objects: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->number_of_composition_objects,
    param->number_of_composition_objects
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  Composition_objects():\n"
  );
  if (!param->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_DEBUG("   *none*\n");

  for (i = 0; i < param->number_of_composition_objects; i++) {
    HdmvCompositionObjectParameters * obj;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   Composition_object[%u]():\n",
      i
    );

    if (NULL == (obj = getHdmvCompoObjParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* Composition_objects() */
    if (_parseHdmvCompositionObject(ctx, obj) < 0)
      return -1;

    param->composition_objects[i] = obj;
  }

  return 0;
}

static int _parseHdmvPcsSegment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvPcsSegmentParameters param;
  HdmvSequencePtr sequence;

  /* Video_descriptor() */
  if (_parseHdmvVideoDescriptor(ctx, &param.video_descriptor) < 0)
    return -1;

  /* Composition_descriptor() */
  if (_parseHdmvCompositionDescriptor(ctx, &param.composition_descriptor) < 0)
    return -1;

  // TODO: Is Epoch Continue shall avoid initialization of a new DS?
  if (initEpochHdmvContext(ctx, param.composition_descriptor, param.video_descriptor) < 0)
    goto free_return;

  /* Presentation_composition() */
  if (_parseHdmvPresentationComposition(ctx, &param.presentation_composition) < 0)
    goto free_return;

  /* Add parsed segment as a sequence in PCS sequences list. */
  sequence = addSegToSeqDisplaySetHdmvContext(
    ctx,
    HDMV_SEGMENT_TYPE_PCS_IDX,
    segment,
    HDMV_SEQUENCE_LOC_UNIQUE
  );
  if (NULL == sequence)
    goto free_return;
  sequence->data.pc = param.presentation_composition;

  if (completeSeqDisplaySetHdmvContext(ctx, HDMV_SEGMENT_TYPE_PCS_IDX) < 0)
    goto free_return;

  /* Increment counters of PCS segments */
  incrementSegmentsNbHdmvContext(ctx, HDMV_SEGMENT_TYPE_PCS_IDX);

  return 0;

free_return:
  LIBBLU_HDMV_PARSER_ERROR_RETURN(
    " Error in PCS with 'composition_number' == %u.\n",
    param.composition_descriptor.composition_number
  );
}

/* ### Window Definition Segments : ######################################## */

static int _parseHdmvWindowInfo(
  HdmvContextPtr ctx,
  HdmvWindowInfoParameters * param
)
{
  uint32_t value;

  /* [u8 window_id] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->window_id = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_id: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id,
    param->window_id
  );

  /* [u16 window_horizontal_position] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->window_horizontal_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_horizontal_position: %u px (0x%04" PRIX16 ").\n",
    param->window_horizontal_position,
    param->window_horizontal_position
  );

  /* [u16 window_vertical_position] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->window_vertical_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_vertical_position,
    param->window_vertical_position
  );

  /* [u16 window_width] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->window_width = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_width,
    param->window_width
  );

  /* [u16 window_height] */
  if (readValueHdmvContext(ctx, 2, &value) < 0)
    return -1;
  param->window_height = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_height,
    param->window_height
  );

  return 0;
}

static int _parseHdmvWindowsDefinition(
  HdmvContextPtr ctx,
  HdmvWDParameters * param
)
{
  uint32_t value;
  unsigned i;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Window_definition():\n"
  );

  /* [u8 number_of_windows] */
  if (readValueHdmvContext(ctx, 1, &value) < 0)
    return -1;
  param->number_of_windows = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  number_of_windows: %u (0x%02X).\n",
    param->number_of_windows,
    param->number_of_windows
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  Windows():\n"
  );
  if (!param->number_of_windows)
    LIBBLU_HDMV_PARSER_DEBUG("   *none*\n");

  for (i = 0; i < param->number_of_windows; i++) {
    HdmvWindowInfoParameters * win;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   Window[%u]():\n",
      i
    );

    if (NULL == (win = getHdmvWinInfoParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* Window_info() */
    if (_parseHdmvWindowInfo(ctx, win) < 0)
      return -1;

    param->windows[i] = win;
  }

  return 0;
}

static int _parseHdmvWdsSegment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvWDParameters param;
  HdmvSequencePtr sequence;

  /* Window_definition() */
  if (_parseHdmvWindowsDefinition(ctx, &param) < 0)
    goto free_return;

  /* Add parsed segment as a sequence in WDS sequences list. */
  sequence = addSegToSeqDisplaySetHdmvContext(
    ctx,
    HDMV_SEGMENT_TYPE_WDS_IDX,
    segment,
    HDMV_SEQUENCE_LOC_UNIQUE
  );
  if (NULL == sequence)
    goto free_return;
  sequence->data.wd = param;

  if (completeSeqDisplaySetHdmvContext(ctx, HDMV_SEGMENT_TYPE_WDS_IDX) < 0)
    goto free_return;

  /* Increment counters of WDS segments */
  incrementSegmentsNbHdmvContext(ctx, HDMV_SEGMENT_TYPE_WDS_IDX);

  return 0;

free_return:
  LIBBLU_HDMV_PARSER_ERROR_RETURN(
    " Error in WDS ('composition_number' == %u).\n",
    ctx->lastParsedCompositionNumber
  );
}

/* ### Interactive Composition Segments : ################################## */

static int _computeSizeHdmvInteractiveCompositionDataFragment(
  size_t * dst,
  size_t segmentSize
)
{
  if (segmentSize < HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected ICS size (smaller than 9 bytes).\n"
    );

  *dst = segmentSize - HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER;
  return 0;
}

static int _parseHdmvInteractiveCompositionDataFragment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment,
  HdmvSequencePtr sequence
)
{
  size_t size;

  LIBBLU_HDMV_PARSER_DEBUG(" Interactive_composition_data_fragment():\n");

  if (_computeSizeHdmvInteractiveCompositionDataFragment(&size, segment.length) < 0)
    return -1;

  if (parseFragmentHdmvSequence(sequence, inputHdmvContext(ctx), size) < 0)
    return -1;

  return 0;
}

static int _decodeHdmvWindowInfo(
  HdmvSequencePtr sequence,
  HdmvWindowInfoParameters * param
)
{
  uint32_t value;

  /* [u8 window_id] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->window_id = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_id: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id,
    param->window_id
  );

  /* [u16 window_horizontal_position] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->window_horizontal_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_horizontal_position: %u px (0x%04" PRIX16 ").\n",
    param->window_horizontal_position,
    param->window_horizontal_position
  );

  /* [u16 window_vertical_position] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->window_vertical_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_vertical_position,
    param->window_vertical_position
  );

  /* [u16 window_width] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->window_width = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_width,
    param->window_width
  );

  /* [u16 window_height] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->window_height = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_height,
    param->window_height
  );

  return 0;
}

static int _decodeHdmvCompositionObject(
  HdmvSequencePtr sequence,
  HdmvCompositionObjectParameters * param
)
{
  uint32_t value;

  /* [u16 object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->object_id_ref,
    param->object_id_ref
  );

  /* [u8 window_id_ref] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->window_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          window_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id_ref,
    param->window_id_ref
  );

  /* [b1 object_cropped_flag] [v7 reserved] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->object_cropped_flag = (value & 0x80);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_cropped_flag: %s (0b%x).\n",
    BOOL_STR(param->object_cropped_flag),
    param->object_cropped_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "          reserved: 0x%02X.\n",
    value & 0x7F
  );

  /* [u16 object_horizontal_position] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->composition_object_horizontal_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_horizontal_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->composition_object_horizontal_position,
    param->composition_object_horizontal_position
  );

  /* [u16 object_vertical_position] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->composition_object_vertical_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->composition_object_vertical_position,
    param->composition_object_vertical_position
  );

  if (param->object_cropped_flag) {
    LIBBLU_HDMV_PARSER_DEBUG(
      "          if (object_cropped_flag == 0b1):\n"
    );

    /* [u16 object_cropping_horizontal_position] */
    if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
      return -1;
    param->object_cropping.horizontal_position = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_horizontal_position: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.horizontal_position,
      param->object_cropping.horizontal_position
    );

    /* [u16 object_cropping_vertical_position] */
    if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
      return -1;
    param->object_cropping.vertical_position = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_vertical_position: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.vertical_position,
      param->object_cropping.vertical_position
    );

    /* [u16 object_cropping_width] */
    if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
      return -1;
    param->object_cropping.width = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_width: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.width,
      param->object_cropping.width
    );

    /* [u16 object_cropping_height] */
    if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
      return -1;
    param->object_cropping.height = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_height: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.height,
      param->object_cropping.height
    );
  }

  return 0;
}

static int _decodeHdmvEffectInfo(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence,
  HdmvEffectInfoParameters * param
)
{
  uint32_t value;
  unsigned i;

  /* [u24 effect_duration] */
  if (readValueFromHdmvSequence(sequence, 3, &value) < 0)
    return -1;
  param->effect_duration = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        effect_duration: %" PRIu32 " (0x%06" PRIX32 ").\n",
    param->effect_duration,
    param->effect_duration
  );

  /* [u8 palette_id_ref] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->palette_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        palette_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_composition_objects] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->number_of_composition_objects = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        number_of_composition_objects: "
    "%" PRIu8 " objects (0x%02" PRIX8 ").\n",
    param->number_of_composition_objects,
    param->number_of_composition_objects
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "        Composition objects:\n"
  );

  if (!param->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_DEBUG(
      "         *none*\n"
    );

  for (i = 0; i < param->number_of_composition_objects; i++) {
    HdmvCompositionObjectParameters * obj;

    LIBBLU_HDMV_PARSER_DEBUG(
      "         Composition_object[%u]()\n",
      i
    );

    if (NULL == (obj = getHdmvCompoObjParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    if (_decodeHdmvCompositionObject(sequence, obj) < 0)
      return -1;

    param->composition_objects[i] = obj;
  }

  return 0;
}

static int _decodeHdmvEffectSequence(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence,
  HdmvEffectSequenceParameters * param
)
{
  uint32_t value;
  unsigned i;

  LIBBLU_HDMV_PARSER_DEBUG(
    "     Effect_sequence():\n"
  );

  /* [u8 number_of_windows] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->number_of_windows = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "      number_of_windows: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->number_of_windows,
    param->number_of_windows
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "      Windows():\n"
  );
  if (!param->number_of_windows)
    LIBBLU_HDMV_PARSER_DEBUG("       *none*\n");

  assert(param->number_of_windows <= HDMV_MAX_NB_ICS_WINDOWS);
    /* Shall always be the case. */

  for (i = 0; i < param->number_of_windows; i++) {
    HdmvWindowInfoParameters * win;

    LIBBLU_HDMV_PARSER_DEBUG(
      "       Window_info[%u]():\n",
      i
    );

    if (NULL == (win = getHdmvWinInfoParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* Window_info() */
    if (_decodeHdmvWindowInfo(sequence, win) < 0)
      return -1;

    param->windows[i] = win;
  }

  /* [u8 number_of_effects] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->number_of_effects = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "      number_of_effects: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->number_of_effects,
    param->number_of_effects
  );

  LIBBLU_HDMV_PARSER_DEBUG("      Effects():\n");
  if (!param->number_of_effects)
    LIBBLU_HDMV_PARSER_DEBUG("       *none*\n");

  assert(param->number_of_effects <= HDMV_MAX_NB_ICS_EFFECTS);
    /* Shall always be the case. */

  for (i = 0; i < param->number_of_effects; i++) {
    HdmvEffectInfoParameters * eff;

    LIBBLU_HDMV_PARSER_DEBUG(
      "       Effect_info[%u]():\n",
      i
    );

    if (NULL == (eff = getHdmvEffInfoParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* Effect_info() */
    if (_decodeHdmvEffectInfo(ctx, sequence, eff) < 0)
      return -1;

    param->effects[i] = eff;
  }

  return 0;
}

static int _decodeHdmvButtonNeighborInfo(
  HdmvSequencePtr sequence,
  HdmvButtonNeighborInfoParam * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          Neighbor_info():\n"
  );

  /* [u16 upper_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->upper_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           upper_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->upper_button_id_ref,
    param->upper_button_id_ref
  );

  /* [u16 lower_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->lower_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           lower_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->lower_button_id_ref,
    param->lower_button_id_ref
  );

  /* [u16 left_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->left_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           left_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->left_button_id_ref,
    param->left_button_id_ref
  );

  /* [u16 right_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->right_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           right_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->right_button_id_ref,
    param->right_button_id_ref
  );

  return 0;
}

static int _decodeHdmvButtonNormalStateInfo(
  HdmvSequencePtr sequence,
  HdmvButtonNormalStateInfoParam * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          Normal_state_info():\n"
  );

  /* [u16 normal_start_object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->start_object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           normal_start_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->start_object_id_ref,
    param->start_object_id_ref
  );

  /* [u16 normal_end_object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->end_object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           normal_end_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->end_object_id_ref,
    param->end_object_id_ref
  );

  /* [b1 normal_repeat_flag] [b1 normal_complete_flag] [v6 reserved] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->repeat_flag   = (value & 0x80);
  param->complete_flag = (value & 0x40);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           normal_repeat_flag: %s (0b%x).\n",
    BOOL_STR(param->repeat_flag),
    param->repeat_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "           normal_complete_flag: %s (0b%x).\n",
    BOOL_STR(param->complete_flag),
    param->complete_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "           reserved: 0x%02X.\n",
    value & 0x3F
  );

  return 0;
}

static int _decodeHdmvButtonSelectedStateInfo(
  HdmvSequencePtr sequence,
  HdmvButtonSelectedStateInfoParam * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          Selected_state_info():\n"
  );

  /* [u8 selected_state_sound_id_ref] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->state_sound_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_state_sound_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->state_sound_id_ref,
    param->state_sound_id_ref
  );

  /* [u16 selected_start_object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->start_object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_start_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->start_object_id_ref,
    param->start_object_id_ref
  );

  /* [u16 selected_end_object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->end_object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_end_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->end_object_id_ref,
    param->end_object_id_ref
  );

  /* [b1 selected_repeat_flag] [b1 selected_complete_flag] [v6 reserved] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->repeat_flag   = (value & 0x80);
  param->complete_flag = (value & 0x40);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_repeat_flag: %s (0b%x).\n",
    BOOL_STR(param->repeat_flag),
    param->repeat_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_complete_flag: %s (0b%x).\n",
    BOOL_STR(param->complete_flag),
    param->complete_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "           reserved: 0x%02X.\n",
    value & 0x3F
  );

  return 0;
}

static int _decodeHdmvButtonActivatedStateInfo(
  HdmvSequencePtr sequence,
  HdmvButtonActivatedStateInfoParam * param
)
{
  uint32_t value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          Activated_state_info():\n"
  );

  /* [u8 activated_state_sound_id_ref] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->state_sound_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           activated_state_sound_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->state_sound_id_ref,
    param->state_sound_id_ref
  );

  /* [u16 activated_start_object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->start_object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           activated_start_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->start_object_id_ref,
    param->start_object_id_ref
  );

  /* [u16 activated_end_object_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->end_object_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "           activated_end_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->end_object_id_ref,
    param->end_object_id_ref
  );

  return 0;
}

static int _decodeHdmvButtonCommands(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence,
  HdmvButtonParam * button
)
{
  uint32_t value;
  unsigned i;

  HdmvNavigationCommand * last;

  assert(NULL == button->navigation_commands);
    /* Destination list shall be clear */

  last = NULL;
  for (i = 0; i < button->number_of_navigation_commands; i++) {
    HdmvNavigationCommand * com;

    if (NULL == (com = getHdmvNaviComHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* [u32 opcode] */
    if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
      return -1;
    com->opcode = value;

    /* [u32 destination] */
    if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
      return -1;
    com->destination = value;

    /* [u32 source] */
    if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
      return -1;
    com->source = value;

    LIBBLU_HDMV_PARSER_DEBUG(
      "           opcode=0x%08" PRIX32 ", "
      "destination=0x%08" PRIX32 ", "
      "source=0x%08" PRIX32 ";\n",
      com->opcode,
      com->destination,
      com->source
    );

    if (NULL != last)
      setNextHdmvNavigationCommand(last, com);
    else
      button->navigation_commands = com;
    last = com;
  }

  return 0;
}

static int _decodeHdmvButton(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence,
  HdmvButtonParam * param
)
{
  uint32_t value;

  /* [u16 button_id] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->button_id = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_id: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->button_id,
    param->button_id
  );

  /* [u16 button_numeric_select_value] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->button_numeric_select_value = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_numeric_select_value: "
    "%" PRIu16 " (0x%04" PRIX16 ").\n",
    param->button_numeric_select_value,
    param->button_numeric_select_value
  );

  if (param->button_numeric_select_value == 0xFFFF)
    LIBBLU_HDMV_PARSER_DEBUG("           -> Not used.\n");

  /* [b1 auto_action_flag] [v7 reserved] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->auto_action = (value & 0x80);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          auto_action_flag: %s (0x%x).\n",
    BOOL_STR(param->auto_action),
    param->auto_action
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "          reserved: 0x%02X.\n",
    value & 0x7F
  );

  /* [u16 button_horizontal_position] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->button_horizontal_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_horizontal_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->button_horizontal_position,
    param->button_horizontal_position
  );

  /* [u16 button_vertical_position] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->button_vertical_position = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->button_vertical_position,
    param->button_vertical_position
  );

  /* neighbor_info() */
  if (_decodeHdmvButtonNeighborInfo(sequence, &param->neighbor_info) < 0)
    return -1;

  /* normal_state_info() */
  if (_decodeHdmvButtonNormalStateInfo(sequence, &param->normal_state_info) < 0)
    return -1;

  /* selected_state_info() */
  if (_decodeHdmvButtonSelectedStateInfo(sequence, &param->selected_state_info) < 0)
    return -1;

  /* activated_state_info() */
  if (_decodeHdmvButtonActivatedStateInfo(sequence, &param->activated_state_info) < 0)
    return -1;

  /* [u16 number_of_navigation_commands] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->number_of_navigation_commands = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "          number_of_navigation_commands: %u (0x%04" PRIX16 ").\n",
    param->number_of_navigation_commands,
    param->number_of_navigation_commands
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "          Navigation_commands():\n"
  );

  if (!param->number_of_navigation_commands)
    LIBBLU_HDMV_PARSER_DEBUG("           *none*\n");

  /* Commands() */
  if (_decodeHdmvButtonCommands(ctx, sequence, param) < 0)
    return -1;

  return 0;
}

static int _decodeHdmvButtonOverlapGroup(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence,
  HdmvButtonOverlapGroupParameters * param
)
{
  uint32_t value;
  unsigned i;

  /* [u16 default_valid_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->default_valid_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        default_valid_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->default_valid_button_id_ref,
    param->default_valid_button_id_ref
  );

  /* [u8 number_of_buttons] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->number_of_buttons = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "        number_of_buttons: %u (0x%02X).\n",
    param->number_of_buttons,
    param->number_of_buttons
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "        Buttons:\n"
  );

  if (!param->number_of_buttons)
    LIBBLU_HDMV_PARSER_DEBUG("         *empty*\n");

  for (i = 0; i < param->number_of_buttons; i++) {
    HdmvButtonParam * btn;

    LIBBLU_HDMV_PARSER_DEBUG(
      "         Button[%u]():\n",
      i
    );

    if (NULL == (btn = getHdmvBtnParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* Button() */
    if (_decodeHdmvButton(ctx, sequence, btn) < 0)
      return -1;

    param->buttons[i] = btn;
  }

  return 0;
}

static int _decodeHdmvPage(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence,
  HdmvPageParameters * param
)
{
  uint32_t value;
  unsigned i;

  /* [u8 page_id] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->page_id = value & 0xFF;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    page_id: %u (0x%x).\n",
    param->page_id,
    param->page_id
  );

  /* [u8 page_version_number] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->page_version_number = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    page_version_number: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->page_version_number,
    param->page_version_number
  );

  /* [u64 UO_mask_table()] */
  if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
    return -1;
  param->UO_mask_table  = (uint64_t) value << 32;
  if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
    return -1;
  param->UO_mask_table |= (uint64_t) value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    UO_mask_table(): 0x%016" PRIX64 ".\n",
    param->UO_mask_table
  );

  /* In_effects() */
  LIBBLU_HDMV_PARSER_DEBUG("    In_effects():\n");
  if (_decodeHdmvEffectSequence(ctx, sequence, &param->in_effects) < 0)
    return -1;
  /* Out_effects() */
  LIBBLU_HDMV_PARSER_DEBUG("    Out_effects():\n");
  if (_decodeHdmvEffectSequence(ctx, sequence, &param->out_effects) < 0)
    return -1;

  /* [u8 animation_frame_rate_code] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->animation_frame_rate_code = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    animation_frame_rate_code: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->animation_frame_rate_code,
    param->animation_frame_rate_code
  );
  if (param->animation_frame_rate_code)
    LIBBLU_HDMV_PARSER_DEBUG(
      "     -> Animation at 1/%" PRIu8 " of frame-rate speed.\n",
      param->animation_frame_rate_code
    );
  else
    LIBBLU_HDMV_PARSER_DEBUG(
      "     -> Only first picture of animation displayed.\n"
    );

  /* [u16 default_selected_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->default_selected_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    default_selected_button_id_ref: %" PRIu16 " (0x%" PRIX16 ").\n",
    param->default_selected_button_id_ref,
    param->default_selected_button_id_ref
  );

  /* [u16 default_activated_button_id_ref] */
  if (readValueFromHdmvSequence(sequence, 2, &value) < 0)
    return -1;
  param->default_activated_button_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    default_activated_button_id_ref: %" PRIu16 " (0x%" PRIX16 ").\n",
    param->default_activated_button_id_ref,
    param->default_activated_button_id_ref
  );

  /* [u8 palette_id_ref] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->palette_id_ref = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    palette_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_BOGs] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->number_of_BOGs = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "    number_of_BOGs: %u BOGs (0x%02X).\n",
    param->number_of_BOGs,
    param->number_of_BOGs
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "     Button Overlap Groups:\n"
  );

  if (!param->number_of_BOGs)
    LIBBLU_HDMV_PARSER_DEBUG("      *none*\n");

  for (i = 0; i < param->number_of_BOGs; i++) {
    HdmvButtonOverlapGroupParameters * bog;

    LIBBLU_HDMV_PARSER_DEBUG(
      "      Button_overlap_group[%u]():\n",
      i
    );

    if (NULL == (bog = getHdmvBOGParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    if (_decodeHdmvButtonOverlapGroup(ctx, sequence, bog) < 0)
      return -1;

    param->bogs[i] = bog;
  }

  return 0;
}

static int _decodeHdmvInteractiveCompositionData(
  HdmvContextPtr ctx,
  HdmvSequencePtr sequence
)
{
  HdmvICParameters * param = &sequence->data.ic;
  uint32_t value;
  unsigned i;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Interactive_composition(): "
    "/* Recomposed from Interactive_composition_data_fragment() */\n"
  );

  /* [u24 interactive_composition_length] */
  if (readValueFromHdmvSequence(sequence, 3, &value) < 0)
    return -1;
  param->interactive_composition_length = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  interactive_composition_length: %" PRIu32 " bytes (0x%06" PRIX32 ").\n",
    param->interactive_composition_length,
    param->interactive_composition_length
  );

  /* [b1 stream_model] [b1 user_interface_model] [v6 reserved] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->stream_model         = (value >> 7) & 0x1;
  param->user_interface_model = (value >> 6) & 0x1;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  stream_model: %s (0b%x).\n",
    HdmvStreamModelStr(param->stream_model),
    param->stream_model
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  user_interface_model: %s (0b%x).\n",
    HdmvUserInterfaceModelStr(param->user_interface_model),
    param->user_interface_model
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%02X.\n",
    value & 0x3F
  );

  if (param->stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    /* In Mux related parameters */
    LIBBLU_HDMV_PARSER_DEBUG("  Mux related parameters:\n");

    /* [v7 reserved] [u33 composition_time_out_pts] */
    if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
    return -1;
    param->oomRelatedParam.composition_time_out_pts =
      ((uint64_t) value & 0x1FFFFFF) << 8
    ;
    if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
    param->oomRelatedParam.composition_time_out_pts |=
      (uint64_t) value & 0xFF
    ;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   composition_time_out_pts: %" PRIu64 " (0x%09" PRIX64 ").\n",
      param->oomRelatedParam.composition_time_out_pts,
      param->oomRelatedParam.composition_time_out_pts
    );

    /* [v7 reserved] [u33 selection_time_out_pts] */
    if (readValueFromHdmvSequence(sequence, 4, &value) < 0)
    return -1;
    param->oomRelatedParam.selection_time_out_pts =
      ((uint64_t) value & 0x1FFFFFF) << 8
    ;
    if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
    param->oomRelatedParam.selection_time_out_pts |=
      (uint64_t) value & 0xFF
    ;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   selection_time_out_pts: %" PRIu64 " (0x%09" PRIX64 ").\n",
      param->oomRelatedParam.selection_time_out_pts,
      param->oomRelatedParam.selection_time_out_pts
    );
  }

  /* [u24 user_time_out_duration] */
  if (readValueFromHdmvSequence(sequence, 3, &value) < 0)
    return -1;
  param->user_time_out_duration = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  user_time_out_duration: %" PRIu32 " (0x%06" PRIX32 ").\n",
    param->user_time_out_duration,
    param->user_time_out_duration
  );

  /* [u8 number_of_pages] */
  if (readValueFromHdmvSequence(sequence, 1, &value) < 0)
    return -1;
  param->number_of_pages = value;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  number_of_pages: %u (0x%02X).\n",
    param->number_of_pages,
    param->number_of_pages
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  Pages():\n"
  );
  if (!param->number_of_pages)
    LIBBLU_HDMV_PARSER_DEBUG("   *none*\n");

  for (i = 0; i < param->number_of_pages; i++) {
    HdmvPageParameters * page;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   Page[%u]():\n",
      i
    );

    if (NULL == (page = getHdmvPageParamHdmvSegmentsInventory(ctx->segInv)))
      return -1;

    /* Page() */
    if (_decodeHdmvPage(ctx, sequence, page) < 0)
      return -1;

    param->pages[i] = page;
  }

  return 0;
}

static int _parseHdmvIcsSegment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvIcsSegmentParameters param;

  /* Video_descriptor() */
  if (_parseHdmvVideoDescriptor(ctx, &param.video_descriptor) < 0)
    return -1;

  /* Composition_descriptor() */
  if (_parseHdmvCompositionDescriptor(ctx, &param.composition_descriptor) < 0)
    return -1;

  /* Sequence_descriptor() */
  if (_parseHdmvSequenceDescriptor(ctx, &param.sequence_descriptor) < 0)
    return -1;
  HdmvSequenceLocation location = getHdmvSequenceLocation(param.sequence_descriptor);

  if (location & HDMV_SEQUENCE_LOC_START) {
    // TODO: Is Epoch Continue shall avoid initialization of a new DS?
    if (initEpochHdmvContext(ctx, param.composition_descriptor, param.video_descriptor) < 0)
      goto free_return;
  }

  /* Add parsed segment as a sequence in ICS sequences list. */
  HdmvSequencePtr sequence = addSegToSeqDisplaySetHdmvContext(
    ctx,
    HDMV_SEGMENT_TYPE_ICS_IDX,
    segment,
    location
  );
  if (NULL == sequence)
    goto free_return;

  /* Interactive_composition_data_fragment() */
  if (_parseHdmvInteractiveCompositionDataFragment(ctx, segment, sequence) < 0)
    goto free_return;

  if (isLastSegmentHdmvSequenceLocation(location)) {
    /* Decode complete interactive composition data. */

    /* Interactive_composition() */
    if (_decodeHdmvInteractiveCompositionData(ctx, sequence) < 0)
      goto free_return;

    /* Mark sequence as completed/decoded. */
    if (completeSeqDisplaySetHdmvContext(ctx, HDMV_SEGMENT_TYPE_ICS_IDX) < 0)
      goto free_return;
  }

  /* Increment counter of ICS segments */
  incrementSegmentsNbHdmvContext(ctx, HDMV_SEGMENT_TYPE_ICS_IDX);

  return 0;

free_return:
  LIBBLU_HDMV_PARSER_ERROR_RETURN(
    " Error in ICS with 'composition_number' == %u.\n",
    param.composition_descriptor.composition_number
  );
}

/* ### END segments : ###################################################### */

static int _parseHdmvEndSegment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvSequencePtr sequence;

  /* Add parsed segment as a sequence in END sequences list. */
  sequence = addSegToSeqDisplaySetHdmvContext(
    ctx,
    HDMV_SEGMENT_TYPE_END_IDX,
    segment,
    HDMV_SEQUENCE_LOC_UNIQUE
  );
  if (NULL == sequence)
    return -1;

  if (completeSeqDisplaySetHdmvContext(ctx, HDMV_SEGMENT_TYPE_END_IDX) < 0)
    return -1;

  /* Increment counters of END segments */
  incrementSegmentsNbHdmvContext(ctx, HDMV_SEGMENT_TYPE_END_IDX);

  return 0;
}

/* ######################################################################### */

int parseHdmvSegment(
  HdmvContextPtr ctx
)
{
  HdmvSegmentParameters segment;

  if (!ctx->param.rawStreamInputMode) {
    /* SUP/MNU header */
    if (_parseHdmvHeader(ctx, &segment) < 0)
      return -1;
  }
  else if (!ctx->param.generationMode) {
    // TODO: Emit warning (raw input, missing timecodes).
    // TODO: Add timecodes using option?
    if (addTimecodeHdmvContext(ctx, 0) < 0)
      return -1;
  }

  /* Segment_descriptor() */
  if (_parseHdmvSegmentDescriptor(ctx, &segment) < 0)
    return -1;

  HdmvSegmentType segment_type;
  if (checkHdmvSegmentType(segment.type, ctx->type, &segment_type) < 0)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid segment header at offset 0x%" PRIX64 ".\n",
      segment.inputFileOffset
    );

  switch (segment_type) {
    case HDMV_SEGMENT_TYPE_PDS:
      /* Palette Definition Segment */
      return _parseHdmvPdsSegment(ctx, segment);

    case HDMV_SEGMENT_TYPE_ODS:
      /* Object Definition Segment */
      return _parseHdmvOdsSegment(ctx, segment);

    case HDMV_SEGMENT_TYPE_PCS:
      /* Presentation Composition Segment */
      return _parseHdmvPcsSegment(ctx, segment);

    case HDMV_SEGMENT_TYPE_WDS:
      /* Window Definition Segment */
      return _parseHdmvWdsSegment(ctx, segment);

    case HDMV_SEGMENT_TYPE_ICS:
      /* Interactive Composition Segment */
      return _parseHdmvIcsSegment(ctx, segment);

    case HDMV_SEGMENT_TYPE_END:
      /* END Segment */
      if (_parseHdmvEndSegment(ctx, segment) < 0)
        return -1;

      /* Complete the DS */
      return completeDisplaySetHdmvContext(ctx);
  }

  LIBBLU_ERROR_RETURN(
    "Broken program, unexpected segment type.\n"
  );
}