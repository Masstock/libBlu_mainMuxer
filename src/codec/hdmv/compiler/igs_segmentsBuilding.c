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

HdmvSegmentsBuildingContextPtr createIgsCompilerSegmentsBuldingContext(
  void
)
{
  HdmvSegmentsBuildingContextPtr ctx;

  ctx = (HdmvSegmentsBuildingContextPtr) malloc(
    sizeof(HdmvSegmentsBuildingContext)
  );
  if (NULL == ctx)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");

  ctx->data = NULL;
  ctx->allocatedData = ctx->usedData = 0;

  return ctx;
}

void destroyIgsCompilerSegmentsBuldingContext(
  HdmvSegmentsBuildingContextPtr ctx
)
{
  if (NULL == ctx)
    return;

  free(ctx->data);
  free(ctx);
}

int requestBufferCompilerSegmentsBuldingContext(
  HdmvSegmentsBuildingContextPtr ctx,
  size_t requestedSize
)
{
  size_t newSize;
  uint8_t * newData;

  assert(NULL != ctx);

  if (ctx->usedData + requestedSize < ctx->usedData)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Segments buffer requested size overflow.\n"
    );

  /* fprintf(stderr, "Require %zu\n", requestedSize); */
  for (newSize = 1; newSize < (ctx->usedData + requestedSize) && newSize; )
    newSize <<= 1;

  if (!newSize)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN("Segments buffer size overflow.\n");
  if (newSize <= ctx->allocatedData)
    return 0; /* Nothing to do, allocation is big enough */

  newData = (uint8_t *) realloc(ctx->data, newSize);
  if (NULL == newData)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN("Memory allocation error.\n");

  ctx->data = newData;
  ctx->allocatedData = newSize;

  return 0;
}

int writeBytesHdmvSegmentsBuildingContext(
  HdmvSegmentsBuildingContextPtr ctx,
  uint8_t * data,
  size_t size
)
{

  /* fprintf(stderr, "Write %zu\n", size); */
  if (remainingBufferSizeHdmvSegmentsBuildingContext(ctx) < size)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Output buffer is too small (%zu < %zu bytes), broken program.\n",
      remainingBufferSizeHdmvSegmentsBuildingContext(ctx),
      size
    );

  WA_ARRAY(ctx->data, ctx->usedData, data, size);
  return 0;
}

int writeBufferOnOutputIgsCompilerSegmentsBuldingContext(
  const HdmvSegmentsBuildingContextPtr ctx,
  FILE * output
)
{
  assert(NULL != ctx);
  assert(NULL != output);

  if (!ctx->usedData)
    return 0; /* Nothing to write. */

  if (!WA_FILE(output, ctx->data, ctx->usedData))
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Unable to write to output file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  ctx->usedData = 0; /* Reset */
  return 0;
}

/* ### HDMV segments : ##################################################### */

int writeSegmentHeader(
  HdmvSegmentsBuildingContextPtr ctx,
  uint8_t type,
  size_t length
)
{
  uint8_t buf[HDMV_SIZE_SEGMENT_HEADER];
  size_t off = 0;

  assert(NULL != ctx);

  if (UINT16_MAX < length)
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Segment size exceed 0x%" PRIX16 " bytes (0x%zu), "
      "broken program.\n",
      UINT16_MAX, length
    );

  /* [u8 segment_type] */
  WB_ARRAY(buf, off, type);

  /* [u16 segment_length] */
  WB_ARRAY(buf, off, length >> 8);
  WB_ARRAY(buf, off, length);

  return writeBytesHdmvSegmentsBuildingContext(
    ctx, buf, off
  );
}

int writeVideoDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const HdmvVDParameters param
)
{
  uint8_t buf[HDMV_SIZE_VIDEO_DESCRIPTOR];
  size_t off = 0;

  assert(NULL != ctx);

  /* [u16 video_width] */
  WB_ARRAY(buf, off, param.video_width >> 8);
  WB_ARRAY(buf, off, param.video_width);

  /* [u16 video_height] */
  WB_ARRAY(buf, off, param.video_height >> 8);
  WB_ARRAY(buf, off, param.video_height);

  /* [u4 frame_rate_id] [v4 reserved] */
  WB_ARRAY(buf, off, param.frame_rate << 4);

  return writeBytesHdmvSegmentsBuildingContext(
    ctx, buf, off
  );
}

int writeCompositionDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const HdmvCDParameters param
)
{
  uint8_t buf[HDMV_SIZE_COMPOSITION_DESCRIPTOR];
  size_t off = 0;

  assert(NULL != ctx);

  /* [u16 composition_number] */
  WB_ARRAY(buf, off, param.composition_number >> 8);
  WB_ARRAY(buf, off, param.composition_number);

  /* [u2 composition_state] [v6 reserved] */
  WB_ARRAY(buf, off, param.composition_state << 6);

  return writeBytesHdmvSegmentsBuildingContext(
    ctx, buf, off
  );
}

int writeSequenceDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  bool first_in_sequence,
  bool last_in_sequence
)
{
  uint8_t buf[HDMV_SIZE_SEQUENCE_DESCRIPTOR];
  size_t off = 0;

  assert(NULL != ctx);

  /* [b1 first_in_sequence] [b1 last_in_sequence] [v6 reserved] */
  WB_ARRAY(
    buf, off,
    (first_in_sequence  << 7)
    | (last_in_sequence << 6)
  );

  return writeBytesHdmvSegmentsBuildingContext(
    ctx, buf, off
  );
}

/* ###### Palette Definition Segment (0x14) : ############################## */

uint8_t * buildPaletteDefinitionEntriesIgsCompiler(
  const HdmvPaletteDefinitionPtr pal,
  size_t * size
)
{
  uint8_t * arr;
  size_t off;

  size_t i, palNbEntries;
  size_t computedLen;

  assert(NULL != pal);

  palNbEntries = getNbEntriesHdmvPaletteDefinition(pal);
  if (HDMV_MAX_NB_PDS_ENTRIES < palNbEntries)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN(
      "Bulding error, too many palette entries in PDS (%u < %u), "
      "broken program.\n",
      HDMV_MAX_NB_PDS_ENTRIES,
      palNbEntries
    );

  if (0 == (computedLen = computeSizeHdmvPaletteEntries(pal))) {
    if (NULL != size)
      *size = 0;
    return 0; /* Empty palette */
  }

  if (NULL == (arr = (uint8_t *) malloc(computedLen)))
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");
  off = 0;

  for (i = 0; i < palNbEntries; i++) {
    uint32_t yCbCrA;

    /* palette_entry() */
    if (getYCbCrEntryHdmvPaletteDefinition(pal, i, &yCbCrA) < 0)
      goto free_return;

    /* [u8 palette_entry_id] */
    WB_ARRAY(arr, off, i);

    /* [u8 Y_value] */
    WB_ARRAY(arr, off, GET_CHANNEL(yCbCrA, C_Y));

    /* [u8 Cr_value] */
    WB_ARRAY(arr, off, GET_CHANNEL(yCbCrA, C_CR));

    /* [u8 Cb_value] */
    WB_ARRAY(arr, off, GET_CHANNEL(yCbCrA, C_CB));

    /* [u8 T_value] */
    WB_ARRAY(arr, off, GET_CHANNEL(yCbCrA, C_A));
  }

  /* Checking final length */
  if (off != computedLen)
    LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(
      "Bulding error, missing %zu bytes in palette_descriptor(), "
      "broken program.\n",
      computedLen - off
    );

  if (NULL != size)
    *size = off;

  return arr;

free_return:
  free(arr);
  return NULL;
}

int writePaletteDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  uint8_t id,
  uint8_t versionNb
)
{
  uint8_t buf[HDMV_SIZE_PALETTE_DESCRIPTOR];
  size_t off = 0;

  assert(NULL != ctx);

  /* [u8 palette_id] */
  WB_ARRAY(buf, off, id);

  /* [u8 palette_version_number] */
  WB_ARRAY(buf, off, versionNb);

  return writeBytesHdmvSegmentsBuildingContext(
    ctx, buf, off
  );
}

int writePaletteDefinitionSegmentsIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const IgsCompilerCompositionPtr compo
)
{
  int ret;

  size_t segmentsSize;
  unsigned i;

  uint8_t * palData;

  assert(NULL != ctx);
  assert(NULL != compo);

  /* Compute required output buffer size. */
  segmentsSize = computeSizeHdmvPaletteDefinitionSegments(
    compo->pals,
    compo->nbUsedPals
  );
  if (!segmentsSize) {
    LIBBLU_WARNING("No palette in composition.\n");
    return 0;
  }

  if (requestBufferCompilerSegmentsBuldingContext(ctx, segmentsSize) < 0)
    return -1;

  palData = NULL;
  for (i = 0; i < compo->nbUsedPals; i++) {
    HdmvPaletteDefinitionPtr pal;
    size_t palSize;

    pal = compo->pals[i];
    if (NULL == (palData = buildPaletteDefinitionEntriesIgsCompiler(pal, &palSize)))
      return -1;

    ret = writeSegmentHeader(
      ctx,
      HDMV_SEGMENT_TYPE_PDS,
      HDMV_SIZE_PALETTE_DESCRIPTOR + palSize
    );
    if (ret < 0)
      goto free_return;

    if (writePaletteDescriptorIgsCompiler(ctx, i, pal->version) < 0)
      goto free_return;

    if (0 < palSize) {
      ret = writeBytesHdmvSegmentsBuildingContext(ctx, palData, palSize);
      if (ret < 0)
        goto free_return;
    }

    free(palData);
  }

  return 0;

free_return:
  free(palData);

  return -1;
}

