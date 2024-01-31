#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "hdmv_parser.h"

#define READ_VALUE(d, c, s, e)                                                \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readValueHdmvContext(c, s, &_val) < 0)                                \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

static void _initReferenceClockFromHdmvHeader(
  HdmvContextPtr ctx,
  int32_t first_seg_dts_value
)
{
  int64_t first_DTS = first_seg_dts_value - ctx->param.initial_delay;

  /* Update reference timecode */
  int64_t reference = ctx->param.ref_timestamp;
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
  // uint32_t value;

  int64_t hdr_off = tellPos(inputHdmvContext(ctx));

  LIBBLU_HDMV_PARSER_DEBUG(
    "0x%08" PRIX64 ": %s Header.\n",
    hdr_off,
    HdmvStreamTypeStr(ctx->type)
  );

  /* [u16 format_identifier] */
  uint16_t format_identifier;
  READ_VALUE(&format_identifier, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    " format_identifier: \"%c%c\" (0x%04" PRIX16 ").\n",
    (char) (format_identifier >> 8),
    (char) (format_identifier),
    format_identifier
  );

  if (!checkFormatIdentifierHdmvStreamType(format_identifier, ctx->type))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unexpected %s magic word, expect 0x%02X, "
      "got 0x%02X at offset 0x%08" PRIX64 ".\n",
      HdmvStreamTypeStr(ctx->type),
      expectedFormatIdentifierHdmvStreamType(ctx->type),
      format_identifier,
      hdr_off
    );

  /* [d32 pts] */
  READ_VALUE(&param->timestamps_header.pts, ctx, 4, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    " pts: %" PRId32 " (0x%08" PRIX32 ").\n",
    param->timestamps_header.pts,
    param->timestamps_header.pts
  );

  /* [d32 dts] */
  READ_VALUE(&param->timestamps_header.dts, ctx, 4, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    " dts: %" PRId32 " (0x%08" PRIX32 ").\n",
    param->timestamps_header.dts,
    param->timestamps_header.dts
  );

  if (
    !getTotalHdmvContextSegmentTypesCounter(ctx->global_counters.nb_seg)
    && !ctx->param.force_retiming
  ) {
    /**
     * Initializing reference start clock with first segment DTS.
     * This value is used as offset to allow negative time
     * values (pre-buffering).
     */
    _initReferenceClockFromHdmvHeader(
      ctx,
      param->timestamps_header.dts
    );
  }

  return 0;
}

static int _parseHdmvSegmentDescriptor(
  HdmvContextPtr ctx,
  HdmvSegmentDescriptor * ret
)
{
  assert(NULL != ret);

  /* [u8 segment_type] */
  READ_VALUE(&ret->segment_type, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "0x%08" PRIX64 ": %s.\n",
    ret->segment_type,
    HdmvSegmentTypeStr(ret->segment_type)
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    " Segment_descriptor():\n"
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  segment_type: %s (0x%02" PRIX8 ").\n",
    HdmvSegmentTypeStr(ret->segment_type),
    ret->segment_type
  );

  /* [u16 segment_length] */
  READ_VALUE(&ret->segment_length, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  segment_length: %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
    ret->segment_length,
    ret->segment_length
  );

  return 0;
}

/* ### Palette Definition Segments : ####################################### */

static int _parseHdmvPaletteDescriptor(
  HdmvContextPtr ctx,
  HdmvPDParameters * param
)
{
  LIBBLU_HDMV_PARSER_DEBUG(" Palette_descriptor():\n");

  /* [u8 palette_id] */
  READ_VALUE(&param->palette_id, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_id: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id,
    param->palette_id
  );

  /* [u8 palette_version_number] */
  READ_VALUE(&param->palette_version_number, ctx, 1, return -1);

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
  /* [u8 palette_entry_id] */
  uint8_t palette_entry_id;
  READ_VALUE(&palette_entry_id, ctx, 1, return -1);

  if (HDMV_PAL_FF == palette_entry_id)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected palette entry id 0xFF, "
      "this value shall not be used as it is reserved for transparency.\n"
    );

  HdmvPaletteEntryParameters * entry = &entries[palette_entry_id];

  /* [u8 Y_value] */
  READ_VALUE(&entry->y_value, ctx, 1, return -1);
  /* [u8 Cr_value] */
  READ_VALUE(&entry->cr_value, ctx, 1, return -1);
  /* [u8 Cb_value] */
  READ_VALUE(&entry->cb_value, ctx, 1, return -1);
  /* [u8 T_value] */
  READ_VALUE(&entry->t_value, ctx, 1, return -1);

#if 1
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

  entry->updated = true;
  return 0;
}

