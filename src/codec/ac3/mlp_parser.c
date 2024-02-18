#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "mlp_parser.h"


int parseMlpMinorSyncHeader(
  BitstreamReaderPtr bs,
  MlpSyncHeaderParameters * sh,
  uint8_t * parity
)
{
  uint8_t mins_header[4];

  /* Read minor sync header */
  if (readBytes(bs, mins_header, 4) < 0)
    return -1;

  /* [v4 check_nibble] */
  sh->check_nibble = (mins_header[0] >> 4);

  /* [u12 access_unit_length] */
  sh->access_unit_length = ((mins_header[0] & 0xF) << 8) | mins_header[1];

  /* [u16 input_timing] */
  sh->input_timing = (mins_header[2] << 8) | mins_header[3];

  *parity = lb_xor_32_to_8(*((uint32_t *) mins_header));
  return 0;
}


#define MLP_READ_BITS(d, br, s, e)                                            \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readLibbluBitReader(br, &_val, s) < 0)                                \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)


void _unpackMlpMajorSyncFormatInfo(
  MlpMajorSyncFormatInfo * dst,
  uint32_t value
)
{
  /** [v32 format_info]
   * -> u4:  audio_sampling_frequency
   * -> v1:  6ch_multichannel_type
   * -> v1:  8ch_multichannel_type
   * -> v2:  reserved
   * -> u2:  2ch_presentation_channel_modifier
   * -> u2:  6ch_presentation_channel_modifier
   * -> u5:  6ch_presentation_channel_assignment
   * -> u2:  8ch_presentation_channel_modifier
   * -> u13: 8ch_presentation_channel_assignment
   */
  *dst = (MlpMajorSyncFormatInfo) {
    .value = value,

    .audio_sampling_frequency = (value >> 28) & 0xF,
    .u6ch_multichannel_type   = (value >> 27) & 0x1,
    .u8ch_multichannel_type   = (value >> 26) & 0x1,

    .u2ch_presentation_channel_modifier   = (value >> 22) & 0x3,
    .u6ch_presentation_channel_modifier   = (value >> 20) & 0x3,
    .u6ch_presentation_channel_assignment = (value >> 15) & 0x1F,
    .u8ch_presentation_channel_modifier   = (value >> 13) & 0x3,
    .u8ch_presentation_channel_assignment = value & 0x1FFF,
  };
}

static void _unpackMlpMajorSyncextSubstreamInfo(
  uint8_t value,
  MlpSubstreamInfo * si
)
{
  /** [v8 substream_info]
   * -> v2: reserved
   * -> u2: 6ch_presentation
   * -> u3: 8ch_presentation
   * -> b1: 16ch_presentation_present
   */
  *si = (MlpSubstreamInfo) {
    .value = value,

    .reserved_field             = (value     ) & 0x3,
    .u6ch_presentation          = (value >> 2) & 0x3,
    .u8ch_presentation          = (value >> 4) & 0x7,
    .b16ch_presentation_present = (value >> 7) & 0x1
  };
}

/** \~english
 * \brief
 *
 * \param br
 * \param ecm
 * \param substream_info
 * \param length Size of extra_channel_meaning_data() section in bits.
 * \return int
 */
static int _parseMlpExtraChannelMeaningData(
  LibbluBitReader * br,
  MlpExtraChannelMeaningData * ecmd,
  MlpSubstreamInfo si,
  unsigned length
)
{
  unsigned ecm_used_length = 0;
  ecmd->type = MLP_EXTRA_CH_MEANING_CONTENT_UNKNOWN;

  if (si.b16ch_presentation_present) {
    /* 16ch_channel_meaning() */
    Mlp16ChChannelMeaning * v16ch_cm = &ecmd->content.v16ch_channel_meaning;

    ecmd->type = MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING;

    /* [u5 16ch_dialogue_norm] */
    MLP_READ_BITS(&v16ch_cm->u16ch_dialogue_norm, br, 5, return -1);

    /* [u6 16ch_mix_level] */
    MLP_READ_BITS(&v16ch_cm->u16ch_mix_level, br, 6, return -1);

    /* [u5 16ch_channel_count] */
    MLP_READ_BITS(&v16ch_cm->u16ch_channel_count, br, 5, return -1);

    /* [b1 dyn_object_only] */
    MLP_READ_BITS(&v16ch_cm->dyn_object_only, br, 1, return -1);

    if (v16ch_cm->dyn_object_only) {
      /* [b1 lfe_present] */
      MLP_READ_BITS(&v16ch_cm->lfe_present, br, 1, return -1);
      ecm_used_length = 18;
    }
    else {
      // TODO: Implement (!dyn_object_only)
      LIBBLU_MLP_ERROR_RETURN(
        "16ch_channel_meaning() with multiple type content not supported yet, "
        "please send sample.\n"
      );
    }
  }

  /* Check extension size. */
  if (length < ecm_used_length)
    LIBBLU_MLP_ERROR_RETURN(
      "Too many bits in extra_channel_meaning_data "
      "(expect %u bits, got %u).\n",
      length, ecm_used_length
    );

  /* [vn reserved] */
  ecmd->reserved_size = length - ecm_used_length;

  for (unsigned i = 0, size = ecmd->reserved_size; i < size; i += 8) {
    MLP_READ_BITS(&ecmd->reserved[i >> 3], br, MIN(8, size - i), return -1);
  }

  return 0;
}

static int _parseMlpChannelMeaning(
  LibbluBitReader * br,
  MlpChannelMeaning * param,
  MlpSubstreamInfo si
)
{

  /* [v6 reserved] */
  MLP_READ_BITS(&param->reserved_field, br, 6, return -1);

  /* [b1 2ch_control_enabled] */
  MLP_READ_BITS(&param->b2ch_control_enabled, br, 1, return -1);

  /* [b1 6ch_control_enabled] */
  MLP_READ_BITS(&param->b6ch_control_enabled, br, 1, return -1);

  /* [b1 8ch_control_enabled] */
  MLP_READ_BITS(&param->b8ch_control_enabled, br, 1, return -1);

  /* [v1 reserved] */
  MLP_READ_BITS(&param->reserved_bool_1, br, 1, return -1);

  /* [s7 drc_start_up_gain] */
  MLP_READ_BITS(&param->drc_start_up_gain, br, 7, return -1);

  /* [u6 2ch_dialogue_norm] */
  MLP_READ_BITS(&param->u2ch_dialogue_norm, br, 6, return -1);

  /* [u6 2ch_mix_level] */
  MLP_READ_BITS(&param->u2ch_mix_level, br, 6, return -1);

  /* [u5 6ch_dialogue_norm] */
  MLP_READ_BITS(&param->u6ch_dialogue_norm, br, 5, return -1);

  /* [u6 6ch_mix_level] */
  MLP_READ_BITS(&param->u6ch_mix_level, br, 6, return -1);

  /* [u5 6ch_source_format] */
  MLP_READ_BITS(&param->u6ch_source_format, br, 5, return -1);

  /* [u5 8ch_dialogue_norm] */
  MLP_READ_BITS(&param->u8ch_dialogue_norm, br, 5, return -1);

  /* [u6 8ch_mix_level] */
  MLP_READ_BITS(&param->u8ch_mix_level, br, 6, return -1);

  /* [u6 8ch_source_format] */
  MLP_READ_BITS(&param->u8ch_source_format, br, 6, return -1);

  /* [v1 reserved] */
  MLP_READ_BITS(&param->reserved_bool_2, br, 1, return -1);

  /* [b1 extra_channel_meaning_present] */
  MLP_READ_BITS(&param->extra_channel_meaning_present, br, 1, return -1);
  param->extra_channel_meaning_length = 0;

  if (param->extra_channel_meaning_present) {
    MlpExtraChannelMeaningData * ecmd = &param->extra_channel_meaning_data;

    /* [u4 extra_channel_meaning_length] */
    MLP_READ_BITS(&param->extra_channel_meaning_length, br, 4, return -1);

    /* [vn extra_channel_meaning_data] */
    unsigned length = ((param->extra_channel_meaning_length + 1) << 4) - 4;
    if (_parseMlpExtraChannelMeaningData(br, ecmd, si, length) < 0)
      return -1;

    /* [vn padding] */
    if (padWordLibbluBitReader(br) < 0)
      return -1;
  }

  return 0;
}

static void _unpackMlpMajorSyncFlags(
  MlpMajorSyncFlags * dst,
  uint16_t value
)
{
  /** [v16 flags]
   * -> b1:  constantFifoBufDelay;
   * -> v3:  reserved;
   * -> b1:  formatInfoAlternative8chAssSyntax;
   * -> v11: reserved.
   *
   * NOTE: reserved fields are ignored.
   */
  *dst = (MlpMajorSyncFlags) {
    .value = value,

    .constant_fifo_buf_delay          = (value >> 15) & 0x1,
    .fmt_info_alter_8ch_asgmt_syntax  = (value >> 11) & 0x1
  };
}

