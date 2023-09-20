#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "scriptParsing.h"

/* ### ESMS ES Properties section : ######################################## */

#define READ_BYTES(f, s, d, e)                                                \
  do {                                                                        \
    if (readBytes(f, d, s) < 0)                                               \
      e;                                                                      \
  } while (0)

/** \~english
 * \brief
 *
 * \param f BitstreamReaderPtr source file.
 * \param s size_t size of the value in bytes (between 1 and 8 bytes).
 * \param d Numerical destination pointer.
 */
#define READ_VALUE(f, s, d, e)                                                \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readValue64BigEndian(f, s, &_val) < 0)                                \
      e;                                                                      \
    if (NULL != (d))                                                          \
      *(d) = _val;                                                            \
  } while (0)

#define SKIP_VALUE(f, s)                                                      \
  do {                                                                        \
    if (skipBytes(f, s) < 0)                                                  \
      return -1;                                                              \
  } while (0)

int parseESPropertiesHeaderEsms(
  BitstreamReaderPtr esms_bs,
  LibbluESProperties * dst,
  uint64_t * PTS_reference_ret,
  uint64_t * PTS_final_ret
)
{
  LIBBLU_SCRIPTR_DEBUG(
    "ES Properties section:\n"
  );

  /* [v32 ESPR_magic] */
  if (checkDirectoryMagic(esms_bs, ES_PROPERTIES_HEADER_MAGIC, 4) < 0)
    return -1;

  LIBBLU_SCRIPTR_DEBUG(
    " ES Properties section magic (ESPR_magic): "
    STR(ES_PROPERTIES_HEADER_MAGIC) ".\n"
  );

  /* [u8 type] */
  READ_VALUE(esms_bs, 1, &dst->type, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Type (type): %s (0x%02" PRIX8 ").\n",
    libbluESTypeStr(dst->type),
    dst->type
  );

  /* [u8 coding_type] */
  READ_VALUE(esms_bs, 1, &dst->coding_type, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Stream coding type (coding_type): %s (0x%02" PRIX8 ").\n",
    LibbluStreamCodingTypeStr(dst->coding_type),
    dst->coding_type
  );

  if (!isSupportedStreamCodingType(dst->coding_type))
    LIBBLU_ERROR_RETURN(
      "Unsupported stream coding type 0x%02" PRIX8 ".\n",
      dst->coding_type
    );

  /* Check concordance between coding_type and type : */
  LibbluESType expected_type;
  if (determineLibbluESType(dst->coding_type, &expected_type) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to identify stream coding type 0x%02" PRIX8 ".\n",
      dst->coding_type
    );

  if (dst->type != expected_type)
    LIBBLU_ERROR_RETURN(
      "Stream type mismatch, expect %s, got %s.\n",
      libbluESTypeStr(expected_type),
      libbluESTypeStr(dst->type)
    );

  /* [u64 PTS_reference] */
  uint64_t PTS_reference;
  READ_VALUE(esms_bs, 8, &PTS_reference, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Presentation Time Stamp reference (PTS_reference): "
    "%" PRIu64 " (0x%016" PRIX64 ").\n",
    PTS_reference,
    PTS_reference
  );

  /* [u32 bitrate] */
  READ_VALUE(esms_bs, 4, &dst->bitrate, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Nominal Bit-Rate (bitrate): %" PRIu32 " (0x%08" PRIX32 ").\n",
    dst->bitrate,
    dst->bitrate
  );

  /* [u64 PTS_final] */
  uint64_t PTS_final;
  READ_VALUE(esms_bs, 8, &PTS_final, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Last Presentation Time Stamp (PTS_final): "
    "%" PRIu64 " (0x%016" PRIX64 ").\n",
    PTS_final,
    PTS_final
  );

  /* [v64 scripting_flags] */
  uint64_t scripting_flags;
  READ_VALUE(esms_bs, 8, &scripting_flags, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Script building flags (scripting_flags): 0x%016" PRIX64 ".\n",
    scripting_flags
  );

  if (NULL != PTS_reference_ret)
    *PTS_reference_ret = PTS_reference;
  if (NULL != PTS_final_ret)
    *PTS_final_ret = PTS_final;

  return 0;
}

