#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "igs_segmentsBuilding.h"

/* ### HDMV Segments building utilities : ################################## */

int initHdmvSegmentsBuildingContext(
  HdmvSegmentsBuildingContext * dst,
  const lbc * out_filepath
)
{
  assert(NULL != dst);
  assert(NULL != out_filepath);

  LIBBLU_HDMV_SEGBUILD_INFO(
    "Writing output HDMV stream to '%" PRI_LBCS "'...\n",
    out_filepath
  );

  LIBBLU_HDMV_SEGBUILD_DEBUG("Opening output file.\n");
  FILE * output_fd = lbc_fopen(out_filepath, "wb");
  if (NULL == output_fd)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Unable to open output file '%" PRI_LBCS "', %s (errno: %d).\n",
      out_filepath,
      strerror(errno),
      errno
    );

  *dst = (HdmvSegmentsBuildingContext) {
    .out_fd = output_fd
  };

  return 0;
}

/** \~english
 * \brief Request a minimum remaining buffer size for segments building.
 *
 * \param ctx Context to edit.
 * \param requestedSize Requested buffer minimum size in bytes.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating a memory allocation error.
 *
 * This function must be called before any writing using context.
 * If requested value is smaller than already allocated buffer, nothing
 * is done.
 */
static int _reqBufSizeCtx(
  HdmvSegmentsBuildingContext * ctx,
  uint32_t req_size
)
{
  if (ctx->used_size + req_size < ctx->used_size)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Segments buffer requested size overflow.\n"
    );

  uint32_t new_size = lb_ceil_pow2_32(ctx->used_size + req_size);
  if (!new_size || new_size < ctx->used_size)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN("Segments buffer size overflow.\n");
  if (new_size <= ctx->allocated_size)
    return 0; // Nothing to do, allocation is big enough

  uint8_t * new_data = (uint8_t *) realloc(ctx->data, new_size);
  if (NULL == new_data)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN("Memory allocation error.\n");

  ctx->data = new_data;
  ctx->allocated_size = new_size;

  return 0;
}

static uint32_t _remBufSizeCtx(
  const HdmvSegmentsBuildingContext * ctx
)
{
  return ctx->allocated_size - ctx->used_size;
}

/** \~english
 * \brief Write supplied amount of bytes in reserved building buffer.
 *
 * \param ctx Destination building context.
 * \param data Data array of bytes.
 * \param size Size of the data array in bytes.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating reserved buffer size is not large enough.
 *
 * Prior to any writing, #_reqBufSizeCtx() shall be used to reserve enough
 * space.
 */
int _writeBytesCtx(
  HdmvSegmentsBuildingContext * ctx,
  uint8_t * data,
  uint32_t size
)
{
  if (_remBufSizeCtx(ctx) < size)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Output buffer is too small (%zu < %zu bytes), broken program.\n",
      _remBufSizeCtx(ctx),
      size
    );

  WA_ARRAY(ctx->data, ctx->used_size, data, size);
  return 0;
}

static int _writeCtxBufferOnOutput(
  HdmvSegmentsBuildingContext * ctx,
  FILE * output_fd
)
{
  if (!ctx->used_size)
    return 0; /* Nothing to write. */

  if (!WA_FILE(output_fd, ctx->data, ctx->used_size))
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Unable to write to output file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  ctx->used_size = 0; // Reset
  return 0;
}

/* ### HDMV segments : ##################################################### */

static int _writeSegmentHeader(
  HdmvSegmentsBuildingContext * ctx,
  HdmvSegmentDescriptor seg_desc
)
{
  uint8_t buf[HDMV_SIZE_SEGMENT_HEADER];
  uint32_t off = 0;

  assert(NULL != ctx);

  if (HDMV_MAX_SIZE_SEGMENT_PAYLOAD < seg_desc.segment_length)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Segment size exceed 0x%" PRIX16 " bytes (0x%zu), "
      "broken program.\n",
      HDMV_MAX_SIZE_SEGMENT_PAYLOAD,
      seg_desc.segment_length
    );

  /* [u8 segment_type] */
  WB_ARRAY(buf, off, seg_desc.segment_type);

  /* [u16 segment_length] */
  WB_ARRAY(buf, off, seg_desc.segment_length >> 8);
  WB_ARRAY(buf, off, seg_desc.segment_length);

  return _writeBytesCtx(
    ctx, buf, off
  );
}

static int _writeVideoDescriptor(
  HdmvSegmentsBuildingContext * ctx,
  const HdmvVDParameters param
)
{
  uint8_t buf[HDMV_SIZE_VIDEO_DESCRIPTOR];
  uint32_t off = 0;

  assert(NULL != ctx);

  /* [u16 video_width] */
  WB_ARRAY(buf, off, param.video_width >> 8);
  WB_ARRAY(buf, off, param.video_width);

  /* [u16 video_height] */
  WB_ARRAY(buf, off, param.video_height >> 8);
  WB_ARRAY(buf, off, param.video_height);

  /* [u4 frame_rate_id] [v4 reserved] */
  WB_ARRAY(buf, off, param.frame_rate << 4);

  return _writeBytesCtx(
    ctx, buf, off
  );
}