/* ###### Object Definition Segment (0x15) : ############################### */

uint8_t * buildObjectDataIgsCompiler(
  const HdmvPicturePtr pic,
  size_t * size
)
{
  uint8_t * arr;
  size_t off, expectedSize;

  size_t rleSize;
  unsigned width, height;

  const uint8_t * rleData;
  HdmvODParameters param;

  assert(NULL != pic);

  if (0 == (rleSize = getRleSizeHdmvPicture(pic)))
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Empty object RLE data.\n");
  if (HDMV_MAX_OBJ_DATA_LEN < rleSize)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN(
      "Bulding error, RLE data is too long in ODS (%zu < %zu bytes), "
      "broken program.\n",
      HDMV_MAX_OBJ_DATA_LEN,
      rleSize
    );

  getDimensionsHdmvPicture(pic, &width, &height);
  if (UINT16_MAX < width || UINT16_MAX < height)
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN(
      "Bulding error, object dimensions are out of range in ODS, "
      "broken program.\n"
    );

  if (NULL == (rleData = getRleHdmvPicture(pic)))
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Missing RLE data.\n");

  setHdmvObjectDataParameters(&param, rleSize + 4U, width, height);
  /* object_data_length = RLE size + width/height header fields */
  expectedSize = computeSizeHdmvObjectData(param);

  if (NULL == (arr = (uint8_t *) malloc(expectedSize)))
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");
  off = 0;

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
  WA_ARRAY(arr, off, rleData, rleSize);

  /* Checking final length */
  if (off != expectedSize)
    LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(
      "Bulding error, missing %zu bytes in object_data(), broken program.\n",
      expectedSize - off
    );

  if (NULL != size)
    *size = off;

  return arr;

free_return:
  free(arr);
  return NULL;
}

int writeObjectDefinitionSegHeaderIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  uint16_t id,
  uint8_t versionNb
)
{
  uint8_t buf[HDMV_SIZE_OBJECT_DESCRIPTOR];
  size_t off = 0;

  assert(NULL != ctx);

  /* [u16 object_id] */
  WB_ARRAY(buf, off, id >> 8);
  WB_ARRAY(buf, off, id     );

  /* [u8 object_version_number] */
  WB_ARRAY(buf, off, versionNb);

  return writeBytesHdmvSegmentsBuildingContext(
    ctx, buf, off
  );
}

int writeObjectDefinitionSegmentsIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const IgsCompilerCompositionPtr compo
)
{
  int ret;

  size_t segmentsSize;
  unsigned i;

  uint8_t * objData;

  assert(NULL != ctx);
  assert(NULL != compo);

  LIBBLU_HDMV_SEGBUILD_DEBUG(
    "  Computing required size (performing RLE compression if required).\n"
  );

  /* Compute required output buffer size. */
  segmentsSize = computeSizeHdmvObjectDefinitionSegments(
    compo->objPics,
    compo->nbUsedObjPics
  );
  if (!segmentsSize) {
    LIBBLU_WARNING("No object in composition.\n");
    return 0;
  }

  if (requestBufferCompilerSegmentsBuldingContext(ctx, segmentsSize) < 0)
    return -1;

  LIBBLU_HDMV_SEGBUILD_DEBUG(
    "  Writting segments...\n"
  );

  for (i = 0; i < compo->nbUsedObjPics; i++) {
    HdmvPicturePtr obj;
    uint8_t * objFragData;
    uint8_t version;
    size_t objSize = 0;

    bool frstInSeq;

    obj = compo->objPics[i];
    getVersionHdmvPicture(obj, &version);
    if (NULL == (objData = buildObjectDataIgsCompiler(obj, &objSize)))
      return -1;
    objFragData = objData;

    /* Build fragments while data remaining */
    frstInSeq = true;
    while (0 < objSize) {
      size_t objFragSize;

      bool lstInSeq;

      objFragSize = MIN(objSize, HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT);
      objSize -= objFragSize;

      ret = writeSegmentHeader(
        ctx,
        HDMV_SEGMENT_TYPE_ODS,
        HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER + objFragSize
      );
      if (ret < 0)
        goto free_return;

      if (writeObjectDefinitionSegHeaderIgsCompiler(ctx, i, version) < 0)
        goto free_return;

      /* sequence_descriptor() */
      lstInSeq = (0 == objSize);
      if (writeSequenceDescriptorIgsCompiler(ctx, frstInSeq, lstInSeq) < 0)
        goto free_return;


      ret = writeBytesHdmvSegmentsBuildingContext(
        ctx, objFragData, objFragSize
      );
      if (ret < 0)
        goto free_return;

      objFragData += objFragSize;
      frstInSeq = false;
    }

    free(objData);
  }

  return 0;

free_return:
  free(objData);
  return -1;
}