static int _checkCrcEntryESPropertiesSourceFilesEsms(
  const lbc * filepath,
  EsmsESSourceFileProperties prop
)
{

  BitstreamReaderPtr file;
  if (NULL == (file = createBitstreamReaderDefBuf(filepath)))
    LIBBLU_ERROR_RETURN("Unable to open source file.\n");

  uint8_t * buf;
  if (NULL == (buf = (uint8_t *) malloc(prop.crc_checked_bytes)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  READ_BYTES(file, prop.crc_checked_bytes, buf, return -1);

  uint32_t crc_result = lb_compute_crc32(buf, 0, prop.crc_checked_bytes);

  closeBitstreamReader(file);
  free(buf);

  if (crc_result != prop.crc)
    LIBBLU_ERROR_RETURN(
      "Source file '%" PRI_LBCS "' checksum error.\n",
      filepath
    );

  return 0;

free_return:
  closeBitstreamReader(file);
  return -1;
}

static int _parseEntryESPropertiesSourceFilesEsms(
  BitstreamReaderPtr esms_bs,
  EsmsESSourceFiles * dst
)
{

  /* [u16 src_filepath_size] */
  uint16_t src_filepath_size;
  READ_VALUE(esms_bs, 2, &src_filepath_size, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "  Filepath size (src_filepath_size): %" PRIu16 " (0x%04" PRIX16 ").\n",
    src_filepath_size,
    src_filepath_size
  );

  if (!src_filepath_size || PATH_BUFSIZE <= src_filepath_size)
    LIBBLU_ERROR_RETURN(
      "Invalid source filepath size (%" PRIu16 " characters).\n",
      src_filepath_size
    );

  /* [u<8*src_filepath_size> src_filepath] */
  uint8_t enc_src_filepath[PATH_BUFSIZE];
  READ_BYTES(esms_bs, src_filepath_size, enc_src_filepath, return -1);
  enc_src_filepath[src_filepath_size] = 0x00;

  /* Convert from UTF-8 to internal char repr. type (if required) */
  lbc * src_filepath;
  if (NULL == (src_filepath = lbc_utf8_convto(enc_src_filepath)))
    return -1;

  LIBBLU_SCRIPTR_DEBUG(
    "  Filepath (src_filepath): '%" PRI_LBCS "'.\n",
    src_filepath
  );

  EsmsESSourceFileProperties prop;

  /* [u16 crc_checked_bytes] */
  READ_VALUE(esms_bs, 2, &prop.crc_checked_bytes, goto free_return);

  LIBBLU_SCRIPTR_DEBUG(
    "  Checksum checked bytes (crc_checked_bytes): "
    "%" PRIu16 " (0x%04" PRIX16 ").\n",
    prop.crc_checked_bytes,
    prop.crc_checked_bytes
  );

  /* [u32 crc] */
  READ_VALUE(esms_bs, 4, &prop.crc, goto free_return);

  LIBBLU_SCRIPTR_DEBUG(
    "  Checksum (crc): 0x%08" PRIX32 ".\n",
    prop.crc,
    prop.crc
  );

  /* Check CRC-32 */
  if (_checkCrcEntryESPropertiesSourceFilesEsms(src_filepath, prop) < 0)
    goto free_return;

  /* Save file */
  if (appendEsmsESSourceFiles(dst, src_filepath, prop) < 0)
    goto free_return;

  free(src_filepath);
  return 0;

free_return:
  free(src_filepath);
  return -1;
}

int parseESPropertiesSourceFilesEsms(
  BitstreamReaderPtr esms_bs,
  EsmsESSourceFiles * dst
)
{
  assert(0 == dst->nb_source_files);

  /* [u8 nb_source_files] */
  uint8_t nb_source_files;
  READ_VALUE(esms_bs, 1, &nb_source_files, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Number of source files (nb_source_files): %" PRIu8 " (0x%02" PRIX8 ").\n",
    nb_source_files,
    nb_source_files
  );

  for (unsigned i = 0; i < nb_source_files; i++) {
    LIBBLU_SCRIPTR_DEBUG(" Source file %u:\n", i);
    if (_parseEntryESPropertiesSourceFilesEsms(esms_bs, dst) < 0)
      return -1;
  }

  assert(nb_source_files == dst->nb_source_files);
  return 0;
}

/* ### ESMS ES Format Properties section : ################################# */

static int _parseH264VideoESFmtPropertiesEsms(
  BitstreamReaderPtr esms_bs,
  LibbluESFmtProp * dst
)
{
  LibbluESH264SpecProperties * prop = dst->h264;

  LIBBLU_SCRIPTR_DEBUG(" H.264 Format specific informations:\n");

  /* [v8 constraint_flags] */
  READ_VALUE(esms_bs, 1, &prop->constraint_flags, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "  Constraints flags (constraint_flags): 0x%02" PRIX8 ".\n",
    prop->constraint_flags
  );

  /* [u32 CpbSize] */
  READ_VALUE(esms_bs, 4, &prop->CpbSize, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "  Coded Picture Buffer size (CpbSize): %" PRIu32 " (0x%08" PRIX32 ").\n",
    prop->CpbSize,
    prop->CpbSize
  );

  /* [u32 BitRate] */
  READ_VALUE(esms_bs, 4, &prop->BitRate, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "  Bit-Rate (BitRate): %" PRIu32 " bytes (0x%08" PRIX32 ").\n",
    prop->BitRate,
    prop->BitRate
  );

  return 0;
}

static int _parseVideoESFmtPropertiesEsms(
  BitstreamReaderPtr esms_bs,
  LibbluESProperties * dst,
  LibbluESFmtProp * fmt_prop_ret
)
{

  /* [v64 CSPMVIDO_magic] */
  if (checkDirectoryMagic(esms_bs, ES_CODEC_SPEC_PARAM_HEADER_VIDEO_MAGIC, 8) < 0)
    return -1;

  LIBBLU_SCRIPTR_DEBUG(
    " Video format specific properties magic (CSPMVIDO_magic): "
    STR(ES_CODEC_SPEC_PARAM_HEADER_VIDEO_MAGIC) ".\n"
  );

  /* [u4 video_format] [u4 frame_rate] */
  uint8_t byte;
  READ_VALUE(esms_bs, 1, &byte, return -1);
  dst->video_format = (byte >> 4);
  dst->frame_rate   = (byte & 0xF);

  LIBBLU_SCRIPTR_DEBUG(
    " Video format (video_format): %s (0x%X).\n",
    HdmvVideoFormatStr(dst->video_format),
    dst->video_format
  );
  LIBBLU_SCRIPTR_DEBUG(
    " Video format (frame_rate): %s (0x%X).\n",
    HdmvFrameRateCodeStr(dst->frame_rate),
    dst->frame_rate
  );

  /* [u8 profile_IDC] */
  READ_VALUE(esms_bs, 1, &dst->profile_IDC, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Profile IDC (profile_IDC): %" PRIu8 " (0x%02" PRIX8 ").\n",
    dst->profile_IDC,
    dst->profile_IDC
  );

  /* [u8 level_IDC] */
  READ_VALUE(esms_bs, 1, &dst->level_IDC, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Level IDC (level_IDC): %" PRIu8 " (0x%02" PRIX8 ").\n",
    dst->level_IDC,
    dst->level_IDC
  );

  /* [b1 still_picture] [v7 reserved] */
  READ_VALUE(esms_bs, 1, &byte, return -1);
  dst->still_picture = (byte & 0x80);

  LIBBLU_SCRIPTR_DEBUG(
    " Still picture (still_picture): %s (0b%x).\n",
    dst->still_picture,
    dst->still_picture
  );

  if (dst->coding_type == STREAM_CODING_TYPE_AVC) {
    if (initLibbluESFmtSpecProp(fmt_prop_ret, FMT_SPEC_INFOS_H264) < 0)
      return -1;

    if (_parseH264VideoESFmtPropertiesEsms(esms_bs, fmt_prop_ret) < 0)
      return -1;
  }

  return 0;
}

static int _parseAc3AudioESFmtPropertiesEsms(
  BitstreamReaderPtr esms_bs,
  LibbluESFmtProp * dst
)
{
  LibbluESAc3SpecProperties * prop = dst->ac3;

  LIBBLU_SCRIPTR_DEBUG(" AC-3 Format specific informations:\n");

  /* [u3 sample_rate_code] [u5 bsid] */
  uint8_t byte;
  READ_VALUE(esms_bs, 1, &byte, return -1);
  prop->sample_rate_code = (byte >> 5);
  prop->bsid             = (byte & 0x1F);

  LIBBLU_SCRIPTR_DEBUG(
    "  Audio sample rate code (sample_rate_code): 0x%X.\n",
    prop->sample_rate_code
  );
  LIBBLU_SCRIPTR_DEBUG(
    "  Bit Stream Identification (bsid): 0x%" PRIX8 ".\n",
    prop->bsid
  );

  /* [u6 bit_rate_code] [u2 surround_mode] */
  READ_VALUE(esms_bs, 1, &byte, return -1);
  prop->bit_rate_code = (byte >> 2);
  prop->surround_mode = (byte & 0x3);

  LIBBLU_SCRIPTR_DEBUG(
    "  Nominal data rate code (bit_rate_code): 0x%" PRIX8 ".\n",
    prop->bit_rate_code
  );
  LIBBLU_SCRIPTR_DEBUG(
    "  Surround code (surround_mode): 0x%" PRIX8 ".\n",
    prop->surround_mode
  );

  /* [u3 bsmod] [u4 num_channels] [b1 full_svc] */
  READ_VALUE(esms_bs, 1, &byte, return -1);
  prop->bsmod        = (byte >> 5);
  prop->num_channels = ((byte >> 1) & 0xF);
  prop->full_svc     = (byte & 0x1);

  LIBBLU_SCRIPTR_DEBUG(
    "  Bit stream mode code (bsmod): 0x%" PRIX8 ".\n",
    prop->bsmod
  );
  LIBBLU_SCRIPTR_DEBUG(
    "  Number of audio channels (num_channels): 0x%" PRIX8 ".\n",
    prop->num_channels
  );
  LIBBLU_SCRIPTR_DEBUG(
    "  Full service (full_svc): 0x%" PRIX8 ".\n",
    prop->full_svc
  );

  return 0;
}

static int _parseAudioESFmtPropertiesEsms(
  BitstreamReaderPtr esms_bs,
  LibbluESProperties * dst,
  LibbluESFmtProp * fmt_prop_ret
)
{

  /* [v64 CSPMAUDO_magic] */
  if (checkDirectoryMagic(esms_bs, ES_CODEC_SPEC_PARAM_HEADER_AUDIO_MAGIC, 8) < 0)
    return -1;

  LIBBLU_SCRIPTR_DEBUG(
    " Audio format specific properties magic (CSPMAUDO_magic): "
    STR(ES_CODEC_SPEC_PARAM_HEADER_AUDIO_MAGIC) ".\n"
  );

  /* [u4 audio_format] [u4 sample_rate] */
  uint8_t byte;
  READ_VALUE(esms_bs, 1, &byte, return -1);
  dst->audio_format = (byte >> 4);
  dst->sample_rate  = (byte & 0xF);

  LIBBLU_SCRIPTR_DEBUG(
    " Audio format (audio_format): %s (0x%X).\n",
    AudioFormatCodeStr(dst->audio_format),
    dst->audio_format
  );
  LIBBLU_SCRIPTR_DEBUG(
    " Sample-rate (sample_rate): %u Hz (0x%X).\n",
    valueSampleRateCode(dst->sample_rate),
    dst->sample_rate
  );

  /* [u8 bit_depth] */
  READ_VALUE(esms_bs, 1, &dst->bit_depth, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Bit-depth (bit_depth): %u bits (0x%X).\n",
    valueBitDepthCode(dst->bit_depth),
    dst->bit_depth
  );

  /* [v8 reserved] */
  SKIP_VALUE(esms_bs, 1);

  if (isAc3AudioStreamCodingType(dst->coding_type)) {
    if (initLibbluESFmtSpecProp(fmt_prop_ret, FMT_SPEC_INFOS_AC3) < 0)
      return -1;

    if (_parseAc3AudioESFmtPropertiesEsms(esms_bs, fmt_prop_ret) < 0)
      return -1;
  }

  return 0;
}

int parseESFmtPropertiesEsms(
  BitstreamReaderPtr esms_bs,
  LibbluESProperties * dst,
  LibbluESFmtProp * fmt_prop_ret
)
{
  LIBBLU_SCRIPTR_DEBUG(
    "Format specific Properties section:\n"
  );

  switch (dst->type) {
    case ES_VIDEO:
      return _parseVideoESFmtPropertiesEsms(esms_bs, dst, fmt_prop_ret);
    case ES_AUDIO:
      return _parseAudioESFmtPropertiesEsms(esms_bs, dst, fmt_prop_ret);
    default: // ES_HDMV
      return 0; // No ES Format Properties section defined.
  }
}

/* ### ESMS ES Data Blocks Definition section : ############################ */

static int parseEntryESDataBlocksDefinitionEsms(
  BitstreamReaderPtr esms_bs,
  EsmsDataBlocks * dst
)
{

  /* [u32 data_block_size] */
  uint32_t data_block_size;
  READ_VALUE(esms_bs, 4, &data_block_size, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "  Size (data_block_size): %" PRIu32 " (0x%08" PRIX32 ").\n",
    data_block_size,
    data_block_size
  );

  /* [v<data_block_size> data_block] */
  uint8_t * data_block = NULL;
  if (0 < data_block_size) {
    if (NULL == (data_block = (uint8_t *) malloc(data_block_size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    if (readBytes(esms_bs, data_block, data_block_size) < 0)
      goto free_return;
  }

  EsmsDataBlockEntry entry = (EsmsDataBlockEntry) {
    .data_block = data_block,
    .data_block_size = data_block_size
  };

  if (appendEsmsDataBlocks(dst, entry, NULL) < 0)
    goto free_return;
  return 0;

free_return:
  free(data_block);
  return -1;
}

int parseESDataBlocksDefinitionEsms(
  BitstreamReaderPtr esms_bs,
  EsmsDataBlocks * dst
)
{
  LIBBLU_SCRIPTR_DEBUG(
    "Data Blocks Definition section:\n"
  );

  /* [v32 DTBK_magic] */
  if (checkDirectoryMagic(esms_bs, DATA_BLOCKS_DEF_HEADER_MAGIC, 4) < 0)
    return -1;

  LIBBLU_SCRIPTR_DEBUG(
    " Data Blocks Definition section magic (DTBK_magic): "
    STR(DATA_BLOCKS_DEF_HEADER_MAGIC) ".\n"
  );

  /* [u8 nb_data_blocks] */
  uint8_t nb_data_blocks;
  READ_VALUE(esms_bs, 1, &nb_data_blocks, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    " Number of data blocks (nb_data_blocks): %" PRIu8 " (0x%02" PRIX8 ").\n",
    nb_data_blocks,
    nb_data_blocks
  );

  for (unsigned i = 0; i < nb_data_blocks; i++) {
    LIBBLU_SCRIPTR_DEBUG(" Data block definition %u:\n", i);
    if (parseEntryESDataBlocksDefinitionEsms(esms_bs, dst) < 0)
      return -1;
  }

  return 0;
}

/* ### ESMS PES Cutting section : ########################################## */
/* ###### PES packet properties : ########################################## */

#define CHECK_DATA_SIZE(n, rms, exp)                                          \
  do {                                                                        \
    if ((rms) < (exp))                                                        \
      LIBBLU_ERROR_RETURN(                                                    \
        "Broken script, unexpected ESMS " n " size "                          \
        "(%u bytes expected, got %u).\n"                                      \
      );                                                                      \
    (rms) -= (exp);                                                           \
  } while (0)

#define SKIP_REMAINING_DATA(scrt, s)                                          \
  skipBytes(scrt, s)

static int parseEsmsPesPacketH264ExtData(
  EsmsPesPacketH264ExtData * dst,
  BitstreamReaderPtr esms_bs,
  uint16_t ext_data_size
)
{
  /* H.264 AVC Extension data */

  LIBBLU_SCRIPTR_DEBUG(
    "    Type: H.264 Extension data (inferred).\n"
  );
  CHECK_DATA_SIZE("H.264 Extension Data", ext_data_size, 1);

  /* [b1 large_time_fields] [v7 reserved] */
  uint8_t byte;
  READ_VALUE(esms_bs, 1, &byte, return -1);
  bool large_tf = (byte & 0x80);

  LIBBLU_SCRIPTR_DEBUG(
    "    Large time fields (large_time_fields): %s (0x%02X).\n",
    large_tf,
    large_tf << 7
  );

  CHECK_DATA_SIZE("H.264 Extension Data", ext_data_size, 8 << large_tf);

  /* [u<32 << large_time_fields> cpb_removal_time] */
  READ_VALUE(esms_bs, 4 << large_tf, &dst->cpb_removal_time, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "    Coded Picture Buffer removal time (cpb_removal_time): "
    "%" PRIu64 " (0x%0*" PRIX64 ").\n",
    dst->cpb_removal_time,
    8 << large_tf,
    dst->cpb_removal_time
  );

  /* [u<32 << large_time_fields> dpb_output_time] */
  READ_VALUE(esms_bs, 4 << large_tf, &dst->dpb_output_time, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "    Decoded Picture Buffer output time (dpb_output_time): "
    "%" PRIu64 " (0x%0*" PRIX64 ").\n",
    dst->dpb_output_time,
    8 << large_tf,
    dst->dpb_output_time
  );

  return SKIP_REMAINING_DATA(esms_bs, ext_data_size);
}

static int _parseEsmsPesPacketExtData(
  EsmsPesPacketExtData * dst,
  BitstreamReaderPtr esms_bs,
  LibbluStreamCodingType coding_type
)
{

  /* [u16 ext_data_size] */
  uint16_t ext_data_size;
  READ_VALUE(esms_bs, 2, &ext_data_size, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "    Extension data size (ext_data_size): %" PRIu16 " (0x%04" PRIX16 ").\n",
    ext_data_size,
    ext_data_size
  );

  switch (coding_type) {
    case STREAM_CODING_TYPE_AVC:
      return parseEsmsPesPacketH264ExtData(&dst->h264, esms_bs, ext_data_size);
    default:
      break; /* Unsupported Extension data */
  }

  LIBBLU_SCRIPTR_DEBUG(
    "    Extension data size (ext_data_size): %" PRIu16 " (unknown).\n",
    ext_data_size
  );

  /* [vn reserved] */
  return SKIP_REMAINING_DATA(esms_bs, ext_data_size);
}

#undef CHECK_DATA_SIZE
#undef SKIP_REMAINING_DATA

static int _parseEsmsPesPacketProperties(
  EsmsPesPacket * dst,
  BitstreamReaderPtr esms_bs,
  LibbluESType type,
  LibbluStreamCodingType coding_type
)
{
  EsmsPesPacketProp * dst_prop = &dst->prop;
  dst_prop->type = type;

  /** [v16 frame_prop_word]
   * -> v8  : *depend on ESMS type*
   * -> v8  : frame_prop_flags
   */
  uint16_t frame_prop_word;
  READ_VALUE(esms_bs, 2, &frame_prop_word, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "   Properties word (frame_prop_word): 0x%04" PRIX16 ".\n",
    frame_prop_word
  );

  if (ES_VIDEO == type) {
    assert(isVideoStreamCodingType(coding_type));
    EsmsPesPacketPropVideo * vprop = &dst_prop->prefix.video;
    /* [u2 picture_type] [v5 reserved] [v1 marker_bit] */
    vprop->picture_type = (frame_prop_word >> 14);

    LIBBLU_SCRIPTR_DEBUG(
      "   Picture type (picture_type): 0x%X.\n",
      vprop->picture_type
    );
  }
  else if (ES_AUDIO == type) {
    assert(isAudioStreamCodingType(coding_type));
    EsmsPesPacketPropAudio * aprop = &dst_prop->prefix.audio;
    /* [b1 extension_frame] [v7 reserved] */
    aprop->extension_frame = (frame_prop_word & 0x8000);

    LIBBLU_SCRIPTR_DEBUG(
      "   Audio extension frame (extension_frame): %s (0x%X).\n",
      BOOL_STR(aprop->extension_frame),
      aprop->extension_frame
    );
  }
  else {
    /* [v8 reserved] / ignored */
  }

  /** [v8 frame_prop_flags]
   * -> b1  : pts_long_field
   * -> b1  : dts_present
   * -> b1  : dts_long_field
   * -> b1  : size_long_field
   * -> b1  : ext_data_present
   * -> v3  : reserved
   */
  dst_prop->pts_long_field   = (frame_prop_word & 0x80);
  dst_prop->dts_present      = (frame_prop_word & 0x40);
  dst_prop->dts_long_field   = (frame_prop_word & 0x20);
  dst_prop->size_long_field  = (frame_prop_word & 0x10);
  dst_prop->ext_data_present = (frame_prop_word & 0x08);

  LIBBLU_SCRIPTR_DEBUG(
    "   Flags properties flags (frame_prop_flags): 0x%02" PRIX16 ".\n",
    frame_prop_word & 0xFF
  );
#define _P(s, v)  LIBBLU_SCRIPTR_DEBUG("    " s ": %s;\n", BOOL_STR(v))
  _P("Long PTS field", dst_prop->pts_long_field);
  _P("DTS field presence", dst_prop->dts_present);
  _P("Long DTS field", dst_prop->dts_long_field);
  _P("Long size field", dst_prop->size_long_field);
  _P("Extension data presence", dst_prop->ext_data_present);
#undef _P

  /* [u<32 << pts_long_field> pts] */
  READ_VALUE(esms_bs, 4 << dst_prop->pts_long_field, &dst->pts, return -1);
  dst->dts = dst->pts;

  LIBBLU_SCRIPTR_DEBUG(
    "   PTS (pts): %" PRIu64 " (0x%0*" PRIX64 ").\n",
    dst->pts,
    8 << dst_prop->pts_long_field,
    dst->pts
  );

  if (dst_prop->dts_present) {
    /* [u<32 << dts_long_field> dts] */
    READ_VALUE(esms_bs, 4 << dst_prop->dts_long_field, &dst->dts, return -1);

    LIBBLU_SCRIPTR_DEBUG(
      "   DTS (dts): %" PRIu64 " (0x%0*" PRIX64 ").\n",
      dst->dts,
      8 << dst_prop->dts_long_field,
      dst->dts
    );
  }

  if (dst_prop->ext_data_present) {
    LIBBLU_SCRIPTR_DEBUG("   Extension data:\n");
    if (_parseEsmsPesPacketExtData(&dst->ext_data, esms_bs, coding_type) < 0)
      return -1;
  }

  /* [u<16 << size_long_field> size] */
  READ_VALUE(esms_bs, 2 << dst_prop->size_long_field, &dst->size, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "   PES packet size (size): %" PRIu32 " (0x%0*" PRIX32 ").\n",
    dst->size,
    8 << dst_prop->size_long_field,
    dst->size
  );

  return 0;
}

/* ###### Script commands utilities : ###################################### */

static int _checkAllocAndSetCommandSize(
  EsmsCommandParsingData * dst,
  uint16_t command_size
)
{
  if (command_size <= dst->command_data_alloc_size) {
    dst->command_size = command_size;
    return 0; // Enough room
  }

  uint8_t * new_data = realloc(dst->command_data, command_size);
  if (NULL == new_data)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  *dst = (EsmsCommandParsingData) {
    .command_data = new_data,
    .command_data_alloc_size = command_size,
    .command_size = command_size
  };

  return 0;
}

static int _readRawCommand(
  EsmsCommandParsingData * dst,
  BitstreamReaderPtr esms_bs
)
{

  /* [u16 command_size] */
  uint16_t command_size;
  READ_VALUE(esms_bs, 2, &command_size, return -1);

  LIBBLU_SCRIPTR_DEBUG(
    "     Command size (command_size): %" PRIu16 " (0x%04" PRIX16 ").\n",
    command_size,
    command_size
  );

  if (_checkAllocAndSetCommandSize(dst, command_size) < 0)
    return -1;

  /* [vn command()] */
  READ_BYTES(esms_bs, command_size, dst->command_data, return -1);

  return 0;
}

/* ###### Script commands data : ########################################### */

#define READ_ARRAY_BYTES(a, o, s, d)                                          \
  do {                                                                        \
    size_t _s = (s);                                                          \
    memcpy(d, &(a)[o], _s);                                                   \
    o += (_s);                                                                \
  } while (0)

#define READ_ARRAY_VALUE(a, o, s, d)                                          \
  do {                                                                        \
    size_t _s = (s);                                                          \
    uint64_t _val = 0;                                                        \
    for (size_t _i = 0; _i < _s; _i++)                                        \
      _val = (_val << 8) | a[o++];                                            \
    *(d) = _val;                                                              \
  } while (0)

static int _parseAddDataCommand(
  EsmsCommand * dst_command,
  const EsmsCommandParsingData * data
)
{
  size_t parsing_off = 0;

  if (data->command_size < ADD_DATA_COM_LEN)
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add data\" command too short size %u.\n",
      data->command_size
    );

  /* [u32 insert_offset] */
  uint32_t insert_offset;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &insert_offset);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion offset (insert_offset): %" PRIu32 " (0x%08" PRIX32 ").\n",
    insert_offset,
    insert_offset
  );

  /* [u8 insert_mode] */
  uint8_t insert_mode;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &insert_mode);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion mode (insert_mode): %s (0x%02" PRIX8 ").\n",
    EsmsDataInsertionModeStr(insert_mode),
    insert_mode
  );

  if (!isValidEsmsDataInsertionMode(insert_mode))
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add data\" command unknown insertion mode %u.\n",
      insert_mode
    );

  /* [vn data] */
  uint16_t data_size = data->command_size - ADD_DATA_COM_LEN;

  LIBBLU_SCRIPTR_DEBUG(
    "     Inserted data (data): %" PRIu16 " byte(s).\n",
    data_size
  );

  uint8_t * data_bytes = NULL;
  if (0 < data_size) {
    if (NULL == (data_bytes = (uint8_t *) malloc(data_size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    READ_ARRAY_BYTES(data->command_data, parsing_off, data_size, data_bytes);
  }

  *dst_command = (EsmsCommand) {
    .type = ESMS_ADD_DATA,
    .data.add_data = {
      .insert_offset = insert_offset,
      .insert_mode = insert_mode,
      .data = data_bytes,
      .data_size = data_size
    }
  };

  return 0;
}

static int _parseChangeByteOrderCommand(
  EsmsCommand * dst_command,
  const EsmsCommandParsingData * data
)
{
  size_t parsing_off = 0;

  if (data->command_size < CHANGE_BYTEORDER_COM_LEN)
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Change byte order\" command too short size %u.\n",
      data->command_size
    );

  /* [u8 unit_size] */
  uint8_t unit_size;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &unit_size);

  LIBBLU_SCRIPTR_DEBUG(
    "     Unit size (unit_size): %" PRIu8 " bytes (0x%02" PRIX8 ").\n",
    unit_size,
    unit_size
  );

  /* [u32 section_offset] */
  uint32_t section_offset;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &section_offset);

  LIBBLU_SCRIPTR_DEBUG(
    "     Section offset (section_offset): %" PRIu32 " (0x%08" PRIX32 ").\n",
    section_offset,
    section_offset
  );

  /* [u32 section_size] */
  uint32_t section_size;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &section_size);

  LIBBLU_SCRIPTR_DEBUG(
    "     Section offset (section_size): %" PRIu32 " (0x%08" PRIX32 ").\n",
    section_size,
    section_size
  );

  *dst_command = (EsmsCommand) {
    .type = ESMS_CHANGE_BYTEORDER,
    .data.change_byte_order = {
      .unit_size = unit_size,
      .section_offset = section_offset,
      .section_size = section_size
    }
  };

  return 0;
}

