#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "systemStream.h"

static uint32_t _tableDataLength(
  uint16_t section_length
)
{
  /**
   * Compute how many Transport Packets are required to carry the table,
   * A TP can carry max TP_SIZE - 4 bytes (header)
   * = 184 bytes :
   */
  return DIV_ROUND_UP(section_length + 4, (TP_SIZE - 4)) * (TP_SIZE - 4);
}

static int _initLibbluSystemStream(
  LibbluSystemStream * sys_stream,
  uint32_t data_length
)
{
  uint8_t * data = NULL;

  if (0 < data_length) {
    data = (uint8_t *) malloc(data_length);
    if (NULL == data)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  *sys_stream = (LibbluSystemStream) {
    .table_data = data,
    .table_data_length = data_length
  };

  return 0;
}

#define WB_SYSSTREAM(strm, off, val)                                          \
  WB_ARRAY(strm->table_data, off, val)
#define WA_SYSSTREAM(strm, off, val, len)                                     \
  WA_ARRAY(strm->table_data, off, val, len)

int preparePATParam(
  PatParameters * dst,
  uint16_t transport_stream_id,
  uint8_t nb_programs,
  const uint16_t * program_numbers,
  const uint16_t * network_program_map_PIDs
)
{
  if (MAX_NB_PAT_PROG < nb_programs)
    LIBBLU_ERROR_RETURN(
      "Unsupported number of PAT programs (%u < %u).\n",
      MAX_NB_PAT_PROG, nb_programs
    );

  PatParameters pat = (PatParameters) {
    .transport_stream_id = transport_stream_id,
    .current_next_indicator = 0x1,
    .nb_programs = nb_programs
  };

  for (unsigned i = 0; i < nb_programs; i++) {
    ProgramPatIndex * program = &pat.programs[i];
    program->program_number = (NULL != program_numbers) ? program_numbers[i] : i;
    program->network_program_map_PID = network_program_map_PIDs[i];
  }

  *dst = pat;
  return 0;
}

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
static uint16_t _sectionLengthPatParameters(
  PatParameters param
)
{
  uint32_t length = 9u + param.nb_programs * 4u;
  if (1021 < length)
    LIBBLU_ERROR_ZRETURN(
      "PMT 'section_length' exceed 1021 bytes (%u bytes).\n",
      length
    );

  return length;
}

int preparePATSystemStream(
  LibbluSystemStream * dst,
  PatParameters param
)
{
  uint16_t section_length;
  if (0 == (section_length = _sectionLengthPatParameters(param)))
    return -1;

  uint32_t pat_size = _tableDataLength(section_length);
  assert(pat_size <= (TP_SIZE - 4)); // TODO: Support multi-TP PAT tables?

  if (_initLibbluSystemStream(dst, pat_size) < 0)
    return -1;
  size_t offset = 0;

  /* [u8 pointer_field] // 0x00 = Start directly */
  WB_SYSSTREAM(dst, offset, 0x00);

  /* [u8 table_id] // 0x00 = 'program_association_section' (PAT) */
  WB_SYSSTREAM(dst, offset, 0x00);

  /**
   * [b1 section_syntax_indicator] // 0b1
   * [b1 private_bit_flag/'0'] // 0b0
   * [v2 reserved] // 0b11
   * [u12 section_length]
   */
  WB_SYSSTREAM(dst, offset, ((section_length >> 8) & 0x0F) | 0xB0);
  WB_SYSSTREAM(dst, offset,   section_length);

  /* [v16 transport_stream_id] */
  WB_SYSSTREAM(dst, offset, param.transport_stream_id >> 8);
  WB_SYSSTREAM(dst, offset, param.transport_stream_id & 0xFF);

  /**
   * [v2 reserved] // 0b11
   * [u5 version_number]
   * [b1 current_next_indicator]
   */
  WB_SYSSTREAM(
    dst, offset,
    (param.version_number << 1) | param.current_next_indicator | 0xC0
  );

  /* [u8 section_number] */
  WB_SYSSTREAM(dst, offset, 0x00);
  /* [u8 last_section_number] */
  WB_SYSSTREAM(dst, offset, 0x00);

  for (uint8_t i = 0; i < param.nb_programs; i++) {
    const ProgramPatIndex * program = &param.programs[i];

    /* [u16 program_number] */
    WB_SYSSTREAM(dst, offset, program->program_number >> 8);
    WB_SYSSTREAM(dst, offset, program->program_number);

    /* [v3 reserved] [u13 network_PID/program_map_PID] */
    WB_SYSSTREAM(dst, offset, (program->network_program_map_PID >> 8) | 0xE0);
    WB_SYSSTREAM(dst, offset,  program->network_program_map_PID);
  }

  /**
   * Compute CRC of whole table
   * (from 'table_id' inclusive to 'CRC_32' excluded).
   */
  uint32_t CRC_32 = lb_compute_crc32(dst->table_data, 1, section_length - 1);

  /* [u32 CRC_32] */
  WB_SYSSTREAM(dst, offset, CRC_32 >> 24);
  WB_SYSSTREAM(dst, offset, CRC_32 >> 16);
  WB_SYSSTREAM(dst, offset, CRC_32 >>  8);
  WB_SYSSTREAM(dst, offset, CRC_32);

  /* Padding */
  while (offset < pat_size)
    WB_SYSSTREAM(dst, offset, 0xFF);

  assert(offset == pat_size);

  return 0;
}

/* ###### Program and Program Element Descriptors : ######################## */

#define WB_DESC(dst, b)                                                       \
  do {                                                                        \
    if (UINT8_MAX == (dst)->descriptor_length)                                \
      LIBBLU_ERROR_RETURN(                                                    \
        "Descriptor (tag: 0x%02" PRIX8 ") is too big.\n",                     \
        (dst)->descriptor_tag                                                 \
      );                                                                      \
    (dst)->data[(dst)->descriptor_length++] = (b);                            \
  } while (0)

/* ######### Registration Descriptor (0x05) : ############################## */

static int _setRegistrationDescriptor(
  Descriptor * dst,
  DescriptorParameters desc_param
)
{
  RegistrationDescriptorParameters param = desc_param.registration_descriptor;

  /* [u32 format_identifier] */
  WB_DESC(dst, param.format_identifier >> 24);
  WB_DESC(dst, param.format_identifier >> 16);
  WB_DESC(dst, param.format_identifier >> 8);
  WB_DESC(dst, param.format_identifier);

  /* [vn additional_identification_info] */
  for (size_t i = 0; i < param.additional_identification_info_len; i++)
    WB_DESC(dst, param.additional_identification_info[i]);

  return 0;
}

/* ######### AVC Video Descriptor (0x28) : ################################# */

static int _setAVCVideoDescriptorParameters(
  Descriptor * dst,
  DescriptorParameters desc_param
)
{
  AVCVideoDescriptorParameters param = desc_param.AVC_video_descriptor;

  /* [u8 profile_idc] */
  WB_DESC(dst, param.profile_idc);

  /** u8 constraint_flags
   * -> b1  : constraint_set0_flag
   * -> b1  : constraint_set1_flag
   * -> b1  : constraint_set2_flag
   * -> b1  : constraint_set3_flag
   * -> b1  : constraint_set4_flag
   * -> b1  : constraint_set5_flag
   * -> u2  : AVC_compatible_flags
   */
  WB_DESC(dst, param.constraint_flags);

  /* [u8 level_idc] */
  WB_DESC(dst, param.level_idc);

  /**
   * [b1 AVC_still_present]
   * [b1 AVC_24_hour_picture_flag]
   * [b1 Frame_Packing_SEI_not_present_flag]
   * [v5 reserved]
   */
  WB_DESC(
    dst,
    (param.AVC_still_present << 7)
    | (param.AVC_24_hour_picture_flag << 6)
    | (param.Frame_Packing_SEI_not_present_flag << 5)
    | 0x1F
  );

  return 0;
}

/* ######### Partial Transport Stream Descriptor (0x63) : ################## */

static int _setPartialTSDescriptor(
  Descriptor * dst,
  DescriptorParameters desc_param
)
{
  PartialTSDescriptorParameters param = desc_param.partial_transport_stream_descriptor;

  /* [v2 DVB_reserved_future_use] [u22 peak_rate] */
  WB_DESC(dst, (param.peak_rate >> 16) | 0xC0);
  WB_DESC(dst,  param.peak_rate >> 8);
  WB_DESC(dst,  param.peak_rate);

  /* [v2 DVB_reserved_future_use] [u22 minimum_overall_smoothing_rate] */
  WB_DESC(dst, (param.minimum_overall_smoothing_rate >> 16) | 0xC0);
  WB_DESC(dst,  param.minimum_overall_smoothing_rate >> 8);
  WB_DESC(dst,  param.minimum_overall_smoothing_rate);

  /* [v2 DVB_reserved_future_use] [u14 maximum_overall_smoothing_buffer] */
  WB_DESC(dst, (param.maximum_overall_smoothing_buffer >> 8) | 0xC0);
  WB_DESC(dst,  param.maximum_overall_smoothing_buffer);

  return 0;
}

/* ######### AC-3 Audio Descriptor (0x81) : ################################ */

static int _setAC3AudioDescriptorParameters(
  Descriptor * dst,
  DescriptorParameters desc_param
)
{
  AC3AudioDescriptorParameters param = desc_param.ac3_audio_stream_descriptor;

  /* [u3 sample_rate_code] [u5 bsid] */
  WB_DESC(dst, (param.sample_rate_code << 5) | param.bsid);

  /* [u6 bit_rate_code] [u2 surround_mode] */
  WB_DESC(dst, (param.bit_rate_code << 2) | param.surround_mode);

  /* [u3 bsmod] [u4 num_channels] [b1 full_svc] */
  WB_DESC(dst, (param.bsmod << 5) | (param.num_channels << 1) | param.full_svc);

  /* [v8 langcod] // deprecated (0xFF) */
  WB_DESC(dst, 0xFF);

  return 0;
}

/* ######### DTCP Descriptor (0x88) : ###################################### */

static int _setDTCPDescriptor(
  Descriptor * dst,
  DescriptorParameters desc_param
)
{
  DTCPDescriptorParameters param = desc_param.DTCP_descriptor;

  /* [u16 CA_System_ID] */
  WB_DESC(dst, param.CA_System_ID >> 8);
  WB_DESC(dst, param.CA_System_ID);

  /**
   * [v1 reserved]
   * [b1 Retention_Move_mode]
   * [u3 Retention_State]
   * [b1 EPN]
   * [u2 DTCP_CCI]
   */
  WB_DESC(
    dst,
    (param.Retention_Move_mode << 6)
    | ((param.Retention_State << 3) & 0x38)
    | (param.EPN << 2)
    | (param.DTCP_CCI & 0x3)
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
    (param.DOT << 4)
    | (param.AST << 3)
    | (param.Image_Constraint_Token << 2)
    | (param.APS & 0x3)
    | 0xE0
  );

  return 0;
}

/* ######################################################################### */

typedef int (*DescriptorSetFun) (Descriptor *, DescriptorParameters);

static int _prepareDescriptor(
  Descriptor * dst,
  DescriptorTagValue descriptor_tag,
  DescriptorParameters descriptor_param,
  DescriptorSetFun descriptor_set_fun
)
{
  assert(NULL != dst);

  *dst = (Descriptor) {
    .descriptor_tag = descriptor_tag
  };

  return descriptor_set_fun(dst, descriptor_param);
}

/* ######################################################################### */

static Descriptor * _getNewDescriptorPmtProgramElement(
  PmtProgramElement * elem
)
{
  if (PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS <= elem->nb_descriptors)
    return NULL;
  return &elem->descriptors[elem->nb_descriptors++];
}

static int _prepareElementDescriptorsPMTParam(
  PmtProgramElement * dst,
  LibbluESProperties prop,
  LibbluESFmtProp fmt_prop
)
{
  RegistationDescriptorFormatIdentifierValue format_id = REG_DESC_FMT_ID_HDMV;
  uint8_t add_id_info[PMT_REG_DESC_MAX_ADD_ID_INFO_LENGTH];
  uint8_t add_id_info_len = 0;

  /* Stream registration descriptors : */
  switch (prop.coding_type) {
    /* Audio: DTS, DTS-HDMA, DTS-HDHR */
    case STREAM_CODING_TYPE_DTS:
    case STREAM_CODING_TYPE_HDHR:
    case STREAM_CODING_TYPE_HDMA:
    case STREAM_CODING_TYPE_DTSE_SEC:
    /* HDMV */
    case STREAM_CODING_TYPE_IG:
    case STREAM_CODING_TYPE_PG:
      return 0; // No descriptor needed

    case STREAM_CODING_TYPE_VC1:
      format_id = REG_DESC_FMT_ID_VC_1; /* "VC-1" */

      /* VC-1 Additional Identification */
      /* [u8 subDescriptorTag] [v8 VC1ProfileLevel] */
      add_id_info[0] = 0x01; /* Profile and Level sub-descriptor */
      add_id_info[1] = (prop.profile_IDC << 4) | (prop.level_IDC & 0xF);
      add_id_info_len = 2;
      break;

    case STREAM_CODING_TYPE_AC3:
    case STREAM_CODING_TYPE_TRUEHD:
    case STREAM_CODING_TYPE_EAC3:
    case STREAM_CODING_TYPE_EAC3_SEC:
      format_id = REG_DESC_FMT_ID_AC_3; /* "AC-3" */
      /* Following AC-3 Audio descriptor (Dolby Digital, TrueHD Audio...) */
      break;

    case STREAM_CODING_TYPE_H262:
    case STREAM_CODING_TYPE_AVC:
    case STREAM_CODING_TYPE_HEVC:
      format_id = REG_DESC_FMT_ID_HDMV;

      /* HDMV Video Additional Identification */
      /* [v8 reserved] */
      add_id_info[0] = 0xFF;
      /* [u8 streamCodingType] */
      add_id_info[1] = prop.coding_type;
      /* [u4 videoFormat] [u4 frameRate] */
      add_id_info[2] = (prop.video_format << 4) | (prop.frame_rate & 0xF);
      /* MPEG-2/AVC/HEVC Additional Identification */
      /* [v2 reserved 0x0] [v6 reserved 0x1] */
      add_id_info[3] = 0x3F;
      add_id_info_len = 4;
      break;

    case STREAM_CODING_TYPE_LPCM:
      format_id = REG_DESC_FMT_ID_HDMV;

      /* LPCM Audio Additional Identification */
      /* [v8 reserved] */
      add_id_info[0] = 0xFF;
      /* [u8 streamCodingType] */
      add_id_info[1] = prop.coding_type;
      /* [u4 audioFormat] [u4 sampleRate] */
      add_id_info[2] = (prop.audio_format << 4) | (prop.sample_rate & 0xF);
      /* [u2 bitDepth] [v6 reserved] */
      add_id_info[3] = (prop.bit_depth << 6) | 0x3F;
      add_id_info_len = 4;
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unable to define the PMT content for a program element "
        "of type '%s' ('stream_coding_type' == 0x%02" PRIX8 ").\n",
        LibbluStreamCodingTypeStr(prop.coding_type),
        prop.coding_type
      );
  }

  DescriptorParameters desc_param = (DescriptorParameters) {
    .registration_descriptor = {
      .format_identifier = format_id,
      .additional_identification_info_len = add_id_info_len
    }
  };
  memcpy(
    desc_param.registration_descriptor.additional_identification_info,
    add_id_info, add_id_info_len
  );

  Descriptor * desc = _getNewDescriptorPmtProgramElement(dst);
  if (NULL == desc)
    LIBBLU_ERROR_RETURN(
      "Too many PMT program element descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS,
      dst->nb_descriptors
    );

  uint8_t desc_tag = DESC_TAG_REGISTRATION_DESCRIPTOR;
  if (_prepareDescriptor(desc, desc_tag, desc_param, _setRegistrationDescriptor) < 0)
    return -1;

  /* Additional descriptors : */
  /* Some formats requires more descriptors */
  DescriptorSetFun desc_set_fun;

  if (
    prop.coding_type == STREAM_CODING_TYPE_AVC
    && prop.still_picture
  ) {
    /* H.264 video with still picture */

    /* AVC video descriptor */
    desc_tag = DESC_TAG_AVC_VIDEO_DESCRIPTOR;
    desc_param = (DescriptorParameters) {
      .AVC_video_descriptor = {
        .profile_idc = prop.profile_IDC,
        .constraint_flags = fmt_prop.h264->constraint_flags,
        .level_idc = prop.level_IDC,
        .AVC_still_present = prop.still_picture,
        .Frame_Packing_SEI_not_present_flag = true
      }
    };
    desc_set_fun = _setAVCVideoDescriptorParameters;
  }
  else if (
    prop.coding_type == STREAM_CODING_TYPE_AC3
    || prop.coding_type == STREAM_CODING_TYPE_TRUEHD
    || prop.coding_type == STREAM_CODING_TYPE_EAC3
    || prop.coding_type == STREAM_CODING_TYPE_EAC3_SEC
  ) {
    /* Dolby Digital or Dolby TrueHD audio */

    /* AC-3 Audio DescriptorPtr */
    desc_tag = DESC_TAG_AC3_AUDIO_DESCRIPTOR;
    desc_param = (DescriptorParameters) {
      .ac3_audio_stream_descriptor = {
        .sample_rate_code = fmt_prop.ac3->sample_rate_code,
        .bsid             = fmt_prop.ac3->bsid,
        .bit_rate_code    = fmt_prop.ac3->bit_rate_code,
        .surround_mode    = fmt_prop.ac3->surround_mode,
        .bsmod            = fmt_prop.ac3->bsmod,
        .num_channels     = fmt_prop.ac3->num_channels,
        .full_svc         = fmt_prop.ac3->full_svc
      }
    };
    desc_set_fun = _setAC3AudioDescriptorParameters;
  }
  else
    return 0; /* No more descriptors */

  if (NULL == (desc = _getNewDescriptorPmtProgramElement(dst)))
    LIBBLU_ERROR_RETURN(
      "Too many PMT program element descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_PROGRAM_ELEMENT_DESCRIPTORS,
      dst->nb_descriptors
    );

  if (_prepareDescriptor(desc, desc_tag, desc_param, desc_set_fun) < 0)
    return -1;

  return 0;
}

