#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hdmv_data.h"

/* ### HDMV Segments : ##################################################### */

bool isValidHdmvSegmentType(
  uint8_t type
)
{
  unsigned i;

  static const uint8_t types[] = {
    HDMV_SEGMENT_TYPE_PDS,
    HDMV_SEGMENT_TYPE_ODS,
    HDMV_SEGMENT_TYPE_PCS,
    HDMV_SEGMENT_TYPE_WDS,
    HDMV_SEGMENT_TYPE_ICS,
    HDMV_SEGMENT_TYPE_END
  };

  for (i = 0; i < ARRAY_SIZE(types); i++) {
    if (types[i] == type)
      return true;
  }

  return false;
}

int checkHdmvSegmentType(
  uint8_t segment_type_value,
  HdmvStreamType stream_type,
  HdmvSegmentType * segment_type
)
{
  unsigned i;

  static const struct {
    HdmvSegmentType type;
    uint8_t streamTypesMask;
  } values[] = {
#define D_(t, m)  {.type = (t), .streamTypesMask = (m)}
    D_(HDMV_SEGMENT_TYPE_PDS, HDMV_STREAM_TYPE_MASK_COMMON),
    D_(HDMV_SEGMENT_TYPE_ODS, HDMV_STREAM_TYPE_MASK_COMMON),
    D_(HDMV_SEGMENT_TYPE_PCS, HDMV_STREAM_TYPE_MASK_PGS),
    D_(HDMV_SEGMENT_TYPE_WDS, HDMV_STREAM_TYPE_MASK_PGS),
    D_(HDMV_SEGMENT_TYPE_ICS, HDMV_STREAM_TYPE_MASK_IGS),
    D_(HDMV_SEGMENT_TYPE_END, HDMV_STREAM_TYPE_MASK_COMMON)
#undef D_
  };

  for (i = 0; i < ARRAY_SIZE(values); i++) {
    if (values[i].type == segment_type_value) {
      if (HDMV_STREAM_TYPE_MASK(stream_type) & values[i].streamTypesMask) {
        *segment_type = values[i].type;
        return 0;
      }

      LIBBLU_HDMV_COM_ERROR(
        "Unexpected segment type %s (0x%02X) for a %s stream.\n",
        HdmvSegmentTypeStr(segment_type_value),
        segment_type_value,
        HdmvStreamTypeStr(stream_type)
      );
      return -1;
    }
  }

  LIBBLU_HDMV_COM_ERROR_RETURN(
    "Unknown segment type 0x%02X.\n",
    segment_type_value
  );
}

/* ###### HDMV Windows Definition Segment : ################################ */
/* ######### Windows Definition : ########################################## */

int updateHdmvWDParameters(
  HdmvWDParameters * dst,
  const HdmvWDParameters * src
)
{
  if (dst->number_of_windows != src->number_of_windows) {
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid Epoch continuous WDS, the number of windows is different.\n"
    );
    return 0;
  }

  bool valid = true;
  for (unsigned i = 0; i < src->number_of_windows; i++) {
    const HdmvWindowInfoParameters * dw = dst->windows[i];
    const HdmvWindowInfoParameters * sw = src->windows[i];

    if (dw->window_id != sw->window_id) {
      LIBBLU_HDMV_FAIL_RETURN(
        "Different 'window_id' (from 0x%02X to 0x%02X on window #%u.\n",
        dw->window_id,
        sw->window_id
      );
      valid = false;
    }

    if (dw->window_horizontal_position != sw->window_horizontal_position) {
      LIBBLU_HDMV_FAIL_RETURN(
        "Different 'window_horizontal_position' "
        "(from 0x%04X to 0x%04X on window #%u.\n",
        dw->window_horizontal_position,
        sw->window_horizontal_position
      );
      valid = false;
    }

    if (dw->window_vertical_position != sw->window_vertical_position) {
      LIBBLU_HDMV_FAIL_RETURN(
        "Different 'window_vertical_position' "
        "(from 0x%04X to 0x%04X on window #%u.\n",
        dw->window_vertical_position,
        sw->window_vertical_position
      );
      valid = false;
    }

    if (dw->window_width != sw->window_width) {
      LIBBLU_HDMV_FAIL_RETURN(
        "Different 'window_width' "
        "(from 0x%04X to 0x%04X on window #%u.\n",
        dw->window_width,
        sw->window_width
      );
      valid = false;
    }

    if (dw->window_height != sw->window_height) {
      LIBBLU_HDMV_FAIL_RETURN(
        "Different 'window_height' "
        "(from 0x%04X to 0x%04X on window #%u.\n",
        dw->window_height,
        sw->window_height
      );
      valid = false;
    }
  }

  if (!valid)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid Epoch continuous WDS, parameters are different (see above).\n"
    );

  return 0;
}