static int _parseAddPesPayloadCommandode(
  EsmsCommand * dst_command,
  const EsmsCommandParsingData * data
)
{
  size_t parsing_off = 0;

  if (data->command_size < 1)
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add PES payload\" command too short size %u.\n",
      data->command_size
    );

  /* [b1 src_offset_ext_present] */
  /* [b1 size_ext_present] */
  /* [v6 reserved] */
  uint8_t flags;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &flags);
  bool src_offset_ext_present = (flags & 0x80);
  bool size_ext_present       = (flags & 0x40);

  uint16_t expected_size = sizeEsmsAddPesPayloadCommand(
    src_offset_ext_present,
    size_ext_present
  );
  if (data->command_size < expected_size)
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add PES payload\" command too short size %u.\n",
      data->command_size
    );

  LIBBLU_SCRIPTR_DEBUG(
    "     Source offset extension present (src_offset_ext_present): "
    "%s (0x%02X).\n",
    BOOL_STR(src_offset_ext_present),
    src_offset_ext_present << 7
  );
  LIBBLU_SCRIPTR_DEBUG(
    "     Size extension present (size_ext_present): "
    "%s (0x%02X).\n",
    BOOL_STR(size_ext_present),
    size_ext_present << 6
  );

  /* [u8 src_file_id] */
  uint8_t src_file_id;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &src_file_id);

  LIBBLU_SCRIPTR_DEBUG(
    "     Source file id (src_file_id): 0x%02" PRIX8 ".\n",
    src_file_id
  );

  /* [u32 insert_offset] */
  uint32_t insert_offset;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &insert_offset);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion offset (insert_offset): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    insert_offset,
    insert_offset
  );

  /* [u32 src_offset] */
  uint64_t src_offset;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &src_offset);

  if (src_offset_ext_present) {
    /* [u32 src_offset_ext] */
    uint64_t src_offset_ext;
    READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &src_offset_ext);
    src_offset |= src_offset_ext << 32;
  }

  LIBBLU_SCRIPTR_DEBUG(
    "     Source offset (src_offset/src_offset_ext): "
    "%" PRId64 " (0x%0*" PRIX64 ").\n",
    src_offset,
    8 << src_offset_ext_present,
    src_offset
  );

  if (INT64_MAX < src_offset)
    LIBBLU_ERROR_RETURN("Out of range script source offset.\n");

  /* [u16 size] */
  uint32_t size;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 2, &size);

  if (size_ext_present) {
    /* [u16 size_ext] */
    uint32_t size_ext;
    READ_ARRAY_VALUE(data->command_data, parsing_off, 2, &size_ext);
    size |= size_ext << 16;
  }

  LIBBLU_SCRIPTR_DEBUG(
    "     Size (size/size_ext): %" PRIu32 " (0x%0*" PRIX32 ").\n",
    size,
    4 << size_ext_present,
    size
  );

  *dst_command = (EsmsCommand) {
    .type = ESMS_ADD_PAYLOAD_DATA,
    .data.add_pes_payload = {
      .src_file_id = src_file_id,
      .insert_offset = insert_offset,
      .src_offset = src_offset,
      .size = size,
    }
  };

  return 0;
}