static Descriptor * _getNewDescriptorPmtParameters(
  PmtParameters * param
)
{
  if (PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS <= param->nb_descriptors)
    return NULL;
  return &param->descriptors[param->nb_descriptors++];
}

static PmtProgramElement * _getNewProgramElementPmtParameters(
  PmtParameters * param,
  LibbluStreamCodingType stream_type,
  uint16_t elementary_PID
)
{
  if (LIBBLU_MAX_NB_STREAMS <= param->nb_elements)
    LIBBLU_ERROR_NRETURN("Too many stream indexes in PMT parameters.\n");

  PmtProgramElement * element = &param->elements[param->nb_elements++];
  *element = (PmtProgramElement) {
    .stream_type = stream_type,
    .elementary_PID = elementary_PID
  };

  return element;
}

int preparePMTParam(
  PmtParameters * dst,
  const LibbluESProperties * streams_prop,
  const LibbluESFmtProp * streams_fmt_spec_prop,
  const uint16_t * streams_PIDs,
  unsigned nb_streams,
  LibbluDtcpSettings DTCP_settings,
  uint16_t program_number,
  uint16_t PCR_pid
)
{

  assert(NULL != dst);

  PmtParameters pmt = (PmtParameters) {
    .program_number = program_number,
    .PCR_PID = PCR_pid
  };

  /* Main registration descriptor HDMV */
  DescriptorParameters desc_param = (DescriptorParameters) {
    .registration_descriptor = {
      .format_identifier = REG_DESC_FMT_ID_HDMV
    }
  };

  Descriptor * desc = _getNewDescriptorPmtParameters(&pmt);
  if (NULL == desc)
    LIBBLU_ERROR_RETURN(
      "Too many PMT program descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      pmt.nb_descriptors
    );

  uint8_t desc_tag = DESC_TAG_REGISTRATION_DESCRIPTOR;
  if (_prepareDescriptor(desc, desc_tag, desc_param, _setRegistrationDescriptor) < 0)
    return -1;

  /* DTCP descriptor */
  desc_param = (DescriptorParameters) {
    .DTCP_descriptor = {
      .CA_System_ID = DTCP_settings.caSystemId,
      .Retention_Move_mode = DTCP_settings.retentionMoveMode,
      .Retention_State = DTCP_settings.retentionState,
      .EPN = DTCP_settings.epn,
      .DTCP_CCI = DTCP_settings.dtcpCci,
      .DOT = DTCP_settings.dot,
      .AST = DTCP_settings.ast,
      .Image_Constraint_Token = DTCP_settings.imageConstraintToken,
      .APS = DTCP_settings.aps
    }
  };

  if (NULL == (desc = _getNewDescriptorPmtParameters(&pmt)))
    LIBBLU_ERROR_RETURN(
      "Too many PMT program descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      pmt.nb_descriptors
    );

  desc_tag = DESC_TAG_DTCP_DESCRIPTOR;
  if (_prepareDescriptor(desc, desc_tag, desc_param, _setDTCPDescriptor) < 0)
    return -1;

  for (uint8_t i = 0; i < nb_streams; i++) {
    LibbluStreamCodingType coding_type = streams_prop[i].coding_type;
    PmtProgramElement * elem = _getNewProgramElementPmtParameters(
      &pmt, coding_type, streams_PIDs[i]
    );
    if (NULL == elem)
      return -1;

    if (_prepareElementDescriptorsPMTParam(elem, streams_prop[i], streams_fmt_spec_prop[i]) < 0)
      return -1;
  }

  *dst = pmt;
  return 0;
}

