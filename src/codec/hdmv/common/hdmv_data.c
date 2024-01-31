#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hdmv_data.h"

static bool _interpretVersionNumber(
  uint8_t prev_version_number,
  uint8_t new_version_number,
  bool * is_same_ret
)
{
  assert(NULL != is_same_ret);

  *is_same_ret   = ( prev_version_number      == new_version_number);
  bool is_update = ((prev_version_number + 1) == new_version_number);

  return *is_same_ret || is_update;
}

/* ### HDMV Segments : ##################################################### */

bool isValidHdmvSegmentType(
  uint8_t type
)
{
  static const uint8_t types[] = {
    HDMV_SEGMENT_TYPE_PDS,
    HDMV_SEGMENT_TYPE_ODS,
    HDMV_SEGMENT_TYPE_PCS,
    HDMV_SEGMENT_TYPE_WDS,
    HDMV_SEGMENT_TYPE_ICS,
    HDMV_SEGMENT_TYPE_END
  };

  for (unsigned i = 0; i < ARRAY_SIZE(types); i++) {
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

  for (unsigned i = 0; i < ARRAY_SIZE(values); i++) {
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
  HdmvWDParameters * prev,
  const HdmvWDParameters * cur
)
{
  if (prev->number_of_windows != cur->number_of_windows)
    LIBBLU_HDMV_PARSER_ERROR(
      "Invalid updated WDS value with mismatch on 'number_of_windows', "
      "values shall remain constant over the entire Epoch.\n"
    );

  bool invalid = false;

  for (unsigned i = 0; i < cur->number_of_windows; i++) {
    const HdmvWindowInfoParameters * prevw = prev->windows[i];
    const HdmvWindowInfoParameters * curw  = cur->windows[i];

#define CHECK_PARAM(name)                                                     \
  do {                                                                        \
    if (prevw->name != curw->name) {                                          \
      LIBBLU_HDMV_PARSER_ERROR(                                               \
        "Invalid updated WDS value, "                                         \
        "'" #name "' shall remain the same accross the Epoch.\n"              \
      );                                                                      \
      invalid = true;                                                         \
    }                                                                         \
  } while (0)

    CHECK_PARAM(window_id);
    CHECK_PARAM(window_horizontal_position);
    CHECK_PARAM(window_vertical_position);
    CHECK_PARAM(window_width);
    CHECK_PARAM(window_height);
  }

#undef CHECK_PARAM

  if (invalid)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Values in Window Definition Segment must "
      "remain constant over the entire Epoch.\n"
    );

  return 0;
}

/* ###### HDMV Presentation Composition Segment : ########################## */

static int _checkUpdateHdmvPCParameters(
  const HdmvPCParameters * prev,
  const HdmvPCParameters * cur
)
{
  if (!cur->palette_update_flag)
    return 0; // No specific update check for non palette update

  bool invalid = false;

  if (prev->number_of_composition_objects != cur->number_of_composition_objects)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid updated PCS value, 'number_of_composition_objects' "
      "shall remain the same for palette update.\n"
    );

  for (unsigned i = 0; i < cur->number_of_composition_objects; i++) {
    const HdmvCOParameters * prev_co = prev->composition_objects[i];
    const HdmvCOParameters * cur_co  = cur->composition_objects[i];

#define CHECK_PARAM(name)                                                     \
  do {                                                                        \
    if (prev_co->name != cur_co->name) {                                      \
      LIBBLU_HDMV_PARSER_ERROR(                                               \
        "Invalid updated PCS value, '" #name "' of composition object #%u "   \
        "shall remain the same for palette update.\n", i                      \
      );                                                                      \
      invalid = true;                                                         \
    }                                                                         \
  } while (0)

    CHECK_PARAM(object_id_ref);
    CHECK_PARAM(window_id_ref);
    CHECK_PARAM(object_cropped_flag);
    CHECK_PARAM(forced_on_flag);
    CHECK_PARAM(composition_object_horizontal_position);
    CHECK_PARAM(composition_object_vertical_position);
    CHECK_PARAM(object_cropping.horizontal_position);
    CHECK_PARAM(object_cropping.vertical_position);
    CHECK_PARAM(object_cropping.width);
    CHECK_PARAM(object_cropping.height);
  }

#undef CHECK_PARAM

  if (invalid)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid values present in Presentation Composition Segment marked "
      "as palette update only.\n"
    );

  return 0;
}