static int _parseAddPaddingCommand(
  EsmsCommand * dst_command,
  const EsmsCommandParsingData * data
)
{
  size_t parsing_off = 0;

  if (data->command_size < ADD_PADDING_COM_LEN)
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add padding\" command too short size %u.\n",
      data->command_size
    );

  /* [u32 insert_offset] */
  uint32_t insert_offset;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &insert_offset);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion offset (insert_offset): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    insert_offset,
    insert_offset
  );

  /* [u8 insert_mode] */
  uint8_t insert_mode;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &insert_mode);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion mode (insert_mode): %s (0x%02" PRIX8 ").\n",
    EsmsDataInsertionModeStr(insert_mode),
    insert_mode
  );

  if (!isValidEsmsDataInsertionMode(insert_mode))
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add Padding Data\" command unknown "
      "insertion mode %u.\n",
      insert_mode
    );

  /* [u32 size] */
  uint32_t size;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &size);

  LIBBLU_SCRIPTR_DEBUG(
    "     Size (size): %" PRIu32 " (0x%08" PRIX32 ").\n",
    size,
    size
  );

  /* [u8 filling_byte] */
  uint8_t filling_byte;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &filling_byte);

  LIBBLU_SCRIPTR_DEBUG(
    "     Filling byte (filling_byte): 0x%02" PRIX8 ".\n",
    filling_byte
  );

  *dst_command = (EsmsCommand) {
    .type = ESMS_ADD_PADDING_DATA,
    .data.add_padding = {
      .insert_offset = insert_offset,
      .insert_mode = insert_mode,
      .size = size,
      .filling_byte = filling_byte
    }
  };

  return 0;
}

