#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "systemStream.h"

int preparePATParam(
  PatParameters * dst,
  uint16_t transportStreamId,
  unsigned nbPrograms,
  const uint16_t * progNumbers,
  const uint16_t * pmtPids
)
{
  unsigned i;

  if (MAX_NB_PAT_PROG < nbPrograms)
    LIBBLU_ERROR_RETURN(
      "Unsupported number of PAT programs (%u < %u).\n",
      MAX_NB_PAT_PROG, nbPrograms
    );

  setDefaultPatParameters(dst, transportStreamId, 0x0);

  for (i = 0; i < nbPrograms; i++) {
    if (NULL != progNumbers)
      dst->programs[i].programNumber = progNumbers[i];
    else
      dst->programs[i].programNumber = i;

    if (0x0000 == dst->programs[i].programNumber)
      dst->programs[i].networkPid = pmtPids[i];
    else
      dst->programs[i].programMapPid = pmtPids[i];
  }
  dst->usedPrograms = nbPrograms;

  return 0;
}

int preparePATSystemStream(
  LibbluSystemStream * dst,
  PatParameters param
)
{
  unsigned i;

  size_t tableSize;
  uint16_t sectionLength;

  uint32_t crc32;
  size_t offset;

  /**
   * Computing how many TS packets are needed to quarry PAT,
   * TS packets can currently quarry max TP_SIZE - 4 bytes (header)
   * = 184 bytes :
   */
  if (0 == (sectionLength = sectionLengthPatParameters(param)))
    return -1;
  tableSize =
    DIV_ROUND_UP(sectionLength + 4, (TP_SIZE - 4))
    * (TP_SIZE - 4)
  ;

  assert(0 < tableSize);

  /* TODO: Support multi-packets PAT tables */

  initLibbluSystemStream(dst);
  if (NULL == (dst->data = (uint8_t *) malloc(tableSize * sizeof(uint8_t))))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  dst->dataLength = tableSize;

  offset = 0;

  /* [u8 pointer_field] // 0x00 = Start directly */
  WB_ARRAY(dst->data, offset, 0x00);

  /* [u8 table_id] // 0x00 = 'program_association_section' (PAT) */
  WB_ARRAY(dst->data, offset, 0x00);

  /**
   * [b1 section_syntax_indicator] // 0b1
   * [b1 private_bit_flag/'0'] // 0b0
   * [v2 reserved] // 0b11
   * [u12 section_length]
   */
  WB_ARRAY(dst->data, offset, ((sectionLength >> 8) & 0x0F) | 0xB0);
  WB_ARRAY(dst->data, offset, sectionLength & 0xFF);

  /* [v16 transport_stream_id] */
  WB_ARRAY(dst->data, offset, param.transportStreamId >> 8);
  WB_ARRAY(dst->data, offset, param.transportStreamId & 0xFF);

  /**
   * [v2 reserved] // 0b11
   * [u5 version_number]
   * [b1 current_next_indicator]
   */
  dst->data[offset++] =
    ((param.version & 0x1F) << 1) | param.curNextIndicator | 0xC0
  ;

  /* TODO: Support multi-packets PAT tables */
  /* [u8 section_number] */
  WB_ARRAY(dst->data, offset, 0x00);
  /* [u8 last_section_number] */
  WB_ARRAY(dst->data, offset, 0x00);

  for (i = 0; i < param.usedPrograms; i++) {
    ProgramPatIndex * program;

    program = param.programs + i;

    /* [u16 program_number] */
    WB_ARRAY(dst->data, offset, program->programNumber >> 8);
    WB_ARRAY(dst->data, offset, program->programNumber & 0xFF);

    if (program->programNumber == 0x00) {
      /* [v3 reserved] [u13 network_PID] */
      dst->data[offset++] =
        ((program->networkPid >> 8) & 0x1F) | 0xE0
      ;
      WB_ARRAY(dst->data, offset, program->networkPid & 0xFF);
    }
    else {
      /* [v3 reserved] [u13 program_map_PID] */
      dst->data[offset++] =
        ((program->programMapPid >> 8) & 0x1F) | 0xE0
      ;
      WB_ARRAY(dst->data, offset, program->programMapPid & 0xFF);
    }
  }

  /**
   * Compute CRC of whole table
   * (from 'table_id' inclusive to 'CRC_32' excluded).
   */
  crc32 = lb_compute_crc32(dst->data, 1, sectionLength - 1);

  /* [u32 CRC32] */
  WB_ARRAY(dst->data, offset, crc32 >> 24);
  WB_ARRAY(dst->data, offset, (crc32 >> 16) & 0xFF);
  WB_ARRAY(dst->data, offset, (crc32 >> 8) & 0xFF);
  WB_ARRAY(dst->data, offset, crc32 & 0xFF);

  /* Padding */
  for (; offset < tableSize; offset++)
    dst->data[offset] = 0xFF;

  assert(offset == tableSize);

  return 0;
}

