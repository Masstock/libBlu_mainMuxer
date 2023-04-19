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
 *  [3] AC-3/E-AC3 - ATSC Standard A52-2018;\n
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
  uint8_t * data;     /**< System packet table data array.                   */
  size_t dataLength;  /**< Table data array size in bytes.                   */
  size_t dataOffset;  /**< Current table data writing offset.                */

  bool firstFullTableSupplied;  /**< Indicate if at least one complete
    system table as already been fully transmitted.                          */
  bool useContinuityCounter;
} LibbluSystemStream;

static inline void initLibbluSystemStream(
  LibbluSystemStream * stream
)
{
  *stream = (LibbluSystemStream) {
    .useContinuityCounter = true
  };
}

static inline void cleanLibbluSystemStream(
  LibbluSystemStream stream
)
{
  free(stream.data);
}

/* ### Program Association Table (PAT) : ################################### */

typedef struct programElement {
  uint16_t programNumber;
  union {
    uint16_t programMapPid;
    uint16_t networkPid;
  };
} ProgramPatIndex;

#define MAX_NB_PAT_PROG  2

typedef struct {
  uint16_t transportStreamId;
  uint8_t version:5;
  uint8_t curNextIndicator:1;

  ProgramPatIndex programs[MAX_NB_PAT_PROG];
  unsigned usedPrograms;
} PatParameters;

static inline void setDefaultPatParameters(
  PatParameters * dst,
  uint16_t transportStreamId,
  uint8_t version
)
{
  *dst = (PatParameters) {
    .transportStreamId = transportStreamId,
    .version = version,
    .curNextIndicator = 1
  };
}

static inline void cleanPatParameters(
  PatParameters param
)
{
  (void) param; // No extra allocation currently.
}

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

int preparePATParam(
  PatParameters * dst,
  uint16_t transportStreamId,
  unsigned nbPrograms,
  const uint16_t * progNumbers,
  const uint16_t * pmtPids
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
 *  for _ in N: // Number of programs
 *    - u16 : program_number
 *    - v3  : reserved
 *    - u13 : network_PID / program_map_PID
 *  - u32 : CRC_32
 *
 * => 9 + N * 4 bytes.
 */
static inline uint16_t sectionLengthPatParameters(
  PatParameters param
)
{
  unsigned length = 9 + param.usedPrograms * 4;

  if (1021 < length)
    LIBBLU_ERROR_ZRETURN(
      "PMT 'section_length' exceed 1021 bytes (%u bytes).\n",
      length
    );

  return (uint16_t) length;
}

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
  DescriptorTagValue tag;  /**< Descriptor tag.                              */

  uint8_t * data;         /**< Descriptor byte array.                        */
  uint8_t usedSize;       /**< Descriptor byte array used size.              */
  uint8_t allocatedSize;  /**< Descriptor byte array allocated size.         */
} Descriptor;

static inline void initDescriptor(
  Descriptor * dst,
  DescriptorTagValue tag
)
{
  *dst = (Descriptor) {
    .tag = tag
  };
}

static inline void cleanDescriptor(
  Descriptor desc
)
{
  free(desc.data);
}

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
  RegistationDescriptorFormatIdentifierValue formatIdentifier;  /**<
    'format_identifier' field.                                               */

  uint8_t additionalIdentificationInfo[
    PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH
  ];                                          /**<
    'additional_identification_info' field.                                  */
  size_t additionalIdentificationInfoLength;  /**<
    'additional_identification_info' field used size. This value is in the
    inclusive range between 0 and #PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH.      */
} RegistrationDescriptorParameters;

int setRegistrationDescriptor(
  Descriptor * dst,
  union DescriptorParameters descParam
);

/* ######### Partial Transport Stream Descriptor (0x63) : ################## */

/** \~english
 * \brief Partial Transport Stream Descriptor structure.
 *
 * Descriptor tag: #DESC_TAG_PARTIAL_TS_DESCRIPTOR (0x63).
 *
 * [5] 7.2.1 Partial Transport Stream (TS) descriptor.
 */
typedef struct {
  uint32_t peakRate;
  uint32_t minimumOverallSmoothingRate;
  uint32_t maximumOverallSmoothingBuffer;
} PartialTSDescriptorParameters;

int setPartialTSDescriptor(
  Descriptor * dst,
  union DescriptorParameters descParam
);

