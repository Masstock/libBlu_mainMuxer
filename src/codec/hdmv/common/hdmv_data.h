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
  HdmvVDParameters first,
  HdmvVDParameters second
)
{
  return CHECK(
    EQUAL(.video_width)
    EQUAL(.video_height)
    EQUAL(.frame_rate)
  );
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
  HDMV_COMPO_STATE_ACQUISITION_START  = 0x1,
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
#define IGS_COMPILER_WINDOW_INFO_LENGTH  9

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
 *  - u16 : object_horizontal_position;
 *  - u16 : object_vertical_position;
 *  if (object_cropped_flag)
 *    - u16 : object_cropping_horizontal_position;
 *    - u16 : object_cropping_vertical_position;
 *    - u16 : object_cropping_width;
 *    - u16 : object_cropping_height.
 *
 * => 8 + (object_cropped_flag ? 8 : 0) bytes.
 */
static inline size_t computeSizeHdmvCompositionObject(
  const HdmvCOParameters * param
)
{
  assert(NULL != param);

  return 8 + (param->object_cropped_flag ? 8 : 0);
}

/* ######### Presentation Composition : #################################### */

#define HDMV_MAX_NB_PCS_COMPOS  2

typedef struct HdmvPresentationCompositionParameters {
  bool palette_update_flag;
  uint8_t palette_id_ref;

  uint8_t number_of_composition_objects;
  HdmvCOParameters * composition_objects[
    HDMV_MAX_NB_PCS_COMPOS
  ];
} HdmvPCParameters;

int updateHdmvPCParameters(
  HdmvPCParameters * dst,
  const HdmvPCParameters * src
);

/* ######################################################################### */

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvPCParameters presentation_composition;
} HdmvPcsSegmentParameters;

/* ###### HDMV Windows Definition Segment : ################################ */
/* ######### Windows Definition : ########################################## */

#define HDMV_MAX_NB_WDS_WINDOWS  2

typedef struct HdmvWindowDefinitionParameters {
  uint8_t number_of_windows;
  HdmvWindowInfoParameters * windows[
    HDMV_MAX_NB_WDS_WINDOWS
  ];
} HdmvWDParameters;

int updateHdmvWDParameters(
  HdmvWDParameters * dst,
  const HdmvWDParameters * src
);

/* ######################################################################### */

typedef struct {
  HdmvWDParameters window_definition;
} HdmvWdsSegmentParameters;

/* ###### HDMV Interactive Composition Segment : ########################### */
/* ######### Effect Info : ################################################# */

#define HDMV_MAX_NB_ICS_COMP_OBJ  2

typedef struct {
  uint32_t effect_duration;
  uint8_t palette_id_ref;

  uint8_t number_of_composition_objects;
  HdmvCOParameters * composition_objects[
    HDMV_MAX_NB_ICS_COMP_OBJ
  ];
} HdmvEffectInfoParameters;

static inline void initHdmvEffectInfoParameters(
  HdmvEffectInfoParameters * dst
)
{
  *dst = (HdmvEffectInfoParameters) {
    0
  };
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
  const HdmvEffectInfoParameters * param
)
{
  assert(NULL != param);

  size_t size = 5ull;
  for (uint8_t i = 0; i < param->number_of_composition_objects; i++)
    size += computeSizeHdmvCompositionObject(param->composition_objects[i]);

  return size;
}

/* ######### Effect Sequence : ############################################# */

#define HDMV_MAX_NB_ICS_WINDOWS  2

#define HDMV_MAX_NB_ICS_EFFECTS  128

typedef struct {
  uint8_t number_of_windows;
  HdmvWindowInfoParameters * windows[HDMV_MAX_NB_ICS_WINDOWS];
  uint8_t number_of_effects;
  HdmvEffectInfoParameters * effects[HDMV_MAX_NB_ICS_EFFECTS];
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
  size_t size = 2ull + param.number_of_windows * IGS_COMPILER_WINDOW_INFO_LENGTH;
  for (uint8_t i = 0; i < param.number_of_effects; i++)
    size += computeSizeHdmvEffectInfo(param.effects[i]);
  return size;
}

/* ######### Navigation Command : ########################################## */

typedef struct HdmvNavigationCommand {
  struct HdmvNavigationCommand * next;
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

/* ######### Button : ###################################################### */

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

  uint16_t max_initial_width;  /**< Button normal and selected states objects
    maximum width. Computed at check. */
  uint16_t max_initial_height;  /**< Buttons normal and selected states objects
    maximum height. Computed at check. */
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
  const HdmvButtonParam * btn
)
{
  assert(NULL != btn);

  return 35 + btn->number_of_navigation_commands * 12;
}

/* ######### Button Overlap Group : ######################################## */

#define HDMV_MAX_NB_ICS_BUTTONS  255

typedef struct {
  uint16_t default_valid_button_id_ref;

  uint8_t number_of_buttons;
  HdmvButtonParam * buttons[HDMV_MAX_NB_ICS_BUTTONS];
} HdmvButtonOverlapGroupParameters;