static int _computeNumberOfPaletteEntries(
  uint16_t * nb_pal_entries_ret,
  uint16_t segment_size
)
{
  if (segment_size < HDMV_SIZE_PALETTE_DESCRIPTOR)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected PDS size (smaller than 2 bytes).\n"
    );

  uint16_t pal_size = segment_size - HDMV_SIZE_PALETTE_DESCRIPTOR;
  if (0 != (pal_size % HDMV_SIZE_PALETTE_DEFINITION_ENTRY))
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected PDS size (not a multiple of entries size).\n"
    );

  uint16_t nb_pal_entries = pal_size / HDMV_SIZE_PALETTE_DEFINITION_ENTRY;
  if (HDMV_MAX_NB_PDS_ENTRIES < nb_pal_entries)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected PDS size (max number of entries exceeded: %u/%u).\n",
      nb_pal_entries,
      HDMV_MAX_NB_PDS_ENTRIES
    );

  *nb_pal_entries_ret = nb_pal_entries;
  return 0;
}

static int _parseHdmvPDS(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvPDSegmentParameters param;
  memset(&param, 0, sizeof(HdmvPDSegmentParameters));

  /* Palette_descriptor() */
  if (_parseHdmvPaletteDescriptor(ctx, &param.palette_descriptor) < 0)
    return -1;

  uint16_t segment_length = segment.desc.segment_length;
  if (_computeNumberOfPaletteEntries(&param.number_of_palette_entries, segment_length) < 0)
    goto free_return;

  if (0 < param.number_of_palette_entries) {
    LIBBLU_HDMV_PAL_DEBUG("  Palette_entries():\n");

    LIBBLU_HDMV_PAL_DEBUG(
      "   => %u entrie(s).\n",
      param.number_of_palette_entries
    );

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
    ctx->last_compo_nb_parsed
  );
}

/* ### Object Definition Segments : ######################################## */

static int _parseHdmvObjectDescriptor(
  HdmvContextPtr ctx,
  HdmvODescParameters * param
)
{
  LIBBLU_HDMV_PARSER_DEBUG(
    " Object_descriptor():\n"
  );

  /* [u16 object_id] */
  READ_VALUE(&param->object_id, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_id: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->object_id,
    param->object_id
  );

  /* [u8 object_version_number] */
  READ_VALUE(&param->object_version_number, ctx, 1, return -1);

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
  LIBBLU_HDMV_PARSER_DEBUG(
    " Sequence_descriptor():\n"
  );

  /* [b1 first_in_sequence] [b1 last_in_sequence] [v6 reserved] */
  uint8_t flags;
  READ_VALUE(&flags, ctx, 1, return -1);
  param->first_in_sequence = (flags & 0x80);
  param->last_in_sequence  = (flags & 0x40);

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
    flags & 0x3F
  );

  return 0;
}

static int _computeSizeHdmvObjectDataFragment(
  uint16_t * od_frag_length_ret,
  uint16_t segment_length
)
{
  if (segment_length < HDMV_SIZE_OD_SEGMENT_HEADER)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected ODS size (smaller than 4 bytes).\n"
    );

  *od_frag_length_ret = segment_length - HDMV_SIZE_OD_SEGMENT_HEADER;
  return 0;
}

static int _parseHdmvObjectDataFragment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment,
  HdmvSequencePtr sequence
)
{
  LIBBLU_HDMV_PARSER_DEBUG(" Object_data_fragment():\n");

  uint16_t segment_length = segment.desc.segment_length;
  uint16_t frag_length;
  if (_computeSizeHdmvObjectDataFragment(&frag_length, segment_length) < 0)
    return -1;

  if (parseFragmentHdmvSequence(sequence, inputHdmvContext(ctx), frag_length) < 0)
    return -1;

  return 0;
}

#define READ_VALUE_SEQ(d, seg, s, e)                                          \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (_readValueFromHdmvSequence(seg, s, &_val) < 0)                        \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

static int _decodeHdmvObjectData(
  HdmvSequencePtr sequence,
  HdmvODescParameters object_descriptor
)
{
  HdmvODParameters * od = &sequence->data.od;

  od->object_descriptor = object_descriptor;

  LIBBLU_HDMV_PARSER_DEBUG(
    " Object_data(): /* Recomposed from object_data_fragment() */\n"
  );

  /* [u24 object_data_length] */
  READ_VALUE_SEQ(&od->object_data_length, sequence, 3, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_data_length: %" PRIu32 " bytes (0x%06" PRIX32 ").\n",
    od->object_data_length,
    od->object_data_length
  );

  /* [u16 object_width] */
  READ_VALUE_SEQ(&od->object_width, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    od->object_width,
    od->object_width
  );

  /* [u16 object_height] */
  READ_VALUE_SEQ(&od->object_height, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  object_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    od->object_height,
    od->object_height
  );

  return 0;
}

static int _parseHdmvODS(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvODSegmentParameters param;
  memset(&param, 0, sizeof(HdmvODSegmentParameters));

  /* Object_descriptor() */
  if (_parseHdmvObjectDescriptor(ctx, &param.object_descriptor) < 0)
    return -1;

  /* Sequence_descriptor() */
  if (_parseHdmvSequenceDescriptor(ctx, &param.sequence_descriptor) < 0)
    return -1;
  HdmvSequenceLocation location = getHdmvSequenceLocation(
    param.sequence_descriptor
  );

  /* Add parsed segment as a sequence in ODS sequences list. */
  HdmvSequencePtr sequence = addSegToSeqDisplaySetHdmvContext(
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
    ctx->last_compo_nb_parsed
  );
}