/* ###### Interactive Composition Segment (0x18) : ######################### */

size_t appendButtonIgsCompiler(
  const HdmvButtonParam * param,
  uint8_t * arr,
  size_t off
)
{
  unsigned i;

  assert(NULL != param);
  assert(NULL != arr);

  /* [u16 button_id] */
  WB_ARRAY(arr, off, param->button_id >> 8);
  WB_ARRAY(arr, off, param->button_id);

  /* [u16 button_numeric_select_value] */
  WB_ARRAY(arr, off, param->button_numeric_select_value >> 8);
  WB_ARRAY(arr, off, param->button_numeric_select_value);

  /* [b1 auto_action_flag] [v7 reserved] */
  WB_ARRAY(arr, off, param->auto_action << 7);

  /* [u16 button_horizontal_position] */
  WB_ARRAY(arr, off, param->button_horizontal_position >> 8);
  WB_ARRAY(arr, off, param->button_horizontal_position);

  /* [u16 button_vertical_position] */
  WB_ARRAY(arr, off, param->button_vertical_position >> 8);
  WB_ARRAY(arr, off, param->button_vertical_position);

  /* neighbor_info() */
  {
    /* [u16 upper_button_id_ref] */
    WB_ARRAY(arr, off, param->neighbor_info.upper_button_id_ref >> 8);
    WB_ARRAY(arr, off, param->neighbor_info.upper_button_id_ref);

    /* [u16 left_button_id_ref] */
    WB_ARRAY(arr, off, param->neighbor_info.lower_button_id_ref >> 8);
    WB_ARRAY(arr, off, param->neighbor_info.lower_button_id_ref);

    /* [u16 lower_button_id_ref] */
    WB_ARRAY(arr, off, param->neighbor_info.left_button_id_ref >> 8);
    WB_ARRAY(arr, off, param->neighbor_info.left_button_id_ref);

    /* [u16 right_button_id_ref] */
    WB_ARRAY(arr, off, param->neighbor_info.right_button_id_ref >> 8);
    WB_ARRAY(arr, off, param->neighbor_info.right_button_id_ref);
  }

  /* normal_state_info() */
  {
    /* [u16 normal_start_object_id_ref] */
    WB_ARRAY(arr, off, param->normal_state_info.start_object_id_ref >> 8);
    WB_ARRAY(arr, off, param->normal_state_info.start_object_id_ref);

    /* [u16 normal_end_object_id_ref] */
    WB_ARRAY(arr, off, param->normal_state_info.end_object_id_ref >> 8);
    WB_ARRAY(arr, off, param->normal_state_info.end_object_id_ref);

    /* [b1 normal_repeat_flag] [b1 normal_complete_flag] [v6 reserved] */
    WB_ARRAY(
      arr, off,
      (param->normal_state_info.repeat_flag     << 7)
      | (param->normal_state_info.complete_flag << 6)
    );
  }

  /* selected_state_info() */
  {
    /* [u8 selected_state_sound_id_ref] */
    WB_ARRAY(arr, off, param->selected_state_info.state_sound_id_ref);

    /* [u16 selected_start_object_id_ref] */
    WB_ARRAY(arr, off, param->selected_state_info.start_object_id_ref >> 8);
    WB_ARRAY(arr, off, param->selected_state_info.start_object_id_ref);

    /* [u16 selected_end_object_id_ref] */
    WB_ARRAY(arr, off, param->selected_state_info.end_object_id_ref >> 8);
    WB_ARRAY(arr, off, param->selected_state_info.end_object_id_ref);

    /* [b1 selected_repeat_flag] [b1 selected_complete_flag] [v6 reserved] */
    WB_ARRAY(
      arr, off,
      (param->selected_state_info.repeat_flag     << 7)
      | (param->selected_state_info.complete_flag << 6)
    );
  }

  /* activated_state_info() */
  {
    /* [u8 activated_state_sound_id_ref] */
    WB_ARRAY(arr, off, param->activated_state_info.state_sound_id_ref);

    /* [u16 activated_start_object_id_ref] */
    WB_ARRAY(arr, off, param->activated_state_info.start_object_id_ref >> 8);
    WB_ARRAY(arr, off, param->activated_state_info.start_object_id_ref);

    /* [u16 activated_end_object_id_ref] */
    WB_ARRAY(arr, off, param->activated_state_info.end_object_id_ref >> 8);
    WB_ARRAY(arr, off, param->activated_state_info.end_object_id_ref);
  }

  /* [u16 number_of_navigation_commands] */
  WB_ARRAY(arr, off, param->number_of_navigation_commands >> 8);
  WB_ARRAY(arr, off, param->number_of_navigation_commands);

  {
    HdmvNavigationCommand * com = param->navigation_commands;

    for (i = 0; i < param->number_of_navigation_commands; com = com->next, i++) {
      /* Navigation_command() */
      if (NULL == com)
        LIBBLU_HDMV_SEGBUILD_ERROR_ZRETURN(
          "Unexpected too short commands list (%u/%u).\n",
          i, param->number_of_navigation_commands
        );

      /* [u32 opcode] */
      WB_ARRAY(arr, off, com->opcode >> 24);
      WB_ARRAY(arr, off, com->opcode >> 16);
      WB_ARRAY(arr, off, com->opcode >>  8);
      WB_ARRAY(arr, off, com->opcode);

      /* [u32 destination] */
      WB_ARRAY(arr, off, com->destination >> 24);
      WB_ARRAY(arr, off, com->destination >> 16);
      WB_ARRAY(arr, off, com->destination >>  8);
      WB_ARRAY(arr, off, com->destination);

      /* [u32 source] */
      WB_ARRAY(arr, off, com->source >> 24);
      WB_ARRAY(arr, off, com->source >> 16);
      WB_ARRAY(arr, off, com->source >>  8);
      WB_ARRAY(arr, off, com->source);
    }

    if (NULL != com)
      LIBBLU_HDMV_SEGBUILD_ERROR_ZRETURN(
        "Unexpected too long commands list (%u).\n",
        param->number_of_navigation_commands
      );
  }

  return off;
}

