/** \~english
 * \file hdmv_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV bitstreams data structures.
 *
 * REFERENCE:
 *  - Patent US 2009/0185789 A1
 *  - Patent US No. 8,638,861 B2
 */

/** \~english
 * \dir hdmv
 *
 * \brief HDMV menus and subtitles (IGS, PGS) bitstreams handling modules.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__DATA_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__DATA_H__

#include "../../common/constantCheckFunctionsMacros.h"
#include "../../../elementaryStreamProperties.h"

#define IGS_MNU_WORD             0x4947 /* "IG" */
#define IGS_SUP_WORD             0x5047 /* "PG" */

#define IGS_MAX_NB_SEG_ICS       1
#define IGS_MAX_NB_SEG_PDS       256
#define IGS_MAX_NB_SEG_ODS       4096
#define IGS_MAX_NB_SEG_END       1

#define PGS_MAX_NB_SEG_PCS       8
#define PGS_MAX_NB_SEG_WDS       1
#define PGS_MAX_NB_SEG_PDS       8
#define PGS_MAX_NB_SEG_ODS       64
#define PGS_MAX_NB_SEG_END       1

#define HDMV_MAX_NB_WDS_WINDOWS  255
#define HDMV_MAX_NB_ICS_COMPOS   1   /* TODO: Support more compositions */
#define HDMV_MAX_NB_ICS_WINDOWS  255
#define HDMV_MAX_NB_ICS_EFFECTS  255
#define HDMV_MAX_NB_ICS_COMP_OBJ 255
#define HDMV_MAX_NB_ICS_BOGS     255
#define HDMV_MAX_NB_ICS_BUTTONS  255

#define HDMV_MAX_OBJ_DATA_LEN    0xFFFFFF

/** \~english
 * \brief Transport Buffer removing data rate (in bits/sec).
 *
 * R_x transfer rate value from Transport Buffer (TB) to Coded Data Buffer
 * (CDB).
 */
#define HDMV_RX_BITRATE          16000000

typedef enum {
  HDMV_SEGMENT_TYPE_PDS   = 0x14,
  HDMV_SEGMENT_TYPE_ODS   = 0x15,
  HDMV_SEGMENT_TYPE_PCS   = 0x16,
  HDMV_SEGMENT_TYPE_WDS   = 0x17,
  HDMV_SEGMENT_TYPE_ICS   = 0x18,
  HDMV_SEGMENT_TYPE_END   = 0x80,

  HDMV_SEGMENT_TYPE_ERROR = 0xFF,
} HdmvSegmentType;

static inline const char * HdmvSegmentTypeStr(
  const HdmvSegmentType type
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
    case HDMV_SEGMENT_TYPE_ERROR:
      return "Error value type segment";
  }

  return "Unknown type segment";
}

typedef struct HdmvVideoDescriptorParameters {
  unsigned video_width;
  unsigned video_height;
  HdmvFrameRateCode frame_rate;
} HdmvVDParameters;


static inline bool areIdenticalHdmvVDParameters(
  const HdmvVDParameters * first,
  const HdmvVDParameters * second
)
{
  return START_CHECK
    EQUAL(->video_width)
    EQUAL(->video_height)
    EQUAL(->frame_rate)
  END_CHECK;
}

typedef enum {
  HDMV_COMPOSITION_STATE_NORMAL            = 0x0,
  HDMV_COMPOSITION_STATE_ACQUISITION_START = 0x1,
  HDMV_COMPOSITION_STATE_EPOCH_START       = 0x2,
  HDMV_COMPOSITION_STATE_EPOCH_CONTINUE    = 0x3,
} HdmvCompositionState;

static inline const char * hdmvCompositionStateStr(
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
  unsigned compositionNumber;
  HdmvCompositionState composition_state;
} HdmvCDParameters;

static inline bool isNewEpochHdmvCDParameters(
  const HdmvCDParameters * param
)
{
  return (HDMV_COMPOSITION_STATE_EPOCH_START == param->composition_state);
}

static inline bool areIdenticalHdmvCDParameters(
  const HdmvCDParameters * first,
  const HdmvCDParameters * second
)
{
  return START_CHECK
    EQUAL(->compositionNumber)
    EQUAL(->composition_state)
  END_CHECK;
}

typedef struct HdmvSequenceDescriptorParameters {
  bool first_in_sequence;
  bool last_in_sequence;
} HdmvSDParameters;

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

typedef struct {
  uint16_t object_id_ref;
  uint16_t window_id_ref;

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
} HdmvCompositionObjectParameters;

