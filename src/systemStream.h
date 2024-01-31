/** \~english
 * \file systemStream.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief System stream handling module.
 *
 * \xrefitem references "References" "References list"
 *  [1] Rec. ITU-T H.222 (03/2017);\n
 *  [2] Rec. ITU-T H.264 (08/2021);\n
 *  [3] AC-3/E-AC-3 - ATSC Standard A52-2018;\n
 *  [4] DTLA, Digital Transmission Content Protection Specitification,
 *      Volume 1 (Informational Version), Revision 1.71;\n
 *  [5] ETSI TS 300 468 v1.16.1 (08/2019).\n
 */

#ifndef __LIBBLU_MUXER__SYSTEM_STREAM_H__
#define __LIBBLU_MUXER__SYSTEM_STREAM_H__

#include "elementaryStream.h"
#include "dtcpSettings.h"
#include "muxingSettings.h"
#include "streamCodingType.h"
#include "packetIdentifier.h"
#include "util.h"

/** \~english
 * \brief System Stream handling structure.
 *
 */
typedef struct {
  uint8_t * table_data;      /**< System packet table data array.            */
  size_t table_data_length;  /**< Table data array size in bytes.            */
  size_t table_data_offset;  /**< Current table data writing offset.         */

  bool first_full_table_supplied;  /**< Indicate if at least one complete
    system table as already been fully transmitted.                          */
} LibbluSystemStream;

static inline void cleanLibbluSystemStream(
  LibbluSystemStream stream
)
{
  free(stream.table_data);
}

/* ### Program Association Table (PAT) : ################################### */

typedef struct programElement {
  uint16_t program_number;
  uint16_t network_program_map_PID;  /**< 'network_PID' or 'program_map_PID'. */
} ProgramPatIndex;

#define MAX_NB_PAT_PROG  2

typedef struct {
  uint16_t transport_stream_id;
  uint8_t version_number;
  bool current_next_indicator;

  ProgramPatIndex programs[MAX_NB_PAT_PROG];
  uint8_t nb_programs;
} PatParameters;

#if 0
/** \~english
 * \brief Return in bytes the length of the Program Association Table data
 * (including 'pointer_field') according to supplied parameters.
 *
 * \param param PAT parameters.
 * \return size_t The length of the PAT data in bytes.
 *
 * Composed of:
 *  - u8  : pointer_field
 *  - u8  : table_id
 *  - b1  : section_syntax_indicator
 *  - b1  : privateBitFlag
 *  - v2  : reserved
 *  - u12 : sectionLength
 *  - u16 : transportStreamId
 *  - v2  : reserved
 *  - u5  : versionNumber
 *  - b1  : currentNextIndFlag
 *  - u8  : sectionNumber
 *  - u8  : lastSectionNumber
 *  for _ in N: // Number of programs
 *    - u16 : programNumber
 *    - v3  : reserved
 *    - u13 : programMapPid
 *  - u32 : crc32
 *
 * => 13 + 4 * N bytes.
 */
static inline size_t calcPAT(
  PatParameters param
)
{
  return 13 + 4 * param.usedPrograms;
}
#endif

int preparePATParam(
  PatParameters * dst,
  uint16_t transport_stream_id,
  uint8_t nb_programs,
  const uint16_t * program_numbers,
  const uint16_t * network_program_map_PIDs
);

/** \~english
 * \brief
 *
 * \param dst
 * \param param
 * \return int
 *
 * \todo Enable support of multi-packet PAT ?
 */
int preparePATSystemStream(
  LibbluSystemStream * dst,
  PatParameters param
);

/* ### Program Map Table (PMT) : ########################################### */

/* ###### Program and Program Element Descriptors : ######################## */

/** \~english
 * \brief Program/Program Element descriptor tags values.
 *
 */
typedef enum {
  DESC_TAG_REGISTRATION_DESCRIPTOR  = 0x05,  /**< 5: registration_descriptor */
  DESC_TAG_AVC_VIDEO_DESCRIPTOR     = 0x28,  /**< 40: AVC video descriptor   */

  /* H.222 User Private : */
  DESC_TAG_PARTIAL_TS_DESCRIPTOR    = 0x63,  /**< 99: partial_transport_
    stream_descriptor                                                        */

  DESC_TAG_AC3_AUDIO_DESCRIPTOR     = 0x81,  /**< 129: AC-3 audio descriptor */
  DESC_TAG_DTCP_DESCRIPTOR          = 0x88   /**< 136: DTCP descriptor       */
} DescriptorTagValue;