size_t appendButtonOverlapGroupIgsCompiler(
  const HdmvButtonOverlapGroupParameters * param,
  uint8_t * arr,
  size_t off
)
{
  unsigned i;

  assert(NULL != param);
  assert(NULL != arr);

  /* [u16 default_valid_button_id_ref] */
  WB_ARRAY(arr, off, param->default_valid_button_id_ref >> 8);
  WB_ARRAY(arr, off, param->default_valid_button_id_ref);

  /* [u8 number_of_buttons] */
  WB_ARRAY(arr, off, param->number_of_buttons);

  for (i = 0; i < param->number_of_buttons; i++) {
    /* button() */
    if (0 == (off = appendButtonIgsCompiler(param->buttons[i], arr, off)))
      return 0;
  }

  return off;
}

size_t appendEffectSequenceIgsCompiler(
  const HdmvEffectSequenceParameters * param,
  uint8_t * arr,
  size_t off
)
{
  unsigned i, j;

  assert(NULL != param);
  assert(NULL != arr);

  /* [u8 number_of_windows] */
  WB_ARRAY(arr, off, param->number_of_windows);

  for (i = 0; i < param->number_of_effects; i++) {
    HdmvWindowInfoParameters * window;

    /* window_info() */
    window = param->windows[i];

    /* [u8 window_id] */
    WB_ARRAY(arr, off, window->window_id);

    /* [u16 window_horizontal_position] */
    WB_ARRAY(arr, off, window->window_horizontal_position >> 8);
    WB_ARRAY(arr, off, window->window_horizontal_position);

    /* [u16 window_vertical_position] */
    WB_ARRAY(arr, off, window->window_vertical_position >> 8);
    WB_ARRAY(arr, off, window->window_vertical_position);

    /* [u16 window_width] */
    WB_ARRAY(arr, off, window->window_width >> 8);
    WB_ARRAY(arr, off, window->window_width);

    /* [u16 window_height] */
    WB_ARRAY(arr, off, window->window_height >> 8);
    WB_ARRAY(arr, off, window->window_height);
  }

  /* [u8 number_of_effects] */
  WB_ARRAY(arr, off, param->number_of_effects);

  for (i = 0; i < param->number_of_effects; i++) {
    HdmvEffectInfoParameters * effect;

    /* effect_info() */
    effect = param->effects[i];

    /* [u24 effect_duration] */
    WB_ARRAY(arr, off, effect->effect_duration >> 16);
    WB_ARRAY(arr, off, effect->effect_duration >>  8);
    WB_ARRAY(arr, off, effect->effect_duration);

    /* [u8 palette_id_ref] */
    WB_ARRAY(arr, off, effect->palette_id_ref);

    /* [u8 number_of_composition_objects] */
    WB_ARRAY(arr, off, effect->number_of_composition_objects);

    for (j = 0; j < effect->number_of_composition_objects; j++) {
      HdmvCompositionObjectParameters * obj;

      obj = effect->composition_objects[j];

      /* [u16 object_id_ref] */
      WB_ARRAY(arr, off, obj->object_id_ref >> 8);
      WB_ARRAY(arr, off, obj->object_id_ref);

      /* [u8 window_id_ref] */
      WB_ARRAY(arr, off, obj->window_id_ref);

      /* [b1 object_cropped_flag] [v7 reserved] */
      WB_ARRAY(arr, off, obj->object_cropped_flag << 7);

      /* [u16 object_horizontal_position] */
      WB_ARRAY(arr, off, obj->composition_object_horizontal_position >> 8);
      WB_ARRAY(arr, off, obj->composition_object_horizontal_position);

      /* [u16 object_vertical_position] */
      WB_ARRAY(arr, off, obj->composition_object_vertical_position >> 8);
      WB_ARRAY(arr, off, obj->composition_object_vertical_position);

      if (obj->object_cropped_flag) {
        /* [u16 object_cropping_horizontal_position] */
        WB_ARRAY(arr, off, obj->object_cropping.horizontal_position >> 8);
        WB_ARRAY(arr, off, obj->object_cropping.horizontal_position);

        /* [u16 object_cropping_vertical_position] */
        WB_ARRAY(arr, off, obj->object_cropping.vertical_position >> 8);
        WB_ARRAY(arr, off, obj->object_cropping.vertical_position);

        /* [u16 object_cropping_width] */
        WB_ARRAY(arr, off, obj->object_cropping.width >> 8);
        WB_ARRAY(arr, off, obj->object_cropping.width);

        /* [u16 object_cropping_height] */
        WB_ARRAY(arr, off, obj->object_cropping.height >> 8);
        WB_ARRAY(arr, off, obj->object_cropping.height);
      }
    }
  }

  return off;
}

