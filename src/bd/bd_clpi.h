/** \~english
 * \file bd_clpi.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief CLPI (Clip information) file
 *
 * \xrefitem references "References" "References list"
 *  [1] libbluray (https://www.videolan.org/developers/libbluray.html);\n
 *  [2] tsMuxer (https://github.com/justdan96/tsMuxer);\n
 *  [3] DVDArchitect CLIPDescriptor.xsd.
 */

#ifndef __LIBBLU_MUXER__BD__CLPI_H__
#define __LIBBLU_MUXER__BD__CLPI_H__

#include "bd_common.h"
#include "bd_util.h"

typedef uint8_t BDClip_information_file_name[5];

typedef struct {
  uint16_t length;

  uint8_t Validity_flags;
  uint8_t Format_identifier[4];
  uint8_t Network_information[9];
  uint8_t Stream_format_name[16];
} BDTS_type_info_block;

#define BD_MAX_NB_ATC_DELTA_ENTRIES  16

#define BD_CLIP_STREAM_TYPE__BDAV_MPEG2_TS  0x1

#define BD_APPLICATION_TYPE__RESERVED                        0x0
#define BD_APPLICATION_TYPE__MAIN_TS__MOVIE                  0x1
#define BD_APPLICATION_TYPE__MAIN_TS__TIME_BASED_SLIDE_SHOW  0x2
#define BD_APPLICATION_TYPE__MAIN_TS__BROWSABLE_SLIDE_SHOW   0x3
#define BD_APPLICATION_TYPE__SUB_TS__BROWSABLE_SLIDE_SHOW    0x4
#define BD_APPLICATION_TYPE__SUB_TS__IG_MENU                 0x5
#define BD_APPLICATION_TYPE__SUB_TS__TEXT_SUBTITLE           0x6
#define BD_APPLICATION_TYPE__SUB_TS__ELEMENTARY_STREAM       0x7

typedef struct {
  uint32_t length;

  uint16_t reserved_for_future_use_1;
  uint8_t clip_stream_type;
  uint8_t application_type;

  uint32_t reserved_for_future_use_2;
  bool is_ATC_delta;

  uint32_t TS_recording_rate;
  uint32_t number_of_source_packets;

  uint32_t reserved_for_future_use_3[32];

  BDTS_type_info_block TS_type_info_block;

  // TODO: 'is_ATC_delta' == 0b1
} BDClipInfo;

#define BD_MAX_NB_ATC_SEQUENCES  1
#define BD_MAX_NB_STC_SEQUENCES  1 // TODO: Arbitrary

typedef struct {
  uint32_t length;

  uint8_t reserved_for_word_align;
  uint8_t number_of_ATC_sequences;

  struct {
    uint32_t SPN_ATC_start;
    uint32_t number_of_STC_sequences;
    uint32_t offset_STC_id;
  } ATC_sequence[BD_MAX_NB_ATC_SEQUENCES];

  struct {
    uint16_t PCR_PID;
    uint32_t SPN_STC_start;
    uint32_t presentation_start_time;
    uint32_t presentation_end_time;
  } STC_sequence[BD_MAX_NB_STC_SEQUENCES];
} BDSequenceInfo;

typedef struct {
  uint8_t country_code[2];
  uint8_t copyright_holder[3];
  uint8_t recording_year[2];
  uint8_t recording_number[5];
} BDISRC;

typedef struct {
  uint8_t length;
  uint8_t stream_coding_type;

  union {
    struct {
      uint8_t video_format;
      uint8_t frame_rate;
      uint8_t aspect_ratio;
      bool cc_flag;
      uint32_t reserved_for_future_use_1_2;

      BDISRC ISRC;
      uint32_t reserved_for_future_use_3;
    } video;

    struct {
      uint8_t audio_presentation_type;
      uint8_t sampling_frequency;
      uint8_t audio_language_code[3];

      BDISRC ISRC;
      uint32_t reserved_for_future_use;
    } audio;

    struct {
      uint8_t PG_language_code[3];
      uint8_t reserved_for_future_use_1;

      BDISRC ISRC;
      uint32_t reserved_for_future_use_2;
    } pg;

    struct {
      uint8_t IG_language_code[3];
      uint8_t reserved_for_future_use_1;

      BDISRC ISRC;
      uint32_t reserved_for_future_use_2;
    } ig;

    // TODO: TextST
  };
} BDStreamCodingInfo;

