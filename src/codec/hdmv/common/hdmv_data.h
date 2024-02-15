/** \~english
 * \file hdmv_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IG/PG bitstreams data structures.
 */

/** \~english
 * \dir hdmv
 *
 * \brief HDMV menus and subtitles (IG, PG) bitstreams handling modules.
 *
 * \xrefitem references "References" "References list"
 *  [1] Patent US 8,849,102 B2\n // IG semantics
 *  [2] Patent US 8,638,861 B2\n // PG semantics
 *  [3] Patent US 7,634,739 B2\n // IG timings
 *  [4] Patent US 7,680,394 B2\n // PG timings
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__DATA_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__DATA_H__

#include "hdmv_error.h"
#include "../../common/constantCheckFunctionsMacros.h"
#include "../../../elementaryStreamProperties.h"

/* ### HDMV Stream type : ################################################## */

#define IGS_MNU_WORD  0x4947 /* "IG" */
#define IGS_SUP_WORD  0x5047 /* "PG" */

typedef enum {
  HDMV_STREAM_TYPE_IGS  = 0x0,
  HDMV_STREAM_TYPE_PGS  = 0x1
} HdmvStreamType;

static inline const char * HdmvStreamTypeStr(
  HdmvStreamType type
)
{
  static const char * strings[] = {
    "HDMV IGS",
    "HDMV PGS"
  };

  if (type < ARRAY_SIZE(strings))
    return strings[type];
  return "Unknown HDMV type";
}

static inline uint16_t expectedFormatIdentifierHdmvStreamType(
  HdmvStreamType type
)
{
  static const uint16_t magic[] = {
    IGS_MNU_WORD,
    IGS_SUP_WORD
  };
  return magic[type];
}

static inline bool checkFormatIdentifierHdmvStreamType(
  uint16_t format_identifier,
  HdmvStreamType type
)
{
  return (expectedFormatIdentifierHdmvStreamType(type) == format_identifier);
}

static inline LibbluStreamCodingType codingTypeHdmvStreamType(
  HdmvStreamType type
)
{
  static const LibbluStreamCodingType codingType[] = {
    STREAM_CODING_TYPE_IG,
    STREAM_CODING_TYPE_PG
  };
  return codingType[type];
}

#define HDMV_STREAM_TYPE_MASK(type)  (1u << (type))

#define HDMV_STREAM_TYPE_MASK_IGS                                             \
  HDMV_STREAM_TYPE_MASK(HDMV_STREAM_TYPE_IGS)

#define HDMV_STREAM_TYPE_MASK_PGS                                             \
  HDMV_STREAM_TYPE_MASK(HDMV_STREAM_TYPE_PGS)

#define HDMV_STREAM_TYPE_MASK_COMMON                                          \
  (HDMV_STREAM_TYPE_MASK_IGS | HDMV_STREAM_TYPE_MASK_PGS)

/* ### HDMV Streams Buffer sizes / Transfer rates : ######################## */

/** \~english
 * \brief Transport Buffer removing data rate (in bits/sec).
 *
 * R_x transfer rate value from Transport Buffer (TB) to Coded Data Buffer
 * (CDB).
 */
#define HDMV_RX_BITRATE  16000000

#define HDMV_IG_DB_SIZE  (16u << 20)
#define HDMV_PG_DB_SIZE  ( 4u << 20)

/** \~english
 * \brief HDMV Decoded Object Buffer (DB) size.
 *
 * \param type HDMV stream type.
 * \return uint32_t Associated DB size.
 */
static inline uint32_t getHDMVDecodedObjectBufferSize(
  HdmvStreamType type
)
{
  static const uint32_t sizes[] = {
    HDMV_IG_DB_SIZE, /**< Interactive Graphics  - 16MiB  */
    HDMV_PG_DB_SIZE, /**< Presentation Graphics -  4MiB  */
  };
  return sizes[type];
}

static inline int64_t getHDMVPlaneClearTime(
  HdmvStreamType stream_type,
  uint16_t video_width,
  uint16_t video_height
)
{
  /** Compute PLANE_CLEAR_TIME of Interactive Composition or Presentation
   * Composition, the duration required to clear the whole graphical plane.
   *
   * If stream type is Interactive Graphics:
   *
   *   PLANE_CLEAR_TIME =
   *     ceil(90000 * 8 * (video_width * video_height) / 128000000)
   * <=> ceil(9 * (video_width * video_height) / 1600)
   *
   * Otherwhise, if stream type is Presentation Graphics:
   *
   *   PLANE_CLEAR_TIME =
   *     ceil(90000 * 8 * (video_width * video_height) / 256000000)
   * <=> ceil(9 * (video_width * video_height) / 3200)
   *
   * where:
   *  90000        : PTS/DTS clock frequency (90kHz);
   *  128000000 or
   *  256000000    : Pixel Transfer Rate (Rc = 128 Mb/s for IG,
   *                 256 Mb/s for PG).
   *  video_width  : video_width parameter parsed from ICS/PCS's
   *                 Video_descriptor();
   *  video_height : video_height parameter parsed from ICS/PCS's
   *                 Video_descriptor().
   */
  static const int64_t clearing_rate_divider[] = {
    1600LL,  /*< IG (128Mbps)  */
    3200LL   /*< PG (256Mbps) */
  };

  return DIV_ROUND_UP(
    9LL * (video_width * video_height),
    clearing_rate_divider[stream_type]
  );
}

/* ### HDMV Segments : ##################################################### */

#define HDMV_SEGMENT_HEADER_LENGTH  0x3

typedef enum {
  HDMV_SEGMENT_TYPE_PDS   = 0x14,
  HDMV_SEGMENT_TYPE_ODS   = 0x15,
  HDMV_SEGMENT_TYPE_PCS   = 0x16,
  HDMV_SEGMENT_TYPE_WDS   = 0x17,
  HDMV_SEGMENT_TYPE_ICS   = 0x18,
  HDMV_SEGMENT_TYPE_END   = 0x80
} HdmvSegmentType;

static inline const char * HdmvSegmentTypeStr(
  HdmvSegmentType type
)
{
  switch (type) {
  case HDMV_SEGMENT_TYPE_PDS:
    return "Palette Definition Segment";
  case HDMV_SEGMENT_TYPE_ODS:
    return "Object Definition Segment";
  case HDMV_SEGMENT_TYPE_PCS:
    return "Presentation Composition Segment";
  case HDMV_SEGMENT_TYPE_WDS:
    return "Window Definition Segment";
  case HDMV_SEGMENT_TYPE_ICS:
    return "Interactive Composition Segment";
  case HDMV_SEGMENT_TYPE_END:
    return "End of Display Set Segment";
  }

  return "Unknown type segment";
}

bool isValidHdmvSegmentType(
  uint8_t type
);

int checkHdmvSegmentType(
  uint8_t segment_type_value,
  HdmvStreamType stream_type,
  HdmvSegmentType * segment_type
);