static int _writeCompositionDescriptor(
  HdmvSegmentsBuildingContext * ctx,
  const HdmvCDParameters param
)
{
  uint8_t buf[HDMV_SIZE_COMPOSITION_DESCRIPTOR];
  uint32_t off = 0;

  assert(NULL != ctx);

  /* [u16 composition_number] */
  WB_ARRAY(buf, off, param.composition_number >> 8);
  WB_ARRAY(buf, off, param.composition_number);

  /* [u2 composition_state] [v6 reserved] */
  WB_ARRAY(buf, off, param.composition_state << 6);

  return _writeBytesCtx(
    ctx, buf, off
  );
}

static int _writeSequenceDescriptor(
  HdmvSegmentsBuildingContext * ctx,
  bool first_in_sequence,
  bool last_in_sequence
)
{
  uint8_t buf[HDMV_SIZE_SEQUENCE_DESCRIPTOR];
  uint32_t off = 0;

  assert(NULL != ctx);

  /* [b1 first_in_sequence] [b1 last_in_sequence] [v6 reserved] */
  WB_ARRAY(
    buf, off,
    (first_in_sequence  << 7)
    | (last_in_sequence << 6)
  );

  return _writeBytesCtx(
    ctx, buf, off
  );
}

/* ###### Palette Definition Segment (0x14) : ############################## */

static inline uint32_t _computeSizePaletteEntries(
  const HdmvPalette pal
)
{
  return
    getNbEntriesHdmvPalette(pal)
    * HDMV_SIZE_PALETTE_DEFINITION_ENTRY
  ;
}

static uint8_t * _buildPaletteDefinitionEntries(
  const HdmvPalette pal,
  uint32_t * size_ret
)
{
  if (HDMV_MAX_NB_PDS_ENTRIES < pal.nb_entries)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN(
      "Bulding error, too many palette entries in PDS (%u < %u), "
      "broken program.\n",
      HDMV_MAX_NB_PDS_ENTRIES,
      pal.nb_entries
    );

  uint32_t exp_len = _computeSizePaletteEntries(pal);
  if (!exp_len) {
    if (NULL != size_ret)
      *size_ret = 0;
    return 0; /* Empty palette */
  }

  uint8_t * arr = (uint8_t *) malloc(exp_len);
  if (NULL == arr)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");
  uint32_t off = 0;

  for (unsigned i = 0; i < HDMV_PAL_SIZE; i++) {
    /* palette_entry() */
    uint32_t ycbcr_value;
    if (getYCbCrEntryIfInUseHdmvPalette(pal, &ycbcr_value, i)) {
      /* [u8 palette_entry_id] */
      WB_ARRAY(arr, off, i);

      /* [u8 Y_value] */
      WB_ARRAY(arr, off, GET_CHANNEL(ycbcr_value, C_Y));

      /* [u8 Cr_value] */
      WB_ARRAY(arr, off, GET_CHANNEL(ycbcr_value, C_CR));

      /* [u8 Cb_value] */
      WB_ARRAY(arr, off, GET_CHANNEL(ycbcr_value, C_CB));

      /* [u8 T_value] */
      WB_ARRAY(arr, off, GET_CHANNEL(ycbcr_value, C_A));
    }
  }

  /* Checking final length */
  if (off != exp_len)
    LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(
      "Bulding error, missing %zu bytes in palette_descriptor(), "
      "broken program.\n",
      exp_len - off
    );

  if (NULL != size_ret)
    *size_ret = off;

  return arr;

free_return:
  free(arr);
  return NULL;
}

static int _writePaletteDescriptor(
  HdmvSegmentsBuildingContext * ctx,
  uint8_t palette_id,
  uint8_t palette_version_number
)
{
  uint8_t buf[HDMV_SIZE_PALETTE_DESCRIPTOR];
  uint32_t off = 0;

  assert(NULL != ctx);

  /* [u8 palette_id] */
  WB_ARRAY(buf, off, palette_id);

  /* [u8 palette_version_number] */
  WB_ARRAY(buf, off, palette_version_number);

  return _writeBytesCtx(
    ctx, buf, off
  );
}

static inline uint32_t _computeSizePDS(
  const HdmvPalette * palettes_arr,
  unsigned nb_palettes
)
{
  assert(NULL != palettes_arr);

  uint32_t size = 0;
  for (unsigned i = 0; i < nb_palettes; i++) {
    size +=
      HDMV_SIZE_SEGMENT_HEADER
      + HDMV_SIZE_PALETTE_DESCRIPTOR
      + _computeSizePaletteEntries(palettes_arr[i])
    ;
  }

  return size;
}