/* ### Presentation Composition Segments : ################################# */

static int _parseHdmvVideoDescriptor(
  HdmvContextPtr ctx,
  HdmvVDParameters * param
)
{
  LIBBLU_HDMV_PARSER_DEBUG(
    " Video_descriptor():\n"
  );

  /* [u16 video_width] */
  READ_VALUE(&param->video_width, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  video_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->video_width,
    param->video_width
  );

  /* [u16 video_height] */
  READ_VALUE(&param->video_height, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  video_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->video_height,
    param->video_height
  );

  /* [u4 frame_rate_id] [v4 reserved] */
  uint8_t flags;
  READ_VALUE(&flags, ctx, 1, return -1);
  param->frame_rate = (flags >> 4) & 0xF;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  frame_rate_id: 0x%" PRIX8 " (%.3f).\n",
    param->frame_rate,
    frameRateCodeToFloat(param->frame_rate)
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%" PRIX8 ".\n",
    flags & 0xF
  );

  return 0;
}

static int _parseHdmvCompositionDescriptor(
  HdmvContextPtr ctx,
  HdmvCDParameters * param
)
{
  LIBBLU_HDMV_PARSER_DEBUG(
    " Composition_descriptor():\n"
  );

  /* [u16 composition_number] */
  READ_VALUE(&param->composition_number, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  composition_number: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->composition_number,
    param->composition_number
  );

  /* [u2 composition_state] [v6 reserved] */
  uint8_t flags;
  READ_VALUE(&flags, ctx, 1, return -1);
  param->composition_state = flags >> 6;

  LIBBLU_HDMV_PARSER_DEBUG(
    "  composition_state: %s (0x%X).\n",
    HdmvCompositionStateStr(param->composition_state),
    param->composition_state
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%02X.\n",
    flags & 0x3F
  );

  ctx->last_compo_nb_parsed = param->composition_number;

  return 0;
}

static int _parseHdmvCompositionObject(
  HdmvContextPtr ctx,
  HdmvCOParameters * param
)
{
  /* [u16 object_id_ref] */
  READ_VALUE(&param->object_id_ref, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->object_id_ref,
    param->object_id_ref
  );

  /* [u8 window_id_ref] */
  READ_VALUE(&param->window_id_ref, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id_ref,
    param->window_id_ref
  );

  /* [b1 object_cropped_flag] [b1 forced_on_flag] [v6 reserved] */
  uint8_t flags;
  READ_VALUE(&flags, ctx, 1, return -1);
  param->object_cropped_flag = flags & 0x80;
  param->forced_on_flag      = flags & 0x40;

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
    flags & 0x3F
  );

  /* [u16 object_horizontal_position] */
  READ_VALUE(&param->composition_object_horizontal_position, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    object_horizontal_position: %" PRIu16 " px (0x%02X).\n",
    param->composition_object_horizontal_position,
    param->composition_object_horizontal_position
  );

  /* [u16 object_vertical_position] */
  READ_VALUE(&param->composition_object_vertical_position, ctx, 2, return -1);

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
    READ_VALUE(&param->object_cropping.horizontal_position, ctx, 2, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_horizontal_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.horizontal_position,
      param->object_cropping.horizontal_position
    );

    /* [u16 object_cropping_vertical_position] */
    READ_VALUE(&param->object_cropping.vertical_position, ctx, 2, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.vertical_position,
      param->object_cropping.vertical_position
    );

    /* [u16 object_cropping_width] */
    READ_VALUE(&param->object_cropping.width, ctx, 2, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "     object_cropping_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.width,
      param->object_cropping.width
    );

    /* [u16 object_cropping_height] */
    READ_VALUE(&param->object_cropping.height, ctx, 2, return -1);

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
  LIBBLU_HDMV_PARSER_DEBUG(
    " Presentation_composition():\n"
  );

  /* [b1 palette_update_flag] [v7 reserved] */
  uint8_t flags;
  READ_VALUE(&flags, ctx, 1, return -1);
  param->palette_update_flag = (flags & 0x80);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_update_flag: %s (0b%x).\n",
    BOOL_STR(param->palette_update_flag),
    param->palette_update_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "  reserved: 0x%02X.\n",
    flags & 0x7F
  );

  /* [u8 palette_id_ref] */
  READ_VALUE(&param->palette_id_ref, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  palette_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_composition_objects] */
  READ_VALUE(&param->number_of_composition_objects, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  number_of_composition_objects: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->number_of_composition_objects,
    param->number_of_composition_objects
  );

  if (HDMV_MAX_NB_PCS_COMPOS < param->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Number of composition objects in Presentation Composition Segment shall "
      "be between 0 and 2 inclusive (number_of_composition_objects = %" PRIu8 ").\n",
      param->number_of_composition_objects
    );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  Composition_objects():\n"
  );
  if (!param->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_DEBUG("   *none*\n");

  for (unsigned i = 0; i < param->number_of_composition_objects; i++) {
    LIBBLU_HDMV_PARSER_DEBUG("   Composition_object[%u]():\n", i);

    HdmvCOParameters * obj;
    if (NULL == (obj = getHdmvCompoObjParamHdmvSegmentsInventory(&ctx->seg_inv)))
      return -1;

    /* Composition_objects() */
    if (_parseHdmvCompositionObject(ctx, obj) < 0)
      return -1;
    param->composition_objects[i] = obj;
  }

  return 0;
}