/* ######### AVC Video Descriptor (0x28) : ################################# */

typedef struct {
  uint8_t profileIdc;       /**< 'profile_idc' field.                        */
  uint8_t constraintFlags;  /**<
    From bits 0 to 5, 'constraint_set*n*_flag'. Bits 6 and 7 covers
    'AVC_compatible_flags' field.                                            */
  uint8_t levelIdc;         /**< 'level_idc' field.                          */

  bool avcStillPresent;                /**< The AVC video stream may include
    still pictures.                                                          */
  bool avc24HourPictureFlag;           /**< The AVC video stream may contain
    24-hour pictures as defined in [1] 2.1.2.                                */
  bool framePackingSetNotPresentFlag;  /**< If true, indicates presence of
    Frame packing arrangement SEI message (described in [2] D.2.26) or
    Stereo video information SEI message (described in [2] D.2.23) in video
    bitstream. Otherwise, the presence of these messages is unspecified.     */
} AVCVideoDescriptorParameters;

int setAVCVideoDescriptorParameters(
  Descriptor * dst,
  union DescriptorParameters descParam
);

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

int setAC3AudioDescriptorParameters(
  Descriptor * dst,
  union DescriptorParameters descParam
);

/* ######### DTCP Descriptor (0x88) : ###################################### */

/** \~english
 * \brief DTCP_
 *
 * [4] Appendix B DTCP_Descriptor for MPEG Transport Streams.
 */
typedef struct {
  uint16_t caSystemId; /* 0x0FFF : BDMV HDCP (Sony Corporation) */

  bool retentionMoveMode;
  uint8_t retentionState;
  bool epn;
  uint8_t dtcpCci;
  bool dot;
  bool ast;
  bool imageConstraintToken;
  uint8_t aps;
} DTCPDescriptorParameters;

int setDTCPDescriptor(
  Descriptor * dst,
  union DescriptorParameters descParam
);

/* ######################################################################### */

typedef union DescriptorParameters {
  RegistrationDescriptorParameters registrationDescriptor;
  PartialTSDescriptorParameters partialTSDescriptor;
  AVCVideoDescriptorParameters avcVideoDescriptor;
  AC3AudioDescriptorParameters ac3AudioDescriptor;
  DTCPDescriptorParameters dtcpDescriptor;
} DescriptorParameters;

typedef int (*DescriptorSetFun) (Descriptor *, DescriptorParameters);

/** \~english
 * \brief
 *
 * \param dst
 * \param tag
 * \param param
 * \param setFun
 * \return int
 *
 * Initialized object must be passed to #cleanDescriptor() after use.
 */
int prepareDescriptor(
  Descriptor * dst,
  DescriptorTagValue tag,
  DescriptorParameters param,
  DescriptorSetFun setFun
);

/* ######################################################################### */

/** \~english
 * \brief Maximum supported descriptors per PMT stream index.
 *
 */
#define PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS  8

typedef struct {
  LibbluStreamCodingType streamCodingType;
  uint16_t elementaryPID;

  Descriptor descriptors[PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS];
  unsigned usedDescriptors;
} PmtProgramElement;

static inline void cleanPmtProgramElement(
  PmtProgramElement elem
)
{
  for (unsigned i = 0; i < elem.usedDescriptors; i++)
    cleanDescriptor(elem.descriptors[i]);
}

static inline unsigned esInfoLengthPmtProgramElement(
  PmtProgramElement elem
)
{

  unsigned len = 0;
  for (unsigned i = 0; i < elem.usedDescriptors; i++) {
    /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
    len += 2 + elem.descriptors[i].usedSize;
  }

  return len;
}

#define PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS  4

typedef struct {
  uint16_t programNumber;
  uint16_t pcrPID;

  Descriptor descriptors[PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS];
  unsigned usedDescriptors;

  PmtProgramElement elements[LIBBLU_MAX_NB_STREAMS];
  unsigned usedElements;
} PmtParameters;

static inline void setDefaultPmtParameters(
  PmtParameters * dst,
  uint16_t programNumber,
  uint16_t pcrPID
)
{
  *dst = (PmtParameters) {
    .programNumber = programNumber,
    .pcrPID = pcrPID
  };
}