static int _writePaletteDefinitionSegments(
  HdmvSegmentsBuildingContext * ctx,
  const IgsCompilerComposition compo
)
{
  uint8_t * pal_data = NULL;

  assert(NULL != ctx);

  /* Compute required output buffer size. */
  uint32_t segments_size = _computeSizePDS(
    compo.palettes,
    compo.nb_used_palettes
  );

  if (!segments_size) {
    LIBBLU_WARNING("No palette in composition.\n");
    return 0;
  }

  if (_reqBufSizeCtx(ctx, segments_size) < 0)
    return -1;

  for (unsigned i = 0; i < compo.nb_used_palettes; i++) {
    const HdmvPalette pal = compo.palettes[i];

    uint32_t pal_len;
    if (NULL == (pal_data = _buildPaletteDefinitionEntries(pal, &pal_len)))
      return -1;

    HdmvSegmentDescriptor seg_desc = {
      .segment_type   = HDMV_SEGMENT_TYPE_PDS,
      .segment_length = HDMV_SIZE_PALETTE_DESCRIPTOR + pal_len
    };

    if (_writeSegmentHeader(ctx, seg_desc) < 0)
      goto free_return;

    if (_writePaletteDescriptor(ctx, i, pal.version) < 0)
      goto free_return;

    if (0 < pal_len) {
      if (_writeBytesCtx(ctx, pal_data, pal_len) < 0)
        goto free_return;
    }

    free(pal_data);
  }

  return 0;

free_return:
  free(pal_data);

  return -1;
}

/* ###### Object Definition Segment (0x15) : ############################### */

static uint8_t * _buildObjectData(
  HdmvObject * obj,
  uint32_t * size_ret
)
{
  assert(HDMV_OD_MIN_SIZE <= obj->pal_bitmap.width);
  assert(HDMV_OD_MIN_SIZE <= obj->pal_bitmap.height);

  uint16_t width  = obj->pal_bitmap.width;
  uint16_t height = obj->pal_bitmap.height;

  const uint8_t * rle_data = getOrPerformRleHdmvObject(obj);
  if (NULL == rle_data)
    return NULL;
  uint32_t rle_size = obj->rle_size;

  HdmvODParameters param = (HdmvODParameters) {
    .object_data_length  = rle_size + 4U, // RLE size + width/height header fields
    .object_width        = width,
    .object_height       = height
  };

  uint32_t exp_size = computeSizeHdmvObjectData(param);

  uint8_t * arr = (uint8_t *) malloc(exp_size);
  if (NULL == arr)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");
  uint32_t off = 0;

  /* [u24 object_data_length] */
  WB_ARRAY(arr, off, param.object_data_length >> 16);
  WB_ARRAY(arr, off, param.object_data_length >>  8);
  WB_ARRAY(arr, off, param.object_data_length      );

  /* [u16 object_width] */
  WB_ARRAY(arr, off, param.object_width >> 8);
  WB_ARRAY(arr, off, param.object_width     );

  /* [u16 object_height] */
  WB_ARRAY(arr, off, param.object_height >> 8);
  WB_ARRAY(arr, off, param.object_height     );

  /* [vn encoded_data_string] */
  WA_ARRAY(arr, off, rle_data, rle_size);

  /* Checking final length */
  if (off != exp_size)
    LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(
      "Bulding error, missing %zu bytes in object_data(), broken program.\n",
      exp_size - off
    );

  if (NULL != size_ret)
    *size_ret = off;
  return arr;

free_return:
  free(arr);
  return NULL;
}

static int _writeObjectDefinitionSegHeader(
  HdmvSegmentsBuildingContext * ctx,
  HdmvODescParameters odesc
)
{
  uint8_t buf[HDMV_SIZE_OBJECT_DESCRIPTOR];
  uint32_t off = 0;

  assert(NULL != ctx);

  /* [u16 object_id] */
  WB_ARRAY(buf, off, odesc.object_id >> 8);
  WB_ARRAY(buf, off, odesc.object_id     );

  /* [u8 object_version_number] */
  WB_ARRAY(buf, off, odesc.object_version_number);

  return _writeBytesCtx(
    ctx, buf, off
  );
}

uint32_t computeSizeHdmvObjectDefinitionSegments(
  const HdmvObject * objects,
  unsigned nb_objects
)
{
  assert(NULL != objects);

  uint32_t size = 0;
  for (unsigned i = 0; i < nb_objects; i++) {
    const HdmvObject * obj = &objects[i];

    HdmvODParameters obj_param = {
      .object_data_length = obj->rle_size + 4u
    };

    uint32_t objdef_size = computeSizeHdmvObjectData(obj_param);
    uint32_t nb_seg      = objdef_size / HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT;
    uint32_t extra_pl    = objdef_size % HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT;

    size += nb_seg * HDMV_MAX_SIZE_SEGMENT;
    if (0 < extra_pl)
      size += (
        HDMV_SIZE_OD_SEGMENT_HEADER
        + HDMV_SIZE_SEGMENT_HEADER
        + extra_pl
      );
  }

  return size;
}