static int _parseHdmvPCS(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvPcsSegmentParameters param;

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
  HdmvSequencePtr sequence = addSegToSeqDisplaySetHdmvContext(
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
  /* [u8 window_id] */
  READ_VALUE(&param->window_id, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_id: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id,
    param->window_id
  );

  /* [u16 window_horizontal_position] */
  READ_VALUE(&param->window_horizontal_position, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_horizontal_position: %u px (0x%04" PRIX16 ").\n",
    param->window_horizontal_position,
    param->window_horizontal_position
  );

  /* [u16 window_vertical_position] */
  READ_VALUE(&param->window_vertical_position, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_vertical_position,
    param->window_vertical_position
  );

  /* [u16 window_width] */
  READ_VALUE(&param->window_width, ctx, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    window_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_width,
    param->window_width
  );

  /* [u16 window_height] */
  READ_VALUE(&param->window_height, ctx, 2, return -1);

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
  LIBBLU_HDMV_PARSER_DEBUG(
    " Window_definition():\n"
  );

  /* [u8 number_of_windows] */
  READ_VALUE(&param->number_of_windows, ctx, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  number_of_windows: %u (0x%02X).\n",
    param->number_of_windows,
    param->number_of_windows
  );

  if (HDMV_MAX_NB_WDS_WINDOWS < param->number_of_windows)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Number of windows in Window Definition Segment shall "
      "be between 0 and 2 inclusive (number_of_windows = %" PRIu8 ").\n",
      param->number_of_windows
    );

  LIBBLU_HDMV_PARSER_DEBUG(
    "  Windows():\n"
  );
  if (!param->number_of_windows)
    LIBBLU_HDMV_PARSER_DEBUG("   *none*\n");

  for (unsigned i = 0; i < param->number_of_windows; i++) {
    LIBBLU_HDMV_PARSER_DEBUG("   Window[%u]():\n", i);

    HdmvWindowInfoParameters * win;
    if (NULL == (win = getHdmvWinInfoParamHdmvSegmentsInventory(&ctx->seg_inv)))
      return -1;

    /* Window_info() */
    if (_parseHdmvWindowInfo(ctx, win) < 0)
      return -1;

    param->windows[i] = win;
  }

  return 0;
}

static int _parseHdmvWDS(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvWDParameters param;

  /* Window_definition() */
  if (_parseHdmvWindowsDefinition(ctx, &param) < 0)
    goto free_return;

  /* Add parsed segment as a sequence in WDS sequences list. */
  HdmvSequencePtr sequence = addSegToSeqDisplaySetHdmvContext(
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
    ctx->last_compo_nb_parsed
  );
}

/* ### Interactive Composition Segments : ################################## */

static int _computeSizeHdmvICDataFragment(
  uint16_t * dst,
  uint16_t segment_size
)
{
  if (segment_size < HDMV_SIZE_IC_SEGMENT_HEADER)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected ICS size (smaller than 9 bytes).\n"
    );

  *dst = segment_size - HDMV_SIZE_IC_SEGMENT_HEADER;
  return 0;
}

static int _parseHdmvInteractiveCompositionDataFragment(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment,
  HdmvSequencePtr sequence
)
{
  LIBBLU_HDMV_PARSER_DEBUG(" Interactive_composition_data_fragment():\n");

  uint16_t segment_length = segment.desc.segment_length;
  uint16_t frag_length;
  if (_computeSizeHdmvICDataFragment(&frag_length, segment_length) < 0)
    return -1;

  if (parseFragmentHdmvSequence(sequence, inputHdmvContext(ctx), frag_length) < 0)
    return -1;

  return 0;
}

static int _decodeHdmvWindowInfo(
  HdmvSequencePtr sequence,
  HdmvWindowInfoParameters * param
)
{
  /* [u8 window_id] */
  READ_VALUE_SEQ(&param->window_id, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_id: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id,
    param->window_id
  );

  /* [u16 window_horizontal_position] */
  READ_VALUE_SEQ(&param->window_horizontal_position, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_horizontal_position: %u px (0x%04" PRIX16 ").\n",
    param->window_horizontal_position,
    param->window_horizontal_position
  );

  /* [u16 window_vertical_position] */
  READ_VALUE_SEQ(&param->window_vertical_position, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_vertical_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_vertical_position,
    param->window_vertical_position
  );

  /* [u16 window_width] */
  READ_VALUE_SEQ(&param->window_width, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_width: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_width,
    param->window_width
  );

  /* [u16 window_height] */
  READ_VALUE_SEQ(&param->window_height, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        window_height: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->window_height,
    param->window_height
  );

  return 0;
}