int parseMlpMajorSyncInfo(
  LibbluBitReader * br,
  MlpMajorSyncInfoParameters * msi
)
{
  CrcContext crc_ctx;
  unsigned ms_data_size;

  assert(0 == offsetLibbluBitReader(br));

  /* [v32 format_sync] */
  MLP_READ_BITS(&msi->format_sync, br, 32, return -1);

  if (MLP_SYNCWORD == msi->format_sync) {
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected DVD Audio MLP format sync word "
      "(format_sync == 0x%08X).\n",
      MLP_SYNCWORD
    );
  }
  else if (TRUE_HD_SYNCWORD != msi->format_sync) {
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected sync word in MLP Major Sync "
      "(format_sync == 0x%08" PRIX32 ").\n",
      msi->format_sync
    );
  }

  /* [v32 format_info] */
  uint32_t format_info_value;
  MLP_READ_BITS(&format_info_value, br, 32, return -1);
  /* Unpacking is delayed to flags field parsing */

  /* [v16 signature] */
  MLP_READ_BITS(&msi->signature, br, 16, return -1);

  if (TRUE_HD_SIGNATURE != msi->signature) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid MLP Major Sync signature word "
      "(signature == 0x%04" PRIX32 ", expect 0x%04X).\n",
      msi->signature,
      TRUE_HD_SIGNATURE
    );
  }

  /* [v16 flags] */
  uint16_t flags_value;
  MLP_READ_BITS(&flags_value, br, 16, return -1);
  _unpackMlpMajorSyncFlags(&msi->flags, flags_value);

  /* Unpack format_info field using flags */
  _unpackMlpMajorSyncFormatInfo(&msi->format_info, format_info_value);

  /* [v16 reserved] // ignored */
  MLP_READ_BITS(&msi->reserved_field_1, br, 16, return -1);

  /* [b1 variable_rate] */
  MLP_READ_BITS(&msi->variable_bitrate, br, 1, return -1);

  /* [u15 peak_data_rate] */
  MLP_READ_BITS(&msi->peak_data_rate, br, 15, return -1);

  /* [u4 substreams] */
  MLP_READ_BITS(&msi->substreams, br, 4, return -1);

  /* [v2 reserved] // ignored */
  MLP_READ_BITS(&msi->reserved_field_2, br, 2, return -1);

  /* [u2 extended_substream_info] */
  MLP_READ_BITS(&msi->extended_substream_info, br, 2, return -1);

  /* [v8 substream_info] */
  uint8_t substream_info_value;
  MLP_READ_BITS(&substream_info_value, br, 8, return -1);
  _unpackMlpMajorSyncextSubstreamInfo(substream_info_value, &msi->substream_info);

  /* [v64 channel_meaning()] */
  if (_parseMlpChannelMeaning(br, &msi->channel_meaning, msi->substream_info) < 0)
    return -1;

  /* Compute CRC */
  ms_data_size = offsetLibbluBitReader(br) >> 3;
  initCrcContext(&crc_ctx, MLP_MS_CRC_PARAMS, 0x0);
  applyTableCrcContext(&crc_ctx, br->buf, ms_data_size);

  /* [u16 major_sync_info_CRC] */
  MLP_READ_BITS(&msi->major_sync_info_CRC, br, 16, return -1);

  if (completeCrcContext(&crc_ctx) != msi->major_sync_info_CRC)
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected Major Sync CRC value result.\n"
    );

  return 0;
}

#if 0
static bool _constantMlpMajorSyncChannelMeaningCheck(
  const MlpChannelMeaning * first,
  const MlpChannelMeaning * second
)
{
  return CHECK(
    EQUAL(->b2ch_control_enabled)
    EQUAL(->b6ch_control_enabled)
    EQUAL(->b8ch_control_enabled)
    EQUAL(->drc_start_up_gain)
    EQUAL(->u2ch_dialogue_norm)
    EQUAL(->u2ch_mix_level)
    EQUAL(->u6ch_dialogue_norm)
    EQUAL(->u6ch_mix_level)
    EQUAL(->u6ch_source_format)
    EQUAL(->u8ch_dialogue_norm)
    EQUAL(->u8ch_mix_level)
    EQUAL(->u8ch_source_format)
    EQUAL(->extra_channel_meaning_present)
    START_COND(->extra_channel_meaning_present, 0x1)
      SUB_FUN_PTR(->extra_channel_meaning, constantMlpExtraChannelMeaningCheck)
    END_COND
  );
}

static bool _constantMlpMajorSyncCheck(
  const MlpMajorSyncInfoParameters * first,
  const MlpMajorSyncInfoParameters * second
)
{
  return CHECK(
    EQUAL(->major_sync_info_CRC)
    EQUAL(->format_info.value)
    EQUAL(->flags.value)
    EQUAL(->variable_bitrate)
    EQUAL(->peak_data_rate)
    EQUAL(->substreams)
    EQUAL(->extended_substream_info)
    EQUAL(->substream_info.b16ch_presentation_present)
    EQUAL(->substream_info.u8ch_presentation)
    EQUAL(->substream_info.u6ch_presentation)
    SUB_FUN_PTR(->channel_meaning, _constantMlpMajorSyncChannelMeaningCheck)
  );
}
#endif

/** \~english
 * \brief
 *
 * \param buf
 * \param value
 * \param size Field size in bits, must be between 0-32 and a multiple of 4.
 */
static inline void _applyNibbleXorMlp(
  uint8_t * parity,
  uint32_t value,
  size_t size
)
{
  assert(NULL != parity);
  assert(size <= 32 && 0 == (size % 4));

  for (; 0 < size; size -= 4) {
    *parity ^= (value >> (size - 4)) & 0xF;
  }
}

int parseMlpSyncHeader(
  BitstreamReaderPtr bs,
  LibbluBitReader * br,
  uint8_t ** buffer,
  size_t * buffer_size,
  MlpSyncHeaderParameters * sh
)
{
  uint8_t mins_header[4];

  /* Read minor sync header */
  if (readBytes(bs, mins_header, 4) < 0)
    return -1;

  /* [v4 check_nibble] */
  sh->check_nibble = (mins_header[0] >> 4);

  /* [u12 access_unit_length] */
  sh->access_unit_length = ((mins_header[0] & 0xF) << 8) | mins_header[1];
  if (sh->access_unit_length < 2)
    LIBBLU_MLP_ERROR_RETURN(
      "Access unit length (access_unit_length) is too short (%u).\n",
      sh->access_unit_length
    );

  unsigned access_unit_data_size = (sh->access_unit_length - 2) << 1;

  /* [u16 input_timing] */
  sh->input_timing = (mins_header[2] << 8) | mins_header[3];

  /* Init bit reader */
  if (*buffer_size < access_unit_data_size) {
    /* Reallocate backing buffer */
    uint8_t * new_buf = realloc(*buffer, access_unit_data_size);
    if (NULL == new_buf)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    *buffer = new_buf;
    *buffer_size = access_unit_data_size;
  }

  /* Read Access Unit data */
  if (readBytes(bs, *buffer, access_unit_data_size) < 0)
    LIBBLU_MLP_ERROR_RETURN("Unable to read access unit from source file.\n");
  initLibbluBitReader(br, *buffer, access_unit_data_size << 3);

  /* Check if current frame contain Major Sync */
  uint32_t next_dword;
  if (getLibbluBitReader(br, &next_dword, 32) < 0)
    return -1;
  sh->major_sync = ((next_dword >> 8) == MLP_SYNCWORD_PREFIX);

  if (sh->major_sync) {
    LIBBLU_MLP_DEBUG_PARSING_HDR("  Major Sync present.\n");

    if (parseMlpMajorSyncInfo(br, &sh->major_sync_info) < 0)
      return -1; /* Error happen during decoding. */
  }

  return 0;
}

/** \~english
 * \brief Return the size in 16-bits words of the 'mlp_sync' section.
 *
 * \param param 'mlp_sync' syntax object.
 * \return unsigned Size in 16-bits words.
 */
static unsigned _getSizeMlpSyncHeader(
  const MlpSyncHeaderParameters * param
)
{
  unsigned size = 2; // Minor sync

  if (param->major_sync) {
    const MlpChannelMeaning * chm = &param->major_sync_info.channel_meaning;
    size += 10 + 3; // Major sync info (plus channel_meaning())
    if (chm->extra_channel_meaning_present)
      size += 1 + chm->extra_channel_meaning_length;
  }

  return size;
}

static unsigned _computeLengthMlpSync(
  const MlpSyncHeaderParameters * mlp_sync_header,
  const MlpSubstreamDirectoryEntry * directory
)
{
  /* Compute access_unit headers size (in 16-bits words unit) */
  const MlpMajorSyncInfoParameters * msi = &mlp_sync_header->major_sync_info;

  unsigned au_hdr_length = _getSizeMlpSyncHeader(mlp_sync_header);
  for (unsigned i = 0; i < msi->substreams; i++)
    au_hdr_length += 1 + directory[i].extra_substream_word;

  return au_hdr_length;
}