static int _writeObjectDefinitionSegments(
  HdmvSegmentsBuildingContext * ctx,
  const IgsCompilerComposition compo
)
{
  assert(NULL != ctx);

  LIBBLU_HDMV_SEGBUILD_DEBUG(
    "  Computing required size (performing RLE compression if required).\n"
  );

  /* Compute required output buffer size. */
  uint32_t seg_size = computeSizeHdmvObjectDefinitionSegments(
    compo.objects,
    compo.nb_objects
  );
  if (!seg_size) {
    LIBBLU_WARNING("No object in composition.\n");
    return 0;
  }

  if (_reqBufSizeCtx(ctx, seg_size) < 0)
    return -1;

  LIBBLU_HDMV_SEGBUILD_DEBUG(
    "  Writting segments...\n"
  );

  uint8_t * obj_data = NULL;
  for (unsigned i = 0; i < compo.nb_objects; i++) {
    HdmvObject * obj_ptr = &compo.objects[i];

    uint32_t obj_size;
    if (NULL == (obj_data = _buildObjectData(obj_ptr, &obj_size)))
      return -1;
    uint8_t * obj_frag_data = obj_data;

    /* Build fragments while data remaining */
    bool fst_in_seq = true;
    while (0 < obj_size) {
      uint32_t frag_length = MIN(
        obj_size,
        HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT
      );
      obj_size -= frag_length;

      HdmvSegmentDescriptor seg_desc = {
        .segment_type   = HDMV_SEGMENT_TYPE_ODS,
        .segment_length = HDMV_SIZE_OD_SEGMENT_HEADER + frag_length
      };

      if (_writeSegmentHeader(ctx, seg_desc) < 0)
        goto free_return;

      if (_writeObjectDefinitionSegHeader(ctx, obj_ptr->desc) < 0)
        goto free_return;

      /* sequence_descriptor() */
      bool lst_in_seq = (0 == obj_size);
      if (_writeSequenceDescriptor(ctx, fst_in_seq, lst_in_seq) < 0)
        goto free_return;

      if (_writeBytesCtx(ctx, obj_frag_data, frag_length) < 0)
        goto free_return;

      obj_frag_data += frag_length;
      fst_in_seq = false;
    }

    free(obj_data);
  }

  return 0;

free_return:
  free(obj_data);
  return -1;
}

/* ###### Interactive Composition Segment (0x18) : ######################### */

static uint32_t _appendButtonNeighborInfo(
  const HdmvButtonNeighborInfoParam param,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u16 upper_button_id_ref] */
  WB_ARRAY(arr, off, param.upper_button_id_ref >> 8);
  WB_ARRAY(arr, off, param.upper_button_id_ref);

  /* [u16 left_button_id_ref] */
  WB_ARRAY(arr, off, param.lower_button_id_ref >> 8);
  WB_ARRAY(arr, off, param.lower_button_id_ref);

  /* [u16 lower_button_id_ref] */
  WB_ARRAY(arr, off, param.left_button_id_ref >> 8);
  WB_ARRAY(arr, off, param.left_button_id_ref);

  /* [u16 right_button_id_ref] */
  WB_ARRAY(arr, off, param.right_button_id_ref >> 8);
  WB_ARRAY(arr, off, param.right_button_id_ref);

  return off;
}

static uint32_t _appendButtonNormalStateInfo(
  const HdmvButtonNormalStateInfoParam param,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u16 normal_start_object_id_ref] */
  WB_ARRAY(arr, off, param.start_object_id_ref >> 8);
  WB_ARRAY(arr, off, param.start_object_id_ref);

  /* [u16 normal_end_object_id_ref] */
  WB_ARRAY(arr, off, param.end_object_id_ref >> 8);
  WB_ARRAY(arr, off, param.end_object_id_ref);

  /* [b1 normal_repeat_flag] [b1 normal_complete_flag] [v6 reserved] */
  WB_ARRAY(arr, off, (param.repeat_flag << 7) | (param.complete_flag << 6));

  return off;
}

static uint32_t _appendButtonSelectedStateInfo(
  const HdmvButtonSelectedStateInfoParam param,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u8 selected_state_sound_id_ref] */
  WB_ARRAY(arr, off, param.state_sound_id_ref);

  /* [u16 selected_start_object_id_ref] */
  WB_ARRAY(arr, off, param.start_object_id_ref >> 8);
  WB_ARRAY(arr, off, param.start_object_id_ref);

  /* [u16 selected_end_object_id_ref] */
  WB_ARRAY(arr, off, param.end_object_id_ref >> 8);
  WB_ARRAY(arr, off, param.end_object_id_ref);

  /* [b1 selected_repeat_flag] [b1 selected_complete_flag] [v6 reserved] */
  WB_ARRAY(arr, off, (param.repeat_flag << 7) | (param.complete_flag << 6));

  return off;
}