static inline void cleanPmtParameters(
  PmtParameters param
)
{
  for (unsigned i = 0; i < param.usedDescriptors; i++)
    cleanDescriptor(param.descriptors[i]);
  for (unsigned i = 0; i < param.usedElements; i++)
    cleanPmtProgramElement(param.elements[i]);
}

static inline unsigned programInfoLengthPmtParameters(
  PmtParameters param
)
{

  unsigned len = 0;
  for (unsigned i = 0; i < param.usedDescriptors; i++) {
    /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
    len += 2 + param.descriptors[i].usedSize;
  }

  return len;
}

int preparePMTParam(
  PmtParameters * dst,
  LibbluESProperties * esStreamsProp,
  LibbluESFmtSpecProp * esStreamsFmtSpecProp,
  uint16_t * esStreamsPids,
  unsigned nbEsStreams,
  LibbluDtcpSettings dtcpSettings,
  uint16_t programNumber,
  uint16_t pcrPID
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
  PmtParameters param
)
{

  unsigned length = 13 + programInfoLengthPmtParameters(param);
  for (unsigned i = 0; i < param.usedElements; i++)
    length += 5 + esInfoLengthPmtProgramElement(param.elements[i]);

  if (1021 < length)
    LIBBLU_ERROR_ZRETURN(
      "PMT 'section_length' exceed 1021 bytes (%u bytes).\n",
      length
    );

  return (uint16_t) length;
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
  PmtParameters param
);

/* ### Program Clock Reference (PCR) : ##################################### */

int preparePCRSystemStream(
  LibbluSystemStream * dst
);

/* ### Selection Information Section (SIT) : ############################### */

#define SIT_MAX_NB_ALLOWED_PROGRAM_DESCRIPTORS  1

typedef struct {
  uint16_t serviceId;         /* 16b */
  uint8_t runningStatus;      /*  3b */

  Descriptor descriptors[SIT_MAX_NB_ALLOWED_PROGRAM_DESCRIPTORS];
  unsigned usedDescriptors;
} SitService;

static inline void cleanSitService(
  SitService serv
)
{
  for (unsigned i = 0; i < serv.usedDescriptors; i++)
    cleanDescriptor(serv.descriptors[i]);
}

static inline unsigned serviceLoopLengthSitParameters(
  SitService param
)
{

  unsigned len = 0;
  for (unsigned i = 0; i < param.usedDescriptors; i++) {
    /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
    len += 2 + param.descriptors[i].usedSize;
  }

  return len;
}

#define SIT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS  1
#define SIT_MAX_NB_ALLOWED_SERVICES  1

typedef struct {
  Descriptor descriptors[SIT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS];
  unsigned usedDescriptors;

  SitService services[SIT_MAX_NB_ALLOWED_SERVICES];
  unsigned usedServices;
} SitParameters;

static inline void setDefaultSitParameters(
  SitParameters * dst
)
{
  *dst = (SitParameters) {
    0
  };
}

static inline void cleanSitParameters(
  SitParameters param
)
{
  for (unsigned i = 0; i < param.usedDescriptors; i++)
    cleanDescriptor(param.descriptors[i]);
  for (unsigned i = 0; i < param.usedServices; i++)
    cleanSitService(param.services[i]);
}

static inline unsigned transmissionInfoLoopLengthSitParameters(
  SitParameters param
)
{

  unsigned len = 0;
  for (unsigned i = 0; i < param.usedDescriptors; i++) {
    /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
    len += 2 + param.descriptors[i].usedSize;
  }

  return len;
}

int prepareSITParam(
  SitParameters * dst,
  uint64_t muxingRate,
  uint16_t programNumber
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
  SitParameters param
)
{

  unsigned length = 11 + transmissionInfoLoopLengthSitParameters(param);
  for (unsigned i = 0; i < param.usedServices; i++)
    length += 4 + serviceLoopLengthSitParameters(param.services[i]);

  if (4093 < length)
    LIBBLU_ERROR_ZRETURN(
      "SIT 'section_length' exceed 4093 bytes (%u bytes).\n",
      length
    );

  return (uint16_t) length;
}

int prepareSITSystemStream(
  LibbluSystemStream * dst,
  SitParameters param
);

/* ### Null packets (Null) : ############################################### */

int prepareNULLSystemStream(
  LibbluSystemStream * dst
);

#endif