static inline void initHdmvButtonOverlapGroupParameters(
  HdmvButtonOverlapGroupParameters * dst
)
{
  *dst = (HdmvButtonOverlapGroupParameters) {
    0
  };
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
  const HdmvButtonOverlapGroupParameters * param
)
{
  assert(NULL != param);

  size_t size = 3ull;
  for (uint8_t i = 0; i < param->number_of_buttons; i++)
    size += computeSizeHdmvButton(param->buttons[i]);
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
  HdmvButtonOverlapGroupParameters * bogs[HDMV_MAX_NB_ICS_BOGS];
} HdmvPageParameters;

static inline void initHdmvPageParameters(
  HdmvPageParameters * dst
)
{
  *dst = (HdmvPageParameters) {
    0
  };
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
  const HdmvPageParameters * param
)
{
  assert(NULL != param);

  size_t size = 17ull;
  size += computeSizeHdmvEffectSequence(param->in_effects);
  size += computeSizeHdmvEffectSequence(param->out_effects);
  for (uint8_t i = 0; i < param->number_of_BOGs; i++)
    size += computeSizeHdmvButtonOverlapGroup(param->bogs[i]);
  return size;
}

/* ######### Interactive Composition : ##################################### */

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

#define HDMV_MAX_NB_IC_PAGES  255

typedef struct HdmvInteractiveCompositionParameters {
  uint32_t interactive_composition_length;

  HdmvStreamModel stream_model;
  HdmvUserInterfaceModel user_interface_model;

  uint64_t composition_time_out_pts;
  uint64_t selection_time_out_pts;  /**< Muxed stream_model related parameters. */
  uint32_t user_time_out_duration;

  uint8_t number_of_pages;
  HdmvPageParameters * pages[HDMV_MAX_NB_IC_PAGES];
} HdmvICParameters;

int updateHdmvICParameters(
  HdmvICParameters * dst,
  const HdmvICParameters * src
);

/** \~english
 * \brief Computes and return size required by
 * interactive_composition() structure.
 *
 * \param param Structure parameters.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Used to define how many bytes are required to store generated
 * interactive_composition().
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
  const HdmvICParameters * param
)
{
  assert(NULL != param);

  size_t size = 8ull;
  if (param->stream_model == HDMV_STREAM_MODEL_MULTIPLEXED)
    size += 10ull;
  for (uint8_t i = 0; i < param->number_of_pages; i++)
    size += computeSizeHdmvPage(param->pages[i]);
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
#define HDMV_SIZE_IC_SEGMENT_HEADER                                           \
  (                                                                           \
    HDMV_SIZE_VIDEO_DESCRIPTOR                                                \
    + HDMV_SIZE_COMPOSITION_DESCRIPTOR                                        \
    + HDMV_SIZE_SEQUENCE_DESCRIPTOR                                           \
  )

/* ###### HDMV Palette Definition Segment : ################################ */
/* ######### Palette Descriptor : ########################################## */

typedef struct HdmvPaletteDescriptorParameters {
  uint8_t palette_id;
  uint8_t palette_version_number;
} HdmvPDParameters;

#define HDMV_MAX_PALETTE_ID  255

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

#define HDMV_MAX_NB_PDS_ENTRIES  255

/** \~english
 * \brief Transparent color 0xFF id.
 *
 */
#define HDMV_PAL_FF  HDMV_MAX_NB_PDS_ENTRIES

typedef struct {
  HdmvPDParameters palette_descriptor;

  uint16_t number_of_palette_entries;
  HdmvPaletteEntryParameters palette_entries[HDMV_MAX_NB_PDS_ENTRIES];
} HdmvPDSegmentParameters;

static inline bool constantHdmvPDSegmentParameters(
  HdmvPDSegmentParameters first,
  HdmvPDSegmentParameters second
)
{
  return CHECK(
    SUB_FUN(.palette_descriptor, constantHdmvPDParameters)
    EQUAL(.number_of_palette_entries)
    EXPR(constantEntriesHdmvPaletteEntryParameters(
      first.palette_entries,
      second.palette_entries,
      first.number_of_palette_entries
    ))
  );
}

int updateHdmvPDSegmentParameters(
  HdmvPDSegmentParameters * dst,
  const HdmvPDSegmentParameters * src
);

/* ###### HDMV Object Definition Segment : ################################# */

int32_t computeObjectDataDecodeDuration(
  HdmvStreamType stream_type,
  uint16_t object_width,
  uint16_t object_height
);

/* ######### Object Descriptor : ########################################### */

#define HDMV_OD_PG_MAX_NB_OBJ  64

#define HDMV_OD_IG_MAX_NB_OBJ  4096

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

int updateHdmvObjectDataParameters(
  HdmvODParameters * dst,
  const HdmvODParameters * src
);

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

/* ######################################################################### */

typedef struct {
  HdmvSegmentDescriptor desc;
  struct {
    int32_t pts;
    int32_t dts;
  } timestamps_header;  /**< Only present if !raw_stream_input_mode. */

  uint64_t orig_file_offset;
} HdmvSegmentParameters;

/* ######################################################################### */

#endif