static inline void initHdmvCompositionObjectParameters(
  HdmvCompositionObjectParameters * dst
)
{
  *dst = (HdmvCompositionObjectParameters) {
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
 *  - u16 : window_id_ref;
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
 * => 9 + (object_cropped_flag ? 8 : 0) bytes.
 */
static inline size_t computeSizeHdmvCompositionObject(
  const HdmvCompositionObjectParameters * param
)
{
  assert(NULL != param);

  return 9 + (param->object_cropped_flag ? 8 : 0);
}

#define HDMV_MAX_NB_PCS_COMPOS  255

typedef struct HdmvPresentationCompositionParameters {
  bool palette_update_flag;
  uint8_t palette_id_ref;

  uint8_t number_of_composition_objects;
  HdmvCompositionObjectParameters * composition_objects[
    HDMV_MAX_NB_PCS_COMPOS
  ];
} HdmvPCParameters;

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvPCParameters presentation_composition;
} HdmvPcsSegmentParameters;

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

typedef struct HdmvWindowDefinitionParameters {
  uint8_t number_of_windows;

  HdmvWindowInfoParameters * windows[
    HDMV_MAX_NB_WDS_WINDOWS
  ];
} HdmvWDParameters;

typedef struct {
  HdmvWDParameters window_definition;
} HdmvWdsSegmentParameters;

typedef struct {
  uint32_t effect_duration:24;
  uint8_t palette_id_ref;

  unsigned number_of_composition_objects;
  HdmvCompositionObjectParameters * composition_objects[
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
  size_t size;
  unsigned i;

  assert(NULL != param);

  size = 5;
  for (i = 0; i < param->number_of_composition_objects; i++)
    size += computeSizeHdmvCompositionObject(param->composition_objects[i]);

  return size;
}

typedef struct {
  unsigned number_of_windows;
  HdmvWindowInfoParameters * windows[HDMV_MAX_NB_ICS_WINDOWS];

  unsigned number_of_effects;
  HdmvEffectInfoParameters * effects[HDMV_MAX_NB_ICS_EFFECTS];
} HdmvEffectSequenceParameters;

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
  size_t size;
  unsigned i;

  size = 2 + param.number_of_windows * IGS_COMPILER_WINDOW_INFO_LENGTH;
  for (i = 0; i < param.number_of_effects; i++)
    size += computeSizeHdmvEffectInfo(param.effects[i]);

  return size;
}

typedef struct HdmvNavigationCommand {
  struct HdmvNavigationCommand * next;
  uint32_t opCode;
  uint32_t dst;
  uint32_t src;
} HdmvNavigationCommand;

static inline void initHdmvNavigationCommand(
  HdmvNavigationCommand * dst
)
{
  *dst = (HdmvNavigationCommand) {
    0
  };
}

static inline void setNextHdmvNavigationCommand(
  HdmvNavigationCommand * dst,
  HdmvNavigationCommand * next
)
{
  dst->next = next;
}

typedef struct {
  uint16_t button_id;
  uint16_t button_numeric_select_value;

  bool auto_action;

  uint16_t button_horizontal_position;
  uint16_t button_vertical_position;

  struct {
    uint16_t upper_button_id_ref;
    uint16_t lower_button_id_ref;
    uint16_t left_button_id_ref;
    uint16_t right_button_id_ref;
  } neighbor_info;

  struct {
    uint16_t start_object_ref;
    uint16_t end_object_ref;
    bool repeat;
    bool complete;
  } normal_state_info;

  struct {
    uint8_t sound_id_ref;
    uint16_t start_object_ref;
    uint16_t end_object_ref;
    bool repeat;
    bool complete;
  } selected_state_info;

  struct {
    uint8_t sound_id_ref;
    uint16_t start_object_ref;
    uint16_t end_object_ref;
  } activated_state_info;

  unsigned number_of_navigation_commands;
  HdmvNavigationCommand * commands;
} HdmvButtonParam;

static inline void initHdmvButtonParam(
  HdmvButtonParam * dst
)
{
  *dst = (HdmvButtonParam) {
    0
  };
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
  size_t size;
  unsigned i;

  assert(NULL != param);

  size = 3;
  for (i = 0; i < param->number_of_buttons; i++)
    size += computeSizeHdmvButton(param->buttons[i]);

  return size;
}

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
  unsigned char number_of_BOGs;
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
  size_t size;
  unsigned i;

  assert(NULL != param);

  size =
    17
    + computeSizeHdmvEffectSequence(param->in_effects)
    + computeSizeHdmvEffectSequence(param->out_effects)
  ;
  for (i = 0; i < param->number_of_BOGs; i++)
    size += computeSizeHdmvButtonOverlapGroup(param->bogs[i]);

  return size;
}

typedef enum {
  HDMV_STREAM_MODEL_OOM          = 0x0,  /**< Solely constitute the AV clip. */
  HDMV_STREAM_MODEL_MULTIPLEXED  = 0x1   /**< Muxed in AV stream.            */
} HdmvStreamModel;