int decodeMlpSubstreamDirectory(
  LibbluBitReader * br,
  MlpParsingContext * ctx,
  uint8_t * parity
)
{
  const MlpMajorSyncInfoParameters * msi = &ctx->mlp_sync_header.major_sync_info;

  uint16_t parity_word = 0x0000;
  for (unsigned i = 0; i < msi->substreams; i++) {
    MlpSubstreamDirectoryEntry * entry = &ctx->substream_directory[i];

    uint16_t word;
    MLP_READ_BITS(&word, br, 16, return -1);
    parity_word ^= word;

    /** [v4 flags]
     * -> b1: extra_substream_word
     * -> b1: restart_nonexistent
     * -> b1: crc_present
     * -> v1: reserved
     */
    uint8_t flags = word >> 12;
    entry->extra_substream_word = flags & 0x8;
    entry->restart_nonexistent  = flags & 0x4;
    entry->crc_present          = flags & 0x2;
    entry->reserved_field_1     = flags & 0x1;

    /* [u12 substream_end_ptr] */
    entry->substream_end_ptr = word & 0xFFF;

    if (entry->extra_substream_word) {
      /** [v16 extra_substream_word]
       * -> s9: drc_gain_update
       * -> u3: drc_time_update
       * -> v4: reserved_field_2
       */
      uint16_t drc_fields;
      MLP_READ_BITS(&drc_fields, br, 16, return -1);
      parity_word ^= drc_fields;

      entry->drc_gain_update  = (drc_fields >> 7);
      entry->drc_time_update  = (drc_fields >> 4) & 0x7;
      entry->reserved_field_2 = (drc_fields     ) & 0xF;
    }
  }

  unsigned access_unit_length = ctx->mlp_sync_header.access_unit_length;
  unsigned mlp_sync_length = _computeLengthMlpSync(
    &ctx->mlp_sync_header,
    ctx->substream_directory
  );

  if (access_unit_length < mlp_sync_length)
    LIBBLU_MLP_ERROR_RETURN(
      "Too many words in access unit header, "
      "header is longer than 'access_unit_length' (%u, actual %u words).\n",
      access_unit_length,
      mlp_sync_length
    );

  unsigned unit_end = access_unit_length - mlp_sync_length;

  int ret = checkAndComputeSSSizesMlpSubstreamDirectory(
    ctx->substream_directory,
    msi,
    unit_end,
    ctx->mlp_sync_header.major_sync
  );
  if (ret < 0)
    return -1;

  *parity = lb_xor_16_to_8(parity_word);
  return 0;
}

static uint8_t _computeRestartHeaderChecksum(
  LibbluBitReader * br,
  unsigned start_offset,
  unsigned end_offset
)
{
  const uint8_t * buf = br->buf;
  CrcContext crc_ctx;

  /**
   * Perform CRC check on Restart Header bits in three steps:
   *  - First last 6 bits from the first byte (just used as start value);
   *  - Restart header middle whole bytes (using byte LUT);
   *  - Remaining bits (up to 7, done bit per bit in this function).
   */

  /* Init CRC context with LUT and first 6 bits. */
  initCrcContext(&crc_ctx, MLP_RH_CRC_PARAMS, buf[start_offset >> 3] & 0x3F);

  /* Perform CRC on Restart Header whole bytes */
  unsigned byte_size = (end_offset - start_offset - 6) >> 3;
  applyCrcContext(&crc_ctx, &buf[(start_offset >> 3) + 1], byte_size);

  /* Compute CRC for remaining bits (shifted algorithm) */
  uint32_t crc = (completeCrcContext(&crc_ctx) << 8) | buf[end_offset >> 3];
  for (unsigned off = end_offset & ~0x7; off < end_offset; off++) {
    if (crc & 0x8000)
      crc = (crc << 1) ^ 0x11D00;
    else
      crc <<= 1;
  }

  return (crc >> 8) & 0xFF; // Resulting CRC.
}

/** \~english
 * \brief MLP Huffman codebook look-up-table entry.
 *
 */
typedef struct {
  unsigned mask;  /**< Code word mask, binary mask of the code word size. */
  unsigned code;  /**< Huffman code word. */
  unsigned size;  /**< Code word size in bits. */
  int32_t value;  /**< Actual associated symbol. */
} MlpHuffmanLUTEntry;

#define MLP_HUFFMAN_LONGEST_CODE_SIZE  9

/** \~english
 * \brief MLP Huffman codebook look-up-table.
 *
 */
typedef struct {
  MlpHuffmanLUTEntry entries[18];
  unsigned book_size[MLP_HUFFMAN_LONGEST_CODE_SIZE+1];
} MlpHuffmanLUT;

static const MlpHuffmanLUT mlpHuffmanTables[3] = {
  { /* Huffman table 0, -7 - +10 */
    .entries = {
      {0x1C0, 0x040, 3,  -1},
      {0x1C0, 0x140, 3,   0},
      {0x1C0, 0x100, 3,  +1},
      {0x1C0, 0x180, 3,  +2},
      {0x1C0, 0x1C0, 3,  +3},
      {0x1C0, 0x0C0, 3,  +4},
      {0x1E0, 0x020, 4,  -2},
      {0x1E0, 0x0A0, 4,  +5},
      {0x1F0, 0x010, 5,  -3},
      {0x1F0, 0x090, 5,  +6},
      {0x1F8, 0x008, 6,  -4},
      {0x1F8, 0x088, 6,  +7},
      {0x1FC, 0x004, 7,  -5},
      {0x1FC, 0x084, 7,  +8},
      {0x1FE, 0x002, 8,  -6},
      {0x1FE, 0x082, 8,  +9},
      {0x1FF, 0x001, 9,  -7},
      {0x1FF, 0x081, 9, +10}
    },
    .book_size = {
      0, 0, 0, 6, 8, 10, 12, 14, 16, 18
    }
  },
  { /* Huffman table 1, -7 - +8 */
    .entries = {
      {0x180, 0x100, 2,   0},
      {0x180, 0x180, 2,  +1},
      {0x1C0, 0x040, 3,  -1},
      {0x1C0, 0x0C0, 3,  +2},
      {0x1E0, 0x020, 4,  -2},
      {0x1E0, 0x0A0, 4,  +3},
      {0x1F0, 0x010, 5,  -3},
      {0x1F0, 0x090, 5,  +4},
      {0x1F8, 0x008, 6,  -4},
      {0x1F8, 0x088, 6,  +5},
      {0x1FC, 0x004, 7,  -5},
      {0x1FC, 0x084, 7,  +6},
      {0x1FE, 0x002, 8,  -6},
      {0x1FE, 0x082, 8,  +7},
      {0x1FF, 0x001, 9,  -7},
      {0x1FF, 0x081, 9,  +8}
    },
    .book_size = {
      0, 0, 2, 4, 6, 8, 10, 12, 14, 16
    }
  },
  { /* Huffman table 2, -7 - +7 */
    .entries = {
      {0x100, 0x100, 1,   0},
      {0x1C0, 0x040, 3,  -1},
      {0x1C0, 0x0C0, 3,  +1},
      {0x1E0, 0x020, 4,  -2},
      {0x1E0, 0x0A0, 4,  +2},
      {0x1F0, 0x010, 5,  -3},
      {0x1F0, 0x090, 5,  +3},
      {0x1F8, 0x008, 6,  -4},
      {0x1F8, 0x088, 6,  +4},
      {0x1FC, 0x004, 7,  -5},
      {0x1FC, 0x084, 7,  +5},
      {0x1FE, 0x002, 8,  -6},
      {0x1FE, 0x082, 8,  +6},
      {0x1FF, 0x001, 9,  -7},
      {0x1FF, 0x081, 9,  +7}
    },
    .book_size = {
      0, 1, 1, 3, 5, 7, 9, 11, 13, 15
    }
  }
};

static int _decodeVlcMlpSubstreamBitsReader(
  LibbluBitReader * br,
  const MlpHuffmanLUT * huffman_book_lut,
  int32_t * value
)
{
  /* https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art007 */
  const MlpHuffmanLUTEntry * entries = huffman_book_lut->entries;

  /* Compute Huffman VLC maximum size */
  unsigned max_code_size = remainingBitsLibbluBitReader(br);
  if (0 == max_code_size)
    LIBBLU_MLP_ERROR_RETURN(
      "Too many bits to unpack from substream, "
      "unable to decode block properly.\n"
    );
  if (MLP_HUFFMAN_LONGEST_CODE_SIZE < max_code_size)
    max_code_size = MLP_HUFFMAN_LONGEST_CODE_SIZE; // Constrain to maximum code size

  /* Constrain LUT size to the maximum code size */
  unsigned lut_size = huffman_book_lut->book_size[max_code_size];
  /* Pick but do not read next bits of data */
  uint32_t input = 0x00;
  getLibbluBitReader(br, &input, max_code_size);
  input <<= (MLP_HUFFMAN_LONGEST_CODE_SIZE - max_code_size);

  for (unsigned i = 0; i < lut_size; i++) {
    if ((input & entries[i].mask) == entries[i].code) {
      /* Huffman code match! */
      skipLibbluBitReader(br, entries[i].size);
      *value = entries[i].value;
      return 0;
    }
  }

  LIBBLU_MLP_ERROR_RETURN(
    "Unable to decode a valid huffman code word in block data "
    "(input: 0x%03" PRIX32 ", max_code_size: %u, lut_size: %u).\n",
    input, max_code_size, lut_size
  );
}

static int _isValidMlpRestartSyncWord(
  uint16_t restart_sync_word,
  unsigned ss_idx
)
{
  uint16_t allowed_range[][2] = {
    {0x31EA, 0x31EA},  // 0: 0x31EA
    {0x31EA, 0x31EB},  // 1: 0x31EA-0x31EB
    {0x31EB, 0x31EB},  // 2: 0x31EB
    {0x31EC, 0x31EC}   // 3: 0x31EC
  };

  return (
    allowed_range[ss_idx][0] <= restart_sync_word
    && restart_sync_word <= allowed_range[ss_idx][1]
  );
}

