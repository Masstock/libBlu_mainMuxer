#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "hdmv_data.h"

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
  HdmvSegmentType *segment_type
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
   *     ceil(90000 * 8 * (OD.object_width *OD.object_height) / 64000000)
   * <=> ceil(9 * (OD.object_width *OD.object_height) / 800) // Compacted version
   *
   * or (for Presentation Graphics):
   *
   *   OD_DECODE_DURATION(OD) =
   *     ceil(90000 * 8 * (OD.object_width *OD.object_height) / 128000000)
   * <=> ceil(9 * (OD.object_width *OD.object_height) / 1600) // Compacted version
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
    9 * (object_width *object_height),
    pixel_decoding_rate_divider[stream_type]
  );
}

int32_t computeObjectDataDecodeDelay(
  HdmvStreamType stream_type,
  uint16_t object_width,
  uint16_t object_height,
  float decode_delay_factor
)
{
  /** Compute OD_DECODE_DELAY of Object Definition (OD).
   *
   * This function calculates additional decoding delay for an object, to
   * facilitate multiplexing and reduce decoder load, based on the time
   * required to transfer it from the EB to the DB.
   *
   * Equation performed is:
   *
   * \code{.unparsed}
   *  OD_DECODE_DELAY(DS_n):
   *    return OD_DECODE_DURATION(OD) * decode_delay_factor
   *
   * where:
   *  OD_DECODE_DURATION  : Time required to decode an object.
   *                   Equation described in computeObjectDataDecodeDuration().
   *  decode_delay_factor : Multiplication factor applied to the decoding
   *                        time.
   * \endcode
   *
   * This process is inspired by functionality "BitRate Adjustment" of
   * Scenarist.
   */
  int32_t od_decode_duration = computeObjectDataDecodeDuration(
    stream_type,
    object_width,
    object_height
  );

  return (int32_t) ceilf(decode_delay_factor *od_decode_duration);
}

void cleanHdmvPageParameters(
  HdmvPageParameters page
)
{
  for (uint8_t i = 0; i < page.number_of_BOGs; i++)
    cleanHdmvButtonOverlapGroupParameters(page.bogs[i]);
  free(page.bogs);
}

int allocateBogsHdmvPageParameters(
  HdmvPageParameters *page
)
{
  if (!page->number_of_BOGs)
    return 0;
  page->bogs = calloc(
    page->number_of_BOGs,
    sizeof(HdmvButtonOverlapGroupParameters)
  );
  if (NULL == page->bogs)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

int copyHdmvPageParameters(
  HdmvPageParameters *dst,
  const HdmvPageParameters src
)
{
  HdmvPageParameters page_copy = src;
  if (allocateBogsHdmvPageParameters(&page_copy) < 0)
    return -1;
  for (uint8_t i = 0; i < src.number_of_BOGs; i++) {
    if (copyHdmvButtonOverlapGroupParameters(&page_copy.bogs[i], src.bogs[i]) < 0)
      return -1;
  }
  *dst = page_copy;
  return 0;
}

size_t computeSizeHdmvPage(
  const HdmvPageParameters param
)
{
  size_t size = 17ull;
  size += computeSizeHdmvEffectSequence(param.in_effects);
  size += computeSizeHdmvEffectSequence(param.out_effects);
  for (uint8_t i = 0; i < param.number_of_BOGs; i++)
    size += computeSizeHdmvButtonOverlapGroup(param.bogs[i]);
  return size;
}

size_t computeSizeHdmvInteractiveComposition(
  const HdmvICParameters param
)
{
  size_t size = 8ull;
  if (param.stream_model == HDMV_STREAM_MODEL_MULTIPLEXED)
    size += 10ull;
  for (uint8_t i = 0; i < param.number_of_pages; i++)
    size += computeSizeHdmvPage(param.pages[i]);
  return size;
}