int updateHdmvPCParameters(
  HdmvPCParameters * dst,
  const HdmvPCParameters * src
)
{
  if (_checkUpdateHdmvPCParameters(dst, src) < 0)
    return -1;

  memcpy(dst, src, sizeof(HdmvPCParameters));
  return 0;
}

/* ###### HDMV Interactive Composition Segment : ########################### */
/* ######### Interactive Composition : ##################################### */

static int _checkUpdateHdmvICParameters(
  const HdmvICParameters * prev,
  const HdmvICParameters * cur
)
{
  bool invalid = false;

#define CHECK_PARAM(name)                                                     \
  do {                                                                        \
    if (prev->name != cur->name) {                                            \
      LIBBLU_HDMV_PARSER_ERROR(                                               \
        "Invalid updated ICS value, "                                         \
        "'" #name "' shall remain the same accross the Epoch.\n"              \
      );                                                                      \
      invalid = true;                                                         \
    }                                                                         \
  } while (0)

  CHECK_PARAM(stream_model);
  CHECK_PARAM(user_interface_model);
  CHECK_PARAM(composition_time_out_pts);
  CHECK_PARAM(selection_time_out_pts);
  CHECK_PARAM(user_time_out_duration);
  CHECK_PARAM(number_of_pages);

#undef CHECK_PARAM

  if (invalid)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid values present in Interactive Composition Segment.\n"
    );

  return 0;
}

int updateHdmvICParameters(
  HdmvICParameters * dst,
  const HdmvICParameters * src
)
{
  if (_checkUpdateHdmvICParameters(dst, src) < 0)
    return -1;

  for (unsigned page_id = 0; page_id < src->number_of_pages; page_id++) {
    HdmvPageParameters * prev_page = dst->pages[page_id];
    HdmvPageParameters * new_page  = src->pages[page_id];
    assert(page_id == new_page->page_id);
    assert(prev_page->page_id == new_page->page_id);

    bool is_same_ver;
    bool ver_is_valid = _interpretVersionNumber(
      prev_page->page_version_number,
      new_page->page_version_number,
      &is_same_ver
    );

    if (!ver_is_valid)
      LIBBLU_HDMV_PARSER_ERROR_RETURN(
        "Invalid ICS, "
        "unexpected 'page_version_number' not equal or incremented correctly "
        "(old: 0x%02X / new: 0x%02X).\n",
        prev_page->page_version_number,
        new_page->page_version_number
      );

    if (is_same_ver) {
      if (!lb_data_equal(prev_page, new_page, sizeof(HdmvPageParameters)))
        LIBBLU_HDMV_PARSER_ERROR_RETURN(
          "Invalid duplicated ICS, "
          "unexpected mismatch between pages ('page_id' = 0x%02X) "
          "sharing same version number.\n",
          page_id
        );
      continue;
    }

    memcpy(prev_page, new_page, sizeof(HdmvPageParameters));
  }

  dst->interactive_composition_length = src->interactive_composition_length;

  return 0;
}

/* ###### HDMV Palette Definition Segment : ################################ */
/* ######### Palette Descriptor : ########################################## */

static int _checkUpdateHdmvPDParameters(
  const HdmvPDParameters * prev,
  const HdmvPDParameters * cur,
  bool * shall_be_identical_ret
)
{
  assert(NULL != shall_be_identical_ret);
  assert(prev->palette_id == cur->palette_id);

  bool is_same_ver;
  bool ver_is_valid = _interpretVersionNumber(
    prev->palette_version_number,
    cur->palette_version_number,
    &is_same_ver
  );
  if (!ver_is_valid)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid PDS, "
      "unexpected 'palette_version_number' not equal or incremented correctly "
      "(old: 0x%02X / new: 0x%02X).\n",
      prev->palette_version_number,
      cur->palette_version_number
    );

  *shall_be_identical_ret = is_same_ver;
  return 0;
}

/* ######################################################################### */