/* ###### Program and Program Element Descriptors : ######################## */

static int writeByteDescriptor(
  Descriptor * dst,
  uint8_t byte
)
{
  assert(NULL != dst);

  if (dst->allocatedSize <= dst->usedSize) {
    uint8_t newSize;
    uint8_t * newArray;

    newSize = GROW_ALLOCATION(dst->allocatedSize, 4);
    if (newSize <= dst->usedSize)
      LIBBLU_ERROR_RETURN("Descriptor (tag: 0x%02" PRIX8 ") is too big.\n");

    if (NULL == (newArray = (uint8_t *) realloc(dst->data, newSize)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    dst->data = newArray;
    dst->allocatedSize = newSize;
  }

  dst->data[dst->usedSize++] = byte;

  return 0;
}

#define WB_DESC(dst, b)                                                       \
  if (writeByteDescriptor(dst, b) < 0)                                        \
    return -1

/* ######### Registration Descriptor (0x05) : ############################## */

int setRegistrationDescriptor(
  Descriptor * dst,
  DescriptorParameters descParam
)
{
  RegistrationDescriptorParameters param = descParam.registrationDescriptor;
  size_t i;

  /* [u32 formatIdentifier] */
  WB_DESC(dst, param.formatIdentifier >> 24);
  WB_DESC(dst, param.formatIdentifier >> 16);
  WB_DESC(dst, param.formatIdentifier >> 8);
  WB_DESC(dst, param.formatIdentifier);

  /* [vn additionnalIdentificationInfoData] */
  if (param.additionalIdentificationInfoLength != 0) {
    for (i = 0; i < param.additionalIdentificationInfoLength; i++)
      WB_DESC(dst, param.additionalIdentificationInfo[i]);
  }

  return 0;
}

/* ######### Partial Transport Stream Descriptor (0x63) : ################## */

int setPartialTSDescriptor(
  Descriptor * dst,
  DescriptorParameters descParam
)
{
  PartialTSDescriptorParameters param = descParam.partialTSDescriptor;

  /* [v2 DVB_reserved_future_use] [u22 peak_rate] */
  WB_DESC(dst, (param.peakRate >> 16) | 0xC0);
  WB_DESC(dst, param.peakRate >> 8);
  WB_DESC(dst, param.peakRate);

  /* [v2 DVB_reserved_future_use] [u22 minimum_overall_smoothing_rate] */
  WB_DESC(dst, (param.minimumOverallSmoothingRate >> 16) | 0xC0);
  WB_DESC(dst, param.minimumOverallSmoothingRate >> 8);
  WB_DESC(dst, param.minimumOverallSmoothingRate);

  /* [v2 DVB_reserved_future_use] [u14 maximum_overall_smoothing_buffer] */
  WB_DESC(dst, (param.maximumOverallSmoothingBuffer >> 8) | 0xC0);
  WB_DESC(dst, param.maximumOverallSmoothingBuffer);

  return 0;
}

/* ######### AVC Video Descriptor (0x28) : ################################# */

int setAVCVideoDescriptorParameters(
  Descriptor * dst,
  DescriptorParameters descParam
)
{
  AVCVideoDescriptorParameters param = descParam.avcVideoDescriptor;

  /* [u8 profile_idc] */
  WB_DESC(dst, param.profileIdc);

  /** u8 constraintFlags
   * -> b1  : constraint_set0_flag
   * -> b1  : constraint_set1_flag
   * -> b1  : constraint_set2_flag
   * -> b1  : constraint_set3_flag
   * -> b1  : constraint_set4_flag
   * -> b1  : constraint_set5_flag
   * -> u2  : AVC_compatible_flags
   */
  WB_DESC(dst, param.constraintFlags);

  /* [u8 level_idc] */
  WB_DESC(dst, param.levelIdc);

  /**
   * [b1 AVC_still_present]
   * [b1 AVC_24_hour_picture_flag]
   * [b1 Frame_Packing_SEI_not_present_flag]
   * [v5 reserved]
   */
  WB_DESC(
    dst,
    (param.avcStillPresent << 7)
    | (param.avc24HourPictureFlag << 6)
    | (param.framePackingSetNotPresentFlag << 5)
    | 0x1F
  );

  return 0;
}

/* ######### AC-3 Audio Descriptor (0x81) : ################################ */

int setAC3AudioDescriptorParameters(
  Descriptor * dst,
  DescriptorParameters descParam
)
{
  AC3AudioDescriptorParameters param = descParam.ac3AudioDescriptor;

  /* [u3 sample_rate_code] [u5 bsid] */
  WB_DESC(dst, (param.sampleRateCode << 5) | (param.bsid & 0x1F));

  /* [u6 bit_rate_code] [u2 surround_mode] */
  WB_DESC(
    dst,
    (param.bitRateMode << 7)
    | ((param.bitRateCode << 2) & 0x7C)
    | (param.surroundMode & 0x3)
  );

  /* [u3 bsmod] [u4 num_channels] [b1 full_svc] */
  WB_DESC(
    dst,
    (param.bsmod << 5)
    | ((param.numChannels << 1) & 0x1E)
    | param.fullSVC
  );

  /* [v8 langcod] // deprecated (0xFF) */
  WB_DESC(dst, 0xFF);

  return 0;
}

/* ######### DTCP Descriptor (0x88) : ###################################### */

int setDTCPDescriptor(
  Descriptor * dst,
  DescriptorParameters descParam
)
{
  DTCPDescriptorParameters param = descParam.dtcpDescriptor;

  /* [u16 CA_System_ID] */
  WB_DESC(dst, param.caSystemId >> 8);
  WB_DESC(dst, param.caSystemId);

  /**
   * [v1 reserved]
   * [b1 Retention_Move_mode]
   * [u3 Retention_State]
   * [b1 EPN]
   * [u2 DTCP_CCI]
   */
  WB_DESC(
    dst,
    (param.retentionMoveMode << 6)
    | ((param.retentionState << 3) & 0x38)
    | (param.epn << 2)
    | (param.dtcpCci & 0x3)
    | 0x80
  );

  /**
   * [v3 Reserved]
   * [b1 DOT]
   * [b1 AST]
   * [b1 Image_Constraint_Token]
   * [u2 APS]
   */
  WB_DESC(
    dst,
    (param.dot << 4)
    | (param.ast << 3)
    | (param.imageConstraintToken << 2)
    | (param.aps & 0x3)
    | 0xE0
  );

  return 0;
}

/* ######################################################################### */

int prepareDescriptor(
  Descriptor * dst,
  DescriptorTagValue tag,
  DescriptorParameters param,
  DescriptorSetFun setFun
)
{
  assert(NULL != dst);

  initDescriptor(dst, tag);

  return setFun(dst, param);
}

/* ######################################################################### */

static inline Descriptor * getNewDescriptorPmtProgramElement(
  PmtProgramElement * elem
)
{
  Descriptor * desc;

  if (PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS <= elem->usedDescriptors)
    return NULL;

  desc = elem->descriptors + elem->usedDescriptors++;
  desc->tag = 0x00; /* Set to Reserved */
  desc->data = NULL;
  desc->allocatedSize = desc->usedSize = 0;

  return desc;
}

static int prepareElementDescriptorsPMTParam(
  PmtProgramElement * dst,
  LibbluESProperties prop,
  LibbluESFmtSpecProp fmtSpecProp
)
{
  int ret;

  DescriptorTagValue descTag;
  DescriptorParameters descParam;
  DescriptorSetFun descSetFun;

  RegistationDescriptorFormatIdentifierValue fmtId;
  uint8_t addFmtIdInfo[PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH];
  size_t addFmtIdInfoLen;

  Descriptor * desc;

  /* Stream registration descriptors : */
  switch (prop.codingType) {
    /* Audio: DTS, DTS-HDMA, DTS-HDHR */
    case STREAM_CODING_TYPE_DTS:
    case STREAM_CODING_TYPE_HDHR:
    case STREAM_CODING_TYPE_HDMA:
    case STREAM_CODING_TYPE_DTSE_SEC:
    /* HDMV */
    case STREAM_CODING_TYPE_IG:
    case STREAM_CODING_TYPE_PG:
      return 0; /* No descriptor needed */

    case STREAM_CODING_TYPE_VC1:
      fmtId = REG_DESC_FMT_ID_VC_1; /* "VC-1" */

      /* VC-1 Additional Identification */
      /* [u8 subDescriptorTag] [v8 VC1ProfileLevel] */
      addFmtIdInfo[0] = 0x01; /* Profile and Level sub-descriptor */
      addFmtIdInfo[1] = (prop.profileIDC << 4) | (prop.levelIDC & 0xF);
      addFmtIdInfoLen = 2;
      break;

    case STREAM_CODING_TYPE_AC3:
    case STREAM_CODING_TYPE_TRUEHD:
    case STREAM_CODING_TYPE_EAC3:
    case STREAM_CODING_TYPE_EAC3_SEC:
      fmtId = REG_DESC_FMT_ID_AC_3; /* "AC-3" */

      /* AC-3 Audio descriptor (Dolby Digital, TrueHD Audio...) */
      addFmtIdInfoLen = 0;
      break;

    case STREAM_CODING_TYPE_H262:
    case STREAM_CODING_TYPE_AVC:
    case STREAM_CODING_TYPE_HEVC:
      fmtId = REG_DESC_FMT_ID_HDMV;

      /* HDMV Video Additional Identification */
      /* [v8 reserved] */
      addFmtIdInfo[0] = 0xFF;
      /* [u8 streamCodingType] */
      addFmtIdInfo[1] = prop.codingType;
      /* [u4 videoFormat] [u4 frameRate] */
      addFmtIdInfo[2] = (prop.videoFormat << 4) | (prop.frameRate & 0xF);
      /* MPEG-2/AVC/HEVC Additional Identification */
      /* [v2 reserved 0x0] [v6 reserved 0x1] */
      addFmtIdInfo[3] = 0x3F;
      addFmtIdInfoLen = 4;
      break;

    case STREAM_CODING_TYPE_LPCM:
      fmtId = REG_DESC_FMT_ID_HDMV;

      /* LPCM Audio Additional Identification */
      /* [v8 reserved] */
      addFmtIdInfo[0] = 0xFF;
      /* [u8 streamCodingType] */
      addFmtIdInfo[1] = prop.codingType;
      /* [u4 audioFormat] [u4 sampleRate] */
      addFmtIdInfo[2] = (prop.audioFormat << 4) | (prop.sampleRate & 0xF);
      /* [u2 bitDepth] [v6 reserved] */
      addFmtIdInfo[3] = (prop.bitDepth << 6) | 0x3F;
      addFmtIdInfoLen = 4;
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unable to define the PMT content for a program element "
        "of type '%s' ('stream_coding_type' == 0x%02" PRIX8 ").\n",
        streamCodingTypeStr(prop.codingType),
        prop.codingType
      );
  }

  descParam.registrationDescriptor = (RegistrationDescriptorParameters) {
    .formatIdentifier = fmtId,
    .additionalIdentificationInfoLength = addFmtIdInfoLen
  };
  memcpy(
    descParam.registrationDescriptor.additionalIdentificationInfo,
    addFmtIdInfo, addFmtIdInfoLen
  );

  if (NULL == (desc = getNewDescriptorPmtProgramElement(dst)))
    LIBBLU_ERROR_RETURN(
      "Too many PMT program element descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS,
      dst->usedDescriptors
    );

  ret = prepareDescriptor(
    desc,
    DESC_TAG_REGISTRATION_DESCRIPTOR,
    descParam,
    setRegistrationDescriptor
  );
  if (ret < 0)
    return -1;

  /* Additional descriptors : */
  /* Some formats requires more descriptors */
  if (
    prop.codingType == STREAM_CODING_TYPE_AVC
    && prop.stillPicture
  ) {
    /* H.264 video with still picture */

    /* AVC video descriptor */
    descTag = DESC_TAG_AVC_VIDEO_DESCRIPTOR;
    descParam.avcVideoDescriptor = (AVCVideoDescriptorParameters) {
      .profileIdc = prop.profileIDC,
      .constraintFlags = fmtSpecProp.h264->constraintFlags,
      .levelIdc = prop.stillPicture,
      .avc24HourPictureFlag = false,
      .framePackingSetNotPresentFlag = true
    };
    descSetFun = setAVCVideoDescriptorParameters;
  }
  else if (
    prop.codingType == STREAM_CODING_TYPE_AC3
    || prop.codingType == STREAM_CODING_TYPE_TRUEHD
    || prop.codingType == STREAM_CODING_TYPE_EAC3
    || prop.codingType == STREAM_CODING_TYPE_EAC3_SEC
  ) {
    /* Dolby Digital or Dolby TrueHD audio */

    /* AC-3 Audio DescriptorPtr */
    descTag = DESC_TAG_AC3_AUDIO_DESCRIPTOR;
    descParam.ac3AudioDescriptor = (AC3AudioDescriptorParameters) {
      .sampleRateCode = fmtSpecProp.ac3->subSampleRate,
      .bsid = fmtSpecProp.ac3->bsid,
      .bitRateMode = fmtSpecProp.ac3->bitrateMode,
      .bitRateCode = fmtSpecProp.ac3->bitrateCode,
      .surroundMode = fmtSpecProp.ac3->surroundCode,
      .bsmod = fmtSpecProp.ac3->bsmod,
      .numChannels = fmtSpecProp.ac3->numChannels,
      .fullSVC = fmtSpecProp.ac3->fullSVC
    };
    descSetFun = setAC3AudioDescriptorParameters;
  }
  else
    return 0; /* No more descriptors */

  if (NULL == (desc = getNewDescriptorPmtProgramElement(dst)))
    LIBBLU_ERROR_RETURN(
      "Too many PMT program element descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS,
      dst->usedDescriptors
    );

  ret = prepareDescriptor(
    desc,
    descTag,
    descParam,
    descSetFun
  );
  if (ret < 0)
    return -1;

  return 0;
}

static inline Descriptor * getNewDescriptorPmtParameters(
  PmtParameters * param
)
{
  if (PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS <= param->usedDescriptors)
    return NULL;
  return param->descriptors + param->usedDescriptors++;
}

static inline PmtProgramElement * getNewProgramElementPmtParameters(
  PmtParameters * param,
  LibbluStreamCodingType streamCodingType,
  uint16_t elementaryPid
)
{
  PmtProgramElement * element;

  if (LIBBLU_MAX_NB_STREAMS <= param->usedElements)
    LIBBLU_ERROR_NRETURN("Too many stream indexes in PMT parameters.\n");

  element = param->elements + param->usedElements++;
  element->streamCodingType = streamCodingType;
  element->elementaryPID = elementaryPid;
  element->usedDescriptors = 0;

  return element;
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
)
{
  int ret;
  unsigned i;

  DescriptorParameters descParam;
  Descriptor * desc;

  assert(NULL != dst);

  setDefaultPmtParameters(dst, programNumber, pcrPID);

  /* Main registration descriptor HDMV */
  descParam.registrationDescriptor = (RegistrationDescriptorParameters) {
    .formatIdentifier = REG_DESC_FMT_ID_HDMV,
    .additionalIdentificationInfoLength = 0
  };

  if (NULL == (desc = getNewDescriptorPmtParameters(dst)))
    LIBBLU_ERROR_RETURN(
      "Too many PMT program descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      dst->usedDescriptors
    );

  ret = prepareDescriptor(
    desc,
    DESC_TAG_REGISTRATION_DESCRIPTOR,
    descParam,
    setRegistrationDescriptor
  );
  if (ret < 0)
    return -1;

  /* DTCP descriptor */
  descParam.dtcpDescriptor = (DTCPDescriptorParameters) {
    .caSystemId = dtcpSettings.caSystemId,
    .retentionMoveMode = dtcpSettings.retentionMoveMode,
    .retentionState = dtcpSettings.retentionState,
    .epn = dtcpSettings.epn,
    .dtcpCci = dtcpSettings.dtcpCci,
    .dot = dtcpSettings.dot,
    .ast = dtcpSettings.ast,
    .imageConstraintToken = dtcpSettings.imageConstraintToken,
    .aps = dtcpSettings.aps
  };

  if (NULL == (desc = getNewDescriptorPmtParameters(dst)))
    LIBBLU_ERROR_RETURN(
      "Too many PMT program descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      dst->usedDescriptors
    );

  ret = prepareDescriptor(
    desc,
    DESC_TAG_DTCP_DESCRIPTOR,
    descParam,
    setDTCPDescriptor
  );
  if (ret < 0)
    return -1;

  for (i = 0; i < nbEsStreams; i++) {
    PmtProgramElement * progElem;
    LibbluStreamCodingType codingType;

    codingType = esStreamsProp[i].codingType;

    progElem = getNewProgramElementPmtParameters(
      dst, codingType, esStreamsPids[i]
    );
    if (NULL == progElem)
      return -1;

    ret = prepareElementDescriptorsPMTParam(
      progElem,
      esStreamsProp[i],
      esStreamsFmtSpecProp[i]
    );
    if (ret < 0)
      return -1;
  }

  return 0;
}

int preparePMTSystemStream(
  LibbluSystemStream * dst,
  PmtParameters param
)
{
  unsigned i, j;

  uint16_t sectionLength;
  size_t tableSize;

  unsigned programInfoLength;

  uint32_t crc32;
  size_t offset;

  /**
   * Computing how many TS packets are needed to quarry PMT,
   * TS packets can currently quarry max TP_SIZE - 4 bytes (header)
   * = 184 bytes :
   */
  if (0 == (sectionLength = sectionLengthPmtParameters(param)))
    return -1;
  tableSize =
    DIV_ROUND_UP(sectionLength + 4, (TP_SIZE - 4))
    * (TP_SIZE - 4)
  ;

  initLibbluSystemStream(dst);
  if (NULL == (dst->data = (uint8_t *) malloc(tableSize * sizeof(uint8_t))))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  dst->dataLength = tableSize;

  offset = 0;

  /* [u8 pointer_field] // 0x00 = Start directly */
  WB_ARRAY(dst->data, offset, 0x00);

  /* [u8 table_id] // 0x02 = 'TS_program_map_section' (PMT) */
  WB_ARRAY(dst->data, offset, 0x02);

  /**
   * [b1 section_syntax_indicator] // 0b1
   * [b1 private_bit_flag/'0'] // 0b0
   * [v2 reserved] // 0b11
   * [u12 section_length]
   */
  WB_ARRAY(dst->data, offset, ((sectionLength >> 8) & 0x0F) | 0xB0);
  WB_ARRAY(dst->data, offset, sectionLength & 0xFF);

  /* [v16 programNumber] */
  WB_ARRAY(dst->data, offset, param.programNumber >> 8);
  WB_ARRAY(dst->data, offset, param.programNumber);

  /**
   * [v2 reserved] // 0b11
   * [u5 version_number] // 0x0 = Only one version.
   * [b1 current_next_indicator] // 0b1
   */
  WB_ARRAY(dst->data, offset, 0xC1);

  /* [u8 section_number] // 0x00 */
  WB_ARRAY(dst->data, offset, 0x00);
  /* [u8 last_section_number] // 0x00 = Only one section */
  WB_ARRAY(dst->data, offset, 0x00);

  /* [v3 reserved] [u13 PCR_PID] */
  WB_ARRAY(dst->data, offset, (param.pcrPID >> 8) | 0xE0);
  WB_ARRAY(dst->data, offset, param.pcrPID);

  programInfoLength = programInfoLengthPmtParameters(param);
  /* [v4 reserved] [u12 program_info_length] */
  WB_ARRAY(dst->data, offset, (programInfoLength >> 8) | 0xF0);
  WB_ARRAY(dst->data, offset, programInfoLength);

  for (i = 0; i < param.usedDescriptors; i++) {
    Descriptor * desc;

    desc = param.descriptors + i;

    /* descriptor() */
    /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
    WB_ARRAY(dst->data, offset, desc->tag);
    WB_ARRAY(dst->data, offset, desc->usedSize);
    WA_ARRAY(dst->data, offset, desc->data, desc->usedSize);
  }

  for (i = 0; i < param.usedElements; i++) {
    PmtProgramElement * elem;
    unsigned esInfoLength;

    elem = param.elements + i;

    /* [u8 stream_type] */
    WB_ARRAY(dst->data, offset, elem->streamCodingType);

    /* [v3 reserved] [u13 elementary_PID] */
    WB_ARRAY(dst->data, offset, ((elem->elementaryPID >> 8) & 0x1F) | 0xE0);
    WB_ARRAY(dst->data, offset, elem->elementaryPID);

    esInfoLength = esInfoLengthPmtProgramElement(*elem);

    /* [v4 reserved] [u12 ES_info_length] */
    WB_ARRAY(dst->data, offset, ((esInfoLength >> 8) & 0x0F) | 0xF0);
    WB_ARRAY(dst->data, offset, esInfoLength);

    for (j = 0; j < elem->usedDescriptors; j++) {
      Descriptor * desc;

      desc = elem->descriptors + j;

      /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
      WB_ARRAY(dst->data, offset, desc->tag);
      WB_ARRAY(dst->data, offset, desc->usedSize);
      WA_ARRAY(dst->data, offset, desc->data, desc->usedSize);
    }
  }

  /**
   * Compute CRC of whole table
   * (from 'table_id' inclusive to 'CRC_32' excluded).
   * section_length + 3 - 4
   */
  crc32 = lb_compute_crc32(dst->data, 1, sectionLength - 1);

  /* [u32 CRC32] */
  WB_ARRAY(dst->data, offset, crc32 >> 24);
  WB_ARRAY(dst->data, offset, (crc32 >> 16) & 0xFF);
  WB_ARRAY(dst->data, offset, (crc32 >> 8) & 0xFF);
  WB_ARRAY(dst->data, offset, crc32 & 0xFF);

  /* Padding */
  while (offset < tableSize)
    WB_ARRAY(dst->data, offset, 0xFF);

  assert(offset == tableSize);

  return 0;
}

/* ### Program Clock Reference (PCR) : ##################################### */

int preparePCRSystemStream(
  LibbluSystemStream * dst
)
{
  initLibbluSystemStream(dst);
  dst->useContinuityCounter = false;
  return 0;
}

/* ### Selection Information Section (SIT) : ############################### */

static inline Descriptor * getNewDescriptorSitService(
  SitService * elem
)
{
  Descriptor * desc;

  if (SIT_MAX_NB_ALLOWED_PROGRAM_DESCRIPTORS <= elem->usedDescriptors)
    return NULL;

  desc = elem->descriptors + elem->usedDescriptors++;
  desc->tag = 0x00; /* Set to Reserved */
  desc->data = NULL;
  desc->allocatedSize = desc->usedSize = 0;

  return desc;
}

static inline Descriptor * getNewDescriptorSitParameters(
  SitParameters * param
)
{
  if (SIT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS <= param->usedDescriptors)
    return NULL;
  return param->descriptors + param->usedDescriptors++;
}

static inline SitService * getNewServiceSitParameters(
  SitParameters * param,
  uint16_t serviceId,
  uint8_t runningStatus
)
{
  SitService * prog;

  if (SIT_MAX_NB_ALLOWED_SERVICES <= param->usedServices)
    LIBBLU_ERROR_NRETURN("Too many program indexes in SIT parameters.\n");

  prog = param->services + param->usedServices++;
  prog->serviceId = serviceId;
  prog->runningStatus = runningStatus;
  prog->usedDescriptors = 0;

  return prog;
}

int prepareSITParam(
  SitParameters * dst,
  uint32_t muxingRate,
  uint16_t programNumber
)
{
  int ret;

  DescriptorParameters descParam;
  Descriptor * desc;

  assert(NULL != dst);

  setDefaultSitParameters(dst);

  /* Partial Transport Stream descriptor */
  descParam.partialTSDescriptor = (PartialTSDescriptorParameters) {
    .peakRate = muxingRate / 400,
      /* e.g. muxingRate = 48 Mb/s = 48000000 b/s */
    .minimumOverallSmoothingRate = 0x3FFFFF,
    .maximumOverallSmoothingBuffer = 0x3FFF
  };

  if (NULL == (desc = getNewDescriptorSitParameters(dst)))
    LIBBLU_ERROR_RETURN(
      "Too many SIT program descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      dst->usedDescriptors
    );

  ret = prepareDescriptor(
    desc,
    DESC_TAG_PARTIAL_TS_DESCRIPTOR,
    descParam,
    setPartialTSDescriptor
  );
  if (ret < 0)
    return -1;

  /* Program */
  if (NULL == getNewServiceSitParameters(dst, programNumber, 0x0))
    LIBBLU_ERROR_RETURN(
      "Too many SIT programs defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      dst->usedDescriptors
    );

  return 0;
}



int prepareSITSystemStream(
  LibbluSystemStream * dst,
  SitParameters param
)
{
  unsigned i, j;

  size_t tableSize;
  uint16_t sectionLength;

  unsigned transmissionInfoLoopLength;

  uint32_t crc32;
  size_t offset;

  if (0 == (sectionLength = sectionLengthSitParameters(param)))
    return -1;
  tableSize =
    DIV_ROUND_UP(sectionLength + 4, (TP_SIZE - 4))
    * (TP_SIZE - 4)
  ;

  initLibbluSystemStream(dst);
  if (NULL == (dst->data = (uint8_t *) malloc(tableSize * sizeof(uint8_t))))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  dst->dataLength = tableSize;

  offset = 0;

  /* [u8 pointer_field] // 0x00 = Start directly */
  WB_ARRAY(dst->data, offset, 0x00);

  /* [u8 table_id] // 0x7F = 'selection_information_section' (SIT) */
  WB_ARRAY(dst->data, offset, 0x7F);

  /**
   * [b1 section_syntax_indicator] // 0b1
   * [b1 private_bit_flag/'1'] // 0b1
   * [v2 reserved] // 0b11
   * [u12 section_length]
   */
  WB_ARRAY(dst->data, offset, ((sectionLength >> 8) & 0x0F) | 0xF0);
  WB_ARRAY(dst->data, offset, sectionLength & 0xFF);

  /* [v16 DVB_reserved_future_use] // 0xFFFF */
  WB_ARRAY(dst->data, offset, 0xFF);
  WB_ARRAY(dst->data, offset, 0xFF);

  /**
   * [v2 ISO_reserved] // 0b11
   * [u5 version_number] // 0x0 = Only one version.
   * [b1 current_next_indicator] // 0b1
   */
  WB_ARRAY(dst->data, offset, 0xC1);

  /* [u8 section_number] // 0x00 */
  WB_ARRAY(dst->data, offset, 0x00);
  /* [u8 last_section_number] // 0x00 = Only one section */
  WB_ARRAY(dst->data, offset, 0x00);

  transmissionInfoLoopLength = transmissionInfoLoopLengthSitParameters(
    param
  );

  /* [v4 dvb_reserved] // 0xF [u12 transmission_info_loop_length] */
  WB_ARRAY(dst->data, offset, (transmissionInfoLoopLength >> 8) | 0xF0);
  WB_ARRAY(dst->data, offset, transmissionInfoLoopLength);

  for (i = 0; i < param.usedDescriptors; i++) {
    Descriptor * desc;

    desc = param.descriptors + i;

    /* descriptor() */
    /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
    WB_ARRAY(dst->data, offset, desc->tag);
    WB_ARRAY(dst->data, offset, desc->usedSize);
    WA_ARRAY(dst->data, offset, desc->data, desc->usedSize);
  }

  for (i = 0; i < param.usedServices; i++) {
    SitService * serv;
    unsigned serviceLoopLength;

    serv = param.services + i;

    /* [u16 service_id] */
    WB_ARRAY(dst->data, offset, serv->serviceId >> 8);
    WB_ARRAY(dst->data, offset, serv->serviceId);

    serviceLoopLength = serviceLoopLengthSitParameters(*serv);

    /* [v1 dvb_reserved] [u3 running_status] [u12 service_loop_length] */
    WB_ARRAY(
      dst->data, offset,
      ((serv->runningStatus & 0x7) << 4)
      | ((serviceLoopLength >> 8) & 0x0F)
      | 0x80
    );
    WB_ARRAY(dst->data, offset, serviceLoopLength);

    for (j = 0; j < serv->usedDescriptors; j++) {
      Descriptor * desc;

      desc = serv->descriptors + j;

      /* [u8 descriptorTag] [u8 descriptorLength] [vn descriptorData] */
      WB_ARRAY(dst->data, offset, desc->tag);
      WB_ARRAY(dst->data, offset, desc->usedSize);
      WA_ARRAY(dst->data, offset, desc->data, desc->usedSize);
    }
  }

  /**
   * Compute CRC of whole table
   * (from 'table_id' inclusive to 'CRC_32' excluded).
   */
  crc32 = lb_compute_crc32(dst->data, 1, sectionLength - 1);

  /* [u32 CRC32] */
  WB_ARRAY(dst->data, offset, crc32 >> 24);
  WB_ARRAY(dst->data, offset, (crc32 >> 16) & 0xFF);
  WB_ARRAY(dst->data, offset, (crc32 >> 8) & 0xFF);
  WB_ARRAY(dst->data, offset, crc32 & 0xFF);

  /* Padding */
  while (offset < tableSize)
    WB_ARRAY(dst->data, offset, 0xFF);

  assert(offset == tableSize);

  return 0;
}

/* ### Null packets (Null) : ############################################### */

int prepareNULLSystemStream(
  LibbluSystemStream * dst
)
{
  size_t tableSize;
  size_t offset;

  tableSize = TP_SIZE - 4;

  initLibbluSystemStream(dst);
  dst->useContinuityCounter = false;
  if (NULL == (dst->data = (uint8_t *) malloc(tableSize * sizeof(uint8_t))))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  dst->dataLength = tableSize;

  for (offset = 0; offset < tableSize; )
    WB_ARRAY(dst->data, offset, 0xFF);

  assert(offset == tableSize);

  return 0;
}