static int _parseAddDataBlockCommand(
  EsmsCommand * dst_command,
  const EsmsCommandParsingData * data
)
{
  size_t parsing_off = 0;

  if (data->command_size < ADD_DATA_BLOCK_COM_LEN)
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add data block\" command too short size %u.\n",
      data->command_size
    );

  /* [u32 insert_offset] */
  uint32_t insert_offset;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 4, &insert_offset);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion offset (insert_offset): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    insert_offset,
    insert_offset
  );

  /* [u8 insert_mode] */
  uint8_t insert_mode;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &insert_mode);

  LIBBLU_SCRIPTR_DEBUG(
    "     Insertion mode (insert_mode): %s (0x%02" PRIX8 ").\n",
    EsmsDataInsertionModeStr(insert_mode),
    insert_mode
  );

  if (!isValidEsmsDataInsertionMode(insert_mode))
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add Data Block\" command unknown "
      "insertion mode %u.\n",
      insert_mode
    );

  /* [u8 data_block_id] */
  uint8_t data_block_id;
  READ_ARRAY_VALUE(data->command_data, parsing_off, 1, &data_block_id);

  LIBBLU_SCRIPTR_DEBUG(
    "     Data block ID (data_block_id): 0x%02" PRIX8 ".\n",
    data_block_id
  );

  *dst_command = (EsmsCommand) {
    .type = ESMS_ADD_DATA_BLOCK,
    .data.add_data_block = {
      .insert_offset = insert_offset,
      .insert_mode = insert_mode,
      .data_block_id = data_block_id
    }
  };

  return 0;
}