static uint32_t _appendButtonActivatedStateInfo(
  const HdmvButtonActivatedStateInfoParam param,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u8 activated_state_sound_id_ref] */
  WB_ARRAY(arr, off, param.state_sound_id_ref);

  /* [u16 activated_start_object_id_ref] */
  WB_ARRAY(arr, off, param.start_object_id_ref >> 8);
  WB_ARRAY(arr, off, param.start_object_id_ref);

  /* [u16 activated_end_object_id_ref] */
  WB_ARRAY(arr, off, param.end_object_id_ref >> 8);
  WB_ARRAY(arr, off, param.end_object_id_ref);

  return off;
}

static uint32_t _appendNavigationCommand(
  const HdmvNavigationCommand nav_com,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u32 opcode] */
  WB_ARRAY(arr, off, nav_com.opcode >> 24);
  WB_ARRAY(arr, off, nav_com.opcode >> 16);
  WB_ARRAY(arr, off, nav_com.opcode >>  8);
  WB_ARRAY(arr, off, nav_com.opcode);

  /* [u32 destination] */
  WB_ARRAY(arr, off, nav_com.destination >> 24);
  WB_ARRAY(arr, off, nav_com.destination >> 16);
  WB_ARRAY(arr, off, nav_com.destination >>  8);
  WB_ARRAY(arr, off, nav_com.destination);

  /* [u32 source] */
  WB_ARRAY(arr, off, nav_com.source >> 24);
  WB_ARRAY(arr, off, nav_com.source >> 16);
  WB_ARRAY(arr, off, nav_com.source >>  8);
  WB_ARRAY(arr, off, nav_com.source);

  return off;
}

static uint32_t _appendButton(
  const HdmvButtonParam btn,
  uint8_t * arr,
  uint32_t off
)
{
  assert(NULL != arr);

  /* [u16 button_id] */
  WB_ARRAY(arr, off, btn.button_id >> 8);
  WB_ARRAY(arr, off, btn.button_id);

  /* [u16 button_numeric_select_value] */
  WB_ARRAY(arr, off, btn.button_numeric_select_value >> 8);
  WB_ARRAY(arr, off, btn.button_numeric_select_value);

  /* [b1 auto_action_flag] [v7 reserved] */
  WB_ARRAY(arr, off, btn.auto_action << 7);

  /* [u16 button_horizontal_position] */
  WB_ARRAY(arr, off, btn.button_horizontal_position >> 8);
  WB_ARRAY(arr, off, btn.button_horizontal_position);

  /* [u16 button_vertical_position] */
  WB_ARRAY(arr, off, btn.button_vertical_position >> 8);
  WB_ARRAY(arr, off, btn.button_vertical_position);

  /* neighbor_info() */
  off = _appendButtonNeighborInfo(btn.neighbor_info, arr, off);

  /* normal_state_info() */
  off = _appendButtonNormalStateInfo(btn.normal_state_info, arr, off);

  /* selected_state_info() */
  off = _appendButtonSelectedStateInfo(btn.selected_state_info, arr, off);

  /* activated_state_info() */
  off = _appendButtonActivatedStateInfo(btn.activated_state_info, arr, off);

  /* [u16 number_of_navigation_commands] */
  WB_ARRAY(arr, off, btn.number_of_navigation_commands >> 8);
  WB_ARRAY(arr, off, btn.number_of_navigation_commands);

  for (unsigned i = 0; i < btn.number_of_navigation_commands; i++) {
    /* Navigation_command() */
    HdmvNavigationCommand com = btn.navigation_commands[i];
    off = _appendNavigationCommand(com, arr, off);
  }

  return off;
}

static uint32_t _appendButtonOverlapGroup(
  const HdmvButtonOverlapGroupParameters bog,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u16 default_valid_button_id_ref] */
  WB_ARRAY(arr, off, bog.default_valid_button_id_ref >> 8);
  WB_ARRAY(arr, off, bog.default_valid_button_id_ref);

  /* [u8 number_of_buttons] */
  WB_ARRAY(arr, off, bog.number_of_buttons);

  for (unsigned i = 0; i < bog.number_of_buttons; i++) {
    /* button() */
    if (0 == (off = _appendButton(bog.buttons[i], arr, off)))
      return 0;
  }

  return off;
}