/* ###### HDMV Common structures : ######################################### */

/* ######### Segment Descriptor : ########################################## */

/** \~english
 * \brief Size required by segment header in bytes.
 *
 * Composed of:
 *  - u8  : segment_type;
 *  - u16 : segment_length.
 *
 * => 3 bytes.
 */
#define HDMV_SIZE_SEGMENT_HEADER  3

/** \~english
 * \brief Maximum payload size of a segment in bytes.
 *
 * Payload is considered as all segment data content except
 * header.
 * Size is defined as the segment_length max possible value.
 */
#define HDMV_MAX_SIZE_SEGMENT_PAYLOAD  0xFFEF

/** \~english
 * \brief Maximum segment size in bytes.
 *
 * Size is defined as the segment_length max possible value
 * plus segment header length.
 */
#define HDMV_MAX_SIZE_SEGMENT                                                 \
  (HDMV_MAX_SIZE_SEGMENT_PAYLOAD + HDMV_SIZE_SEGMENT_HEADER)

typedef struct {
  uint8_t segment_type;
  uint16_t segment_length;
} HdmvSegmentDescriptor;

/* ######### Video Descriptor : ############################################ */

typedef struct HdmvVideoDescriptorParameters {
  uint16_t video_width;
  uint16_t video_height;
  HdmvFrameRateCode frame_rate;
} HdmvVDParameters;

static inline bool areIdenticalHdmvVDParameters(
  const HdmvVDParameters first,
  const HdmvVDParameters second
)
{
  return CHECK(
    EQUAL(.video_width)
    EQUAL(.video_height)
    EQUAL(.frame_rate)
  );
}

static inline int64_t getFrameDurHdmvVDParameters(
  const HdmvVDParameters vd
)
{
  return frameDur90kHzHdmvFrameRateCode(vd.frame_rate);
}

static inline bool isFrameTcAlignedHdmvVDParameters(
  const HdmvVDParameters vd,
  int64_t timestamp,
  int64_t * aligned_timestamp_ret
)
{
  int64_t frame_dur_27MHz = frameDur27MHzHdmvFrameRateCode(vd.frame_rate);
  assert(0 < frame_dur_27MHz);

  int64_t aligned_timestamp = DIV(
    DIV_ROUND_UP(timestamp * 300ll, frame_dur_27MHz) * frame_dur_27MHz,
    300
  );

  if (NULL != aligned_timestamp_ret)
    *aligned_timestamp_ret = aligned_timestamp;

  return (timestamp == aligned_timestamp);
}

/** \~english
 * \brief Size required by video_descriptor() structure in bytes.
 *
 * Composed of:
 *  - u16 : video_width;
 *  - u16 : video_height;
 *  - u4  : frame_rate_id;
 *  - v4  : reserved.
 *
 * => 5 bytes.
 */
#define HDMV_SIZE_VIDEO_DESCRIPTOR  5

/* ######### Composition Descriptor : ###################################### */

typedef enum {
  HDMV_COMPO_STATE_NORMAL             = 0x0,
  HDMV_COMPO_STATE_ACQUISITION_POINT  = 0x1,
  HDMV_COMPO_STATE_EPOCH_START        = 0x2,
  HDMV_COMPO_STATE_EPOCH_CONTINUE     = 0x3,
} HdmvCompositionState;

static inline const char * HdmvCompositionStateStr(
  HdmvCompositionState composition_state
)
{
  static const char * strings[] = {
    "Normal",
    "Acquisition point",
    "Epoch start",
    "Epoch continue"
  };

  if (composition_state < ARRAY_SIZE(strings))
    return strings[composition_state];
  return "Reserved value";
}

typedef struct HdmvCompositionDescriptorParameters {
  uint16_t composition_number;
  HdmvCompositionState composition_state;
} HdmvCDParameters;

static inline bool isEpochStartHdmvCDParameters(
  HdmvCDParameters composition_desc
)
{
  return HDMV_COMPO_STATE_EPOCH_START == composition_desc.composition_state;
}

static inline bool isNormalHdmvCDParameters(
  HdmvCDParameters composition_desc
)
{
  return HDMV_COMPO_STATE_NORMAL == composition_desc.composition_state;
}

/** \~english
 * \brief Size required by composition_descriptor() structure in bytes.
 *
 * Composed of:
 *  - u16 : composition_number;
 *  - u8  : composition_state.
 *
 * => 3 bytes.
 */
#define HDMV_SIZE_COMPOSITION_DESCRIPTOR  3

/* ######### Sequence Descriptor : ######################################### */

typedef struct HdmvSequenceDescriptorParameters {
  bool first_in_sequence;
  bool last_in_sequence;
} HdmvSDParameters;

static inline void setTrueHdmvSequenceDescriptorParameters(
  HdmvSDParameters * dst
)
{
  *dst = (HdmvSDParameters) {
    .first_in_sequence = true,
    .last_in_sequence = true
  };
}

static inline bool isFirstInSequenceHdmvSDParameters(
  const HdmvSDParameters * sequence_descriptor
)
{
  return sequence_descriptor->first_in_sequence;
}

static inline bool isLastInSequenceHdmvSDParameters(
  const HdmvSDParameters * sequence_descriptor
)
{
  return sequence_descriptor->last_in_sequence;
}

/** \~english
 * \brief Size required by sequence_descriptor() structure in bytes.
 *
 * Composed of:
 *  - b1  : first_in_sequence;
 *  - b1  : last_in_sequence;
 *  - v6  : reserved.
 *
 * => 1 byte.
 */
#define HDMV_SIZE_SEQUENCE_DESCRIPTOR  1

/** \~english
 * \brief In-sequence location of a segment.
 *
 * Segments type missing sequence_descriptor field must use value
 * #HDMV_SEQUENCE_LOC_UNIQUE.
 */
typedef enum {
  HDMV_SEQUENCE_LOC_MIDDLE  = 0x0,  /**< The segment is not a the start
    nor at the end of its sequence (middle).                                 */
  HDMV_SEQUENCE_LOC_END     = 0x1,  /**< The segment marks the end of its
    sequence.                                                                */
  HDMV_SEQUENCE_LOC_START   = 0x2,  /**< The segment marks the start of
    its sequence.                                                            */
  HDMV_SEQUENCE_LOC_UNIQUE  = 0x3,  /**< The segment marks both the
    beginning and the end of its sequence (it is unique the unique segment
    of its sequence). All segments of this type can be decoded
    independently.                                                           */
} HdmvSequenceLocation;