int preparePMTSystemStream(
  LibbluSystemStream * dst,
  const PmtParameters * param
)
{
  uint16_t section_length;
  if (0 == (section_length = sectionLengthPmtParameters(param)))
    return -1;
  size_t pmt_size = _tableDataLength(section_length);

  if (_initLibbluSystemStream(dst, pmt_size) < 0)
    return -1;

  size_t offset = 0;

  /* [u8 pointer_field] // 0x00 = Start directly */
  WB_SYSSTREAM(dst, offset, 0x00);

  /* [u8 table_id] // 0x02 = 'TS_program_map_section' (PMT) */
  WB_SYSSTREAM(dst, offset, 0x02);

  /**
   * [b1 section_syntax_indicator] // 0b1
   * [b1 private_bit_flag/'0'] // 0b0
   * [v2 reserved] // 0b11
   * [u12 section_length]
   */
  WB_SYSSTREAM(dst, offset, ((section_length >> 8) & 0x0F) | 0xB0);
  WB_SYSSTREAM(dst, offset,   section_length);

  /* [v16 program_number] */
  WB_SYSSTREAM(dst, offset, param->program_number >> 8);
  WB_SYSSTREAM(dst, offset, param->program_number);

  /**
   * [v2 reserved] // 0b11
   * [u5 version_number] // 0x0 = Only one version.
   * [b1 current_next_indicator] // 0b1
   */
  WB_SYSSTREAM(dst, offset, 0xC1);

  /* [u8 section_number] // 0x00 */
  WB_SYSSTREAM(dst, offset, 0x00);
  /* [u8 last_section_number] // 0x00 = Only one section */
  WB_SYSSTREAM(dst, offset, 0x00);

  /* [v3 reserved] [u13 PCR_PID] */
  WB_SYSSTREAM(dst, offset, (param->PCR_PID >> 8) | 0xE0);
  WB_SYSSTREAM(dst, offset,  param->PCR_PID);

  uint16_t program_info_length = programInfoLengthPmtParameters(param);
  if (0xFFF < program_info_length)
    LIBBLU_ERROR_RETURN(
      "Too long 'program_info_length' (%u).\n",
      program_info_length
    );

  /* [v4 reserved] [u12 program_info_length] */
  WB_SYSSTREAM(dst, offset, (program_info_length >> 8) | 0xF0);
  WB_SYSSTREAM(dst, offset,  program_info_length);

  for (uint8_t i = 0; i < param->nb_descriptors; i++) {
    const Descriptor * desc = &param->descriptors[i];

    /* descriptor() */
    /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
    WB_SYSSTREAM(dst, offset, desc->descriptor_tag);
    WB_SYSSTREAM(dst, offset, desc->descriptor_length);
    WA_SYSSTREAM(dst, offset, desc->data, desc->descriptor_length);
  }

  for (uint8_t i = 0; i < param->nb_elements; i++) {
    const PmtProgramElement * elem = &param->elements[i];

    /* [u8 stream_type] */
    WB_SYSSTREAM(dst, offset, elem->stream_type);

    /* [v3 reserved] [u13 elementary_PID] */
    WB_SYSSTREAM(dst, offset, (elem->elementary_PID >> 8) | 0xE0);
    WB_SYSSTREAM(dst, offset,  elem->elementary_PID);

    uint16_t ES_info_length = esInfoLengthPmtProgramElement(elem);
    if (0x3FF < ES_info_length)
      LIBBLU_ERROR_RETURN(
        "Too long 'ES_info_length' for program element %u.\n", i
      );

    /* [v4 reserved] [u12 ES_info_length] */
    WB_SYSSTREAM(dst, offset, (ES_info_length >> 8) | 0xF0);
    WB_SYSSTREAM(dst, offset,  ES_info_length);

    for (uint8_t j = 0; j < elem->nb_descriptors; j++) {
      const Descriptor * desc = &elem->descriptors[j];

      /* descriptor() */
      /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
      WB_SYSSTREAM(dst, offset, desc->descriptor_tag);
      WB_SYSSTREAM(dst, offset, desc->descriptor_length);
      WA_SYSSTREAM(dst, offset, desc->data, desc->descriptor_length);
    }
  }

  /**
   * Compute CRC of whole table
   * (from 'table_id' inclusive to 'CRC_32' excluded).
   * section_length + 3 - 4
   */
  uint32_t CRC_32 = lb_compute_crc32(dst->table_data, 1, section_length - 1);

  /* [u32 CRC_32] */
  WB_SYSSTREAM(dst, offset, CRC_32 >> 24);
  WB_SYSSTREAM(dst, offset, CRC_32 >> 16);
  WB_SYSSTREAM(dst, offset, CRC_32 >> 8);
  WB_SYSSTREAM(dst, offset, CRC_32);

  /* Padding */
  while (offset < pmt_size)
    WB_SYSSTREAM(dst, offset, 0xFF);

  assert(offset == pmt_size);
  return 0;
}