static const char * _matrixChanStr(
  uint8_t matrix_chan_idx
)
{
  static const char * strings[] = {
    "L",
    "R",
    "C",
    "LFE",
    "Ls",
    "Rs",
    "Lb",
    "Rb",
    "Cb"
  };

  if (matrix_chan_idx < ARRAY_SIZE(strings))
    return strings[matrix_chan_idx];
  return "unk";
}

static int _decodeMlpRestartHeader(
  LibbluBitReader * br,
  MlpRestartHeader * restart_hdr,
  unsigned ss_idx
)
{
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "       Restart header, restart_header()\n"
  );

  unsigned start_offset = br->offset;

  /* [v14 restart_sync_word] */
  uint16_t restart_sync_word;
  MLP_READ_BITS(&restart_sync_word, br, 14, return -1);
  restart_hdr->restart_sync_word = restart_sync_word;
  restart_hdr->noise_type        = restart_sync_word & 0x1;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Restart header sync word (restart_sync_word): 0x%04" PRIX16 ".\n",
    restart_sync_word
  );

  if (restart_sync_word < 0x31EA || 0x31EC < restart_sync_word) {
    LIBBLU_MLP_ERROR_RETURN(
      "Unknown 'restart_sync_word' value 0x%04" PRIX16 ".\n",
      restart_sync_word
    );
  }

  if (!_isValidMlpRestartSyncWord(restart_sync_word, ss_idx))
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected 'restart_sync_word' value 0x%04" PRIX16 " "
      "for substream %u.\n",
      restart_sync_word,
      ss_idx
    );

  /* [u16 output_timing] */
  uint16_t output_timing;
  MLP_READ_BITS(&output_timing, br, 16, return -1);
  restart_hdr->output_timing = output_timing;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Output timing (output_timing): %u samples (0x%04" PRIX16 ").\n",
    output_timing,
    output_timing
  );

  /* [u4 min_chan] */
  uint8_t min_chan;
  MLP_READ_BITS(&min_chan, br, 4, return -1);
  restart_hdr->min_chan = min_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Substream minimum carried channel number (min_chan): "
    "channel %u (0x%" PRIX8 ").\n",
    min_chan + 1,
    min_chan
  );

  /* [u4 max_chan] */
  uint8_t max_chan;
  MLP_READ_BITS(&max_chan, br, 4, return -1);
  restart_hdr->max_chan = max_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Substream maximum carried channel number (max_chan): "
    "channel %u (0x%" PRIX8 ").\n",
    max_chan + 1,
    max_chan
  );

  if (max_chan + 1 < min_chan)
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected min/max number of encoded channels in a substream "
      "(min_chan = %u, max_chan = %u).\n",
      min_chan, max_chan
    );

  unsigned max_nb_channels = getMlpMaxNbChannels(ss_idx);
  if (max_nb_channels < max_chan + 1u)
    LIBBLU_MLP_ERROR_RETURN(
      "Maximum number of encoded channels in substream %u (max_chan) exceed "
      "maximum (%u < %u).\n",
      ss_idx,
      max_nb_channels,
      max_chan + 1
    );

  if (max_chan <= min_chan)
    LIBBLU_MLP_ERROR_RETURN(
      "Maximum number of substream carried channels is equal or lower "
      "than the minimum (%u <= %u).\n",
      max_chan + 1,
      min_chan + 1
    );

  /* [u4 max_matrix_chan] */
  uint8_t max_matrix_chan;
  MLP_READ_BITS(&max_matrix_chan, br, 4, return -1);
  restart_hdr->max_matrix_chan = max_matrix_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum number of substream carried matrix channels "
    "(max_matrix_chan): %u channels (0x%" PRIX8 ").\n",
    max_matrix_chan + 1,
    max_matrix_chan
  );

  if (MLP_MAX_NB_MATRIX_CHANNELS < max_matrix_chan + 1)
    LIBBLU_MLP_ERROR_RETURN(
      "Substream number of encoded matrix channels (max_matrix_chan) "
      "exceed maximum (%u < %u).\n",
      MLP_MAX_NB_MATRIX_CHANNELS,
      max_matrix_chan + 1
    );

  /* [u4 dither_shift] */
  uint8_t dither_shift;
  MLP_READ_BITS(&dither_shift, br, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Dithering noise left-shift (dither_shift): %u (0x%" PRIX8 ").\n",
    dither_shift,
    dither_shift
  );

  /* [u23 dither_seed] */
  uint32_t dither_seed;
  MLP_READ_BITS(&dither_seed, br, 23, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Dithering noise seed (dither_seed): %u (0x%03" PRIX32 ").\n",
    dither_seed,
    dither_seed
  );

  /* [s4 max_shift] */
  uint8_t max_shift_val;
  MLP_READ_BITS(&max_shift_val, br, 4, return -1);
  int8_t max_shift = restart_hdr->max_shift = lb_sign_extend(max_shift_val, 4);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum LSBs applied left-shift (max_shift): "
    "%d (0x%" PRIX8 ").\n",
    max_shift,
    max_shift_val
  );

  if (max_shift < 0)
    LIBBLU_MLP_ERROR_RETURN(
      "Negative applied LSBs max left-shift (max_shift) in substream %u "
      "restart header (%d).\n",
      ss_idx,
      max_shift
    );
  if (24 < max_shift)
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid applied LSBs max left-shift (max_shift) larger than 24 bits "
      "in substream %u restart header (%d).\n",
      ss_idx,
      max_shift
    );

  /* [u5 max_lsbs] */
  uint8_t max_lsbs;
  MLP_READ_BITS(&max_lsbs, br, 5, return -1);
  restart_hdr->max_lsbs = max_lsbs;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum bit width of LSBs (max_lsbs): %u (0x%" PRIX8 ").\n",
    max_lsbs,
    max_lsbs
  );

  if (24 < max_lsbs)
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid max bit width of LSBs (max_lsbs) larger than 24 bits "
      "in restart header of substream %u (%u).\n",
      ss_idx,
      max_lsbs
    );

  /* [u5 max_bits] */
  uint8_t max_bits_1;
  MLP_READ_BITS(&max_bits_1, br, 5, return -1);

  /* [u5 max_bits] */
  uint8_t max_bits_2;
  MLP_READ_BITS(&max_bits_2, br, 5, return -1);

  if (max_bits_1 != max_bits_2)
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected non-equal 'max_bits' fields in restart header of "
      "substream %u.\n",
      ss_idx
    );
  restart_hdr->max_bits = max_bits_1;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum output audio bit-depth (max_bits): %u (0x%" PRIX8 ").\n",
    max_bits_1,
    max_bits_1
  );

  if (24 < max_bits_1)
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid max output audio bit-depth (max_bits) larger than 24 bits "
      "in restart header of substream %u (%u).\n",
      ss_idx,
      max_bits_1
    );

  /* [b1 error_protect] */
  bool error_protect;
  MLP_READ_BITS(&error_protect, br, 1, return -1);
  restart_hdr->error_protect = error_protect;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Additional error protection (error_protect): %s (0b%x).\n",
    BOOL_PRESENCE(error_protect),
    error_protect
  );

  /* [u8 lossless_check] */
  bool lossless_check;
  MLP_READ_BITS(&lossless_check, br, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Reconstructed lossless audio data parity (lossless_check): "
    "0x%02" PRIX8 ".\n",
    lossless_check
  );

  /* [v16 reserved] */
  uint16_t reserved;
  MLP_READ_BITS(&reserved, br, 16, return -1);

  if (0x0000 != reserved) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        WARNING Unexpected non-zero reserved field (reserved): "
      "0x%04" PRIX16 ".\n",
      reserved
    );
  }

  for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
    /* [u6 ch_assign[ch]] */
    uint8_t ch_assign;
    MLP_READ_BITS(&ch_assign, br, 6, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Channel assignment for matrix channel %u (ch_assign[%u]): "
      "%s (0x%02" PRIX8 ").\n",
      ch, ch,
      _matrixChanStr(ch_assign),
      ch_assign
    );

    if (max_matrix_chan < ch_assign)
      LIBBLU_MLP_ERROR_RETURN(
        "Out of range channel assignment in substream %u.\n",
        ss_idx
      );
  }

  uint8_t computed_crc = _computeRestartHeaderChecksum(
    br, start_offset, br->offset
  );

  /* [u8 restart_header_CRC] */
  uint8_t restart_header_CRC;
  MLP_READ_BITS(&restart_header_CRC, br, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Restart header checksum (restart_header_CRC): 0x%02" PRIX8 ".\n",
    restart_header_CRC
  );

  if (restart_header_CRC != computed_crc)
    LIBBLU_MLP_ERROR_RETURN("Unexpected restart header CRC checksum.\n");

  return 0;
}