typedef enum {
  HDMV_UI_MODEL_POP_UP  = 0x0,
  HDMV_UI_MODEL_NORMAL  = 0x1
} HdmvUserInterfaceModel;

#define HDMV_MAX_NB_ICS_PAGES  255

typedef struct HdmvInteractiveCompositionParameters {
  uint32_t interactive_composition_length:24;

  HdmvStreamModel stream_model;
  HdmvUserInterfaceModel user_interface_model;

  struct {
    uint64_t composition_time_out_pts:33;
    uint64_t selection_time_out_pts:33;
  } oomRelatedParam;
  unsigned user_time_out_duration:24;

  unsigned number_of_pages;
  HdmvPageParameters * pages[HDMV_MAX_NB_ICS_PAGES];
} HdmvICParameters;

/** \~english
 * \brief Computes and return size required by
 * interactive_composition() structure.
 *
 * \param param Structure parameters.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Used to define how many bytes are required to store generated
 * interactive_composition().
 * If a NULL pointer is supplied, an assertion error is raised
 * (or return zero if assert are disabled).
 *
 * Composed of:
 *  - u24 : interactive_composition_length;
 *  - b1  : stream_model;
 *  - b1  : user_interface_model;
 *  - v6  : reserved;
 *  if (stream_model == OoM)
 *    - v7  : reserved;
 *    - u33 : composition_time_out_pts;
 *    - v7  : reserved;
 *    - u33 : selection_time_out_pts;
 *  - u24 : user_time_out_duration;
 *  - u8  : number_of_pages;
 *  for (i = 0; i < number_of_pages; i++)
 *    - vn  : Page(i).
 *
 * => 8 + (stream_model == OoM ? 10 : 0) + Page()s bytes.
 */
static inline size_t computeSizeHdmvInteractiveComposition(
  const HdmvICParameters * param
)
{
  size_t size;
  unsigned i;

  assert(NULL != param);

  size = 8;
  if (param->stream_model == HDMV_STREAM_MODEL_OOM)
    size += 10;
  for (i = 0; i < param->number_of_pages; i++)
    size += computeSizeHdmvPage(param->pages[i]);

  return size;
}

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvSDParameters sequence_descriptor;
} HdmvIcsSegmentParameters;

typedef struct HdmvPaletteDefinitionParameters {
  uint8_t palette_id;
  uint8_t palette_version_number;
} HdmvPDParameters;

typedef struct {
  uint8_t palette_entry_id;
  uint8_t y_value;
  uint8_t cr_value;
  uint8_t cb_value;
  uint8_t t_value;
} HdmvPaletteEntryParameters;

static inline void initHdmvPaletteEntryParameters(
  HdmvPaletteEntryParameters * dst
)
{
  *dst = (HdmvPaletteEntryParameters) {
    0
  };
}

#define HDMV_MAX_NB_PDS_ENTRIES  255

typedef struct {
  HdmvPDParameters palette_definition;

  unsigned number_of_palette_entries;
  HdmvPaletteEntryParameters * palette_entries[HDMV_MAX_NB_PDS_ENTRIES];
} HdmvPdsSegmentParameters;

typedef struct HdmvObjectDescriptorParameters {
  uint16_t object_id;
  uint8_t object_version_number;
} HdmvODParameters;

typedef struct HdmvObjectDataParameters {
  uint32_t object_data_length:24;
  uint16_t object_width;
  uint16_t object_height;
} HdmvODataParameters;

static inline void setHdmvODataParameters(
  HdmvODataParameters * dst,
  uint32_t object_data_length,
  uint16_t width,
  uint16_t height
)
{
  *dst = (HdmvODataParameters) {
    .object_data_length = object_data_length,
    .object_width = width,
    .object_height = height
  };
}

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
static inline size_t computeSizeHdmvObjectData(
  const HdmvODataParameters param
)
{
  return 3 + param.object_data_length;
}

typedef struct {
  HdmvODParameters object_descriptor;
  HdmvSDParameters sequence_descriptor;
} HdmvOdsSegmentParameters;

#define HDMV_SEGMENT_HEADER_LENGTH 0x3

typedef struct {
  /* segment_descriptor() */
  HdmvSegmentType type;
  size_t length;

  union {
    HdmvPcsSegmentParameters pcsParam;
    HdmvWdsSegmentParameters wdsParam;
    HdmvIcsSegmentParameters igsParam;
    HdmvPdsSegmentParameters pdsParam;
    HdmvOdsSegmentParameters odsParam;
  } data;

  uint64_t inputFileOffset;
  uint64_t pts; /* Used if !rawStreamInputMode, from MNU. 90kHz clock. */
  uint64_t dts;
} HdmvSegmentParameters;

#endif