/* ###### HDMV Interactive Composition Segment : ########################### */
/* ######### Interactive Composition : ##################################### */

static int _checkUpdateHdmvICParameters(
  const HdmvICParameters * previous,
  const HdmvICParameters * current
)
{
  if (previous->stream_model != current->stream_model)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ICS, "
      "compositions 'stream_model' shall remain the same accross "
      "the bitstream.\n"
    );

  if (previous->user_interface_model != current->user_interface_model)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ICS, "
      "compositions 'user_interface_model' shall remain the same accross "
      "the bitstream.\n"
    );

  return 0;
}

int updateHdmvICParameters(
  HdmvICParameters * dst,
  const HdmvICParameters * src
)
{
  unsigned i, j;

  if (_checkUpdateHdmvICParameters(dst, src) < 0)
    return -1;

  if (src->stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    dst->oomRelatedParam.composition_time_out_pts =
      src->oomRelatedParam.composition_time_out_pts;
    dst->oomRelatedParam.selection_time_out_pts =
      src->oomRelatedParam.selection_time_out_pts;
  }
  dst->user_time_out_duration = src->user_time_out_duration;

  for (i = 0; i < src->number_of_pages; i++) {
    HdmvPageParameters * new_page = src->pages[i];
    bool idFound = false;

    for (j = 0; j < dst->number_of_pages; j++) {
      if (dst->pages[j]->page_id == new_page->page_id) {
        idFound = true;
        break;
      }
    }

    if (idFound) {
      HdmvPageParameters * prev_page = dst->pages[j];
      unsigned expected_vn = (prev_page->page_version_number + 1) & 0xFF;

      // Check page_version_number:
      if (new_page->page_version_number != expected_vn) {
        LIBBLU_HDMV_FAIL_RETURN(
          "Invalid updated ICS, "
          "'page_version_number' is not equal or incremented correctly "
          "(old: 0x%02X / new: 0x%02X).\n",
          prev_page->page_version_number,
          new_page->page_version_number
        );
      }

      dst->pages[j] = new_page;
    }
    else {

      // Check number of pages:
      if (HDMV_MAX_NB_ICS_PAGES <= dst->number_of_pages)
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Invalid updated ICS, "
          "too many pages present in Display Set.\n"
        );

      // Check page_version_number:
      if (new_page->page_version_number != 0x0)
        LIBBLU_HDMV_FAIL_RETURN(
          "Invalid updated ICS, "
          "update introduced pages shall have 'page_version_number' fixed "
          "to 0x0 ('page_id': %u, value: %u).\n",
          new_page->page_id,
          new_page->page_version_number
        );

      dst->pages[dst->number_of_pages++] = new_page;
    }
  }

  return 0;
}

/* ###### HDMV Palette Definition Segment : ################################ */
/* ######### Palette Descriptor : ########################################## */