static inline HdmvSequenceLocation getHdmvSequenceLocation(
  HdmvSDParameters sequence_descriptor
)
{
#if 0
  if (sequence_descriptor.first_in_sequence && sequence_descriptor.last_in_sequence)
    return HDMV_SEQUENCE_LOC_UNIQUE;
  if (sequence_descriptor.first_in_sequence)
    return HDMV_SEQUENCE_LOC_START;
  if (sequence_descriptor.last_in_sequence)
    return HDMV_SEQUENCE_LOC_END;
  return HDMV_SEQUENCE_LOC_MIDDLE;
#else
  return
    (sequence_descriptor.first_in_sequence << 1)
    | sequence_descriptor.last_in_sequence
  ;
#endif
}

static inline bool isFirstSegmentHdmvSequenceLocation(
  HdmvSequenceLocation location
)
{
  return
    HDMV_SEQUENCE_LOC_START == location
    || HDMV_SEQUENCE_LOC_UNIQUE == location
  ;
}

static inline bool isLastSegmentHdmvSequenceLocation(
  HdmvSequenceLocation location
)
{
  return
    HDMV_SEQUENCE_LOC_END == location
    || HDMV_SEQUENCE_LOC_UNIQUE == location
  ;
}

/* ######### Window : ###################################################### */

typedef struct {
  uint8_t window_id;

  uint16_t window_horizontal_position;
  uint16_t window_vertical_position;
  uint16_t window_width;
  uint16_t window_height;
} HdmvWindowInfoParameters;

static inline void initHdmvWindowInfoParameters(
  HdmvWindowInfoParameters * dst
)
{
  *dst = (HdmvWindowInfoParameters) {
    0
  };
}

static inline bool constantHdmvWindowInfoParameters(
  const HdmvWindowInfoParameters first,
  const HdmvWindowInfoParameters second
)
{
  return CHECK(
    EQUAL(.window_id)
    EQUAL(.window_horizontal_position)
    EQUAL(.window_vertical_position)
    EQUAL(.window_width)
    EQUAL(.window_height)
  );
}

/** \~english
 * \brief Size required by Window_info() structure in bytes.
 *
 * Composed of:
 *  - u8  : window_id;
 *  - u16 : window_horizontal_position;
 *  - u16 : window_vertical_position;
 *  - u16 : window_width;
 *  - u16 : window_height.
 *
 * => 9 bytes.
 */
#define HDMV_SIZE_WINDOW_INFO  9

/* ###### HDMV Presentation Composition Segment : ########################## */
/* ######### Composition Object : ########################################## */

typedef struct {
  uint16_t object_id_ref;
  uint8_t window_id_ref;

  bool object_cropped_flag;
  bool forced_on_flag; /* Only in PGS */

  uint16_t composition_object_horizontal_position;
  uint16_t composition_object_vertical_position;

  struct {
    uint16_t horizontal_position;
    uint16_t vertical_position;
    uint16_t width;
    uint16_t height;
  } object_cropping;
} HdmvCOParameters;

static inline void initHdmvCompositionObjectParameters(
  HdmvCOParameters * dst
)
{
  *dst = (HdmvCOParameters) {
    0
  };
}

static inline bool constantHdmvCOParameters(
  const HdmvCOParameters first,
  const HdmvCOParameters second
)
{
  return CHECK(
    EQUAL(.object_id_ref)
    EQUAL(.window_id_ref)
    EQUAL(.object_cropped_flag)
    EQUAL(.forced_on_flag)
    EQUAL(.composition_object_horizontal_position)
    EQUAL(.composition_object_vertical_position)
    START_COND(.object_cropped_flag, true)
      EQUAL(.object_cropping.horizontal_position)
      EQUAL(.object_cropping.vertical_position)
      EQUAL(.object_cropping.width)
      EQUAL(.object_cropping.height)
    END_COND
  );
}

#define HDMV_MAX_SIZE_COMPOSITION_OBJECT  16

/** \~english
 * \brief
 *
 * \param param
 * \return size_t
 *
 * Composed of:
 *  - u16 : object_id_ref;
 *  - u8  : window_id_ref;
 *  - b1  : object_cropped_flag;
 *  - v7  : reserved;
 *  - u16 : composition_object_horizontal_position;
 *  - u16 : composition_object_vertical_position;
 *  if (object_cropped_flag)
 *    - u16 : object_cropping_horizontal_position;
 *    - u16 : object_cropping_vertical_position;
 *    - u16 : object_cropping_width;
 *    - u16 : object_cropping_height.
 *
 * => 8 + (object_cropped_flag ? 8 : 0) bytes.
 */
static inline size_t computeSizeHdmvCompositionObject(
  const HdmvCOParameters co
)
{
  return 8ull + (co.object_cropped_flag ? 8ull : 0);
}

/* ######### Presentation Composition : #################################### */

#define HDMV_MAX_NB_ACTIVE_PC  8

#define HDMV_MAX_NB_PC_COMPO_OBJ  2

typedef struct HdmvPresentationCompositionParameters {
  bool palette_update_flag;
  uint8_t palette_id_ref;

  uint8_t number_of_composition_objects;
  HdmvCOParameters composition_objects[
    HDMV_MAX_NB_PC_COMPO_OBJ
  ];
} HdmvPCParameters;

#define HDMV_SIZE_PRESENTATION_COMPOSITION_HEADER  3

/** \~english
 * \brief Computes and return size required by Presentation_composition()
 * structure.
 *
 * \param param Structure parameters.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Composed of:
 *  - b1  : palette_update_flag;
 *  - v7  : reserved;
 *  - u8  : palette_id_ref;
 *  - u8  : number_of_composition_objects;
 *  for (i = 0; i < number_of_composition_objects; i++)
 *    - vn  : CompositionObject(i).
 *
 * => 3 + CompositionObject()s bytes.
 */
static inline size_t computeSizeHdmvPresentationComposition(
  const HdmvPCParameters param
)
{
  size_t size = HDMV_SIZE_PRESENTATION_COMPOSITION_HEADER;
  for (uint8_t i = 0; i < param.number_of_composition_objects; i++)
    size += computeSizeHdmvCompositionObject(param.composition_objects[i]);
  return size;
}

/* ######################################################################### */

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvPCParameters presentation_composition;
} HdmvPcsSegmentParameters;

/** \~english
 * \brief Size of Presentation Composition Segment header in bytes.
 *
 * Presentation Compostion segments header is composed of:
 *  - video_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - composition_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 */
#define HDMV_SIZE_PCS_HEADER                                                  \
  (HDMV_SIZE_VIDEO_DESCRIPTOR + HDMV_SIZE_COMPOSITION_DESCRIPTOR)

/* ###### HDMV Window Definition Segment : ################################# */
/* ######### Window Definition : ########################################### */

#define HDMV_MAX_NB_WDS_WINDOWS  2

typedef struct HdmvWindowDefinitionParameters {
  uint8_t number_of_windows;
  HdmvWindowInfoParameters windows[
    HDMV_MAX_NB_WDS_WINDOWS
  ];
} HdmvWDParameters;