#define BD_MAX_NB_PROGRAM_SEQUENCES  1 // TODO: Arbitrary
#define BD_MAX_NB_STREAMS  16 // TODO: Arbitrary

typedef struct {
  uint32_t length;

  uint8_t reserved_for_word_align;
  uint8_t number_of_program_sequences;

  struct {
    uint32_t SPN_program_sequence_start;
    uint16_t program_map_PID;

    uint8_t number_of_streams_in_ps;
    uint8_t reserved_for_future_use;

    struct {
      uint16_t stream_PID;
      BDStreamCodingInfo StreamCodingInfo;
    } stream[BD_MAX_NB_STREAMS];
  } program_sequence[BD_MAX_NB_PROGRAM_SEQUENCES];
} BDProgramInfo;

typedef struct {
  uint32_t ref_to_EP_fine_id:18;
  uint32_t PTS_EP_coarse:14;
  uint32_t SPN_EP_coarse:32;
} BDEP_map_coarse_entries;

typedef struct {
  uint32_t is_angle_change_point:1;
  uint32_t I_end_position_offset:3;
  uint32_t PTS_EP_fine:11;
  uint32_t SPN_EP_fine:17;
} BDEP_map_fine_entries;

typedef struct {
  uint32_t EP_fine_table_start_address;

  BDEP_map_coarse_entries *coarse_entries;
  BDEP_map_fine_entries *fine_entries;
} BDEP_map_for_one_stream_PID;

#define BD_MAX_NB_OF_STREAM_PID_ENTRIES  1 // TODO: Arbitrary

typedef struct {
  uint8_t number_of_stream_PID_entries;

  struct {
    uint16_t stream_PID;
    uint8_t EP_stream_type;
    uint16_t number_of_EP_coarse_entries;
    uint32_t number_of_EP_fine_entries;
    uint32_t EP_map_for_one_stream_PID_start_address;
  } stream[BD_MAX_NB_OF_STREAM_PID_ENTRIES];

  BDEP_map_for_one_stream_PID EP_map_for_one_stream_PID[BD_MAX_NB_OF_STREAM_PID_ENTRIES];
} BDEP_map;

typedef struct {
  uint32_t length;

  uint16_t reserved_for_word_align;
  uint8_t CPI_type;
  BDEP_map EP_map;
} BDCPI;

typedef struct {
  uint32_t length;
} BDClipMark;

typedef struct {
  uint32_t type_indicator;
  uint32_t version_number;

  uint32_t SequenceInfo_start_address;
  uint32_t ProgramInfo_start_address;
  uint32_t CPI_start_address;
  uint32_t ClipMark_start_address;
  uint32_t ExtensionData_start_address;
  uint32_t reserved_for_future_use[3];

  BDClipInfo ClipInfo;
  BDPaddingWord padding_word_ClipInfo;

  BDSequenceInfo SequenceInfo;
  BDPaddingWord padding_word_SequenceInfo;

  BDProgramInfo ProgramInfo;
  BDPaddingWord padding_word_ProgramInfo;

  BDCPI CPI;
  BDPaddingWord padding_word_CPI;

  BDClipMark ClipMark;
  BDPaddingWord padding_word_ClipMark;

  BDExtensionData ExtensionData;
  BDPaddingWord padding_word_ExtensionData;
} BDCLPI;

static inline void cleanBDCLPI(
  BDCLPI CLPI
)
{
  cleanBDPaddingWord(CLPI.padding_word_ClipInfo);
  cleanBDPaddingWord(CLPI.padding_word_SequenceInfo);
  cleanBDPaddingWord(CLPI.padding_word_CPI);
  cleanBDPaddingWord(CLPI.padding_word_ClipMark);
  cleanBDPaddingWord(CLPI.padding_word_ExtensionData);
}

#endif