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

HdmvSegmentType checkHdmvSegmentType(
  uint8_t type,
  HdmvStreamType streamType
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
    if (values[i].type == type) {
      if (HDMV_STREAM_TYPE_MASK(streamType) & values[i].streamTypesMask)
        return values[i].type;

      LIBBLU_HDMV_COM_ERROR(
        "Unexpected segment type %s (0x%02X) for a %s stream.\n",
        HdmvSegmentTypeStr(type),
        type,
        HdmvStreamTypeStr(streamType)
      );
      return HDMV_SEGMENT_TYPE_ERROR;
    }
  }

  LIBBLU_HDMV_COM_ERROR(
    "Unknown segment type 0x%02X.\n",
    type
  );
  return HDMV_SEGMENT_TYPE_ERROR;
}

/* ###### HDMV Interactive Composition Segment : ########################### */
/* ######### Interactive Composition : ##################################### */

static int _checkUpdateHdmvICParameters(
  HdmvICParameters oldest,
  HdmvICParameters newest
)
{
  if (oldest.stream_model != newest.stream_model)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ICS, "
      "compositions 'stream_model' shall remain the same accross "
      "the bitstream.\n"
    );

  if (oldest.user_interface_model != newest.user_interface_model)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ICS, "
      "compositions 'user_interface_model' shall remain the same accross "
      "the bitstream.\n"
    );

  return 0;
}

int updateHdmvICParameters(
  HdmvICParameters * dst,
  HdmvICParameters src
)
{
  unsigned i, j;

  if (_checkUpdateHdmvICParameters(*dst, src) < 0)
    return -1;

  if (src.stream_model == HDMV_STREAM_MODEL_MULTIPLEXED) {
    dst->oomRelatedParam.composition_time_out_pts =
      src.oomRelatedParam.composition_time_out_pts;
    dst->oomRelatedParam.selection_time_out_pts =
      src.oomRelatedParam.selection_time_out_pts;
  }
  dst->user_time_out_duration = src.user_time_out_duration;

  for (i = 0; i < src.number_of_pages; i++) {
    HdmvPageParameters * newPage = src.pages[i];
    bool idFound = false;

    for (j = 0; j < dst->number_of_pages; j++) {
      if (dst->pages[j]->page_id == newPage->page_id) {
        idFound = true;
        break;
      }
    }

    if (idFound) {
      HdmvPageParameters * oldPage = dst->pages[j];

      // Check page_version_number:
      if (
        newPage->page_version_number
        != ((oldPage->page_version_number + 1) & 0xFF)
      ) {
        LIBBLU_HDMV_FAIL_RETURN(
          "Invalid updated ICS, "
          "'page_version_number' is not equal or incremented correctly "
          "(old: 0x%02X / new: 0x%02X).\n",
          oldPage->page_version_number,
          newPage->page_version_number
        );
      }

      dst->pages[j] = newPage;
    }
    else {

      // Check number of pages:
      if (HDMV_MAX_NB_ICS_PAGES <= dst->number_of_pages)
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Invalid updated ICS, "
          "too many pages present in Display Set.\n"
        );

      // Check page_version_number:
      if (newPage->page_version_number != 0x0)
        LIBBLU_HDMV_FAIL_RETURN(
          "Invalid updated ICS, "
          "update introduced pages shall have 'page_version_number' fixed "
          "to 0x0 ('page_id': %u, value: %u).\n",
          newPage->page_id,
          newPage->page_version_number
        );

      dst->pages[dst->number_of_pages++] = newPage;
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
  HdmvPdsSegmentParameters src
)
{
  unsigned i;

  bool shallBeIdentical;

  if (_checkUpdateHdmvPDParameters(dst->palette_descriptor, src.palette_descriptor, &shallBeIdentical) < 0)
    return -1;

  if (shallBeIdentical) {
    if (dst->number_of_palette_entries != src.number_of_palette_entries)
      LIBBLU_HDMV_FAIL_RETURN(
        "Invalid updated PDS, "
        "palettes sharing same 'palette_id' and 'palette_version_number' "
        "shall have the same amount of entries.\n"
      );

    for (i = 0; i < src.number_of_palette_entries; i++) {
      if (!constantHdmvPaletteEntryParameters(dst->palette_entries[i], src.palette_entries[i]))
        LIBBLU_HDMV_FAIL_RETURN(
        "Invalid updated PDS, "
        "palettes sharing same 'palette_id' and 'palette_version_number' "
        "entries shall be identical.\n"
      );
    }

    return 0;
  }

  for (i = 0; i < src.number_of_palette_entries; i++) {
    if (src.palette_entries[i].updated)
      dst->palette_entries[i] = src.palette_entries[i];
  }
  dst->palette_descriptor = src.palette_descriptor;

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
  HdmvODParameters src
)
{
  int ret;

  ret = _checkUpdateHdmvODescParameters(
    dst->object_descriptor,
    src.object_descriptor
  );
  if (ret < 0)
    return -1;

  if (0 < ret) {
    /* The object coded data size shall be identical */
    if (dst->object_data_length != src.object_data_length)
      LIBBLU_HDMV_FAIL_RETURN(
        "Invalid updated ODS, object_data_length of ODS sharing same "
        "version shall remain identical (old: %zu / new :%zu).\n",
        dst->object_data_length,
        src.object_data_length
      );
  }

  if (dst->object_width != src.object_width || dst->object_height != src.object_height)
    LIBBLU_HDMV_FAIL_RETURN(
      "Invalid updated ODS, "
      "picture dimensions mismatch (%ux%u / %ux%u).\n",
      dst->object_width,
      dst->object_height,
      src.object_width,
      src.object_height
    );

  *dst = src;
  return 0;
}