static inline int64_t getFillDurationHdmvWD(
  HdmvWDParameters window_definition
)
{
  int64_t dividend = 0;
  for (uint8_t i = 0; i < window_definition.number_of_windows; i++) {
    HdmvWindowInfoParameters window = window_definition.windows[i];
    dividend += 9LL * window.window_width * window.window_height;
  }
  return DIV_ROUND_UP(dividend, 3200);
}

static inline bool constantHdmvWDParameters(
  const HdmvWDParameters first,
  const HdmvWDParameters second
)
{
  bool is_equal = CHECK(
    EQUAL(.number_of_windows)
  );
  for (uint8_t i = 0; is_equal && i < first.number_of_windows; i++)
    is_equal &= constantHdmvWindowInfoParameters(
      first.windows[i],
      second.windows[i]
    );
  return is_equal;
}

#define HDMV_SIZE_WDS_HEADER  1

static inline size_t computeSizeHdmvWD(
  const HdmvWDParameters wd
)
{
  return HDMV_SIZE_WDS_HEADER + (wd.number_of_windows * HDMV_SIZE_WINDOW_INFO);
}

/* ###### HDMV Interactive Composition Segment : ########################### */
/* ######### Effect Info : ################################################# */

#define HDMV_MAX_NB_EFFECT_COMPO_OBJ  2

typedef struct {
  uint32_t effect_duration;
  uint8_t palette_id_ref;

  uint8_t number_of_composition_objects;
  HdmvCOParameters composition_objects[HDMV_MAX_NB_EFFECT_COMPO_OBJ];
} HdmvEffectInfoParameters;

static inline void initHdmvEffectInfoParameters(
  HdmvEffectInfoParameters * dst
)
{
  *dst = (HdmvEffectInfoParameters) {
    0
  };
}

static inline bool constantHdmvEffectInfoParameters(
  const HdmvEffectInfoParameters first,
  const HdmvEffectInfoParameters second
)
{
  bool is_equal = CHECK(
    EQUAL(.effect_duration)
    EQUAL(.palette_id_ref)
    EQUAL(.number_of_composition_objects)
  );
  for (uint8_t i = 0; is_equal && i < first.number_of_composition_objects; i++)
    is_equal &= constantHdmvCOParameters(
      first.composition_objects[i],
      second.composition_objects[i]
    );
  return is_equal;
}

/** \~english
 * \brief
 *
 * \param param
 * \return size_t
 *
 * Composed of:
 *  - u24 : effect_duration;
 *  - u8  : palette_id_ref;
 *  - u8  : number_of_composition_objects;
 *  for (i = 0; i < number_of_composition_objects; i++)
 *    - vn  : Composition_object(i)s.
 *
 * => 5 + Composition_object()s bytes.
 */
static inline size_t computeSizeHdmvEffectInfo(
  const HdmvEffectInfoParameters ei
)
{
  size_t size = 5ull;
  for (uint8_t i = 0; i < ei.number_of_composition_objects; i++)
    size += computeSizeHdmvCompositionObject(ei.composition_objects[i]);
  return size;
}

/* ######### Effect Sequence : ############################################# */

#define HDMV_MAX_NB_ES_WINDOWS  2

#define HDMV_MAX_NB_ES_EFFECTS  128

typedef struct {
  uint8_t number_of_windows;
  HdmvWindowInfoParameters windows[HDMV_MAX_NB_ES_WINDOWS];
  uint8_t number_of_effects;
  HdmvEffectInfoParameters effects[HDMV_MAX_NB_ES_EFFECTS];
} HdmvEffectSequenceParameters;

/** \~english
 * \brief Computes and return size required by effect_sequence() structure.
 *
 * \param param Structure parameters.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Used to define how many bytes are required to store generated
 * effect_sequence().
 *
 * Composed of:
 *  - u8  : number_of_windows;
 *  for (i = 0; i < number_of_windows; i++)
 *    - v72 : Window_info(i).
 *  - u8  : number_of_effects;
 *  for (i = 0; i < number_of_effects; i++)
 *    - vn  : Effect_info(i).
 *
 * => 2 + Window_info()s + Effect_info()s bytes.
 */
static inline size_t computeSizeHdmvEffectSequence(
  const HdmvEffectSequenceParameters param
)
{
  size_t size = 2ull + param.number_of_windows * HDMV_SIZE_WINDOW_INFO;
  for (uint8_t i = 0; i < param.number_of_effects; i++)
    size += computeSizeHdmvEffectInfo(param.effects[i]);
  return size;
}

static inline bool constantHdmvEffectSequenceParameters(
  const HdmvEffectSequenceParameters first,
  const HdmvEffectSequenceParameters second
)
{
  bool is_equal = CHECK(
    EQUAL(.number_of_windows)
    EQUAL(.number_of_effects)
  );
  for (uint8_t i = 0; is_equal && i < first.number_of_windows; i++)
    is_equal &= constantHdmvWindowInfoParameters(
      first.windows[i],
      second.windows[i]
    );
  for (uint8_t i = 0; is_equal && i < first.number_of_effects; i++)
    is_equal &= constantHdmvEffectInfoParameters(
      first.effects[i],
      second.effects[i]
    );
  return is_equal;
}

/* ######### Navigation Command : ########################################## */

typedef struct HdmvNavigationCommand {
  // struct HdmvNavigationCommand * next;
  uint32_t opcode;
  uint32_t destination;
  uint32_t source;
} HdmvNavigationCommand;

static inline void initHdmvNavigationCommand(
  HdmvNavigationCommand * dst
)
{
  *dst = (HdmvNavigationCommand) {
    0
  };
}

static inline bool constantHdmvNavigationCommand(
  const HdmvNavigationCommand first,
  const HdmvNavigationCommand second
)
{
  return CHECK(
    EQUAL(.opcode)
    EQUAL(.destination)
    EQUAL(.source)
  );
}

/* ######### Button Neighbor Info : ######################################## */

typedef struct {
  uint16_t upper_button_id_ref;
  uint16_t lower_button_id_ref;
  uint16_t left_button_id_ref;
  uint16_t right_button_id_ref;
} HdmvButtonNeighborInfoParam;

static inline void initHdmvButtonNeighborInfoParam(
  HdmvButtonNeighborInfoParam * dst
)
{
  *dst = (HdmvButtonNeighborInfoParam) {
    .upper_button_id_ref = 0xFFFF,
    .lower_button_id_ref = 0xFFFF,
    .left_button_id_ref  = 0xFFFF,
    .right_button_id_ref = 0xFFFF
  };
}

static inline bool constantHdmvButtonNeighborInfoParam(
  const HdmvButtonNeighborInfoParam first,
  const HdmvButtonNeighborInfoParam second
)
{
  return CHECK(
    EQUAL(.upper_button_id_ref)
    EQUAL(.lower_button_id_ref)
    EQUAL(.left_button_id_ref)
    EQUAL(.right_button_id_ref)
  );
}

/* ######### Button States : ############################################### */
/* ############ Button Normal State Information : ######################### */