/* ### Program Clock Reference (PCR) : ##################################### */

int preparePCRSystemStream(
  LibbluSystemStream * dst
)
{
  return _initLibbluSystemStream(dst, 0x0);
}

/* ### Selection Information Section (SIT) : ############################### */

static Descriptor * _getNewDescriptorSitParameters(
  SitParameters * param
)
{
  if (SIT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS <= param->nb_descriptors)
    return NULL;
  return &param->descriptors[param->nb_descriptors++];
}

static SitService * _getNewServiceSitParameters(
  SitParameters * param,
  uint16_t service_id,
  uint8_t running_status
)
{
  if (SIT_MAX_NB_ALLOWED_SERVICES <= param->nb_services)
    LIBBLU_ERROR_NRETURN("Too many program indexes in SIT parameters.\n");

  assert(running_status <= 0x7);

  SitService * service = &param->services[param->nb_services++];
  *service = (SitService) {
    .service_id = service_id,
    .running_status = running_status
  };

  return service;
}

#if 0
static Descriptor * _getNewDescriptorSitService(
  SitService * elem
)
{
  if (SIT_MAX_NB_ALLOWED_PROGRAM_DESCRIPTORS <= elem->nb_descriptors)
    return NULL;
  return &elem->descriptors[elem->nb_descriptors++];
}
#endif