static uint32_t _appendEffectSequence(
  const HdmvEffectSequenceParameters es,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u8 number_of_windows] */
  WB_ARRAY(arr, off, es.number_of_windows);

  for (unsigned i = 0; i < es.number_of_effects; i++) {
    /* window_info() */
    const HdmvWindowInfoParameters window = es.windows[i];

    /* [u8 window_id] */
    WB_ARRAY(arr, off, window.window_id);

    /* [u16 window_horizontal_position] */
    WB_ARRAY(arr, off, window.window_horizontal_position >> 8);
    WB_ARRAY(arr, off, window.window_horizontal_position);

    /* [u16 window_vertical_position] */
    WB_ARRAY(arr, off, window.window_vertical_position >> 8);
    WB_ARRAY(arr, off, window.window_vertical_position);

    /* [u16 window_width] */
    WB_ARRAY(arr, off, window.window_width >> 8);
    WB_ARRAY(arr, off, window.window_width);

    /* [u16 window_height] */
    WB_ARRAY(arr, off, window.window_height >> 8);
    WB_ARRAY(arr, off, window.window_height);
  }

  /* [u8 number_of_effects] */
  WB_ARRAY(arr, off, es.number_of_effects);

  for (unsigned i = 0; i < es.number_of_effects; i++) {
    /* effect_info() */
    const HdmvEffectInfoParameters effect = es.effects[i];

    /* [u24 effect_duration] */
    WB_ARRAY(arr, off, effect.effect_duration >> 16);
    WB_ARRAY(arr, off, effect.effect_duration >>  8);
    WB_ARRAY(arr, off, effect.effect_duration);

    /* [u8 palette_id_ref] */
    WB_ARRAY(arr, off, effect.palette_id_ref);

    /* [u8 number_of_composition_objects] */
    WB_ARRAY(arr, off, effect.number_of_composition_objects);

    for (unsigned j = 0; j < effect.number_of_composition_objects; j++) {
      const HdmvCOParameters obj = effect.composition_objects[j];

      /* [u16 object_id_ref] */
      WB_ARRAY(arr, off, obj.object_id_ref >> 8);
      WB_ARRAY(arr, off, obj.object_id_ref);

      /* [u8 window_id_ref] */
      WB_ARRAY(arr, off, obj.window_id_ref);

      /* [b1 object_cropped_flag] [v7 reserved] */
      WB_ARRAY(arr, off, obj.object_cropped_flag << 7);

      /* [u16 object_horizontal_position] */
      WB_ARRAY(arr, off, obj.composition_object_horizontal_position >> 8);
      WB_ARRAY(arr, off, obj.composition_object_horizontal_position);

      /* [u16 object_vertical_position] */
      WB_ARRAY(arr, off, obj.composition_object_vertical_position >> 8);
      WB_ARRAY(arr, off, obj.composition_object_vertical_position);

      if (obj.object_cropped_flag) {
        /* [u16 object_cropping_horizontal_position] */
        WB_ARRAY(arr, off, obj.object_cropping.horizontal_position >> 8);
        WB_ARRAY(arr, off, obj.object_cropping.horizontal_position);

        /* [u16 object_cropping_vertical_position] */
        WB_ARRAY(arr, off, obj.object_cropping.vertical_position >> 8);
        WB_ARRAY(arr, off, obj.object_cropping.vertical_position);

        /* [u16 object_cropping_width] */
        WB_ARRAY(arr, off, obj.object_cropping.width >> 8);
        WB_ARRAY(arr, off, obj.object_cropping.width);

        /* [u16 object_cropping_height] */
        WB_ARRAY(arr, off, obj.object_cropping.height >> 8);
        WB_ARRAY(arr, off, obj.object_cropping.height);
      }
    }
  }

  return off;
}

static uint32_t _appendPage(
  const HdmvPageParameters page,
  uint8_t * arr,
  uint32_t off
)
{
  /* [u8 page_id] */
  WB_ARRAY(arr, off, page.page_id);

  /* [u8 page_version_number] */
  WB_ARRAY(arr, off, page.page_version_number);

  /* [u64 UO_mask_table()] */
  WB_ARRAY(arr, off, page.UO_mask_table >> 56);
  WB_ARRAY(arr, off, page.UO_mask_table >> 48);
  WB_ARRAY(arr, off, page.UO_mask_table >> 40);
  WB_ARRAY(arr, off, page.UO_mask_table >> 32);
  WB_ARRAY(arr, off, page.UO_mask_table >> 24);
  WB_ARRAY(arr, off, page.UO_mask_table >> 16);
  WB_ARRAY(arr, off, page.UO_mask_table >>  8);
  WB_ARRAY(arr, off, page.UO_mask_table);

  /* In_effects() */
  if (0 == (off = _appendEffectSequence(page.in_effects, arr, off)))
    return 0;

  /* Out_effects() */
  if (0 == (off = _appendEffectSequence(page.out_effects, arr, off)))
    return 0;

  /* [u8 animation_frame_rate_code] */
  WB_ARRAY(arr, off, page.animation_frame_rate_code);

  /* [u16 default_selected_button_id_ref] */
  WB_ARRAY(arr, off, page.default_selected_button_id_ref >> 8);
  WB_ARRAY(arr, off, page.default_selected_button_id_ref);

  /* [u16 default_activated_button_id_ref] */
  WB_ARRAY(arr, off, page.default_activated_button_id_ref >> 8);
  WB_ARRAY(arr, off, page.default_activated_button_id_ref);

  /* [u8 palette_id_ref] */
  WB_ARRAY(arr, off, page.palette_id_ref);

  /* [u8 number_of_BOGs] */
  WB_ARRAY(arr, off, page.number_of_BOGs);

  for (unsigned i = 0; i < page.number_of_BOGs; i++) {
    /* button_overlap_group() */
    if (0 == (off = _appendButtonOverlapGroup(page.bogs[i], arr, off)))
      return 0;
  }

  return off;
}