union DescriptorParameters;

/** \~english
 * \brief Program/Program Element descriptor structure.
 *
 * [1] 2.6 Program and program element descriptors.
 */
typedef struct {
  DescriptorTagValue descriptor_tag;  /**< Descriptor tag.                   */
  uint8_t descriptor_length;          /**< Descriptor data length.           */
  uint8_t data[UINT8_MAX];            /**< Descriptor byte array.            */
} Descriptor;

/* ######### Registration Descriptor (0x05) : ############################## */

/** \~english
 * \brief Registration descriptor format_identifier.
 *
 * Values are attributed by the SMPTE Registration Authority (nominated by the
 * ISO/IEC JTC 1/SC 29). List of registered values can be founded at
 * https://smpte-ra.org/registered-mpeg-ts-ids.
 */
typedef enum {
  REG_DESC_FMT_ID_HDMV  = 0x48444D56,  /**< 'HDMV' Blu-ray identifier.       */
  REG_DESC_FMT_ID_AC_3  = 0x41432D33,  /**< 'AC-3' AC-3 audio identifier.    */
  REG_DESC_FMT_ID_VC_1  = 0x56432D31   /**< 'VC-1' VC-1 video identifier.    */
} RegistationDescriptorFormatIdentifierValue;

/** \~english
 * \brief Registration Descriptor maximum 'additional_identification_info'
 * field supported length in bytes.
 *
 * See #RegistrationDescriptorParameters.
 */
#define PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH  8

/** \~english
 * \brief Registration Descriptor structure.
 *
 * Descriptor tag: #DESC_TAG_REGISTRATION_DESCRIPTOR (0x05).
 *
 * [1] 2.6.8 Registration descriptor.
 */
typedef struct {
  RegistationDescriptorFormatIdentifierValue format_identifier;
  uint8_t additional_identification_info[
    PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH
  ];                                          /**<
    'additional_identification_info' field.                                  */
  size_t additional_identification_info_len;  /**<
    'additional_identification_info' field used size. This value is in the
    inclusive range between 0 and #PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH.      */
} RegistrationDescriptorParameters;

/* ######### Partial Transport Stream Descriptor (0x63) : ################## */

/** \~english
 * \brief Partial Transport Stream Descriptor structure.
 *
 * Descriptor tag: #DESC_TAG_PARTIAL_TS_DESCRIPTOR (0x63).
 *
 * [5] 7.2.1 Partial Transport Stream (TS) descriptor.
 */
typedef struct {
  uint32_t peak_rate;
  uint32_t minimum_overall_smoothing_rate;
  uint32_t maximum_overall_smoothing_buffer;
} PartialTSDescriptorParameters;

/* ######### AVC Video Descriptor (0x28) : ################################# */

typedef struct {
  uint8_t profile_idc;       /**< 'profile_idc' field.                        */
  uint8_t constraint_flags;  /**<
    From bits 0 to 5, 'constraint_set*n*_flag'. Bits 6 and 7 covers
    'AVC_compatible_flags' field.                                            */
  uint8_t level_idc;         /**< 'level_idc' field.                          */

  bool AVC_still_present;                   /**< The AVC video stream may
    include still pictures.                                                  */
  bool AVC_24_hour_picture_flag;            /**< The AVC video stream may
    contain 24-hour pictures as defined in [1] 2.1.2.                        */
  bool Frame_Packing_SEI_not_present_flag;  /**< If true, indicates presence
    of Frame packing arrangement SEI message (described in [2] D.2.26) or
    Stereo video information SEI message (described in [2] D.2.23) in video
    bitstream. Otherwise, the presence of these messages is unspecified.     */
} AVCVideoDescriptorParameters;

/* ######### AC-3 Audio Descriptor (0x81) : ################################ */

/** \~english
 * \brief
 *
 * [3] A.3 AC-3 Audio Descriptor.
 */
typedef struct {
  uint8_t sample_rate_code;  /**< Sample Rate code.                          */
  uint8_t bsid;              /**< Bit Stream Identification.                 */
  uint8_t bit_rate_code;     /**< Bit rate code.                             */
  uint8_t surround_mode;     /**< Surround mode.                             */
  uint8_t bsmod;             /**< Bit Stream Mode.                           */
  uint8_t num_channels;      /**< Number of channels.                        */
  uint8_t full_svc;          /**< Full service.                              */
} AC3AudioDescriptorParameters;

/* ######### DTCP Descriptor (0x88) : ###################################### */