int prepareSITParam(
  SitParameters * dst,
  uint64_t max_TS_rate,
  uint16_t program_number
)
{
  assert(NULL != dst);

  SitParameters sit = (SitParameters) {0};

  /* Partial Transport Stream descriptor */
  DescriptorParameters desc_param = {
    .partial_transport_stream_descriptor = {
      .peak_rate = max_TS_rate / 400, // e.g. muxingRate = 48 Mb/s = 48000000 b/s
      .minimum_overall_smoothing_rate = 0x3FFFFF,
      .maximum_overall_smoothing_buffer = 0x3FFF
    } // TODO: Does these values change with BD-UHD?
  };

  Descriptor * desc = _getNewDescriptorSitParameters(&sit);
  if (NULL == desc)
    LIBBLU_ERROR_RETURN(
      "Too many SIT program descriptors defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      sit.nb_descriptors
    );

  uint8_t desc_tag = DESC_TAG_PARTIAL_TS_DESCRIPTOR;
  if (_prepareDescriptor(desc, desc_tag, desc_param, _setPartialTSDescriptor) < 0)
    return -1;

  /* Program */
  if (NULL == _getNewServiceSitParameters(&sit, program_number, 0x0))
    LIBBLU_ERROR_RETURN(
      "Too many SIT programs defined (%u < %u).\n",
      PMT_MAX_NB_ALLOWED_MAIN_DESCRIPTORS,
      sit.nb_descriptors
    );

  *dst = sit;
  return 0;
}