static uint8_t * _buildInteractiveComposition(
  const HdmvICParameters ic,
  uint32_t * ic_data_length_ret
)
{
  uint32_t ic_data_exp_len = computeSizeHdmvInteractiveComposition(ic);
  uint8_t * arr = (uint8_t *) malloc(ic_data_exp_len);
  if (NULL == arr)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");
  uint32_t off = 0;

  /* [u24 interactive_composition_length] */
  uint32_t interactive_composition_length = ic_data_exp_len - 3;
  WB_ARRAY(arr, off, interactive_composition_length >> 16);
  WB_ARRAY(arr, off, interactive_composition_length >>  8);
  WB_ARRAY(arr, off, interactive_composition_length      );

  /* [b1 stream_model] [b1 user_interface_model] [v6 reserved] */
  WB_ARRAY(
    arr, off,
    ((ic.stream_model           & 0x1) << 7)
    | ((ic.user_interface_model & 0x1) << 6)
  );

  if (ic.stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    /* In Mux related parameters */

    /* [v7 reserved] [u33 composition_time_out_pts] */
    WB_ARRAY(arr, off, ic.composition_time_out_pts >> 32);
    WB_ARRAY(arr, off, ic.composition_time_out_pts >> 24);
    WB_ARRAY(arr, off, ic.composition_time_out_pts >> 16);
    WB_ARRAY(arr, off, ic.composition_time_out_pts >>  8);
    WB_ARRAY(arr, off, ic.composition_time_out_pts);

    /* [v7 reserved] [u33 selection_time_out_pts] */
    WB_ARRAY(arr, off, ic.selection_time_out_pts >> 32);
    WB_ARRAY(arr, off, ic.selection_time_out_pts >> 24);
    WB_ARRAY(arr, off, ic.selection_time_out_pts >> 16);
    WB_ARRAY(arr, off, ic.selection_time_out_pts >>  8);
    WB_ARRAY(arr, off, ic.selection_time_out_pts);
  }

  /* [u24 user_time_out_duration] */
  WB_ARRAY(arr, off, ic.user_time_out_duration >> 16);
  WB_ARRAY(arr, off, ic.user_time_out_duration >>  8);
  WB_ARRAY(arr, off, ic.user_time_out_duration);

  /* [u8 number_of_pages] */
  WB_ARRAY(arr, off, ic.number_of_pages);

  for (unsigned i = 0; i < ic.number_of_pages; i++) {
    /* page() */
    if (0 == (off = _appendPage(ic.pages[i], arr, off)))
      goto free_return;
  }

  /* Checking final length */
  if (off < ic_data_exp_len)
    LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(
      "Bulding error, missing %zu bytes in interactive_composition(), "
      "broken program.\n",
      ic_data_exp_len - off
    );

  if (NULL != ic_data_length_ret)
    *ic_data_length_ret = off;

  return arr;

free_return:
  free(arr);
  return NULL;
}

/** \~english
 * \brief Computes and return size required by
 * interactive composition segments.
 *
 * \param ic_compo_data_length Interactive composition structure size in bytes.
 * \return uint32_t Number of bytes required to store builded structure.
 *
 * Used to define how many bytes are required to store generated
 * Interactive Composition Segments.
 * A interactive_composition() may be splitted (because of its size)
 * between multiple ICS, this function return this global
 * size requirement.
 */
static uint32_t _computeSizeHdmvICSegments(
  uint32_t ic_compo_data_length
)
{
  assert(0 < ic_compo_data_length);

  uint32_t nb_seg =
    ic_compo_data_length
    / HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT
  ;
  uint32_t extra_pl_length =
    ic_compo_data_length
    % HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT
  ;

  uint32_t segments_size = nb_seg * HDMV_MAX_SIZE_SEGMENT;

  if (0 < extra_pl_length)
    segments_size += (
      HDMV_SIZE_SEGMENT_HEADER
      + HDMV_SIZE_IC_SEGMENT_HEADER
      + extra_pl_length
    );

  return segments_size;
}