/** \~english
 * \brief DTCP_
 *
 * [4] Appendix B DTCP_Descriptor for MPEG Transport Streams.
 */
typedef struct {
  uint16_t CA_System_ID; /* 0x0FFF : BDMV HDCP (Sony Corporation) */

  bool Retention_Move_mode;
  uint8_t Retention_State;
  bool EPN;
  uint8_t DTCP_CCI;
  bool DOT;
  bool AST;
  bool Image_Constraint_Token;
  uint8_t APS;
} DTCPDescriptorParameters;

/* ######################################################################### */

typedef union DescriptorParameters {
  RegistrationDescriptorParameters registration_descriptor;  /**< 0x05 */
  AVCVideoDescriptorParameters AVC_video_descriptor;  /**< 0x28 */
  PartialTSDescriptorParameters partial_transport_stream_descriptor;  /**< 0x63 */
  AC3AudioDescriptorParameters ac3_audio_stream_descriptor;  /**< 0x81 */
  DTCPDescriptorParameters DTCP_descriptor;  /**< 0x88 */
} DescriptorParameters;

/* ######################################################################### */

/** \~english
 * \brief Maximum supported descriptors per PMT stream index.
 *
 */
#define PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS  8

typedef struct {
  LibbluStreamCodingType stream_type;
  uint16_t elementary_PID;

  Descriptor descriptors[PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS];
  uint8_t nb_descriptors;
} PmtProgramElement;

static inline uint16_t esInfoLengthPmtProgramElement(
  const PmtProgramElement * elem
)
{
  uint32_t ES_info_length = 0;

  for (uint8_t i = 0; i < elem->nb_descriptors; i++) {
    /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
    ES_info_length += 2 + elem->descriptors[i].descriptor_length;
  }

  return ES_info_length;
}

#define PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS  4

typedef struct {
  uint16_t program_number;
  uint16_t PCR_PID;

  Descriptor descriptors[PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS];
  uint8_t nb_descriptors;

  PmtProgramElement elements[LIBBLU_MAX_NB_STREAMS];
  uint8_t nb_elements;
} PmtParameters;

static inline uint16_t programInfoLengthPmtParameters(
  const PmtParameters * pmt
)
{
  uint32_t program_info_length = 0;

  for (uint8_t i = 0; i < pmt->nb_descriptors; i++) {
    /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
    program_info_length += 2 + pmt->descriptors[i].descriptor_length;
  }

  return program_info_length;
}

int preparePMTParam(
  PmtParameters * dst,
  const LibbluESProperties * streams_prop,
  const LibbluESFmtProp * streams_fmt_spec_prop,
  const uint16_t * streams_PIDs,
  unsigned nb_streams,
  LibbluDtcpSettings DTCP_settings,
  uint16_t program_number,
  uint16_t PCR_PID
);

/** \~english
 * \brief
 *
 * \param param
 * \return uint16_t
 *
 * Composed of:
 *  - u16 : DVB_reserved_future_use
 *  - v2  : ISO_reserved
 *  - u5  : version_number
 *  - b1  : current_next_indicator
 *  - u8  : section_number
 *  - u8  : last_section_number
 *  - v3  : reserved
 *  - u13 : PCR_PID
 *  - v4  : dvb_reserved
 *  - u12 : program_info_length
 *  - v<program_info_length> : descriptor()s
 *  for _ in N1: // Number of program elements (ES)
 *    - u8  : stream_type
 *    - v3  : reserved
 *    - u13 : elementary_PID
 *    - v4  : reserved
 *    - u12 : ES_info_length
 *    - v<ES_info_length> : descriptor()s
 *  - u32 : CRC_32
 *
 * => 13 + program_info_length + N1 * (5 + ES_info_length) bytes.
 */
static inline uint16_t sectionLengthPmtParameters(
  const PmtParameters * pmt
)
{
  uint32_t section_length = 13u;

  uint16_t program_info_length;
  if (1023u < (program_info_length = programInfoLengthPmtParameters(pmt)))
    LIBBLU_ERROR_ZRETURN(
      "PMT 'program_info_length' exceed 1023 bytes (%u bytes).\n",
      program_info_length
    );
  section_length += program_info_length;

  for (uint8_t i = 0; i < pmt->nb_elements; i++) {
    uint16_t ES_info_length = esInfoLengthPmtProgramElement(&pmt->elements[i]);
    if (0x3FF < ES_info_length)
      LIBBLU_ERROR_ZRETURN(
        "PMT element %u 'ES_info_length' exceed 1023 bytes (%u bytes).\n",
        i, ES_info_length
      );
    section_length += 5u + ES_info_length;
  }

  if (1021u < section_length)
    LIBBLU_ERROR_ZRETURN(
      "PMT 'section_length' exceed 1021 bytes (%u bytes).\n",
      section_length
    );

  return section_length;
}