static int _decodeHdmvCompositionObject(
  HdmvSequencePtr sequence,
  HdmvCOParameters * param
)
{
  /* [u16 object_id_ref] */
  READ_VALUE_SEQ(&param->object_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->object_id_ref,
    param->object_id_ref
  );

  /* [u8 window_id_ref] */
  READ_VALUE_SEQ(&param->window_id_ref, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          window_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->window_id_ref,
    param->window_id_ref
  );

  /* [b1 object_cropped_flag] [v7 reserved] */
  uint8_t flags;
  READ_VALUE_SEQ(&flags, sequence, 1, return -1);
  param->object_cropped_flag = (flags & 0x80);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_cropped_flag: %s (0b%x).\n",
    BOOL_STR(param->object_cropped_flag),
    param->object_cropped_flag
  );
  LIBBLU_HDMV_PARSER_DEBUG(
    "          reserved: 0x%02X.\n",
    flags & 0x7F
  );

  /* [u16 object_horizontal_position] */
  READ_VALUE_SEQ(&param->composition_object_horizontal_position, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          object_horizontal_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->composition_object_horizontal_position,
    param->composition_object_horizontal_position
  );

  /* [u16 object_vertical_position] */
  READ_VALUE_SEQ(&param->composition_object_vertical_position, sequence, 2, return -1);

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
    READ_VALUE_SEQ(&param->object_cropping.horizontal_position, sequence, 2, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_horizontal_position: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.horizontal_position,
      param->object_cropping.horizontal_position
    );

    /* [u16 object_cropping_vertical_position] */
    READ_VALUE_SEQ(&param->object_cropping.vertical_position, sequence, 2, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_vertical_position: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.vertical_position,
      param->object_cropping.vertical_position
    );

    /* [u16 object_cropping_width] */
    READ_VALUE_SEQ(&param->object_cropping.width, sequence, 2, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "           object_cropping_width: "
      "%" PRIu16 " px (0x%04" PRIX16 ").\n",
      param->object_cropping.width,
      param->object_cropping.width
    );

    /* [u16 object_cropping_height] */
    READ_VALUE_SEQ(&param->object_cropping.height, sequence, 2, return -1);

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
  /* [u24 effect_duration] */
  READ_VALUE_SEQ(&param->effect_duration, sequence, 3, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        effect_duration: %" PRIu32 " (0x%06" PRIX32 ").\n",
    param->effect_duration,
    param->effect_duration
  );

  /* [u8 palette_id_ref] */
  READ_VALUE_SEQ(&param->palette_id_ref, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        palette_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_composition_objects] */
  READ_VALUE_SEQ(&param->number_of_composition_objects, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        number_of_composition_objects: "
    "%" PRIu8 " objects (0x%02" PRIX8 ").\n",
    param->number_of_composition_objects,
    param->number_of_composition_objects
  );

  if (HDMV_MAX_NB_ICS_COMP_OBJ < param->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Number of composition objects in effect sequence shall "
      "be between 0 and 2 inclusive (number_of_composition_objects = %" PRIu8 ").\n",
      param->number_of_composition_objects
    );

  LIBBLU_HDMV_PARSER_DEBUG(
    "        Composition objects:\n"
  );

  if (!param->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_DEBUG(
      "         *none*\n"
    );

  for (unsigned i = 0; i < param->number_of_composition_objects; i++) {
    LIBBLU_HDMV_PARSER_DEBUG("         Composition_object[%u]()\n", i);

    HdmvCOParameters * obj;
    if (NULL == (obj = getHdmvCompoObjParamHdmvSegmentsInventory(&ctx->seg_inv)))
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
  LIBBLU_HDMV_PARSER_DEBUG(
    "     Effect_sequence():\n"
  );

  /* [u8 number_of_windows] */
  READ_VALUE_SEQ(&param->number_of_windows, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "      number_of_windows: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->number_of_windows,
    param->number_of_windows
  );

  if (HDMV_MAX_NB_ICS_WINDOWS < param->number_of_windows)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Number of windows in effect sequence shall "
      "be between 0 and 2 inclusive (number_of_windows = %" PRIu8 ").\n",
      param->number_of_windows
    );

  LIBBLU_HDMV_PARSER_DEBUG(
    "      Windows():\n"
  );
  if (!param->number_of_windows)
    LIBBLU_HDMV_PARSER_DEBUG("       *none*\n");

  assert(param->number_of_windows <= HDMV_MAX_NB_ICS_WINDOWS);
    /* Shall always be the case. */

  for (unsigned i = 0; i < param->number_of_windows; i++) {
    LIBBLU_HDMV_PARSER_DEBUG(
      "       Window_info[%u]():\n",
      i
    );

    HdmvWindowInfoParameters * win;
    if (NULL == (win = getHdmvWinInfoParamHdmvSegmentsInventory(&ctx->seg_inv)))
      return -1;

    /* Window_info() */
    if (_decodeHdmvWindowInfo(sequence, win) < 0)
      return -1;
    param->windows[i] = win;
  }

  /* [u8 number_of_effects] */
  READ_VALUE_SEQ(&param->number_of_effects, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "      number_of_effects: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->number_of_effects,
    param->number_of_effects
  );

  if (HDMV_MAX_NB_ICS_EFFECTS < param->number_of_effects)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Number of effects in effects sequence shall "
      "be between 0 and 128 inclusive (number_of_effects = %" PRIu8 ").\n",
      param->number_of_effects
    );

  LIBBLU_HDMV_PARSER_DEBUG("      Effects():\n");
  if (!param->number_of_effects)
    LIBBLU_HDMV_PARSER_DEBUG("       *none*\n");

  assert(param->number_of_effects <= HDMV_MAX_NB_ICS_EFFECTS);
    /* Shall always be the case. */

  for (unsigned i = 0; i < param->number_of_effects; i++) {
    LIBBLU_HDMV_PARSER_DEBUG(
      "       Effect_info[%u]():\n",
      i
    );

    HdmvEffectInfoParameters * eff;
    if (NULL == (eff = getHdmvEffInfoParamHdmvSegmentsInventory(&ctx->seg_inv)))
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
  LIBBLU_HDMV_PARSER_DEBUG(
    "          Neighbor_info():\n"
  );

  /* [u16 upper_button_id_ref] */
  READ_VALUE_SEQ(&param->upper_button_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           upper_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->upper_button_id_ref,
    param->upper_button_id_ref
  );

  /* [u16 lower_button_id_ref] */
  READ_VALUE_SEQ(&param->lower_button_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           lower_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->lower_button_id_ref,
    param->lower_button_id_ref
  );

  /* [u16 left_button_id_ref] */
  READ_VALUE_SEQ(&param->left_button_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           left_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->left_button_id_ref,
    param->left_button_id_ref
  );

  /* [u16 right_button_id_ref] */
  READ_VALUE_SEQ(&param->right_button_id_ref, sequence, 2, return -1);

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
  LIBBLU_HDMV_PARSER_DEBUG(
    "          Normal_state_info():\n"
  );

  /* [u16 normal_start_object_id_ref] */
  READ_VALUE_SEQ(&param->start_object_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           normal_start_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->start_object_id_ref,
    param->start_object_id_ref
  );

  /* [u16 normal_end_object_id_ref] */
  READ_VALUE_SEQ(&param->end_object_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           normal_end_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->end_object_id_ref,
    param->end_object_id_ref
  );

  /* [b1 normal_repeat_flag] [b1 normal_complete_flag] [v6 reserved] */
  uint8_t flags;
  READ_VALUE_SEQ(&flags, sequence, 1, return -1);
  param->repeat_flag   = (flags & 0x80);
  param->complete_flag = (flags & 0x40);

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
    flags & 0x3F
  );

  return 0;
}