int _writeICSegments(
  HdmvSegmentsBuildingContext * ctx,
  const IgsCompilerComposition compo
)
{
  assert(NULL != ctx);

  uint32_t compo_size;
  uint8_t * ic_compo_data = _buildInteractiveComposition(
    compo.interactive_composition,
    &compo_size
  );
  if (NULL == ic_compo_data)
    return -1;
  assert(0 < compo_size);

  /* Split interactive_composition() into
  interactive_composition_fragment()s. */

  /* Compute required output buffer size. */
  uint32_t segments_size = _computeSizeHdmvICSegments(compo_size);

  if (_reqBufSizeCtx(ctx, segments_size) < 0)
    goto free_return;

  /* Build fragments while data remaining */
  bool first_in_seq = true;
  uint8_t * ic_compo_frag_ptr = ic_compo_data;

  while (0 < compo_size) {
    uint16_t frag_length = MIN(
      compo_size,
      HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT
    );
    compo_size -= frag_length;

    HdmvSegmentDescriptor seg_desc = {
      .segment_type   = HDMV_SEGMENT_TYPE_ICS,
      .segment_length = HDMV_SIZE_IC_SEGMENT_HEADER + frag_length
    };

    if (_writeSegmentHeader(ctx, seg_desc) < 0)
      goto free_return;

    /* video_descriptor() */
    if (_writeVideoDescriptor(ctx, compo.video_descriptor) < 0)
      goto free_return;

    /* composition_descriptor() */
    if (_writeCompositionDescriptor(ctx, compo.composition_descriptor) < 0)
      goto free_return;

    /* sequence_descriptor() */
    bool last_in_seq = (0 == compo_size);
    if (_writeSequenceDescriptor(ctx, first_in_seq, last_in_seq) < 0)
      goto free_return;

    if (_writeBytesCtx(ctx, ic_compo_frag_ptr, frag_length) < 0)
      goto free_return;

    ic_compo_frag_ptr += frag_length;
    first_in_seq = false;
  }

  free(ic_compo_data);
  return 0;

free_return:
  free(ic_compo_data);
  return -1;
}

/* ###### End Segment (0x80) : ############################################# */

int writeEndOfDisplaySetSegment(
  HdmvSegmentsBuildingContext * ctx
)
{
  if (_reqBufSizeCtx(ctx, HDMV_SIZE_SEGMENT_HEADER) < 0)
    return -1;

  HdmvSegmentDescriptor seg_desc = {
    .segment_type   = HDMV_SEGMENT_TYPE_END,
    .segment_length = 0
  };

  return _writeSegmentHeader(ctx, seg_desc);
}

/* ### Entry point : ####################################################### */

int buildIgsCompiler(
  IgsCompilerData * param,
  const lbc * out_filename
)
{
  HdmvSegmentsBuildingContext ctx = {0};

  assert(NULL != param);
  assert(NULL != out_filename);

  LIBBLU_HDMV_SEGBUILD_INFO(
    "Writing output IGS to '%" PRI_LBCS "'...\n",
    out_filename
  );

  LIBBLU_HDMV_SEGBUILD_DEBUG("Opening output file.\n");
  FILE * output_fd = lbc_fopen(out_filename, "wb");
  if (NULL == output_fd)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Unable to open output file '%" PRI_LBCS "', %s (errno: %d).\n",
      out_filename,
      strerror(errno),
      errno
    );

  LIBBLU_HDMV_SEGBUILD_DEBUG("Building each composition:\n");
  for (unsigned i = 0; i < param->nb_compositions; i++) {
    const IgsCompilerComposition compo = param->compositions[i];

    LIBBLU_HDMV_SEGBUILD_DEBUG(" - Interactive Composition %u:\n", i);

    /* Interactive Composition Segments */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building Interactive Composition Segments...\n");
    if (_writeICSegments(&ctx, compo) < 0)
      goto free_return;

    /* Palette Definition Segments */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building Palette Definition Segments...\n");
    if (_writePaletteDefinitionSegments(&ctx, compo) < 0)
      goto free_return;

    /* Object Definition Segments */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building Object Definition Segments...\n");
    if (_writeObjectDefinitionSegments(&ctx, compo) < 0)
      goto free_return;

    /* END of display set Segment */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building END of display set Segment.\n");
    if (writeEndOfDisplaySetSegment(&ctx) < 0)
      goto free_return;
  }

  LIBBLU_HDMV_SEGBUILD_DEBUG("Writing generated data to ouput buffer.\n");
  if (_writeCtxBufferOnOutput(&ctx, output_fd) < 0)
    goto free_return;

  LIBBLU_HDMV_SEGBUILD_DEBUG("Destroying context.\n");
  cleanHdmvSegmentsBuildingContext(ctx);
  fclose(output_fd);

  LIBBLU_HDMV_SEGBUILD_DEBUG("Completed.\n");
  return 0;

free_return:
  cleanHdmvSegmentsBuildingContext(ctx);
  fclose(output_fd);

  LIBBLU_HDMV_SEGBUILD_DEBUG("Fail.\n");
  return -1;
}