static void _defaultMlpSubstreamParameters(
  MlpSubstreamParameters * ss_param
)
{
  ss_param->block_header_content = MLP_BHC_DEFAULT; // 0xFF, full header.
  ss_param->block_size = 8; // 8 PCM samples
  ss_param->matrix_parameters.num_primitive_matrices = 0; // No matrix
  memset(ss_param->quant_step_size, 0x00, sizeof(ss_param->quant_step_size));

  unsigned min_chan = ss_param->restart_header.min_chan;
  unsigned max_chan = ss_param->restart_header.max_chan;
  for (unsigned ch = min_chan; ch <= max_chan; ch++) {
    MlpChannelParameters * cp = &ss_param->channels_parameters[ch];
    cp->fir_filter.filter_order = 0;
    cp->fir_filter.shift = 0;
    cp->iir_filter.filter_order = 0;
    cp->iir_filter.shift = 0;
    cp->huffman_codebook = MLP_HUFFCB_NONE; // Not entropy coded by default.
    cp->num_huffman_lsbs = 24; // 24 bits.
  }
}

static int _decodeMlpMatrixParameters(
  LibbluBitReader * br,
  MlpMatrixParameters * matrix_param,
  const MlpRestartHeader * restart_hdr
)
{
  bool noise_type = restart_hdr->noise_type;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Matrix parameters, matrix_parameters()\n"
  );

  /* [u4 num_primitive_matrices] */
  unsigned num_primitive_matrices;
  MLP_READ_BITS(&num_primitive_matrices, br, 4, return -1);
  matrix_param->num_primitive_matrices = num_primitive_matrices;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         Number of matrices (num_primitive_matrices): %u (0x%X).\n",
    num_primitive_matrices,
    num_primitive_matrices
  );

  for (unsigned mat = 0; mat < num_primitive_matrices; mat++) {
    MlpMatrix * mat_param = &matrix_param->matrices[mat];

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "          Primitive matrix configuration %u:\n",
      mat
    );

    /* [u4 matrix_output_chan[mat]] */
    uint8_t matrix_output_chan;
    MLP_READ_BITS(&matrix_output_chan, br, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           Output channel (matrix_output_chan[%u]): 0x%" PRIX8 ".\n",
      mat, matrix_output_chan
    );

    /* [u4 num_frac_bits] */
    unsigned num_frac_bits;
    MLP_READ_BITS(&num_frac_bits, br, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           Matrix coefficients number of bits (num_frac_bits): "
      "%u bit(s) (0x%" PRIX8 ").\n",
      num_frac_bits + 2,
      num_frac_bits
    );

    /* [b1 lsb_bypass_exists[mat]] */
    bool lsb_bypass_exists;
    MLP_READ_BITS(&lsb_bypass_exists, br, 1, return -1);
    mat_param->lsb_bypass_exists = lsb_bypass_exists;

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           LSB coding bypass fields presence (lsb_bypass_exists[%u]): "
      "%s (0b%x).\n",
      mat, BOOL_STR(lsb_bypass_exists),
      lsb_bypass_exists
    );

    unsigned max_nb_channels = restart_hdr->max_chan;
    if (!noise_type)
      max_nb_channels += 2; // TODO: +2 noise channels?

    for (unsigned ch = 0; ch <= max_nb_channels; ch++) {
      LIBBLU_MLP_DEBUG_PARSING_SS(
        "           Channel %u:\n",
        ch
      );

      /* [b1 matrix_coeff_present[mat][ch]] */
      bool matrix_coeff_present;
      MLP_READ_BITS(&matrix_coeff_present, br, 1, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "            Matrix coefficient presence (matrix_coeff_present[%u][%u]): "
        "%s (0b%x).\n",
        mat, ch,
        BOOL_STR(matrix_coeff_present),
        matrix_coeff_present
      );

      if (matrix_coeff_present) {
        /* [s(2+num_frac_bits) coeff_value[mat][ch]] */
        unsigned coeff_value_val;
        MLP_READ_BITS(&coeff_value_val, br, 2+num_frac_bits, return -1);
        int coeff_value = lb_sign_extend(coeff_value_val, 2+num_frac_bits);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "             Coefficient value (coeff_value[%u][%u]): %d (0x%X).\n",
          mat, ch,
          coeff_value,
          coeff_value_val
        );
      }
    }

    if (noise_type) {
      /* [u4 matrix_noise_shift[mat]] */
      uint8_t matrix_noise_shift;
      MLP_READ_BITS(&matrix_noise_shift, br, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "           Matrix noise shift value (matrix_noise_shift[%u]): "
        "%" PRIu8 " (0x%" PRIX8 ").\n",
        mat, matrix_noise_shift,
        matrix_noise_shift
      );
    }
  }

  return 0;
}

typedef enum {
  MLP_FIR,
  MLP_IIR
} MlpFilterType;

static const char * MlpFilterTypeStr(
  MlpFilterType filter_type
)
{
  return (MLP_FIR == filter_type) ? "FIR" : "IIR";
}

static unsigned _filterMaxOrder(
  MlpFilterType filter_type
)
{
  // Max order for FIR filter: 4
  // Max order for IIR filter: 8
  return (MLP_FIR == filter_type) ? MLP_FIR_MAX_ORDER : MLP_IIR_MAX_ORDER;
}

static int _decodeMlpFilterParameters(
  LibbluBitReader * br,
  MlpFilter * filter,
  MlpFilterType filter_type
)
{

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "          %s filter parameters, filter_parameters(%s_filter_type)\n",
    MlpFilterTypeStr(filter_type),
    MlpFilterTypeStr(filter_type)
  );

  /* [u4 filter_order] */
  unsigned filter_order;
  MLP_READ_BITS(&filter_order, br, 4, return -1);
  filter->filter_order = filter_order;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "           Filter order N (filter_order): %u (0x%X).\n",
    filter_order,
    filter_order
  );

  unsigned max_filter_order = _filterMaxOrder(filter_type);
  if (max_filter_order < filter_order)
    LIBBLU_MLP_ERROR_RETURN(
      "%s filter order exceed max allowed value (%u < %u).\n",
      MlpFilterTypeStr(filter_type),
      max_filter_order,
      filter_order
    );

  if (0 < filter_order) {
    /* [u4 shift] */
    unsigned shift;
    MLP_READ_BITS(&shift, br, 4, return -1);
    filter->shift = shift;

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           Left shift applied on filter predictions (shift): %u (0x%X).\n",
      shift,
      shift
    );

    /* [u5 coeff_bits] */
    unsigned coeff_bits;
    MLP_READ_BITS(&coeff_bits, br, 5, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           Coefficients size (coeff_bits): %u bits (0x%X).\n",
      coeff_bits,
      coeff_bits
    );

    if (coeff_bits < 1 || 16 < coeff_bits)
      LIBBLU_MLP_ERROR_RETURN(
        "%s filter coefficients size (coeff_bits) "
        "shall be between 1 and 16 bits (%u).\n",
        MlpFilterTypeStr(filter_type),
        coeff_bits
      );

    /* [u3 coeff_shift] */
    unsigned coeff_shift;
    MLP_READ_BITS(&coeff_shift, br, 3, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           Coefficients applied left-shift (coeff_shift): %u (0x%X).\n",
      coeff_shift,
      coeff_shift
    );

    if (16 < coeff_bits + coeff_shift)
      LIBBLU_MLP_ERROR_RETURN(
        "%s filter coefficients dimensions, "
        "sum of their size (coeff_bits = %u) and the applied left-shift "
        "(coeff_shift = %u), exceed 16 bits.\n",
        MlpFilterTypeStr(filter_type),
        coeff_bits,
        coeff_shift
      );

    for (unsigned i = 0; i < filter_order; i++) {
      /* [u<coeff_bits> coeff[i]] */
      uint16_t coeff;
      MLP_READ_BITS(&coeff, br, coeff_bits, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "            Filter coefficient %u (coeff[%u]): "
        "%" PRIu16 " (0x%0*" PRIX16 ").\n",
        i, i,
        coeff,
        (coeff_bits + 3) >> 2,
        coeff
      );
    }

    /* [b1 state_present] */
    bool state_present;
    MLP_READ_BITS(&state_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           Filter state values (state_present): %s (0b%x).\n",
      BOOL_PRESENCE(state_present),
      state_present
    );

    if (state_present && filter_type == MLP_FIR)
      LIBBLU_MLP_ERROR_RETURN(
        "FIR filter shall not have defined state values.\n"
      );

    if (state_present) {
      /* [u4 state_bits] */
      unsigned state_bits;
      MLP_READ_BITS(&state_bits, br, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "            State values size (state_bits): %u bit(s) (0x%X).\n",
        state_bits,
        state_bits
      );

      /* [u4 state_shift] */
      unsigned state_shift;
      MLP_READ_BITS(&state_shift, br, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "            State values left-shift (state_shift): %u (0x%X).\n",
        state_shift,
        state_shift
      );

      if (24 < state_bits + state_shift)
        LIBBLU_MLP_ERROR_RETURN(
          "%s filter state values dimensions, sum of their size (state_bits = %u) "
          "and the applied left-shift (state_shift = %u) exceed 24 bits.\n",
          MlpFilterTypeStr(filter_type),
          state_bits,
          state_shift
        );

      if (0 < state_bits) {
        for (unsigned i = 0; i < filter_order; i++) {
          /* [u<state_bits> state[i]] */
          uint32_t state_val;
          MLP_READ_BITS(&state_val, br, state_bits, return -1);
          int state = lb_sign_extend(state_val, state_bits);

          LIBBLU_MLP_DEBUG_PARSING_SS(
            "             Filter state %u (state[%u]): %d (0x%0*" PRIX32 ").\n",
            i, i,
            state,
            (state_bits + 3) >> 2,
            state_val
          );
        }
      }
    }
  }

  return 0;
}