typedef struct {
  uint16_t start_object_id_ref;
  uint16_t end_object_id_ref;
  bool repeat_flag;
  bool complete_flag;
} HdmvButtonNormalStateInfoParam;

static inline void initHdmvButtonNormalStateInfoParam(
  HdmvButtonNormalStateInfoParam * dst
)
{
  *dst = (HdmvButtonNormalStateInfoParam) {
    .start_object_id_ref = 0xFFFF,
    .end_object_id_ref   = 0xFFFF
  };
}

static inline bool constantHdmvButtonNormalStateInfoParam(
  const HdmvButtonNormalStateInfoParam first,
  const HdmvButtonNormalStateInfoParam second
)
{
  return CHECK(
    EQUAL(.start_object_id_ref)
    EQUAL(.end_object_id_ref)
    EQUAL(.repeat_flag)
    EQUAL(.complete_flag)
  );
}

/* ############ Button Selected State Information : ######################## */

typedef struct {
  uint8_t state_sound_id_ref;
  uint16_t start_object_id_ref;
  uint16_t end_object_id_ref;
  bool repeat_flag;
  bool complete_flag;
} HdmvButtonSelectedStateInfoParam;

static inline void initHdmvButtonSelectedStateInfoParam(
  HdmvButtonSelectedStateInfoParam * dst
)
{
  *dst = (HdmvButtonSelectedStateInfoParam) {
    .state_sound_id_ref  = 0xFF,
    .start_object_id_ref = 0xFFFF,
    .end_object_id_ref   = 0xFFFF
  };
}

static inline bool constantHdmvButtonSelectedStateInfoParam(
  const HdmvButtonSelectedStateInfoParam first,
  const HdmvButtonSelectedStateInfoParam second
)
{
  return CHECK(
    EQUAL(.state_sound_id_ref)
    EQUAL(.start_object_id_ref)
    EQUAL(.end_object_id_ref)
    EQUAL(.repeat_flag)
    EQUAL(.complete_flag)
  );
}

/* ############ Button Activated State Information : ####################### */

typedef struct {
  uint8_t state_sound_id_ref;
  uint16_t start_object_id_ref;
  uint16_t end_object_id_ref;
} HdmvButtonActivatedStateInfoParam;

static inline void initHdmvButtonActivatedStateInfoParam(
  HdmvButtonActivatedStateInfoParam * dst
)
{
  *dst = (HdmvButtonActivatedStateInfoParam) {
    .state_sound_id_ref  = 0xFF,
    .start_object_id_ref = 0xFFFF,
    .end_object_id_ref   = 0xFFFF,
  };
}

static inline bool constantHdmvButtonActivatedStateInfoParam(
  const HdmvButtonActivatedStateInfoParam first,
  const HdmvButtonActivatedStateInfoParam second
)
{
  return CHECK(
    EQUAL(.state_sound_id_ref)
    EQUAL(.start_object_id_ref)
    EQUAL(.end_object_id_ref)
  );
}

/* ######### Button : ###################################################### */

#define HDMV_MAX_NB_BUTTONS  8160

#define HDMV_MAX_BUTTON_NUMERIC_SELECT_VALUE  9999

#define HDMV_MAX_NB_BUTTON_NAV_COM  65535

typedef struct {
  uint16_t button_id;
  uint16_t button_numeric_select_value;

  bool auto_action;

  uint16_t button_horizontal_position;
  uint16_t button_vertical_position;

  HdmvButtonNeighborInfoParam neighbor_info;

  HdmvButtonNormalStateInfoParam normal_state_info;
  HdmvButtonSelectedStateInfoParam selected_state_info;
  HdmvButtonActivatedStateInfoParam activated_state_info;

  uint16_t number_of_navigation_commands;
  HdmvNavigationCommand * navigation_commands;

  /* Values computed at check: */
  uint16_t btn_obj_width;   /**< Button objects width.                       */
  uint16_t btn_obj_height;  /**< Button objects height.                      */
} HdmvButtonParam;

static inline void initHdmvButtonParam(
  HdmvButtonParam * dst
)
{
  *dst = (HdmvButtonParam) {0};
  initHdmvButtonNeighborInfoParam(&dst->neighbor_info);
  initHdmvButtonNormalStateInfoParam(&dst->normal_state_info);
  initHdmvButtonSelectedStateInfoParam(&dst->selected_state_info);
  initHdmvButtonActivatedStateInfoParam(&dst->activated_state_info);
}

static inline void cleanHdmvButtonParam(
  HdmvButtonParam btn
)
{
  free(btn.navigation_commands);
}