#undef RB_COM

/* ###### Script commands parsing : ######################################## */

static int _parseCommand(
  EsmsCommand * dst_command,
  uint8_t command_type,
  const EsmsCommandParsingData * command_data
)
{
  switch (command_type) {
    case ESMS_ADD_DATA:
      return _parseAddDataCommand(dst_command, command_data);
    case ESMS_CHANGE_BYTEORDER:
      return _parseChangeByteOrderCommand(dst_command, command_data);
    case ESMS_ADD_PAYLOAD_DATA:
      return _parseAddPesPayloadCommandode(dst_command, command_data);
    case ESMS_ADD_PADDING_DATA:
      return _parseAddPaddingCommand(dst_command, command_data);
    case ESMS_ADD_DATA_BLOCK:
      return _parseAddDataBlockCommand(dst_command, command_data);
    default:
      break;
  }

  LIBBLU_ERROR_RETURN(
    "Unknown command type value 0x%02X.\n",
    command_type
  );
}

static int _parseEsmsPesPacketCommands(
  EsmsPesPacket * dst,
  BitstreamReaderPtr esms_bs,
  EsmsCommandParsingData * command_data
)
{
  assert(NULL != dst);

  int64_t offset = tellPos(esms_bs);

  /* [u8 nb_commands] */
  uint8_t nb_commands;
  READ_VALUE(esms_bs, 1, &nb_commands, goto free_return);

  LIBBLU_SCRIPTR_DEBUG(
    "   Number of commands (nb_commands): %u (0x%02" PRIX8 ").\n",
    nb_commands,
    nb_commands
  );

  if (ESMS_MAX_SUPPORTED_COMMANDS_NB < nb_commands)
    LIBBLU_ERROR_FRETURN(
      "Number of ESMS commands exceed maximum (%u < %u).\n",
      ESMS_MAX_SUPPORTED_COMMANDS_NB,
      nb_commands
    );

  /* Read modification and save script commands list : */
  dst->nb_commands = 0;
  for (unsigned i = 0; i < nb_commands; i++) {
    int64_t com_offset = tellPos(esms_bs);
    LIBBLU_SCRIPTR_DEBUG("    Command %u (0x%016" PRIX64 "):\n", i, com_offset);

    /* [u8 command_type] */
    uint8_t command_type;
    READ_VALUE(esms_bs, 1, &command_type, goto free_return);

    LIBBLU_SCRIPTR_DEBUG(
      "     Command type: %s (0x%" PRIX8 ").\n",
      EsmsCommandTypeStr(command_type),
      command_type
    );

    /* [u16 command_size] [vn command()] */
    if (_readRawCommand(command_data, esms_bs) < 0)
      LIBBLU_ERROR_FRETURN(
        "Error while reading ESMS command data at 0x%016" PRIX64 ".\n",
        com_offset
      );

    if (_parseCommand(&dst->commands[i], command_type, command_data) < 0)
      LIBBLU_ERROR_FRETURN(
        "Error while parsing ESMS command at 0x%016" PRIX64 ".\n",
        com_offset
      );

    dst->nb_commands++;
  }

  return 0;

free_return:
  LIBBLU_ERROR_RETURN(
    "Error while parsing ESMS PES packet commands at 0x%016" PRIX64 ".\n",
    offset
  );
}

/* ######################################################################### */

int parseFrameNodeESPesCuttingEsms(
  EsmsPesPacket * dst,
  BitstreamReaderPtr esms_bs,
  LibbluESType type,
  LibbluStreamCodingType coding_type,
  EsmsCommandParsingData * command_data
)
{
  int64_t offset = tellPos(esms_bs);

  LIBBLU_SCRIPTR_DEBUG("  Script frame (0x%016" PRIX64 "):\n", offset);

  if (_parseEsmsPesPacketProperties(dst, esms_bs, type, coding_type) < 0)
    goto free_return;

  if (_parseEsmsPesPacketCommands(dst, esms_bs, command_data) < 0)
    goto free_return;

  return 0;

free_return:
  LIBBLU_ERROR_RETURN(
    "Error while parsing ESMS PES packet at 0x%016" PRIX64 ".\n",
    offset
  );
}