static int _decodeMlpChannelParameters(
  LibbluBitReader * br,
  MlpSubstreamParameters * ss_param,
  unsigned ch_idx,
  unsigned ss_idx
)
{
  MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch_idx];

  if (ss_param->block_header_content & MLP_BHC_FIR_FILTER_PARAMETERS) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        FIR filter parameters section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_FIR_FILTER_PARAMETERS
    );

    /* [b1 fir_filter_parameters_present] */
    bool fir_filter_parameters_present;
    MLP_READ_BITS(&fir_filter_parameters_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         FIR filter parameters presence "
      "(fir_filter_parameters_present): %s (0b%x).\n",
      BOOL_PRESENCE(fir_filter_parameters_present),
      fir_filter_parameters_present
    );

    if (fir_filter_parameters_present) {
      if (0 < ch_param->fir_filter_nb_changes++) // FIXME Compliance error?
        LIBBLU_MLP_ERROR_RETURN(
          "FIR filter parameters present more than once in access unit, "
          "which is not allowed.\n"
        );

      /* [vn filter_parameters(FIR_filter_type)] */
      if (_decodeMlpFilterParameters(br, &ch_param->fir_filter, MLP_FIR) < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_IIR_FILTER_PARAMETERS) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        IIR filter parameters section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_IIR_FILTER_PARAMETERS
    );

    /* [b1 iir_filter_parameters_present] */
    bool iir_filter_parameters_present;
    MLP_READ_BITS(&iir_filter_parameters_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         IIR filter parameters presence "
      "(iir_filter_parameters_present): %s (0b%x).\n",
      BOOL_PRESENCE(iir_filter_parameters_present),
      iir_filter_parameters_present
    );

    if (iir_filter_parameters_present) {
      if (0 < ch_param->iir_filter_nb_changes++) // FIXME Compliance error?
        LIBBLU_MLP_ERROR_RETURN(
          "IIR filter parameters present more than once in access unit, "
          "which is not allowed.\n"
        );

      /* [vn filter_parameters(IIR_filter_type)] */
      if (_decodeMlpFilterParameters(br, &ch_param->iir_filter, MLP_IIR) < 0)
        return -1;
    }
  }

  unsigned fir_order = ch_param->fir_filter.filter_order;
  unsigned fir_shift = ch_param->fir_filter.shift;
  unsigned iir_order = ch_param->iir_filter.filter_order;
  unsigned iir_shift = ch_param->iir_filter.shift;
  unsigned filters_order = fir_order + iir_order;
  if (MLP_FIR_IIR_TOTAL_MAX_ORDER < filters_order)
    LIBBLU_MLP_ERROR_RETURN(
      "FIR & IIR filters order sum exceed 8 (%u + %u = %u).\n",
      fir_order,
      iir_order,
      filters_order
    );

  if (fir_order && iir_order && fir_shift != iir_shift)
    LIBBLU_MLP_ERROR_RETURN(
      "FIR & IIR filters precision mismatch (%u != %u).\n",
      fir_order,
      iir_order
    );

  if (ss_param->block_header_content & MLP_BHC_HUFFMAN_OFFSET) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Entropy coded samples offset section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_HUFFMAN_OFFSET
    );

    /* [b1 huffman_offset_present] */
    bool huffman_offset_present;
    MLP_READ_BITS(&huffman_offset_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         Entropy coded samples offset present "
      "(huffman_offset_present): %s (0b%x).\n",
      BOOL_PRESENCE(huffman_offset_present),
      huffman_offset_present
    );

    if (huffman_offset_present) {
      /* [s15 huffman_offset] */
      uint16_t huffman_offset_val;
      MLP_READ_BITS(&huffman_offset_val, br, 15, return -1);
      int huffman_offset = lb_sign_extend(huffman_offset_val, 15);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "          Offset added to entropy decoded samples (huffman_offset): "
        "%d (0x%04" PRIX16 ").\n",
        huffman_offset,
        huffman_offset_val
      );
    }
  }

  /* [u2 huffman_codebook] */
  unsigned huffman_codebook;
  MLP_READ_BITS(&huffman_codebook, br, 2, return -1);
  ch_param->huffman_codebook = huffman_codebook;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Huffman codebook (huffman_codebook): %s (0x%X).\n",
    MlpHuffmanCodebookStr(huffman_codebook),
    huffman_codebook
  );

  /* [u5 num_huffman_lsbs] */
  unsigned num_huffman_lsbs;
  MLP_READ_BITS(&num_huffman_lsbs, br, 5, return -1);
  ch_param->num_huffman_lsbs = num_huffman_lsbs;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        LSB residual size (num_huffman_lsbs): %u bits (0x%02X).\n",
    num_huffman_lsbs,
    num_huffman_lsbs
  );

  if (/* 0 < huffman_codebook && */ 24 < num_huffman_lsbs)
    LIBBLU_MLP_ERROR_RETURN(
      "LSB residual size (num_huffman_lsbs) exceed 24 bits "
      "for channel %u of substream %u (%u).\n",
      ch_idx, ss_idx, num_huffman_lsbs
    );

  return 0;
}

static int _decodeMlpBlockHeader(
  LibbluBitReader * br,
  MlpSubstreamParameters * ss_param,
  unsigned ss_idx
)
{
  unsigned max_matrix_chan = ss_param->restart_header.max_matrix_chan;
  unsigned min_chan        = ss_param->restart_header.min_chan;
  unsigned max_chan        = ss_param->restart_header.max_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "      Block header, block_header()\n"
  );

  if (ss_param->block_header_content & MLP_BHC_BLOCK_HEADER_CONTENT) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Block content section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_BLOCK_HEADER_CONTENT
    );

    /* [b1 block_header_content_exists] */
    bool block_header_content_exists;
    MLP_READ_BITS(&block_header_content_exists, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Block header (block_header_content_exists): %s (0b%x).\n",
      BOOL_PRESENCE(block_header_content_exists),
      block_header_content_exists
    );

    if (block_header_content_exists) {
      /* [u8 block_header_content] */
      uint8_t block_header_content;
      MLP_READ_BITS(&block_header_content, br, 8, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "        Block header content byte (block_header_content): "
        "0x%02" PRIX8 ".\n",
        block_header_content
      );
      ss_param->block_header_content = block_header_content;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_BLOCK_SIZE) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Block size section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_BLOCK_SIZE
    );

    /* [b1 block_size_present] */
    bool block_size_present;
    MLP_READ_BITS(&block_size_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Block size presence (block_size_present): %s (0b%x).\n",
      BOOL_PRESENCE(block_size_present),
      block_size_present
    );

    if (block_size_present) {
      /* [u9 block_size] */
      unsigned block_size;
      MLP_READ_BITS(&block_size, br, 9, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "        Block size (block_size): %u samples (0x%03X).\n",
        block_size,
        block_size
      );
      ss_param->block_size = block_size;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_MATRIX_PARAMETERS) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Matrix parameters section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_MATRIX_PARAMETERS
    );

    /* [b1 matrix_parameters_present] */
    bool matrix_parameters_present;
    MLP_READ_BITS(&matrix_parameters_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Matrix parameters presence (matrix_parameters_present): "
      "%s (0b%x).\n",
      BOOL_PRESENCE(matrix_parameters_present),
      matrix_parameters_present
    );

    if (matrix_parameters_present) {
      if (0 < ss_param->matrix_parameters_nb_changes++) // FIXME Compliance error?
        LIBBLU_MLP_ERROR_RETURN(
          "Matrix parameters present more than once in access unit, "
          "which is not allowed.\n"
        );

      int ret = _decodeMlpMatrixParameters(
        br,
        &ss_param->matrix_parameters,
        &ss_param->restart_header
      );
      if (ret < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_OUTPUT_SHIFT) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Output shift section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_OUTPUT_SHIFT
    );

    /* [b1 output_shift_present] */
    bool output_shift_present;
    MLP_READ_BITS(&output_shift_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Output shift presence (output_shift_present): %s (0b%x).\n",
      BOOL_PRESENCE(output_shift_present),
      output_shift_present
    );

    if (output_shift_present) {
      for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
        /* [s4 output_shift[ch]] */
        uint8_t output_shift_val;
        MLP_READ_BITS(&output_shift_val, br, 4, return -1);
        int ouput_shift = lb_sign_extend(output_shift_val, 4);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "         Left shift applied on ouput samples of channel %u "
          "(output_shift[%u]): %d (0x%" PRIX8 ").\n",
          ch, ch,
          ouput_shift,
          output_shift_val
        );

        if (ouput_shift < 0 || 23 < ouput_shift)
          LIBBLU_MLP_ERROR_RETURN(
            "Invalid output samples shift parameter (output_shift) "
            "for channel %u of substream %u (%d).\n",
            ch, ss_idx, ouput_shift
          );
      }
    }
  }

  if (ss_param->block_header_content & MLP_BHC_QUANT_STEP_SIZE) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Quant step size section (block_header_content & 0x%02" PRIX8 "):\n",
      MLP_BHC_QUANT_STEP_SIZE
    );

    /* [b1 quant_step_size_present] */
    bool quant_step_size_present;
    MLP_READ_BITS(&quant_step_size_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Quant step size presence (quant_step_size_present): "
      "%s (0b%x).\n",
      BOOL_PRESENCE(quant_step_size_present),
      quant_step_size_present
    );

    if (quant_step_size_present) {
      for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
        /* [u4 quant_step_size[ch]] */
        uint8_t quant_step_size;
        MLP_READ_BITS(&quant_step_size, br, 4, return -1);
        ss_param->quant_step_size[ch] = quant_step_size;

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "         Quantization shift applied to entropy decoded "
          "samples of channel %u (quant_step_size[%u]): %u (0x%" PRIX8 ").\n",
          ch, ch,
          quant_step_size,
          quant_step_size
        );
      }
    }
  }

  for (unsigned ch = min_chan; ch <= max_chan; ch++) {
    /* [b1 channel_parameters_present[ch]] */
    bool channel_parameters_present;
    MLP_READ_BITS(&channel_parameters_present, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Channel %u parameters presence "
      "(channel_parameters_present[%u]): %s (0b%x).\n",
      ch, ch,
      BOOL_PRESENCE(channel_parameters_present),
      channel_parameters_present
    );

    if (channel_parameters_present) {
      /* [vn channel_parameters()] */
      if (_decodeMlpChannelParameters(br, ss_param, ch, ss_idx) < 0)
        return -1;
    }
  }

  return 0;
}