static inline int allocateCommandsHdmvButtonParam(
  HdmvButtonParam * dst
)
{
  if (!dst->number_of_navigation_commands)
    return 0;
  dst->navigation_commands = calloc(
    dst->number_of_navigation_commands,
    sizeof(HdmvNavigationCommand)
  );
  if (NULL == dst->navigation_commands)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

static inline bool constantHdmvButtonParam(
  const HdmvButtonParam first,
  const HdmvButtonParam second
)
{
  bool is_equal = CHECK(
    EQUAL(.button_id)
    EQUAL(.button_numeric_select_value)
    EQUAL(.auto_action)
    EQUAL(.button_horizontal_position)
    EQUAL(.button_vertical_position)
    SUB_FUN(.neighbor_info, constantHdmvButtonNeighborInfoParam)
    SUB_FUN(.normal_state_info, constantHdmvButtonNormalStateInfoParam)
    SUB_FUN(.selected_state_info, constantHdmvButtonSelectedStateInfoParam)
    SUB_FUN(.activated_state_info, constantHdmvButtonActivatedStateInfoParam)
    EQUAL(.number_of_navigation_commands)
  );
  for (uint16_t i = 0; is_equal && i < first.number_of_navigation_commands; i++)
    is_equal &= constantHdmvNavigationCommand(
      first.navigation_commands[i],
      second.navigation_commands[i]
    );
  return is_equal;
}

/** \~english
 * \brief
 *
 * \param button
 * \return size_t
 *
 * Composed of:
 *  - u16 : button_id;
 *  - u16 : button_numeric_select_value;
 *  - b1  : auto_action_flag;
 *  - v7  : reserved;
 *  - u16 : button_horizontal_position;
 *  - u16 : button_vertical_position;
 *  - v64 : Neighbor_info();
 *    - u16 : upper_button_id_ref;
 *    - u16 : lower_button_id_ref;
 *    - u16 : left_button_id_ref;
 *    - u16 : right_button_id_ref;
 *  - v40 : Normal_state_info();
 *    - u16 : normal_start_object_id_ref;
 *    - u16 : normal_end_object_id_ref;
 *    - b1  : normal_repeat_flag;
 *    - b1  : normal_complete_flag;
 *    - v6  : reserved;
 *  - v48 : Selected_state_info();
 *    - u8  : selected_state_sound_id_ref;
 *    - u16 : selected_start_object_id_ref;
 *    - u16 : selected_end_object_id_ref;
 *    - b1  : selected_repeat_flag;
 *    - b1  : selected_complete_flag;
 *    - v6  : reserved;
 *  - v40 : Actionned_state_info();
 *    - u8  : activated_state_sound_id_ref;
 *    - u16 : activated_start_object_id_ref;
 *    - u16 : activated_end_object_id_ref;
 *  - u16 : number_of_navigation_commands;
 *  for (i = 0; i < number_of_navigation_commands; i++)
 *    - v96 : Navigation_command().
 *
 * => 35 + number_of_navigation_commands * 12 bytes.
 */
static inline size_t computeSizeHdmvButton(
  const HdmvButtonParam btn
)
{
  return 35ull + btn.number_of_navigation_commands * 12ull;
}

/* ######### Button Overlap Group : ######################################## */

#define HDMV_MAX_NB_ICS_BUTTONS  32

typedef struct {
  uint16_t default_valid_button_id_ref;

  uint8_t number_of_buttons;
  HdmvButtonParam buttons[HDMV_MAX_NB_ICS_BUTTONS];
} HdmvButtonOverlapGroupParameters;

static inline void initHdmvButtonOverlapGroupParameters(
  HdmvButtonOverlapGroupParameters * dst
)
{
  *dst = (HdmvButtonOverlapGroupParameters) {
    0
  };
}

static inline void cleanHdmvButtonOverlapGroupParameters(
  HdmvButtonOverlapGroupParameters bog
)
{
  for (uint8_t i = 0; i < bog.number_of_buttons; i++)
    cleanHdmvButtonParam(bog.buttons[i]);
}

static inline int copyHdmvButtonOverlapGroupParameters(
  HdmvButtonOverlapGroupParameters * dst,
  const HdmvButtonOverlapGroupParameters src
)
{
  HdmvButtonOverlapGroupParameters bog_copy = src;
  for (uint8_t i = 0; i < src.number_of_buttons; i++) {
    HdmvButtonParam * btn = &bog_copy.buttons[i];
    if (allocateCommandsHdmvButtonParam(btn) < 0)
      return -1;
    memcpy(
      btn->navigation_commands,
      src.buttons[i].navigation_commands,
      btn->number_of_navigation_commands * sizeof(HdmvNavigationCommand)
    );
  }
  *dst = bog_copy;
  return 0;
}

static inline bool constantHdmvButtonOverlapGroupParameters(
  const HdmvButtonOverlapGroupParameters first,
  const HdmvButtonOverlapGroupParameters second
)
{
  bool is_equal = CHECK(
    EQUAL(.default_valid_button_id_ref)
    EQUAL(.number_of_buttons)
  );
  for (uint8_t i = 0; is_equal && i < first.number_of_buttons; i++)
    is_equal &= constantHdmvButtonParam(first.buttons[i], second.buttons[i]);
  return is_equal;
}

/** \~english
 * \brief
 *
 * \param param
 * \return size_t
 *
 * Composed of:
 *  - u16 : default_valid_button_id_ref;
 *  - u8  : number_of_buttons;
 *  for (i = 0; i < number_of_BOGs; i++)
 *    - vn  : Buttons(i).
 *
 * => 3 + Buttons()s bytes.
 */
static inline size_t computeSizeHdmvButtonOverlapGroup(
  const HdmvButtonOverlapGroupParameters param
)
{
  size_t size = 3ull;
  for (uint8_t i = 0; i < param.number_of_buttons; i++)
    size += computeSizeHdmvButton(param.buttons[i]);
  return size;
}

/* ######### Page : ######################################################## */

#define HDMV_MAX_NB_ICS_BOGS  255

typedef struct {
  uint8_t page_id;
  uint8_t page_version_number;

  uint64_t UO_mask_table;

  HdmvEffectSequenceParameters in_effects;
  HdmvEffectSequenceParameters out_effects;
  uint8_t animation_frame_rate_code;

  uint16_t default_selected_button_id_ref;
  uint16_t default_activated_button_id_ref;

  uint8_t palette_id_ref;

  uint8_t number_of_BOGs;
  HdmvButtonOverlapGroupParameters * bogs;
} HdmvPageParameters;

static inline void initHdmvPageParameters(
  HdmvPageParameters * dst
)
{
  *dst = (HdmvPageParameters) {
    0
  };
}

static inline void cleanHdmvPageParameters(
  HdmvPageParameters page
)
{
  for (uint8_t i = 0; i < page.number_of_BOGs; i++)
    cleanHdmvButtonOverlapGroupParameters(page.bogs[i]);
  free(page.bogs);
}

static inline int allocateBogsHdmvPageParameters(
  HdmvPageParameters * page
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

static inline int copyHdmvPageParameters(
  HdmvPageParameters * dst,
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

/** \~english
 * \brief Computes and return size required by Page() structure.
 *
 * \param param Structure parameters.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Composed of:
 *  - u8  : page_id;
 *  - u8  : page_version_number;
 *  - u64 : UO_mask_table();
 *  - vn  : In_effects();
 *  - vn  : Out_effects();
 *  - u8  : animation_frame_rate_code;
 *  - u16 : default_selected_button_id_ref;
 *  - u16 : default_activated_button_id_ref;
 *  - u8  : palette_id_ref;
 *  - u8  : number_of_BOGs;
 *  for (i = 0; i < number_of_BOGs; i++)
 *    - vn  : Button_overlap_group(i).
 *
 * => 17 + In_effects() + Out_effects() + Button_overlap_group()s bytes.
 */
static inline size_t computeSizeHdmvPage(
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

static inline bool constantHdmvPageParameters(
  const HdmvPageParameters first,
  const HdmvPageParameters second
)
{
  return CHECK(
    EQUAL(.page_id)
    EQUAL(.page_version_number)
    EQUAL(.UO_mask_table)
    EQUAL(.animation_frame_rate_code)
    SUB_FUN(.in_effects, constantHdmvEffectSequenceParameters)
    SUB_FUN(.out_effects, constantHdmvEffectSequenceParameters)
    EQUAL(.default_selected_button_id_ref)
    EQUAL(.default_activated_button_id_ref)
    EQUAL(.palette_id_ref)
    EQUAL(.number_of_BOGs)
    // TODO
  );
}

/* ######### Interactive Composition : ##################################### */

#define HDMV_MAX_NB_ACTIVE_IC  1

typedef enum {
  HDMV_STREAM_MODEL_MULTIPLEXED  = 0x0,  /**< Muxed in AV stream.            */
  HDMV_STREAM_MODEL_OOM          = 0x1   /**< Solely constitute the AV clip. */
} HdmvStreamModel;

static inline const char * HdmvStreamModelStr(
  HdmvStreamModel stream_model
)
{
  static const char * strings[] = {
    "Multiplexed stream",
    "Out of Mux stream"
  };

  if (stream_model < ARRAY_SIZE(strings))
    return strings[stream_model];
  return "Unknown";
}

typedef enum {
  HDMV_UI_MODEL_NORMAL  = 0x0,  /**< Always-on screen menu interface type.   */
  HDMV_UI_MODEL_POP_UP  = 0x1   /**< Pop-Up menu interface type.             */
} HdmvUserInterfaceModel;

static inline const char * HdmvUserInterfaceModelStr(
  HdmvUserInterfaceModel user_interface_model
)
{
  static const char * strings[] = {
    "Normal",
    "Pop-Up"
  };

  if (user_interface_model < ARRAY_SIZE(strings))
    return strings[user_interface_model];
  return "Unknown";
}

/** \~english
 * \brief Maximum size of an Interactive_composition() structure.
 *
 * Based on a 1MiB Composition Buffer, this value is the maximum possible size
 * to store two Interactive_composition plus 256 Palette Definition Segments
 * (256 PDS of 256 entries, 1285 bytes see #HDMV_MAX_SIZE_PDS, 328960 bytes).
 * That is 1MiB = 1048576 bytes minus 328960 bytes = 719616 bytes divided by
 * two = 359808 bytes.
 *
 * \todo Is this value accurate? Based on the hypothesis of a 1MiB Composition
 * Buffer.
 */
#define HDMV_MAX_IC_LENGTH  359808

#define HDMV_MAX_NB_IC_PAGES  255

typedef struct HdmvInteractiveCompositionParameters {
  uint32_t interactive_composition_length;

  HdmvStreamModel stream_model;
  HdmvUserInterfaceModel user_interface_model;

  int64_t composition_time_out_pts;
  int64_t selection_time_out_pts;  /**< Muxed stream_model related parameters. */
  uint32_t user_time_out_duration;

  uint8_t number_of_pages;
  HdmvPageParameters * pages;
} HdmvICParameters;

static inline void cleanHdmvICParameters(
  HdmvICParameters ic
)
{
  for (uint8_t i = 0; i < ic.number_of_pages; i++)
    cleanHdmvPageParameters(ic.pages[i]);
  free(ic.pages);
}

static inline void resetHdmvICParameters(
  HdmvICParameters * ic
)
{
  cleanHdmvICParameters(*ic);
  memset(ic, 0x00, sizeof(HdmvICParameters));
}

static inline int allocatePagesHdmvICParameters(
  HdmvICParameters * dst
)
{
  if (0 == dst->number_of_pages)
    return 0; // No page
  dst->pages = calloc(dst->number_of_pages, sizeof(HdmvPageParameters));
  if (NULL == dst->pages)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

static inline int copyHdmvICParameters(
  HdmvICParameters * dst,
  const HdmvICParameters src
)
{
  HdmvICParameters ic_copy = src;
  if (allocatePagesHdmvICParameters(&ic_copy) < 0)
    return -1;
  for (uint8_t i = 0; i < src.number_of_pages; i++) {
    if (copyHdmvPageParameters(&ic_copy.pages[i], src.pages[i]) < 0)
      return -1;
  }
  *dst = ic_copy;
  return 0;
}

/** \~english
 * \brief Computes and return size required by
 * Interactive_composition() structure.
 *
 * \param param Structure parameters.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Used to define how many bytes are required to store generated
 * Interactive_composition().
 *
 * Composed of:
 *  - u24 : interactive_composition_length;
 *  - b1  : stream_model;
 *  - b1  : user_interface_model;
 *  - v6  : reserved;
 *  if (stream_model == 0b0)
 *    - v7  : reserved;
 *    - u33 : composition_time_out_pts;
 *    - v7  : reserved;
 *    - u33 : selection_time_out_pts;
 *  - u24 : user_time_out_duration;
 *  - u8  : number_of_pages;
 *  for (i = 0; i < number_of_pages; i++)
 *    - vn  : Page(i).
 *
 * => 8 + (stream_model == 0b0 ? 10 : 0) + Page()s bytes.
 */
static inline size_t computeSizeHdmvInteractiveComposition(
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

/* ######################################################################### */

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvSDParameters sequence_descriptor;
} HdmvICSegmentParameters;

/** \~english
 * \brief Size of Interactive Composition Segment header in bytes.
 *
 * Interactive Compostion segments header is composed of:
 *  - video_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - composition_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_SIZE_ICS_HEADER                                                  \
  (                                                                           \
    HDMV_SIZE_VIDEO_DESCRIPTOR                                                \
    + HDMV_SIZE_COMPOSITION_DESCRIPTOR                                        \
    + HDMV_SIZE_SEQUENCE_DESCRIPTOR                                           \
  )

/* ###### HDMV Palette Definition Segment : ################################ */
/* ######### Palette Descriptor : ########################################## */

#define HDMV_PG_MAX_NB_PAL  8
#define HDMV_IG_MAX_NB_PAL  256
#define HDMV_MAX_NB_PAL  HDMV_IG_MAX_NB_PAL

static inline uint16_t getHdmvMaxNbPal(
  HdmvStreamType type
)
{
  switch (type) {
  case HDMV_STREAM_TYPE_IGS: return HDMV_IG_MAX_NB_PAL;
  case HDMV_STREAM_TYPE_PGS: return HDMV_PG_MAX_NB_PAL;
  }
  LIBBLU_TODO_MSG("Unreachable");
}

typedef struct HdmvPaletteDescriptorParameters {
  uint8_t palette_id;
  uint8_t palette_version_number;
} HdmvPDParameters;

static inline bool constantHdmvPDParameters(
  HdmvPDParameters first,
  HdmvPDParameters second
)
{
  return CHECK(
    EQUAL(.palette_id)
    EQUAL(.palette_version_number)
  );
}

/** \~english
 * \brief Size required by Palette_descriptor() structure in bytes.
 *
 * Composed of:
 *  - u8  : palette_id;
 *  - u8  : palette_version_number.
 *
 * => 2 bytes.
 */
#define HDMV_SIZE_PALETTE_DESCRIPTOR  2

/* ######### Palette Entry : ############################################### */

typedef struct {
  uint8_t y_value;
  uint8_t cr_value;
  uint8_t cb_value;
  uint8_t t_value;
  bool updated;
} HdmvPaletteEntryParameters;

static inline void initHdmvPaletteEntryParameters(
  HdmvPaletteEntryParameters * dst
)
{
  *dst = (HdmvPaletteEntryParameters) {
    0
  };
}

static inline bool constantHdmvPaletteEntryParameters(
  HdmvPaletteEntryParameters first,
  HdmvPaletteEntryParameters second
)
{
  return CHECK(
    EQUAL(.y_value)
    EQUAL(.cr_value)
    EQUAL(.cb_value)
    EQUAL(.t_value)
  );
}

static inline bool constantEntriesHdmvPaletteEntryParameters(
  HdmvPaletteEntryParameters * first,
  HdmvPaletteEntryParameters * second,
  uint16_t nb_pal_entries
)
{
  for (uint16_t i = 0; i < nb_pal_entries; i++) {
    if (!constantHdmvPaletteEntryParameters(first[i], second[i]))
      return false;
  }
  return true;
}

/** \~english
 * \brief Size required by Palette_entry() structure in bytes.
 *
 * Composed of:
 *  - u8  : palette_entry_id;
 *  - u8  : Y_value;
 *  - u8  : Cr_value;
 *  - u8  : Cb_value;
 *  - u8  : T_value.
 *
 * => 5 bytes.
 */
#define HDMV_SIZE_PALETTE_DEFINITION_ENTRY  5u

/* ######################################################################### */

#define HDMV_NB_PAL_ENTRIES  256

#define HDMV_SIZE_PALETTE_DESCRIPTOR  2

#define HDMV_MAX_SIZE_PDS                                                     \
  (HDMV_SIZE_SEGMENT_HEADER + HDMV_SIZE_PALETTE_DESCRIPTOR                    \
   + HDMV_NB_PAL_ENTRIES * HDMV_SIZE_PALETTE_DEFINITION_ENTRY)

#define HDMV_MAX_NB_PDS_ENTRIES  255

/** \~english
 * \brief Transparent color 0xFF id.
 *
 */
#define HDMV_PAL_FF  0xFF

typedef struct {
  HdmvPDParameters palette_descriptor;
  HdmvPaletteEntryParameters palette_entries[HDMV_NB_PAL_ENTRIES];
} HdmvPDSegmentParameters;

static inline bool constantHdmvPDSegmentParameters(
  const HdmvPDSegmentParameters first,
  const HdmvPDSegmentParameters second
)
{
  bool is_equal = CHECK(
    SUB_FUN(.palette_descriptor, constantHdmvPDParameters)
  );
  for (uint16_t i = 0; i < HDMV_MAX_NB_PDS_ENTRIES && is_equal; i++)
    is_equal &= constantHdmvPaletteEntryParameters(
      first.palette_entries[i],
      second.palette_entries[i]
    );
  return is_equal;
}

/* ###### HDMV Object Definition Segment : ################################# */

int32_t computeObjectDataDecodeDuration(
  HdmvStreamType stream_type,
  uint16_t object_width,
  uint16_t object_height
);

int32_t computeObjectDataDecodeDelay(
  HdmvStreamType stream_type,
  uint16_t object_width,
  uint16_t object_height,
  float decode_delay_factor
);

/* ######### Object Descriptor : ########################################### */

#define HDMV_PG_MAX_NB_OBJ  64
#define HDMV_IG_MAX_NB_OBJ  4096
#define HDMV_MAX_NB_OBJ  HDMV_IG_MAX_NB_OBJ

static inline uint16_t getHdmvMaxNbObj(
  HdmvStreamType type
)
{
  switch (type) {
  case HDMV_STREAM_TYPE_IGS: return HDMV_IG_MAX_NB_OBJ;
  case HDMV_STREAM_TYPE_PGS: return HDMV_PG_MAX_NB_OBJ;
  }
  LIBBLU_TODO_MSG("Unreachable");
}

typedef struct HdmvObjectDescriptorParameters {
  uint16_t object_id;
  uint8_t object_version_number;
} HdmvODescParameters;

/** \~english
 * \brief Size required by Object_descritor() structure in bytes.
 *
 * Composed of:
 *  - u16 : object_id;
 *  - u8  : object_version_number;
 *
 * => 3 bytes.
 */
#define HDMV_SIZE_OBJECT_DESCRIPTOR  3

/* ######### Object Data : ################################################# */

#define HDMV_OD_MIN_SIZE  8

#define HDMV_OD_PG_MAX_SIZE  4096

#define HDMV_OD_MAX_DATA_LENGTH  0xFFFFFF

typedef struct HdmvObjectDataParameters {
  HdmvODescParameters object_descriptor;

  uint32_t object_data_length;
  uint16_t object_width;
  uint16_t object_height;
} HdmvODParameters;

/** \~english
 * \brief
 *
 * \param param
 * \return size_t
 *
 * Composed of:
 *  - u24 : object_data_length;
 *  - u16 : object_width;
 *  - u16 : object_height;
 *  - vn  : encoded_data_string. // n = object_data_length
 *
 * => 7 + object_data_length bytes.
 */
static inline uint32_t computeSizeHdmvObjectData(
  const HdmvODParameters param
)
{
  return 3ull + param.object_data_length;
}

/* ######################################################################### */

typedef struct {
  HdmvODescParameters object_descriptor;
  HdmvSDParameters sequence_descriptor;
} HdmvODSegmentParameters;

/** \~english
 * \brief Size required by Object Definition Segment header.
 *
 * Composed of:
 *  - object_descritor() #HDMV_SIZE_OBJECT_DESCRIPTOR;
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_SIZE_OD_SEGMENT_HEADER                                           \
  (HDMV_SIZE_OBJECT_DESCRIPTOR + HDMV_SIZE_SEQUENCE_DESCRIPTOR)

typedef enum {
  ODS_UNUSED_PAGE0,
  ODS_USED_OTHER_PAGE0,
  ODS_DEFAULT_ACTIVATED_PAGE0,
  ODS_DEFAULT_SELECTED_PAGE0,
  ODS_DEFAULT_FIRST_SELECTED_PAGE0,
  ODS_DEFAULT_NORMAL_PAGE0,
  ODS_IN_EFFECT_PAGE0
} HdmvODSUsage;

static inline const char * HdmvODSUsageStr(
  HdmvODSUsage usage
)
{
  static const char * strings[] = {
    "Unused in Page 0",
    "Used in Page 0 for non-first Display Update",
    "Used for a default valid button activated state of Page 0",
    "Used for a default valid button selected state of Page 0",
    "First object used by the default selected button of Page 0",
    "Used for a default valid button normal state of Page 0",
    "Used in in_effect() of Page 0"
  };

  return strings[usage];
}

/* ######################################################################### */

typedef struct {
  HdmvSegmentDescriptor desc;
  struct {
    int32_t pts;
    int32_t dts;
  } timestamps_header;  /**< Only present if !raw_stream_input_mode. */

  uint64_t orig_file_offset;
} HdmvSegmentParameters;

/* ### HDMV Streams Utilities : ############################################ */

static inline bool interpretHdmvVersionNumber(
  uint8_t prev_version_number,
  uint8_t new_version_number,
  bool * is_same_ret
)
{
  assert(NULL != is_same_ret);

  *is_same_ret   = (  prev_version_number              == new_version_number);
  bool is_update = (((prev_version_number + 1) & 0xFF) == new_version_number);

  return *is_same_ret || is_update;
}

/* ######################################################################### */

#endif