int updateHdmvPDSegmentParameters(
  HdmvPDSegmentParameters * dst,
  const HdmvPDSegmentParameters * src
)
{
  bool shall_be_ident;
  if (_checkUpdateHdmvPDParameters(&dst->palette_descriptor, &src->palette_descriptor, &shall_be_ident) < 0)
    return -1;

  if (shall_be_ident) {
    if (!lb_data_equal(dst, src, sizeof(HdmvPDSegmentParameters)))
      LIBBLU_HDMV_PARSER_ERROR_RETURN(
        "Invalid updated PDS, "
        "palettes sharing same 'palette_id' and 'palette_version_number' "
        "shall be identical.\n"
      );
    return 0;
  }

  dst->palette_descriptor = src->palette_descriptor;
  uint16_t nb_entries = 0;
  for (unsigned i = 0; i < src->number_of_palette_entries; i++) {
    if (src->palette_entries[i].updated)
      dst->palette_entries[i] = src->palette_entries[i];
    nb_entries += dst->palette_entries[i].updated; // Count if defined
  }
  dst->number_of_palette_entries = nb_entries; // Total number of defined entries

  return 0;
}

/* ###### HDMV Object Definition Segment : ################################# */

int32_t computeObjectDataDecodeDuration(
  HdmvStreamType stream_type,
  uint16_t object_width,
  uint16_t object_height
)
{
  /** Compute OD_DECODE_DURATION of Object Definition (OD).
   *
   * \code{.unparsed}
   * Equation performed is (for Interactive Graphics):
   *
   *   OD_DECODE_DURATION(OD) =
   *     ceil(90000 * 8 * (OD.object_width * OD.object_height) / 64000000)
   * <=> ceil(9 * (OD.object_width * OD.object_height) / 800) // Compacted version
   *
   * or (for Presentation Graphics):
   *
   *   OD_DECODE_DURATION(OD) =
   *     ceil(90000 * 8 * (OD.object_width * OD.object_height) / 128000000)
   * <=> ceil(9 * (OD.object_width * OD.object_height) / 1600) // Compacted version
   *
   * where:
   *  OD            : Object Definition.
   *  90000         : PTS/DTS clock frequency (90kHz);
   *  64000000 or
   *  128000000     : Pixel Decoding Rate
   *                  (Rd = 64 Mb/s for IG, Rd = 128 Mb/s for PG).
   *  object_width  : object_width parameter parsed from n-ODS's Object_data().
   *  object_height : object_height parameter parsed from n-ODS's Object_data().
   * \endcode
   */
  static const int32_t pixel_decoding_rate_divider[] = {
    800,  // IG (64Mbps)
    1600  // PG (128Mbps)
  };

  return DIV_ROUND_UP(
    9 * (object_width * object_height),
    pixel_decoding_rate_divider[stream_type]
  );
}

/* ######### Object Descriptor : ########################################### */

static int _checkUpdateHdmvODescParameters(
  const HdmvODescParameters * prev,
  const HdmvODescParameters * cur,
  bool * shall_be_identical_ret
)
{
  assert(prev->object_id == cur->object_id);

  bool is_same_ver;
  bool ver_is_valid = _interpretVersionNumber(
    prev->object_version_number,
    cur->object_version_number,
    &is_same_ver
  );
  if (!ver_is_valid)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid ODS, "
      "unexpected 'object_version_number' not equal or incremented correctly "
      "(old: 0x%02X / new: 0x%02X).\n",
      prev->object_version_number,
      cur->object_version_number
    );

  *shall_be_identical_ret = is_same_ver;

  return 0;
}

/* ######### Object Data : ################################################# */

int updateHdmvObjectDataParameters(
  HdmvODParameters * dst,
  const HdmvODParameters * src
)
{
  bool shall_be_ident;
  if (_checkUpdateHdmvODescParameters(&dst->object_descriptor, &src->object_descriptor, &shall_be_ident) < 0)
    return -1;

  if (shall_be_ident) {
    if (!lb_data_equal(dst, src, sizeof(HdmvODParameters)))
      LIBBLU_HDMV_PARSER_ERROR_RETURN(
        "Invalid updated ODS, "
        "objects sharing same 'object_id' and 'object_version_number' "
        "shall be identical.\n"
      );
    return 0;
  }

  if (dst->object_width != src->object_width || dst->object_height != src->object_height)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid updated ODS, "
      "object dimensions shall remain the same (%ux%u / %ux%u).\n",
      dst->object_width,
      dst->object_height,
      src->object_width,
      src->object_height
    );

  memcpy(dst, src, sizeof(HdmvODParameters));
  return 0;
}