/** \~english
 * \brief
 *
 * \param dst
 * \param param
 * \return int
 */
int preparePMTSystemStream(
  LibbluSystemStream * dst,
  const PmtParameters * param
);

/* ### Program Clock Reference (PCR) : ##################################### */

int preparePCRSystemStream(
  LibbluSystemStream * dst
);

/* ### Selection Information Section (SIT) : ############################### */

#define SIT_MAX_NB_ALLOWED_PROGRAM_DESCRIPTORS  1

typedef struct {
  uint16_t service_id;
  uint8_t running_status;

  Descriptor descriptors[SIT_MAX_NB_ALLOWED_PROGRAM_DESCRIPTORS];
  unsigned nb_descriptors;
} SitService;

static inline uint16_t serviceLoopLengthSitParameters(
  const SitService * service
)
{
  uint16_t service_loop_length = 0u;

  for (unsigned i = 0; i < service->nb_descriptors; i++) {
    /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
    service_loop_length += 2 + service->descriptors[i].descriptor_length;
  }

  return service_loop_length;
}

#define SIT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS  1
#define SIT_MAX_NB_ALLOWED_SERVICES  1

typedef struct {
  Descriptor descriptors[SIT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS];
  uint8_t nb_descriptors;

  SitService services[SIT_MAX_NB_ALLOWED_SERVICES];
  uint8_t nb_services;
} SitParameters;

static inline unsigned transmissionInfoLoopLengthSitParameters(
  const SitParameters * sit
)
{
  uint32_t trans_info_loop_length = 0u;

  for (unsigned i = 0; i < sit->nb_descriptors; i++) {
    /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
    trans_info_loop_length += 2 + sit->descriptors[i].descriptor_length;
  }

  return trans_info_loop_length;
}

int prepareSITParam(
  SitParameters * dst,
  uint64_t max_TS_rate,
  uint16_t program_number
);

/** \~english
 * \brief
 *
 * \param param
 * \return uint16_t
 *
 * Composed of:
 *  - u16 : DVB_reserved_future_use
 *  - v2  : ISO_reserved
 *  - u5  : version_number
 *  - b1  : current_next_indicator
 *  - u8  : section_number
 *  - u8  : last_section_number
 *  - v4  : dvb_reserved
 *  - u12 : transmission_info_loop_length
 *  - v<transmission_info_loop_length> : descriptor()s
 *  for _ in N: // Number of programs
 *    - u16 : service_id
 *    - v1  : DVB_reserved_future_use
 *    - u3  : running_status
 *    - u12 : service_loop_length
 *    - v<service_loop_length> : descriptor()s
 *  - u32 : CRC_32
 *
 * => 11 + transmission_info_loop_length + N * (4 + service_loop_length) bytes.
 */
static inline uint16_t sectionLengthSitParameters(
  const SitParameters * param
)
{
  uint32_t section_length = 11u;

  uint16_t trans_info_loop_length;
  if (0xFFF < (trans_info_loop_length = transmissionInfoLoopLengthSitParameters(param)))
    LIBBLU_ERROR_ZRETURN(
      "SIT 'transmission_info_loop_length' exceeds 4095 bytes (%u bytes).\n",
      trans_info_loop_length
    );
  section_length += trans_info_loop_length;

  for (unsigned i = 0; i < param->nb_descriptors; i++) {
    uint16_t service_loop_length = serviceLoopLengthSitParameters(
      &param->services[i]
    );
    if (0xFFF < service_loop_length)
      LIBBLU_ERROR_ZRETURN(
        "SIT service %u 'service_loop_length' exceeds 4095 bytes (%u bytes).\n",
        i, service_loop_length
      );
    section_length += 4u + service_loop_length;
  }

  if (4093 < section_length)
    LIBBLU_ERROR_ZRETURN(
      "SIT 'section_length' exceed 4093 bytes (%u bytes).\n",
      section_length
    );

  return section_length;
}

int prepareSITSystemStream(
  LibbluSystemStream * dst,
  const SitParameters * param
);

/* ### Null packets (Null) : ############################################### */

int prepareNULLSystemStream(
  LibbluSystemStream * dst
);

#endif