size_t appendPageIgsCompiler(
  const HdmvPageParameters * param,
  uint8_t * arr,
  size_t off
)
{
  unsigned i;

  assert(NULL != param);
  assert(NULL != arr);

  /* [u8 page_id] */
  WB_ARRAY(arr, off, param->page_id);

  /* [u8 page_version_number] */
  WB_ARRAY(arr, off, param->page_version_number);

  /* [u64 UO_mask_table()] */
  WB_ARRAY(arr, off, param->UO_mask_table >> 56);
  WB_ARRAY(arr, off, param->UO_mask_table >> 48);
  WB_ARRAY(arr, off, param->UO_mask_table >> 40);
  WB_ARRAY(arr, off, param->UO_mask_table >> 32);
  WB_ARRAY(arr, off, param->UO_mask_table >> 24);
  WB_ARRAY(arr, off, param->UO_mask_table >> 16);
  WB_ARRAY(arr, off, param->UO_mask_table >>  8);
  WB_ARRAY(arr, off, param->UO_mask_table);

  /* In_effects() */
  if (0 == (off = appendEffectSequenceIgsCompiler(&param->in_effects, arr, off)))
    return 0;

  /* Out_effects() */
  if (0 == (off = appendEffectSequenceIgsCompiler(&param->out_effects, arr, off)))
    return 0;

  /* [u8 animation_frame_rate_code] */
  WB_ARRAY(arr, off, param->animation_frame_rate_code);

  /* [u16 default_selected_button_id_ref] */
  WB_ARRAY(arr, off, param->default_selected_button_id_ref >> 8);
  WB_ARRAY(arr, off, param->default_selected_button_id_ref);

  /* [u16 default_activated_button_id_ref] */
  WB_ARRAY(arr, off, param->default_activated_button_id_ref >> 8);
  WB_ARRAY(arr, off, param->default_activated_button_id_ref);

  /* [u8 palette_id_ref] */
  WB_ARRAY(arr, off, param->palette_id_ref);

  /* [u8 number_of_BOGs] */
  WB_ARRAY(arr, off, param->number_of_BOGs);

  for (i = 0; i < param->number_of_BOGs; i++) {
    /* button_overlap_group() */
    if (0 == (off = appendButtonOverlapGroupIgsCompiler(param->bogs[i], arr, off)))
      return 0;
  }

  return off;
}