int prepareSITSystemStream(
  LibbluSystemStream * dst,
  const SitParameters * param
)
{
  uint16_t section_length;
  if (0 == (section_length = sectionLengthSitParameters(param)))
    return -1;
  uint32_t sit_length = _tableDataLength(section_length);

  if (_initLibbluSystemStream(dst, sit_length) < 0)
    return -1;
  size_t offset = 0;

  /* [u8 pointer_field] // 0x00 = Start directly */
  WB_SYSSTREAM(dst, offset, 0x00);

  /* [u8 table_id] // 0x7F = 'selection_information_section' (SIT) */
  WB_SYSSTREAM(dst, offset, 0x7F);

  /**
   * [b1 section_syntax_indicator] // 0b1
   * [b1 private_bit_flag/'1'] // 0b1
   * [v2 reserved] // 0b11
   * [u12 section_length]
   */
  WB_SYSSTREAM(dst, offset, ((section_length >> 8) & 0x0F) | 0xF0);
  WB_SYSSTREAM(dst, offset,   section_length);

  /* [v16 DVB_reserved_future_use] // 0xFFFF */
  WB_SYSSTREAM(dst, offset, 0xFF);
  WB_SYSSTREAM(dst, offset, 0xFF);

  /**
   * [v2 ISO_reserved] // 0b11
   * [u5 version_number] // 0x0 = Only one version.
   * [b1 current_next_indicator] // 0b1
   */
  WB_SYSSTREAM(dst, offset, 0xC1);

  /* [u8 section_number] // 0x00 */
  WB_SYSSTREAM(dst, offset, 0x00);
  /* [u8 last_section_number] // 0x00 = Only one section */
  WB_SYSSTREAM(dst, offset, 0x00);

  uint16_t trans_info_loop_length = transmissionInfoLoopLengthSitParameters(
    param
  );
  assert(0 < trans_info_loop_length);

  /* [v4 dvb_reserved] // 0xF [u12 transmission_info_loop_length] */
  WB_SYSSTREAM(dst, offset, (trans_info_loop_length >> 8) | 0xF0);
  WB_SYSSTREAM(dst, offset,  trans_info_loop_length);

  for (uint8_t i = 0; i < param->nb_descriptors; i++) {
    const Descriptor * desc = &param->descriptors[i];

    /* descriptor() */
    /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
    WB_SYSSTREAM(dst, offset, desc->descriptor_tag);
    WB_SYSSTREAM(dst, offset, desc->descriptor_length);
    WA_SYSSTREAM(dst, offset, desc->data, desc->descriptor_length);
  }

  for (uint8_t i = 0; i < param->nb_services; i++) {
    const SitService * service = &param->services[i];

    /* [u16 service_id] */
    WB_SYSSTREAM(dst, offset, service->service_id >> 8);
    WB_SYSSTREAM(dst, offset, service->service_id);

    uint16_t service_loop_length = serviceLoopLengthSitParameters(service);
    assert(service_loop_length <= 0xFFF);

    /* [v1 dvb_reserved] [u3 running_status] [u12 service_loop_length] */
    WB_SYSSTREAM(
      dst, offset,
      (service->running_status << 4)
      | ((service_loop_length >> 8) & 0x0F)
      | 0x80
    );
    WB_SYSSTREAM(dst, offset, service_loop_length);

    for (uint8_t j = 0; j < service->nb_descriptors; j++) {
      const Descriptor * desc = &service->descriptors[j];

      /* [u8 descriptor_tag] [u8 descriptor_length] [vn data] */
      WB_SYSSTREAM(dst, offset, desc->descriptor_tag);
      WB_SYSSTREAM(dst, offset, desc->descriptor_length);
      WA_SYSSTREAM(dst, offset, desc->data, desc->descriptor_length);
    }
  }

  /**
   * Compute CRC of whole table
   * (from 'table_id' inclusive to 'CRC_32' excluded).
   */
  uint32_t CRC_32 = lb_compute_crc32(dst->table_data, 1, section_length - 1);

  /* [u32 CRC_32] */
  WB_SYSSTREAM(dst, offset, CRC_32 >> 24);
  WB_SYSSTREAM(dst, offset, CRC_32 >> 16);
  WB_SYSSTREAM(dst, offset, CRC_32 >>  8);
  WB_SYSSTREAM(dst, offset, CRC_32);

  /* Padding */
  while (offset < sit_length)
    WB_SYSSTREAM(dst, offset, 0xFF);

  assert(offset == sit_length);
  return 0;
}

/* ### Null packets (Null) : ############################################### */

int prepareNULLSystemStream(
  LibbluSystemStream * dst
)
{
  uint32_t null_length = _tableDataLength(0x0);

  if (_initLibbluSystemStream(dst, null_length) < 0)
    return -1;

  size_t offset = 0;
  while (offset < null_length)
    WB_SYSSTREAM(dst, offset, 0xFF);

  assert(offset == null_length);
  return 0;
}