static int _decodeMlpBlockData(
  LibbluBitReader * br,
  MlpSubstreamParameters * ss_param,
  unsigned ss_idx
)
{
  unsigned min_chan   = ss_param->restart_header.min_chan;
  unsigned max_chan   = ss_param->restart_header.max_chan;
  unsigned block_size = ss_param->block_size;
  const uint8_t * quant_step_size = ss_param->quant_step_size;

  const MlpMatrixParameters * matrix_param = &ss_param->matrix_parameters;
  unsigned num_pmat = matrix_param->num_primitive_matrices;

  LIBBLU_MLP_DEBUG_PARSING_SS("      Block data, block_data()\n");

  for (unsigned sample = 0; sample < block_size; sample++) {

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Huffman coded channels for sample %u, huffman_channels[%u]:\n",
      sample, sample
    );

    for (unsigned mat = 0; mat < num_pmat; mat++) {
      bool lsb_bypass_exists = matrix_param->matrices[mat].lsb_bypass_exists;

      if (lsb_bypass_exists) {
        bool lsb_bypass;
        MLP_READ_BITS(&lsb_bypass, br, 1, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        Bypassed sample LSB value for primitive matrix %u "
          "(lsb_bypass[%u]): %u (0b%x).\n",
          mat, mat,
          lsb_bypass,
          lsb_bypass
        );
      }
    }

    for (unsigned ch = min_chan; ch <= max_chan; ch++) {
      const MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];
      unsigned huffman_codebook = ch_param->huffman_codebook;

      uint8_t num_huffman_lsbs = ch_param->num_huffman_lsbs;
      if (num_huffman_lsbs < quant_step_size[ch])
        LIBBLU_MLP_ERROR_RETURN(
          "Decoded sample quantization size (quant_step_size) "
          "exceed LSB residual size (num_huffman_lsbs) "
          "of channel %u of substream %u (%u < %u).\n",
          ch, ss_idx,
          num_huffman_lsbs,
          quant_step_size[ch]
        );
      unsigned num_lsb_bits = num_huffman_lsbs - quant_step_size[ch];

      if (0 < huffman_codebook) {
        /* Huffman VLC */
        const MlpHuffmanLUT * huffman_book_lut = &mlpHuffmanTables[huffman_codebook-1];

        /* [vn huffman_code] */
        int32_t value;
        if (_decodeVlcMlpSubstreamBitsReader(br, huffman_book_lut, &value) < 0)
          return -1;

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        Huffman entropy-coded MSB sample (huffman_code[%u]): "
          "%" PRId32 ".\n",
          ch, value
        );
      }

      if (0 < num_lsb_bits) {
        /* Residual */
        /* [u<num_lsb_bits> lsb_bits[ch]] */
        uint32_t lsb_bits;
        MLP_READ_BITS(&lsb_bits, br, num_lsb_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        Plain LSB sample residual (lsb_bits[%u]): "
          "0x%0*" PRIX32 " (%u bit(s)).\n",
          ch, (num_lsb_bits + 3) >> 2,
          lsb_bits,
          num_lsb_bits
        );
      }
    }
  }

  return 0;
}

static int _decodeMlpBlock(
  LibbluBitReader * br,
  MlpSubstreamParameters * ss_param,
  unsigned ss_idx,
  unsigned blk_idx
)
{

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "    Audio data block %u, block()\n",
    blk_idx
  );

  /* [b1 block_header_exists] */
  bool block_header_exists;
  MLP_READ_BITS(&block_header_exists, br, 1, return -1);
  bool restart_header_exists = block_header_exists;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "     Block header (block_header_exists): %s (0b%x).\n",
    BOOL_PRESENCE(block_header_exists),
    block_header_exists
  );

  if (block_header_exists) {

    /* [b1 restart_header_exists] */
    MLP_READ_BITS(&restart_header_exists, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "      Restart header (restart_header_exists): %s (0b%x).\n",
      BOOL_PRESENCE(restart_header_exists),
      restart_header_exists
    );

    if (restart_header_exists) {
      MlpRestartHeader * rh = &ss_param->restart_header;

      /* [vn restart_header()] */
      if (_decodeMlpRestartHeader(br, rh, ss_idx) < 0)
        return -1;
      _defaultMlpSubstreamParameters(ss_param); // Set defaults.
      ss_param->restart_header_seen = true;

      if (rh->output_timing != ss_param->cur_output_timing)
        LIBBLU_MLP_ERROR_RETURN(
          "Mismatch with expected counter of samples (%u) and block header "
          "output timing (output_timing = %u) of block %u of substream %u.\n",
          ss_param->cur_output_timing,
          rh->output_timing,
          blk_idx,
          ss_idx
        );
    }

    /* [vn block_header()] */
    if (_decodeMlpBlockHeader(br, ss_param, ss_idx) < 0)
      return -1;
  }

  if (!ss_param->restart_header_seen)
    LIBBLU_MLP_ERROR_RETURN(
      "Missing required substream block first restart header, "
      "decoder won't be able to decode audio.\n"
    );

  if (ss_param->restart_header.error_protect) {
    /* [v16 block_data_bits] */
    uint16_t block_data_bits;
    MLP_READ_BITS(&block_data_bits, br, 16, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     Additional error protection pointer (block_data_bits): "
      "0x%04" PRIX16 ".\n",
      block_data_bits
    );
  }

  /* [vn block_data()] */
  if (_decodeMlpBlockData(br, ss_param, ss_idx) < 0)
    return -1;

  if (ss_param->restart_header.error_protect) {
    /* [v8 block_header_CRC] */
    uint8_t block_header_CRC;
    MLP_READ_BITS(&block_header_CRC, br, 8, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     Additional error protection checksum (block_header_CRC): "
      "0x%02" PRIX8 " (deprecated, not checked).\n",
      block_header_CRC
    );
  }

  return 0;
}


static uint8_t _computeSubstreamCrc(
  LibbluBitReader * br,
  size_t ss_offset,
  size_t ss_size
)
{
  CrcContext crc_ctx;
  initCrcContext(&crc_ctx, MLP_SS_CRC_PARAMS, 0xA2);

  const uint8_t * buf = &accessLibbluBitReader(br)[ss_offset >> 3];
  applyCrcContext(&crc_ctx, buf, ss_size >> 3);

  return completeCrcContext(&crc_ctx);
}


static int _decodeMlpSSParityCrc(
  LibbluBitReader * br,
  size_t ss_start_offset,
  size_t seg_data_size,
  unsigned ss_idx
)
{
  uint8_t seg_parity = computeParityLibbluBitReader(
    br, ss_start_offset, seg_data_size
  );
  uint8_t expected_ss_parity = seg_parity ^ 0xA9;

  /* [v8 substream_parity[i]] */
  uint8_t substream_parity;
  MLP_READ_BITS(&substream_parity, br, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "    Substream parity checksum (substream_parity[%u]): "
    "0x%02" PRIX8 ".\n",
    ss_idx,
    substream_parity
  );
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "     -> Computed: 0x%02" PRIX8 ".\n",
    expected_ss_parity
  );

  if (expected_ss_parity != substream_parity)
    LIBBLU_MLP_ERROR_RETURN("Invalid substream parity checksum.\n");

  uint8_t expected_ss_crc = _computeSubstreamCrc(
    br, ss_start_offset, seg_data_size
  );

  /* [v8 substream_CRC[i]] */
  uint8_t substream_CRC;
  MLP_READ_BITS(&substream_CRC, br, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "    Substream CRC checksum (substream_CRC[%u]): 0x%02" PRIX8 ".\n",
    ss_idx,
    substream_CRC
  );
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "     -> Computed: 0x%02" PRIX8 ".\n",
    expected_ss_crc
  );

  if (expected_ss_crc != substream_CRC)
    LIBBLU_MLP_ERROR_RETURN("Invalid substream CRC checksum.\n");

  return 0;
}