uint8_t * buildInteractiveCompositionIgsCompiler(
  const HdmvICParameters * param,
  size_t * compositionSize
)
{
  uint8_t * arr;
  size_t off;

  size_t computedLen, icLen;
  unsigned i;

  assert(NULL != param);

  computedLen = computeSizeHdmvInteractiveComposition(param);
  if (NULL == (arr = (uint8_t *) malloc(computedLen)))
    LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN("Memory allocation error.\n");
  off = 0;

  /* [u24 interactive_composition_length] */
  icLen = computedLen - 3;
  WB_ARRAY(arr, off, icLen >> 16);
  WB_ARRAY(arr, off, icLen >>  8);
  WB_ARRAY(arr, off, icLen      );

  /* [b1 stream_model] [b1 user_interface_model] [v6 reserved] */
  WB_ARRAY(
    arr, off,
    ((param->stream_model          & 0x1) << 7)
    | ((param->user_interface_model & 0x1) << 6)
  );

  if (param->stream_model == HDMV_STREAM_MODEL_OOM) {
    /* Out of Mux related parameters */

    /* [v7 reserved] [u33 composition_time_out_pts] */
    WB_ARRAY(arr, off, param->oomRelatedParam.composition_time_out_pts >> 32);
    WB_ARRAY(arr, off, param->oomRelatedParam.composition_time_out_pts >> 24);
    WB_ARRAY(arr, off, param->oomRelatedParam.composition_time_out_pts >> 16);
    WB_ARRAY(arr, off, param->oomRelatedParam.composition_time_out_pts >>  8);
    WB_ARRAY(arr, off, param->oomRelatedParam.composition_time_out_pts);

    /* [v7 reserved] [u33 selection_time_out_pts] */
    WB_ARRAY(arr, off, param->oomRelatedParam.selection_time_out_pts >> 32);
    WB_ARRAY(arr, off, param->oomRelatedParam.selection_time_out_pts >> 24);
    WB_ARRAY(arr, off, param->oomRelatedParam.selection_time_out_pts >> 16);
    WB_ARRAY(arr, off, param->oomRelatedParam.selection_time_out_pts >>  8);
    WB_ARRAY(arr, off, param->oomRelatedParam.selection_time_out_pts);
  }

  /* [u24 user_time_out_duration] */
  WB_ARRAY(arr, off, param->user_time_out_duration >> 16);
  WB_ARRAY(arr, off, param->user_time_out_duration >>  8);
  WB_ARRAY(arr, off, param->user_time_out_duration);

  /* [u8 number_of_pages] */
  WB_ARRAY(arr, off, param->number_of_pages);

  for (i = 0; i < param->number_of_pages; i++) {
    /* page() */
    if (0 == (off = appendPageIgsCompiler(param->pages[i], arr, off)))
      goto free_return;
  }

  /* Checking final length */
  if (off < computedLen)
    LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(
      "Bulding error, missing %zu bytes in interactive_composition(), "
      "broken program.\n",
      computedLen - off
    );

  if (NULL != compositionSize)
    *compositionSize = off;

  return arr;

free_return:
  free(arr);
  return NULL;
}

int writeInteractiveCompositionSegmentsIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const IgsCompilerCompositionPtr compo
)
{
  uint8_t * compoArr, * compoFragArr;
  size_t compoSize, segmentsSize;

  bool frstInSeq;

  assert(NULL != ctx);
  assert(NULL != compo);

  compoArr = buildInteractiveCompositionIgsCompiler(
    &compo->interactiveComposition,
    &compoSize
  );
  if (NULL == compoArr)
    return -1;

  /* Split interactive_composition() into
  interactive_composition_fragment()s. */

  /* Compute required output buffer size. */
  segmentsSize = computeSizeHdmvInteractiveCompositionSegments(
    NULL, compoSize
  );

  if (requestBufferCompilerSegmentsBuldingContext(ctx, segmentsSize) < 0)
    goto free_return;

  /* Build fragments while data remaining */
  frstInSeq = true;
  compoFragArr = compoArr;

  assert(0 < compoSize);
  while (0 < compoSize) {
    int ret;

    size_t compoFragSize;
    bool lstInSeq;

    compoFragSize = MIN(
      compoSize,
      HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT
    );
    compoSize -= compoFragSize;

    ret = writeSegmentHeader(
      ctx,
      HDMV_SEGMENT_TYPE_ICS,
      HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER + compoFragSize
    );
    if (ret < 0)
      goto free_return;

    /* video_descriptor() */
    if (writeVideoDescriptorIgsCompiler(ctx, compo->video_descriptor) < 0)
      goto free_return;

    /* composition_descriptor() */
    if (writeCompositionDescriptorIgsCompiler(ctx, compo->composition_descriptor) < 0)
      goto free_return;

    /* sequence_descriptor() */
    lstInSeq = (0 == compoSize);
    if (writeSequenceDescriptorIgsCompiler(ctx, frstInSeq, lstInSeq) < 0)
      goto free_return;

    ret = writeBytesHdmvSegmentsBuildingContext(
      ctx, compoFragArr, compoFragSize
    );
    if (ret < 0)
      goto free_return;

    compoFragArr += compoFragSize;
    frstInSeq = false;
  }

  free(compoArr);
  return 0;

free_return:
  free(compoArr);
  return -1;
}

/* ###### End Segment (0x80) : ############################################# */

int writeEndOfDisplaySetSegment(
  HdmvSegmentsBuildingContextPtr ctx
)
{
  size_t requiredSize = HDMV_SIZE_SEGMENT_HEADER;

  if (requestBufferCompilerSegmentsBuldingContext(ctx, requiredSize) < 0)
    return -1;
  return writeSegmentHeader(ctx, HDMV_SEGMENT_TYPE_END, 0);
}

/* ### Entry point : ####################################################### */

int buildIgsCompiler(
  IgsCompilerData * param,
  const lbc * outputFilename
)
{
  unsigned i;

  FILE * output;
  HdmvSegmentsBuildingContextPtr ctx;

  assert(NULL != param);

  LIBBLU_HDMV_SEGBUILD_INFO(
    "Writing output IGS to '%" PRI_LBCS "'...\n",
    outputFilename
  );

  LIBBLU_HDMV_SEGBUILD_DEBUG("Opening output file.\n");
  if (NULL == (output = lbc_fopen(outputFilename, "wb")))
    LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(
      "Unable to open output file '%" PRI_LBCS "', %s (errno: %d).\n",
      outputFilename,
      strerror(errno),
      errno
    );

  LIBBLU_HDMV_SEGBUILD_DEBUG("Creating context.\n");
  if (NULL == (ctx = createIgsCompilerSegmentsBuldingContext()))
    goto free_return;

  LIBBLU_HDMV_SEGBUILD_DEBUG("Building each composition:\n");
  for (i = 0; i < param->nbCompo; i++) {
    IgsCompilerCompositionPtr compo;

    compo = param->compositions[i];
    assert(NULL != compo);

    LIBBLU_HDMV_SEGBUILD_DEBUG(" - Interactive Composition %u:\n", i);

    /* Interactive Composition Segments */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building Interactive Composition Segments...\n");
    if (writeInteractiveCompositionSegmentsIgsCompiler(ctx, compo) < 0)
      goto free_return;

    /* Palette Definition Segments */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building Palette Definition Segments...\n");
    if (writePaletteDefinitionSegmentsIgsCompiler(ctx, compo) < 0)
      goto free_return;

    /* Object Definition Segments */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building Object Definition Segments...\n");
    if (writeObjectDefinitionSegmentsIgsCompiler(ctx, compo) < 0)
      goto free_return;

    /* END of display set Segment */
    LIBBLU_HDMV_SEGBUILD_DEBUG("  Building END of display set Segment.\n");
    if (writeEndOfDisplaySetSegment(ctx) < 0)
      goto free_return;
  }

  LIBBLU_HDMV_SEGBUILD_DEBUG("Writing generated data to ouput buffer.\n");
  if (writeBufferOnOutputIgsCompilerSegmentsBuldingContext(ctx, output) < 0)
    goto free_return;

  LIBBLU_HDMV_SEGBUILD_DEBUG("Destroying context.\n");
  destroyIgsCompilerSegmentsBuldingContext(ctx);
  fclose(output);

  LIBBLU_HDMV_SEGBUILD_DEBUG("Completed.\n");
  return 0;

free_return:
  destroyIgsCompilerSegmentsBuldingContext(ctx);
  fclose(output);

  LIBBLU_HDMV_SEGBUILD_DEBUG("Fail.\n");
  return -1;
}