static int _decodeHdmvButtonSelectedStateInfo(
  HdmvSequencePtr sequence,
  HdmvButtonSelectedStateInfoParam * param
)
{
  LIBBLU_HDMV_PARSER_DEBUG(
    "          Selected_state_info():\n"
  );

  /* [u8 selected_state_sound_id_ref] */
  READ_VALUE_SEQ(&param->state_sound_id_ref, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_state_sound_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->state_sound_id_ref,
    param->state_sound_id_ref
  );

  /* [u16 selected_start_object_id_ref] */
  READ_VALUE_SEQ(&param->start_object_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_start_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->start_object_id_ref,
    param->start_object_id_ref
  );

  /* [u16 selected_end_object_id_ref] */
  READ_VALUE_SEQ(&param->end_object_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           selected_end_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->end_object_id_ref,
    param->end_object_id_ref
  );

  /* [b1 selected_repeat_flag] [b1 selected_complete_flag] [v6 reserved] */
  uint8_t flags;
  READ_VALUE_SEQ(&flags, sequence, 1, return -1);
  param->repeat_flag   = (flags & 0x80);
  param->complete_flag = (flags & 0x40);

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
    flags & 0x3F
  );

  return 0;
}

static int _decodeHdmvButtonActivatedStateInfo(
  HdmvSequencePtr sequence,
  HdmvButtonActivatedStateInfoParam * param
)
{
  LIBBLU_HDMV_PARSER_DEBUG(
    "          Activated_state_info():\n"
  );

  /* [u8 activated_state_sound_id_ref] */
  READ_VALUE_SEQ(&param->state_sound_id_ref, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           activated_state_sound_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->state_sound_id_ref,
    param->state_sound_id_ref
  );

  /* [u16 activated_start_object_id_ref] */
  READ_VALUE_SEQ(&param->start_object_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "           activated_start_object_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->start_object_id_ref,
    param->start_object_id_ref
  );

  /* [u16 activated_end_object_id_ref] */
  READ_VALUE_SEQ(&param->end_object_id_ref, sequence, 2, return -1);

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
  assert(NULL == button->navigation_commands);
    // Destination list shall be clear

  HdmvNavigationCommand * last = NULL;
  for (unsigned i = 0; i < button->number_of_navigation_commands; i++) {
    HdmvNavigationCommand * com;
    if (NULL == (com = getHdmvNaviComHdmvSegmentsInventory(&ctx->seg_inv)))
      return -1;

    /* [u32 opcode] */
    READ_VALUE_SEQ(&com->opcode, sequence, 4, return -1);
    /* [u32 destination] */
    READ_VALUE_SEQ(&com->destination, sequence, 4, return -1);
    /* [u32 source] */
    READ_VALUE_SEQ(&com->source, sequence, 4, return -1);

    LIBBLU_HDMV_PARSER_DEBUG(
      "           opcode=0x%08" PRIX32 ", "
      "destination=0x%08" PRIX32 ", "
      "source=0x%08" PRIX32 ";\n",
      com->opcode,
      com->destination,
      com->source
    );

    if (NULL != last)
      last->next = com;
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
  /* [u16 button_id] */
  READ_VALUE_SEQ(&param->button_id, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_id: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->button_id,
    param->button_id
  );

  /* [u16 button_numeric_select_value] */
  READ_VALUE_SEQ(&param->button_numeric_select_value, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_numeric_select_value: "
    "%" PRIu16 " (0x%04" PRIX16 ").\n",
    param->button_numeric_select_value,
    param->button_numeric_select_value
  );

  if (param->button_numeric_select_value == 0xFFFF)
    LIBBLU_HDMV_PARSER_DEBUG("           -> Not used.\n");

  /* [b1 auto_action_flag] [v7 reserved] */
  uint8_t flags;
  READ_VALUE_SEQ(&flags, sequence, 1, return -1);
  param->auto_action = (flags & 0x80);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          auto_action_flag: %s (0x%x).\n",
    BOOL_STR(param->auto_action),
    param->auto_action
  );

  LIBBLU_HDMV_PARSER_DEBUG(
    "          reserved: 0x%02X.\n",
    flags & 0x7F
  );

  /* [u16 button_horizontal_position] */
  READ_VALUE_SEQ(&param->button_horizontal_position, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "          button_horizontal_position: %" PRIu16 " px (0x%04" PRIX16 ").\n",
    param->button_horizontal_position,
    param->button_horizontal_position
  );

  /* [u16 button_vertical_position] */
  READ_VALUE_SEQ(&param->button_vertical_position, sequence, 2, return -1);

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
  READ_VALUE_SEQ(&param->number_of_navigation_commands, sequence, 2, return -1);

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
  /* [u16 default_valid_button_id_ref] */
  READ_VALUE_SEQ(&param->default_valid_button_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "        default_valid_button_id_ref: %" PRIu16 " (0x%04" PRIX16 ").\n",
    param->default_valid_button_id_ref,
    param->default_valid_button_id_ref
  );

  /* [u8 number_of_buttons] */
  READ_VALUE_SEQ(&param->number_of_buttons, sequence, 1, return -1);

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

  for (unsigned i = 0; i < param->number_of_buttons; i++) {
    LIBBLU_HDMV_PARSER_DEBUG("         Button[%u]():\n", i);

    HdmvButtonParam * btn;
    if (NULL == (btn = getHdmvBtnParamHdmvSegmentsInventory(&ctx->seg_inv)))
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
  /* [u8 page_id] */
  READ_VALUE_SEQ(&param->page_id, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    page_id: %u (0x%x).\n",
    param->page_id,
    param->page_id
  );

  /* [u8 page_version_number] */
  READ_VALUE_SEQ(&param->page_version_number, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    page_version_number: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->page_version_number,
    param->page_version_number
  );

  /* [u64 UO_mask_table()] */
  READ_VALUE_SEQ(&param->UO_mask_table, sequence, 8, return -1);

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
  READ_VALUE_SEQ(&param->animation_frame_rate_code, sequence, 1, return -1);

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
  READ_VALUE_SEQ(&param->default_selected_button_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    default_selected_button_id_ref: %" PRIu16 " (0x%" PRIX16 ").\n",
    param->default_selected_button_id_ref,
    param->default_selected_button_id_ref
  );

  /* [u16 default_activated_button_id_ref] */
  READ_VALUE_SEQ(&param->default_activated_button_id_ref, sequence, 2, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    default_activated_button_id_ref: %" PRIu16 " (0x%" PRIX16 ").\n",
    param->default_activated_button_id_ref,
    param->default_activated_button_id_ref
  );

  /* [u8 palette_id_ref] */
  READ_VALUE_SEQ(&param->palette_id_ref, sequence, 1, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "    palette_id_ref: %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_BOGs] */
  READ_VALUE_SEQ(&param->number_of_BOGs, sequence, 1, return -1);

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

  for (unsigned i = 0; i < param->number_of_BOGs; i++) {
    LIBBLU_HDMV_PARSER_DEBUG("      Button_overlap_group[%u]():\n", i);

    HdmvButtonOverlapGroupParameters * bog;
    if (NULL == (bog = getHdmvBOGParamHdmvSegmentsInventory(&ctx->seg_inv)))
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

  LIBBLU_HDMV_PARSER_DEBUG(
    " Interactive_composition(): "
    "/* Recomposed from Interactive_composition_data_fragment() */\n"
  );

  /* [u24 interactive_composition_length] */
  READ_VALUE_SEQ(&param->interactive_composition_length, sequence, 3, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  interactive_composition_length: %" PRIu32 " bytes (0x%06" PRIX32 ").\n",
    param->interactive_composition_length,
    param->interactive_composition_length
  );

  /* [b1 stream_model] [b1 user_interface_model] [v6 reserved] */
  uint8_t flags;
  READ_VALUE_SEQ(&flags, sequence, 1, return -1);
  param->stream_model         = flags & 0x80;
  param->user_interface_model = flags & 0x40;

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
    flags & 0x3F
  );

  if (param->stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    /* In Mux related parameters */
    LIBBLU_HDMV_PARSER_DEBUG("  Mux related parameters:\n");

    /* [v7 reserved] [u33 composition_time_out_pts] */
    READ_VALUE_SEQ(&param->composition_time_out_pts, sequence, 5, return -1);
    param->composition_time_out_pts &= 0x1FFFFFFFF;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   composition_time_out_pts: %" PRIu64 " (0x%09" PRIX64 ").\n",
      param->composition_time_out_pts,
      param->composition_time_out_pts
    );

    /* [v7 reserved] [u33 selection_time_out_pts] */
    READ_VALUE_SEQ(&param->selection_time_out_pts, sequence, 5, return -1);
    param->selection_time_out_pts &= 0x1FFFFFFFF;

    LIBBLU_HDMV_PARSER_DEBUG(
      "   selection_time_out_pts: %" PRIu64 " (0x%09" PRIX64 ").\n",
      param->selection_time_out_pts,
      param->selection_time_out_pts
    );
  }

  /* [u24 user_time_out_duration] */
  READ_VALUE_SEQ(&param->user_time_out_duration, sequence, 3, return -1);

  LIBBLU_HDMV_PARSER_DEBUG(
    "  user_time_out_duration: %" PRIu32 " (0x%06" PRIX32 ").\n",
    param->user_time_out_duration,
    param->user_time_out_duration
  );

  /* [u8 number_of_pages] */
  READ_VALUE_SEQ(&param->number_of_pages, sequence, 1, return -1);

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

  for (unsigned i = 0; i < param->number_of_pages; i++) {
    LIBBLU_HDMV_PARSER_DEBUG("   Page[%u]():\n", i);

    HdmvPageParameters * page;
    if (NULL == (page = getHdmvPageParamHdmvSegmentsInventory(&ctx->seg_inv)))
      return -1;

    /* Page() */
    if (_decodeHdmvPage(ctx, sequence, page) < 0)
      return -1;
    param->pages[i] = page;
  }

  return 0;
}

static int _parseHdmvICS(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  HdmvICSegmentParameters param;

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

static int _parseHdmvEND(
  HdmvContextPtr ctx,
  HdmvSegmentParameters segment
)
{
  /* Add parsed segment as a sequence in END sequences list. */
  HdmvSequencePtr sequence = addSegToSeqDisplaySetHdmvContext(
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

  if (!ctx->param.raw_stream_input_mode) {
    /* SUP/MNU header */
    if (_parseHdmvHeader(ctx, &segment) < 0)
      return -1;
  }
  else if (!ctx->param.generation_mode) {
    // TODO: Emit warning (raw input, missing timecodes).
    // TODO: Add timecodes using option?
    if (addTimecodeHdmvContext(ctx, 0) < 0)
      return -1;
  }

  segment.orig_file_offset = tellPos(inputHdmvContext(ctx));

  /* Segment_descriptor() */
  if (_parseHdmvSegmentDescriptor(ctx, &segment.desc) < 0)
    return -1;

  HdmvSegmentType segment_type;
  if (checkHdmvSegmentType(segment.desc.segment_type, ctx->type, &segment_type) < 0)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid segment header at offset 0x%" PRIX64 ".\n",
      segment.orig_file_offset
    );

  switch (segment_type) {
    case HDMV_SEGMENT_TYPE_PDS:
      /* Palette Definition Segment */
      return _parseHdmvPDS(ctx, segment);

    case HDMV_SEGMENT_TYPE_ODS:
      /* Object Definition Segment */
      return _parseHdmvODS(ctx, segment);

    case HDMV_SEGMENT_TYPE_PCS:
      /* Presentation Composition Segment */
      return _parseHdmvPCS(ctx, segment);

    case HDMV_SEGMENT_TYPE_WDS:
      /* Window Definition Segment */
      return _parseHdmvWDS(ctx, segment);

    case HDMV_SEGMENT_TYPE_ICS:
      /* Interactive Composition Segment */
      return _parseHdmvICS(ctx, segment);

    case HDMV_SEGMENT_TYPE_END:
      /* END Segment */
      if (_parseHdmvEND(ctx, segment) < 0)
        return -1;

      /* Complete the DS */
      return completeDSHdmvContext(ctx);
  }

  LIBBLU_ERROR_RETURN(
    "Broken program, unexpected segment type.\n"
  );
}