static int _decodeMlpSubstreamSegment(
  LibbluBitReader * br,
  MlpSubstreamParameters * ss,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx
)
{

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Substream segment %u, substream_segment(%u)\n",
    ss_idx,
    ss_idx
  );

  size_t ss_start_offset = offsetLibbluBitReader(br);
  size_t ss_size         = entry->substream_size << 4;

  if (3 <= ss_idx) {
    LIBBLU_MLP_DEBUG_PARSING_SS("    Skipping substream.\n");
    size_t seg_data_size = ss_size - (entry->crc_present << 4);

    if (remainingBitsLibbluBitReader(br) < seg_data_size)
      LIBBLU_MLP_ERROR_RETURN("Prematurate end of file.\n");
    skipLibbluBitReader(br, seg_data_size);

    if (entry->crc_present) {
      if (_decodeMlpSSParityCrc(br, ss_start_offset, seg_data_size, ss_idx) < 0)
        return -1;
    }

    return 0;
  }

  /* Clear substream changes counters */
  ss->matrix_parameters_nb_changes = 0;
  for (unsigned i = 0; i < MLP_MAX_NB_CHANNELS; i++) {
    ss->channels_parameters[i].fir_filter_nb_changes = 0;
    ss->channels_parameters[i].iir_filter_nb_changes = 0;
  }

  bool last_block_in_segment;
  unsigned block_idx = 0;
  do {
    /* [vn block()] */
    if (_decodeMlpBlock(br, ss, ss_idx, block_idx++) < 0)
      return -1;
    ss->cur_output_timing += ss->block_size;

    /* [b1 last_block_in_segment] */
    MLP_READ_BITS(&last_block_in_segment, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "    Is last block in segment (last_block_in_segment): %s (0b%x).\n",
      BOOL_STR(last_block_in_segment),
      last_block_in_segment
    );
  } while (!last_block_in_segment);

  /* [vn padding] */
  if (padWordLibbluBitReader(br) < 0)
    return -1;

  size_t blocks_size = offsetLibbluBitReader(br) - ss_start_offset;
  if (ss_size < blocks_size)
    LIBBLU_MLP_ERROR_RETURN("Substream audio blocks size exceed size.\n");

  if (32 <= ss_size - blocks_size) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "    End of stream extra parameters present.\n"
    );

    /* [v18 terminatorA] */
    uint32_t terminatorA;
    MLP_READ_BITS(&terminatorA, br, 18, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     Final access unit terminator A (terminatorA): "
      "0x%05" PRIX32 ".\n",
      terminatorA
    );

    if (MLP_TERMINATOR_A != terminatorA)
      LIBBLU_MLP_ERROR_RETURN(
        "Invalid final access unit terminator A (terminatorA), "
        "expect 0x%05" PRIX32 ", got 0x%05" PRIX32 ".\n",
        MLP_TERMINATOR_A,
        terminatorA
      );

    /* [b1 zero_samples_indicated] */
    bool zero_samples_indicated;
    MLP_READ_BITS(&zero_samples_indicated, br, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     Additional Access Unit padding sample periods presence "
      "(zero_samples_indicated): %s (0b%x).\n",
      BOOL_PRESENCE(zero_samples_indicated),
      zero_samples_indicated
    );

    if (zero_samples_indicated) {
      /* [u13 zero_samples] */
      unsigned zero_samples;
      MLP_READ_BITS(&zero_samples, br, 13, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "     Number of extra Access Unit padding sample periods added to "
        "source material (zero_samples): %u (0x%04X).\n",
        zero_samples,
        zero_samples
      );
    }
    else {
      /* [v13 terminatorB] */
      uint32_t terminatorB;
      MLP_READ_BITS(&terminatorB, br, 13, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "     Final access unit terminator B (terminatorB): 0x%05" PRIX32 ".\n",
        terminatorB
      );

      if (MLP_TERMINATOR_B != terminatorB)
        LIBBLU_MLP_ERROR_RETURN(
          "Invalid final access unit terminator B (terminatorB), "
          "expect 0x%05" PRIX32 ", got 0x%05" PRIX32 ".\n",
          MLP_TERMINATOR_B,
          terminatorB
        );
    }

    LIBBLU_MLP_DEBUG_PARSING_HDR("End of stream signaled.\n");
    ss->terminator_reached = true;
  }

  size_t seg_data_size = offsetLibbluBitReader(br) - ss_start_offset;
  if (ss_size != seg_data_size + (entry->crc_present << 4))
    LIBBLU_MLP_ERROR_RETURN("Unexpected substream segment size.\n");

  if (entry->crc_present) {
    if (_decodeMlpSSParityCrc(br, ss_start_offset, seg_data_size, ss_idx) < 0)
      return -1;
  }

  return 0;
}


static uint32_t _getSSCodedChMask(
  MlpSubstreamParameters * ss
)
{
  if (!ss->restart_header_seen)
    return 0x0;

  uint8_t min_chan = ss->restart_header.min_chan;
  uint8_t max_chan = ss->restart_header.max_chan;

  return ((1u << (1u + max_chan - min_chan)) - 1) << min_chan;
}


int decodeMlpSubstreamSegments(
  LibbluBitReader * br,
  MlpParsingContext * mlp
)
{
  const MlpSyncHeaderParameters * sh = &mlp->mlp_sync_header;
  const MlpMajorSyncInfoParameters * msi = &sh->major_sync_info;

  if (sh->major_sync) {
    for (unsigned i = 0; i < MLP_MAX_NB_SS; i++)
      mlp->substreams[i].restart_header_seen = false;
  }

  uint32_t coded_ch_mask = 0x0;
  bool terminator_signaled = false;

  for (unsigned i = 0; i < msi->substreams; i++) {
    const MlpSubstreamDirectoryEntry * entry = &mlp->substream_directory[i];
    MlpSubstreamParameters * ss = &mlp->substreams[i];

    /* substream_segment(i) */
    if (_decodeMlpSubstreamSegment(br, ss, entry, i) < 0)
      return -1;

    /* Check for min_chan/max_chan non-overlapping between substreams */
    uint32_t ss_coded_ch_mask = _getSSCodedChMask(ss);
    if (0 != (coded_ch_mask & ss_coded_ch_mask))
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u has overlapping coded channels with previous "
        "substream(s).\n"
      );
    coded_ch_mask |= ss_coded_ch_mask;

    /* Get maximum output bit-depth to guess source audio bit-depth */
    mlp->info.observed_bit_depth = MAX(
      mlp->info.observed_bit_depth,
      ss->restart_header.max_bits
    );


    if (0 < i && terminator_signaled != ss->terminator_reached)
      LIBBLU_MLP_ERROR_RETURN(
        "Unexpected mismatch in end-of-stream signaling in substream %u.\n",
        i
      );
    terminator_signaled |= ss->terminator_reached;

    // substream_parity/substream_CRC parsed/checked in substream_segment.
  }

  mlp->terminator_reached = terminator_signaled;
  return 0;
}


int decodeMlpExtraData(
  LibbluBitReader * br,
  int64_t au_offset
)
{
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "  Extension data, EXTRA_DATA()\n"
  );

  /** [v16 EXTRA_DATA_length_word]
   * -> v4:  length_check_nibble
   * -> u12: EXTRA_DATA_length
   */
  uint16_t length_word;
  MLP_READ_BITS(&length_word, br, 16, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Extension data length field checksum (length_check_nibble): 0x%X.\n",
    length_word >> 12
  );

  if (lb_xor_16_to_4(length_word) != 0xF)
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid EXTRA DATA length nibble check (0x%04" PRIX16 ").\n",
      length_word
    );

  unsigned EXTRA_DATA_length = length_word & 0xFFF;
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Extension data length (EXTRA_DATA_length): %u bytes, 0x%X (0x%02X).\n",
    (EXTRA_DATA_length + 1) << 1,
    (EXTRA_DATA_length + 1) << 1,
    EXTRA_DATA_length
  );

  if (EXTRA_DATA_length < 1)
    LIBBLU_MLP_ERROR_RETURN("EXTRA DATA length is negative.\n");
  if (remainingBitsLibbluBitReader(br) < EXTRA_DATA_length)
    LIBBLU_MLP_ERROR_RETURN("EXTRA DATA length is out of access unit.\n");

  unsigned data_length = (EXTRA_DATA_length << 4) - 8;
  size_t data_offset   = offsetLibbluBitReader(br);
  uint8_t data_parity  = 0xA9 ^ computeParityLibbluBitReader(
    br, data_offset, data_length
  );
  skipLibbluBitReader(br, data_length);

  /* [vn data()] [vn EXTRA_DATA_padding] */
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Offset of Extension Data content: "
    "0x%016" PRIX64 ".\n",
    au_offset + 4 + (data_offset >> 3)
  );

  /* [v8 EXTRA_DATA_parity] */
  uint8_t EXTRA_DATA_parity;
  MLP_READ_BITS(&EXTRA_DATA_parity, br, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Extension data checksum (EXTRA_DATA_parity): 0x%02" PRIX8 ".\n",
    EXTRA_DATA_parity
  );

  if (data_parity != EXTRA_DATA_parity)
    LIBBLU_MLP_ERROR_RETURN("Invalid EXTRA DATA parity check.\n");

  return 0;
}