static int _checkUpdateHdmvPDParameters(
  HdmvPDParameters old,
  HdmvPDParameters nw,
  bool * identical
)
{
  bool shallBeIdentical, isUpdate;

  if (old.palette_id != nw.palette_id)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated PDS, 'palette_id' mismatch (0x%02X / 0x%02X).\n",
      old.palette_id,
      nw.palette_id
    );

  shallBeIdentical = (old.palette_version_number == nw.palette_version_number);
  isUpdate = ((old.palette_version_number + 1) == nw.palette_version_number);

  if (!shallBeIdentical && !isUpdate) {
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated PDS, "
      "'palette_version_number' is not equal or incremented correctly "
      "(old: 0x%02X / new: 0x%02X).\n",
      old.palette_version_number,
      nw.palette_version_number
    );
  }

  if (NULL != identical)
    *identical = shallBeIdentical;

  return 0;
}

/* ######################################################################### */

int updateHdmvPdsSegmentParameters(
  HdmvPdsSegmentParameters * dst,
  const HdmvPdsSegmentParameters * src
)
{
  unsigned i;

  bool shallBeIdentical;

  if (_checkUpdateHdmvPDParameters(dst->palette_descriptor, src->palette_descriptor, &shallBeIdentical) < 0)
    return -1;

  if (shallBeIdentical) {
    if (dst->number_of_palette_entries != src->number_of_palette_entries)
      LIBBLU_HDMV_FAIL_RETURN(
        "Invalid updated PDS, "
        "palettes sharing same 'palette_id' and 'palette_version_number' "
        "shall have the same amount of entries.\n"
      );

    for (i = 0; i < src->number_of_palette_entries; i++) {
      if (!constantHdmvPaletteEntryParameters(dst->palette_entries[i], src->palette_entries[i])) {
        LIBBLU_HDMV_FAIL_RETURN(
          "Invalid updated PDS, "
          "palettes sharing same 'palette_id' and 'palette_version_number' "
          "entries shall be identical.\n"
        );
        break;
      }
    }

    return 0;
  }

  for (i = 0; i < src->number_of_palette_entries; i++) {
    if (src->palette_entries[i].updated)
      dst->palette_entries[i] = src->palette_entries[i];
  }
  dst->palette_descriptor = src->palette_descriptor;

  return 0;
}

/* ###### HDMV Object Definition Segment : ################################# */
/* ######### Object Descriptor : ########################################### */

static int _checkUpdateHdmvODescParameters(
  HdmvODescParameters old,
  HdmvODescParameters nw
)
{
  if (old.object_id != nw.object_id)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ODS, 'object_id' mismatch (0x%02X / 0x%02X).\n",
      old.object_id,
      nw.object_id
    );

  if (old.object_version_number == nw.object_version_number)
    return 1; /* The new object shall be identical to the previous one. */
  else if (((old.object_version_number + 1) & 0xFF) != nw.object_version_number)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ODS, "
      "'object_version_number' is not incremented correctly "
      "(old: 0x%02X / new: 0x%02X).\n",
      old.object_version_number,
      nw.object_version_number
    );

  return 0;
}

/* ######### Object Data : ################################################# */

int updateHdmvObjectDataParameters(
  HdmvODParameters * dst,
  const HdmvODParameters * src
)
{
  int ret;

  ret = _checkUpdateHdmvODescParameters(
    dst->object_descriptor,
    src->object_descriptor
  );
  if (ret < 0)
    return -1;

  if (0 < ret) {
    /* The object coded data size shall be identical */
    if (dst->object_data_length != src->object_data_length)
      LIBBLU_HDMV_FAIL_RETURN(
        "Invalid updated ODS, object_data_length of ODS sharing same "
        "version shall remain identical (old: %zu / new :%zu).\n",
        dst->object_data_length,
        src->object_data_length
      );
  }

  if (dst->object_width != src->object_width || dst->object_height != src->object_height)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ODS, picture dimensions mismatch (%ux%u / %ux%u).\n",
      dst->object_width,
      dst->object_height,
      src->object_width,
      src->object_height
    );

  memcpy(dst, src, sizeof(HdmvODParameters));
  return 0;
}