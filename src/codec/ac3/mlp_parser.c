#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "mlp_parser.h"


#define MLP_READ_BITS(d, bs, s, e)                                            \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(bs, &_val, s) < 0)                                           \
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
 * \param bs
 * \param ecm
 * \param substream_info
 * \param length Size of extra_channel_meaning_data() section in bits.
 * \return int
 */
static int _parseMlpExtraChannelMeaningData(
  BitstreamReaderPtr bs,
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
    MLP_READ_BITS(&v16ch_cm->u16ch_dialogue_norm, bs, 5, return -1);

    /* [u6 16ch_mix_level] */
    MLP_READ_BITS(&v16ch_cm->u16ch_mix_level, bs, 6, return -1);

    /* [u5 16ch_channel_count] */
    MLP_READ_BITS(&v16ch_cm->u16ch_channel_count, bs, 5, return -1);

    /* [b1 dyn_object_only] */
    MLP_READ_BITS(&v16ch_cm->dyn_object_only, bs, 1, return -1);

    if (v16ch_cm->dyn_object_only) {
      /* [b1 lfe_present] */
      MLP_READ_BITS(&v16ch_cm->lfe_present, bs, 1, return -1);
      ecm_used_length = 18;
    }
    else {
      // TODO: Implement (!dyn_object_only)
      LIBBLU_AC3_ERROR_RETURN(
        "16ch_channel_meaning() with multiple type content not supported yet, "
        "please send sample.\n"
      );
    }
  }

  /* Check extension size. */
  if (length < ecm_used_length)
    LIBBLU_AC3_ERROR_RETURN(
      "Too many bits in extra_channel_meaning_data "
      "(expect %u bits, got %u).\n",
      length, ecm_used_length
    );

  /* [vn reserved] */
  unsigned size = ecmd->reserved_size = length - ecm_used_length;
  if (readBytes(bs, ecmd->reserved, size >> 3) < 0)
    return -1;
  MLP_READ_BITS(&ecmd->reserved[size >> 3], bs, size & 0x7, return -1);

  return 0;
}

static int _parseMlpChannelMeaning(
  BitstreamReaderPtr bs,
  MlpChannelMeaning * param,
  MlpSubstreamInfo si
)
{

  /* [v6 reserved] */
  MLP_READ_BITS(&param->reserved_field, bs, 6, return -1);

  /* [b1 2ch_control_enabled] */
  MLP_READ_BITS(&param->b2ch_control_enabled, bs, 1, return -1);

  /* [b1 6ch_control_enabled] */
  MLP_READ_BITS(&param->b6ch_control_enabled, bs, 1, return -1);

  /* [b1 8ch_control_enabled] */
  MLP_READ_BITS(&param->b8ch_control_enabled, bs, 1, return -1);

  /* [v1 reserved] */
  MLP_READ_BITS(&param->reserved_bool_1, bs, 1, return -1);

  /* [s7 drc_start_up_gain] */
  MLP_READ_BITS(&param->drc_start_up_gain, bs, 7, return -1);

  /* [u6 2ch_dialogue_norm] */
  MLP_READ_BITS(&param->u2ch_dialogue_norm, bs, 6, return -1);

  /* [u6 2ch_mix_level] */
  MLP_READ_BITS(&param->u2ch_mix_level, bs, 6, return -1);

  /* [u5 6ch_dialogue_norm] */
  MLP_READ_BITS(&param->u6ch_dialogue_norm, bs, 5, return -1);

  /* [u6 6ch_mix_level] */
  MLP_READ_BITS(&param->u6ch_mix_level, bs, 6, return -1);

  /* [u5 6ch_source_format] */
  MLP_READ_BITS(&param->u6ch_source_format, bs, 5, return -1);

  /* [u5 8ch_dialogue_norm] */
  MLP_READ_BITS(&param->u8ch_dialogue_norm, bs, 5, return -1);

  /* [u6 8ch_mix_level] */
  MLP_READ_BITS(&param->u8ch_mix_level, bs, 6, return -1);

  /* [u6 8ch_source_format] */
  MLP_READ_BITS(&param->u8ch_source_format, bs, 6, return -1);

  /* [v1 reserved] */
  if (readBit(bs, &param->reserved_bool_2) < 0)
    return -1;

  /* [b1 extra_channel_meaning_present] */
  if (readBit(bs, &param->extra_channel_meaning_present) < 0)
    return -1;
  param->extra_channel_meaning_length = 0;

  if (param->extra_channel_meaning_present) {
    MlpExtraChannelMeaningData * ecmd = &param->extra_channel_meaning_data;

    /* [u4 extra_channel_meaning_length] */
    MLP_READ_BITS(&param->extra_channel_meaning_length, bs, 4, return -1);

    /* [vn extra_channel_meaning_data] */
    unsigned length = ((param->extra_channel_meaning_length + 1) << 4) - 4;
    if (_parseMlpExtraChannelMeaningData(bs, ecmd, si, length) < 0)
      return -1;

    /* [vn padding] */
    if (paddingBoundary(bs, 16) < 0)
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

static int _parseMlpMajorSyncInfo(
  BitstreamReaderPtr bs,
  MlpMajorSyncInfoParameters * param
)
{

  initCrcContext(crcCtxBitstream(bs), TRUE_HD_MS_CRC_PARAMS, 0);

  /* [v32 format_sync] */
  MLP_READ_BITS(&param->format_sync, bs, 32, return -1);

  if (MLP_SYNCWORD == param->format_sync) {
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected DVD Audio MLP format sync word "
      "(format_sync == 0x%08X).\n",
      MLP_SYNCWORD
    );
  }
  else if (TRUE_HD_SYNCWORD != param->format_sync) {
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected sync word in MLP Major Sync "
      "(format_sync == 0x%08" PRIX32 ").\n",
      param->format_sync
    );
  }

  /* [v32 format_info] */
  uint32_t format_info_value;
  MLP_READ_BITS(&format_info_value, bs, 32, return -1);
  /* Unpacking is delayed to flags field parsing */

  /* [v16 signature] */
  MLP_READ_BITS(&param->signature, bs, 16, return -1);

  if (TRUE_HD_SIGNATURE != param->signature) {
    LIBBLU_AC3_ERROR_RETURN(
      "Invalid MLP Major Sync signature word "
      "(signature == 0x%04" PRIX32 ", expect 0x%04X).\n",
      param->signature,
      TRUE_HD_SIGNATURE
    );
  }

  /* [v16 flags] */
  uint16_t flags_value;
  MLP_READ_BITS(&flags_value, bs, 16, return -1);
  _unpackMlpMajorSyncFlags(&param->flags, flags_value);

  /* Unpack format_info field using flags */
  _unpackMlpMajorSyncFormatInfo(&param->format_info, format_info_value);

  /* [v16 reserved] // ignored */
  MLP_READ_BITS(&param->reserved_field_1, bs, 16, return -1);

  /* [b1 variable_rate] */
  MLP_READ_BITS(&param->variable_bitrate, bs, 1, return -1);

  /* [u15 peak_data_rate] */
  MLP_READ_BITS(&param->peak_data_rate, bs, 15, return -1);

  /* [u4 substreams] */
  MLP_READ_BITS(&param->substreams, bs, 4, return -1);

  /* [v2 reserved] // ignored */
  MLP_READ_BITS(&param->reserved_field_2, bs, 2, return -1);

  /* [u2 extended_substream_info] */
  MLP_READ_BITS(&param->extended_substream_info, bs, 2, return -1);

  /* [v8 substream_info] */
  uint8_t substream_info_value;
  MLP_READ_BITS(&substream_info_value, bs, 8, return -1);
  _unpackMlpMajorSyncextSubstreamInfo(substream_info_value, &param->substream_info);

  /* [v64 channel_meaning()] */
  if (_parseMlpChannelMeaning(bs, &param->channel_meaning, param->substream_info) < 0)
    return -1;

  uint16_t crc_result = completeCrcContext(crcCtxBitstream(bs));

  /* [u16 major_sync_info_CRC] */
  MLP_READ_BITS(&param->major_sync_info_CRC, bs, 16, return -1);

  if (crc_result != param->major_sync_info_CRC)
    LIBBLU_AC3_ERROR_RETURN(
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

/** \~english
 * \brief
 *
 * \param d Value destination pointer.
 * \param f Input BitstreamReaderPtr.
 * \param s Value size in bits.
 * \param n Nibble value pointer.
 * \param e Error instruction.
 */
#define READ_BITS_NIBBLE_MLP(d, f, s, n, e)                                   \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(f, &_val, s) < 0)                                            \
      e;                                                                      \
    _applyNibbleXorMlp(n, _val, s);                                           \
    *(d) = _val;                                                              \
  } while (0)

/** \~english
 * \brief
 *
 * \param bs
 * \param sh
 * \param cnv Check nibble value.
 * \return int
 */
int parseMlpSyncHeader(
  BitstreamReaderPtr bs,
  MlpSyncHeaderParameters * sh,
  uint8_t * cnv
)
{

  /* [v4 check_nibble] */
  READ_BITS_NIBBLE_MLP(&sh->check_nibble, bs, 4, cnv, return -1);

  /* [u12 access_unit_length] */
  READ_BITS_NIBBLE_MLP(&sh->access_unit_length, bs, 12, cnv, return -1);

  /* [u16 input_timing] */
  READ_BITS_NIBBLE_MLP(&sh->input_timing, bs, 16, cnv, return -1);

  /* Check if current frame contain Major Sync : */
  sh->major_sync = ((nextUint32(bs) >> 8) == MLP_SYNCWORD_PREFIX);

  if (sh->major_sync) {
    LIBBLU_MLP_DEBUG_PARSING_HDR("  Major Sync present.\n");

    if (_parseMlpMajorSyncInfo(bs, &sh->major_sync_info) < 0)
      return -1; /* Error happen during decoding. */
  }

  return 0;
}

int parseMlpSubstreamDirectoryEntry(
  BitstreamReaderPtr bs,
  MlpSubstreamDirectoryEntry * entry,
  uint8_t * cnv
)
{

  /** [v4 flags]
   * -> b1: extra_substream_word
   * -> b1: restart_nonexistent
   * -> b1: crc_present
   * -> v1: reserved
   */
  uint8_t flags;
  READ_BITS_NIBBLE_MLP(&flags, bs, 4, cnv, return -1);
  entry->extra_substream_word = flags & 0x8;
  entry->restart_nonexistent  = flags & 0x4;
  entry->crc_present          = flags & 0x2;
  entry->reserved_field_1     = flags & 0x1;

  /* [u12 substream_end_ptr] */
  READ_BITS_NIBBLE_MLP(&entry->substream_end_ptr, bs, 12, cnv, return -1);

  if (entry->extra_substream_word) {
    /** [v16 extra_substream_word]
     * -> s9: drc_gain_update
     * -> u3: drc_time_update
     * -> v4: reserved_field_2
     */
    uint16_t drc_fields;
    READ_BITS_NIBBLE_MLP(&drc_fields, bs, 16, cnv, return -1);
    entry->drc_gain_update  = (drc_fields >> 7);
    entry->drc_time_update  = (drc_fields >> 4) & 0x7;
    entry->reserved_field_2 = (drc_fields     ) & 0xF;
  }

  return 0;
}

typedef struct {
  uint8_t * buf;
  unsigned size;
  unsigned offset;
} MlpSubstreamBitsReader;

static int _initMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  BitstreamReaderPtr bs,
  size_t ss_size,
  uint8_t * parity
)
{

  uint8_t * buffer;
  if (NULL == (buffer = (uint8_t *) malloc(ss_size)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  r->buf     = buffer;
  r->size    = 8 * ss_size;
  r->offset  = 0;

  if (readBytes(bs, r->buf, ss_size) < 0)
    return -1;

  for (size_t i = 0; i < ss_size; i++)
    _applyNibbleXorMlp(parity, r->buf[i], 8);
  *parity ^= 0xA9; // XOR-ed with 0xA9 parity constant.

  return 0;
}

static unsigned _remainingBitsMlpSubstreamBitsReader(
  const MlpSubstreamBitsReader * r
)
{
  return r->size - r->offset;
}

static int _readMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  uint32_t * value,
  unsigned size
)
{
  const uint8_t * buf = r->buf;
  unsigned off = r->offset;
  uint32_t result = 0x0;

  if (r->size < off + size)
    LIBBLU_ERROR_RETURN("Too many bits to unpack.\n");

  for (unsigned i = 0; i < size; i++, off++) {
    result = (result << 1) | ((buf[off >> 3] >> (7 - (off & 0x7))) & 0x1);
  }

  r->offset = off;
  *value = result;

  return 0;
}

static uint32_t _fetchBitsMlpSubstreamBitsReader(
  const MlpSubstreamBitsReader * r,
  unsigned size
)
{
  const uint8_t * buf = r->buf;
  unsigned off = r->offset;
  uint32_t result = 0x0;

  assert(size <= _remainingBitsMlpSubstreamBitsReader(r));

  for (unsigned i = 0; i < size; i++, off++) {
    result = (result << 1) | ((buf[off >> 3] >> (7 - (off & 0x7))) & 0x1);
  }

  return result;
}

static void _skipBitsMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  unsigned size
)
{
  assert(size <= _remainingBitsMlpSubstreamBitsReader(r));
  r->offset += size;
}

static void _padBitsMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r
)
{
  unsigned padding_size = (~(r->offset - 1)) & 0xF;

  assert(padding_size <= _remainingBitsMlpSubstreamBitsReader(r));
  _skipBitsMlpSubstreamBitsReader(r, padding_size);
}

static uint8_t _computeRestartHeaderMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  unsigned start_offset,
  unsigned end_offset
)
{
  const uint8_t * buf = r->buf;

  unsigned crc = 0x00;
  for (unsigned off = start_offset; off < end_offset; off++) {
    crc = (crc << 1) ^ ((buf[off >> 3] >> (7 - (off & 0x7))) & 0x1);
    if (crc & 0x100)
      crc ^= 0x11D;
  }

  return crc & 0xFF;
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
  MlpSubstreamBitsReader * r,
  const MlpHuffmanLUT * huffman_book_lut,
  int32_t * value
)
{
  /* https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art007 */
  const MlpHuffmanLUTEntry * entries = huffman_book_lut->entries;

  /* Compute Huffman VLC maximum size */
  unsigned max_code_size = _remainingBitsMlpSubstreamBitsReader(r);
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
  uint32_t input = _fetchBitsMlpSubstreamBitsReader(r, max_code_size);
  input <<= (MLP_HUFFMAN_LONGEST_CODE_SIZE - max_code_size);

  // LIBBLU_MLP_DEBUG_PARSING_SS("%u %u 0x%X\n", lut_size, max_code_size, input);

  for (unsigned i = 0; i < lut_size; i++) {
    if ((input & entries[i].mask) == entries[i].code) {
      /* Huffman code match! */
      _skipBitsMlpSubstreamBitsReader(r, entries[i].size);
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

static void _cleanMlpSubstreamBitsReader(
  MlpSubstreamBitsReader reader
)
{
  free(reader.buf);
}

#define READ_BITS_MLP_SUBSTR(d, r, s, e)                                      \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (_readMlpSubstreamBitsReader(r, &_val, s) < 0)                         \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

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

static int _decodeMlpRestartHeader(
  MlpSubstreamBitsReader * reader,
  MlpRestartHeader * restart_hdr,
  unsigned ss_idx
)
{
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "       Restart header, restart_header()\n"
  );

  unsigned start_offset = reader->offset;

  /* [v14 restart_sync_word] */
  uint16_t restart_sync_word;
  READ_BITS_MLP_SUBSTR(&restart_sync_word, reader, 14, return -1);
  restart_hdr->restart_sync_word = restart_sync_word;
  restart_hdr->noise_type = restart_sync_word & MLP_RH_SW_NOISE_TYPE_MASK;

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
  READ_BITS_MLP_SUBSTR(&output_timing, reader, 16, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Output timing (output_timing): %u samples (0x%04" PRIX16 ").\n",
    output_timing,
    output_timing
  );

  /* [u4 min_chan] */
  uint8_t min_chan;
  READ_BITS_MLP_SUBSTR(&min_chan, reader, 4, return -1);
  restart_hdr->min_chan = min_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (min_chan): "
    "%u channels (0x%" PRIX8 ").\n",
    min_chan + 1,
    min_chan
  );

  /* [u4 max_chan] */
  uint8_t max_chan;
  READ_BITS_MLP_SUBSTR(&max_chan, reader, 4, return -1);
  restart_hdr->max_chan = max_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_chan): "
    "%u channels (0x%" PRIX8 ").\n",
    max_chan + 1,
    max_chan
  );

  // TODO: Check 'max_chan' according to ss carriage.

  if (max_chan <= min_chan)
    LIBBLU_MLP_ERROR_RETURN(
      "Maximum number of substream carried channels is equal or lower "
      "than the minimum (%u <= %u).\n",
      max_chan + 1,
      min_chan + 1
    );

  /* [u4 max_matrix_chan] */
  uint8_t max_matrix_chan;
  READ_BITS_MLP_SUBSTR(&max_matrix_chan, reader, 4, return -1);
  restart_hdr->max_matrix_chan = max_matrix_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum number of substream carried matrix channels "
    "(max_matrix_chan): %u channels (0x%" PRIX8 ").\n",
    max_matrix_chan + 1,
    max_matrix_chan
  );

  // TODO: Check 'max_matrix_chan'.

  /* [u4 dither_shift] */
  uint8_t dither_shift;
  READ_BITS_MLP_SUBSTR(&dither_shift, reader, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Dithering noise left-shift (dither_shift): %u (0x%" PRIX8 ").\n",
    dither_shift,
    dither_shift
  );

  /* [u23 dither_seed] */
  uint32_t dither_seed;
  READ_BITS_MLP_SUBSTR(&dither_seed, reader, 23, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Dithering noise seed (dither_seed): %u (0x%03" PRIX32 ").\n",
    dither_seed,
    dither_seed
  );

  /* [s4 max_shift] */
  uint8_t max_shift;
  READ_BITS_MLP_SUBSTR(&max_shift, reader, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_shift): %d (0x%" PRIX8 ").\n",
    lb_sign_extend(max_shift, 4),
    max_shift
  );

  /* [u5 max_lsbs] */
  uint8_t max_lsbs;
  READ_BITS_MLP_SUBSTR(&max_lsbs, reader, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum bit width of LSBs (max_lsbs): %u (0x%" PRIX8 ").\n",
    max_lsbs,
    max_lsbs
  );

  /* [u5 max_bits] */
  uint8_t max_bits;
  READ_BITS_MLP_SUBSTR(&max_bits, reader, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_bits): %u (0x%" PRIX8 ").\n",
    max_bits,
    max_bits
  );

  /* [u5 max_bits] */
  // uint8_t max_bits;
  READ_BITS_MLP_SUBSTR(&max_bits, reader, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_bits): %u (0x%" PRIX8 ").\n",
    max_bits,
    max_bits
  );

  /* [b1 error_protect] */
  bool error_protect;
  READ_BITS_MLP_SUBSTR(&error_protect, reader, 1, return -1);
  restart_hdr->error_protect = error_protect;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (error_protect): %s (0b%x).\n",
    BOOL_STR(error_protect),
    error_protect
  );

  /* [u8 lossless_check] */
  bool lossless_check;
  READ_BITS_MLP_SUBSTR(&lossless_check, reader, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (lossless_check): 0x%02" PRIX8 ".\n",
    lossless_check
  );

  /* [v16 reserved] */
  uint16_t reserved;
  READ_BITS_MLP_SUBSTR(&reserved, reader, 16, return -1);

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
    READ_BITS_MLP_SUBSTR(&ch_assign, reader, 6, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Channel assignment for matrix channel %u (ch_assign[%u]): "
      "0x%02" PRIX8 ".\n",
      ch, ch, ch_assign
    );
  }

  uint8_t computed_crc = _computeRestartHeaderMlpSubstreamBitsReader(
    reader, start_offset, reader->offset
  );

  /* [u8 restart_header_CRC] */
  uint8_t restart_header_CRC;
  READ_BITS_MLP_SUBSTR(&restart_header_CRC, reader, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (restart_header_CRC): 0x%02" PRIX8 ".\n",
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
    MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];
    ch_param->huffman_codebook = 0x0; // TODO
    ch_param->num_huffman_lsbs = 24; // 24 bits.
  }
}

static int _decodeMlpMatrixParameters(
  MlpSubstreamBitsReader * reader,
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
  READ_BITS_MLP_SUBSTR(&num_primitive_matrices, reader, 4, return -1);
  matrix_param->num_primitive_matrices = num_primitive_matrices;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         TODO (num_primitive_matrices): %u (0x%X).\n",
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
    READ_BITS_MLP_SUBSTR(&matrix_output_chan, reader, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (matrix_output_chan[%u]): 0x%" PRIX8 ".\n",
      mat, matrix_output_chan
    );

    /* [u4 num_frac_bits] */
    unsigned num_frac_bits;
    READ_BITS_MLP_SUBSTR(&num_frac_bits, reader, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (num_frac_bits): %u bit(s).\n",
      num_frac_bits
    );

    /* [b1 lsb_bypass_exists[mat]] */
    bool lsb_bypass_exists;
    READ_BITS_MLP_SUBSTR(&lsb_bypass_exists, reader, 1, return -1);
    mat_param->lsb_bypass_exists = lsb_bypass_exists;

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (lsb_bypass_exists[%u]): %s (0b%x).\n",
      mat, BOOL_STR(lsb_bypass_exists),
      lsb_bypass_exists
    );

    unsigned max_nb_channels = restart_hdr->max_chan;
    if (!noise_type)
      max_nb_channels += 2; // TODO: +2 noise channels?

    for (unsigned ch = 0; ch <= max_nb_channels; ch++) {
      /* [b1 matrix_coeff_present[mat][ch]] */
      bool matrix_coeff_present;
      READ_BITS_MLP_SUBSTR(&matrix_coeff_present, reader, 1, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "           TODO (matrix_coeff_present[%u][%u]): %s (0b%x).\n",
        mat, ch, BOOL_STR(matrix_coeff_present),
        matrix_coeff_present
      );

      if (matrix_coeff_present) {
        /* [s(2+num_frac_bits) coeff_value[mat][ch]] */
        unsigned coeff_value;
        READ_BITS_MLP_SUBSTR(&coeff_value, reader, 2+num_frac_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "            TODO (coeff_value[%u][%u]): %d (0x%X).\n",
          mat, ch, lb_sign_extend(coeff_value, 2+num_frac_bits),
          coeff_value
        );
      }
    }

    if (noise_type) {
      /* [u4 matrix_noise_shift[mat]] */
      uint8_t matrix_noise_shift;
      READ_BITS_MLP_SUBSTR(&matrix_noise_shift, reader, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "           TODO (matrix_noise_shift[%u]): "
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

static int _decodeMlpFilterParameters(
  MlpSubstreamBitsReader * reader,
  MlpFilterType filter_type
)
{

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "          %s filter parameters, filter_parameters(%s)\n",
    (MLP_FIR == filter_type) ? "FIR" : "IIR",
    (MLP_FIR == filter_type) ? "FIR_filter_type" : "IIR_filter_type"
  );

  /* [u4 filter_order] */
  unsigned filter_order;
  READ_BITS_MLP_SUBSTR(&filter_order, reader, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "           TODO (filter_order): %u (0x%X).\n",
    filter_order,
    filter_order
  );

  if (0 < filter_order) {
    /* [u4 shift] */
    unsigned shift;
    READ_BITS_MLP_SUBSTR(&shift, reader, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (shift): %u (0x%X).\n",
      shift,
      shift
    );

    /* [u5 coeff_bits] */
    unsigned coeff_bits;
    READ_BITS_MLP_SUBSTR(&coeff_bits, reader, 5, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (coeff_bits): %u (0x%02X).\n",
      coeff_bits,
      coeff_bits
    );

    /* [u3 coeff_shift] */
    unsigned coeff_shift;
    READ_BITS_MLP_SUBSTR(&coeff_shift, reader, 3, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (coeff_shift): %u (0x%02X).\n",
      coeff_shift,
      coeff_shift
    );

    for (unsigned i = 0; i < filter_order; i++) {
      /* [u<coeff_bits> coeff[i]] */
      uint32_t coeff;
      READ_BITS_MLP_SUBSTR(&coeff, reader, coeff_bits, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "            TODO (coeff[i]): %" PRIu32 " (0x%0*" PRIX32 ").\n",
        coeff,
        (coeff_bits + 3) >> 2,
        coeff
      );
    }

    /* [b1 state_present] */
    bool state_present;
    READ_BITS_MLP_SUBSTR(&state_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "            TODO (state_present): %s (0b%x).\n",
      BOOL_PRESENCE(state_present),
      state_present
    );

    if (state_present) {
      /* [u4 state_bits] */
      unsigned state_bits;
      READ_BITS_MLP_SUBSTR(&state_bits, reader, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "             TODO (state_bits): %u (0x%X).\n",
        state_bits,
        state_bits
      );

      /* [u4 state_shift] */
      unsigned state_shift;
      READ_BITS_MLP_SUBSTR(&state_shift, reader, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "             TODO (state_shift): %u (0x%X).\n",
        state_shift,
        state_shift
      );

      for (unsigned i = 0; i < filter_order; i++) {
        /* [u<state_bits> state[i]] */
        uint32_t state;
        READ_BITS_MLP_SUBSTR(&state, reader, state_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "             TODO (state[i]): %" PRIu32 " (0x%0*" PRIX32 ").\n",
          state,
          (state_bits + 3) >> 2,
          state
        );
      }
    }
  }

  return 0;
}

static int _decodeMlpChannelParameters(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param,
  unsigned ch_idx
)
{
  MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch_idx];

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Channel %u parameters, channel_parameters[%u]()\n",
    ch_idx, ch_idx
  );

  if (ss_param->block_header_content & MLP_BHC_FIR_FILTER_PARAMETERS) {
    /* [b1 fir_filter_parameters_present] */
    bool fir_filter_parameters_present;
    READ_BITS_MLP_SUBSTR(&fir_filter_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         TODO (fir_filter_parameters_present): %s (0b%x).\n",
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
      if (_decodeMlpFilterParameters(reader, MLP_FIR) < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_IIR_FILTER_PARAMETERS) {
    /* [b1 iir_filter_parameters_present] */
    bool iir_filter_parameters_present;
    READ_BITS_MLP_SUBSTR(&iir_filter_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         TODO (iir_filter_parameters_present): %s (0b%x).\n",
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
      if (_decodeMlpFilterParameters(reader, MLP_IIR) < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_HUFFMAN_OFFSET) {
    /* [b1 huffman_offset_present] */
    bool huffman_offset_present;
    READ_BITS_MLP_SUBSTR(&huffman_offset_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         TODO (huffman_offset_present): %s (0b%x).\n",
      BOOL_PRESENCE(huffman_offset_present),
      huffman_offset_present
    );

    if (huffman_offset_present) {
      /* [s15 huffman_offset] */
      uint16_t huffman_offset;
      READ_BITS_MLP_SUBSTR(&huffman_offset, reader, 15, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "          TODO (huffman_offset): %d (0x%04" PRIX16 ").\n",
        lb_sign_extend(huffman_offset, 15),
        huffman_offset
      );
    }
  }

  /* [u2 huffman_codebook] */
  unsigned huffman_codebook;
  READ_BITS_MLP_SUBSTR(&huffman_codebook, reader, 2, return -1);
  ch_param->huffman_codebook = huffman_codebook;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         TODO (huffman_codebook): %u.\n",
    huffman_codebook
  );

  /* [u5 num_huffman_lsbs] */
  unsigned num_huffman_lsbs;
  READ_BITS_MLP_SUBSTR(&num_huffman_lsbs, reader, 5, return -1);
  ch_param->num_huffman_lsbs = num_huffman_lsbs;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         TODO (num_huffman_lsbs): %u (0x%02X).\n",
    num_huffman_lsbs,
    num_huffman_lsbs
  );

  // TODO
  assert(0 == huffman_codebook || num_huffman_lsbs <= 24);

  return 0;
}

static int _decodeMlpBlockHeader(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param
)
{
  unsigned max_matrix_chan = ss_param->restart_header.max_matrix_chan;
  unsigned min_chan        = ss_param->restart_header.min_chan;
  unsigned max_chan        = ss_param->restart_header.max_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "      Block header, block_header()\n"
  );

  if (ss_param->block_header_content & MLP_BHC_BLOCK_HEADER_CONTENT) {
    /* [b1 block_header_content_exists] */
    bool block_header_content_exists;
    READ_BITS_MLP_SUBSTR(&block_header_content_exists, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (block_header_content_exists): %s (0b%x).\n",
      BOOL_PRESENCE(block_header_content_exists),
      block_header_content_exists
    );

    if (block_header_content_exists) {
      /* [u8 block_header_content] */
      uint8_t block_header_content;
      READ_BITS_MLP_SUBSTR(&block_header_content, reader, 8, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "        TODO (block_header_content): 0x%02" PRIX8 ".\n",
        block_header_content
      );
      ss_param->block_header_content = block_header_content;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_BLOCK_SIZE) {
    /* [b1 block_size_present] */
    bool block_size_present;
    READ_BITS_MLP_SUBSTR(&block_size_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (block_size_present): %s (0b%x).\n",
      BOOL_PRESENCE(block_size_present),
      block_size_present
    );

    if (block_size_present) {
      /* [u9 block_size] */
      unsigned block_size;
      READ_BITS_MLP_SUBSTR(&block_size, reader, 9, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "        TODO (block_size): %u samples (0x%03X).\n",
        block_size,
        block_size
      );
      ss_param->block_size = block_size;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_MATRIX_PARAMETERS) {
    /* [b1 matrix_parameters_present] */
    bool matrix_parameters_present;
    READ_BITS_MLP_SUBSTR(&matrix_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (matrix_parameters_present): %s (0b%x).\n",
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
        reader,
        &ss_param->matrix_parameters,
        &ss_param->restart_header
      );
      if (ret < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_OUTPUT_SHIFT) {
    /* [b1 output_shift_present] */
    bool output_shift_present;
    READ_BITS_MLP_SUBSTR(&output_shift_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (output_shift_present): %s (0b%x).\n",
      BOOL_PRESENCE(output_shift_present),
      output_shift_present
    );

    if (output_shift_present) {
      for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
        /* [s4 output_shift[ch]] */
        uint8_t output_shift;
        READ_BITS_MLP_SUBSTR(&output_shift, reader, 4, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (output_shift[%u]): %d (0x%" PRIX8 ").\n",
          ch, lb_sign_extend(output_shift, 4),
          output_shift
        );
      }
    }
  }

  if (ss_param->block_header_content & MLP_BHC_QUANT_STEP_SIZE) {
    /* [b1 quant_step_size_present] */
    bool quant_step_size_present;
    READ_BITS_MLP_SUBSTR(&quant_step_size_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (quant_step_size_present): %s (0b%x).\n",
      BOOL_PRESENCE(quant_step_size_present),
      quant_step_size_present
    );

    if (quant_step_size_present) {
      for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
        /* [u4 quant_step_size[ch]] */
        uint8_t quant_step_size;
        READ_BITS_MLP_SUBSTR(&quant_step_size, reader, 4, return -1);
        ss_param->quant_step_size[ch] = quant_step_size;

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (quant_step_size[%u]): %u (0x%" PRIX8 ").\n",
          ch, quant_step_size,
          quant_step_size
        );
      }
    }
  }

  for (unsigned ch = min_chan; ch <= max_chan; ch++) {
    /* [b1 channel_parameters_present[ch]] */
    bool channel_parameters_present;
    READ_BITS_MLP_SUBSTR(&channel_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (channel_parameters_present[%u]): %s (0b%x).\n",
      ch, BOOL_PRESENCE(channel_parameters_present),
      channel_parameters_present
    );

    if (channel_parameters_present) {
      /* [vn channel_parameters()] */
      if (_decodeMlpChannelParameters(reader, ss_param, ch) < 0)
        return -1;
    }
  }

  for (unsigned ch = 0; ch <= max_chan; ch++) {
    const MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];

    assert(
      0 == ch_param->huffman_codebook
      || ss_param->quant_step_size[ch] <= ch_param->num_huffman_lsbs
    ); // TODO
  }

  return 0;
}

static int _decodeMlpBlockData(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param
)
{
  unsigned min_chan   = ss_param->restart_header.min_chan;
  unsigned max_chan   = ss_param->restart_header.max_chan;
  unsigned block_size = ss_param->block_size;
  const unsigned * quant_step_size = ss_param->quant_step_size;

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
        READ_BITS_MLP_SUBSTR(&lsb_bypass, reader, 1, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (lsb_bypass[%u]): %s (0b%x).\n",
          mat, BOOL_STR(lsb_bypass),
          lsb_bypass
        );
      }
    }

    for (unsigned ch = min_chan; ch <= max_chan; ch++) {
      const MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];
      unsigned huffman_codebook = ch_param->huffman_codebook;
      unsigned num_huffman_lsbs = ch_param->num_huffman_lsbs;
      int num_lsb_bits = num_huffman_lsbs - quant_step_size[ch];

      assert(0 <= num_lsb_bits);

      if (0 < huffman_codebook) {
        /* Huffman VLC */
        const MlpHuffmanLUT * huffman_book_lut = &mlpHuffmanTables[huffman_codebook-1];

        /* [vn huffman_code] */
        int32_t value;
        if (_decodeVlcMlpSubstreamBitsReader(reader, huffman_book_lut, &value) < 0)
          return -1;

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (huffman_code[%u]): %" PRId32 ".\n",
          ch, value
        );
      }

      if (0 < num_lsb_bits) {
        /* [u<num_lsb_bits> lsb_bits[ch]] */
        uint32_t lsb_bits;
        READ_BITS_MLP_SUBSTR(&lsb_bits, reader, num_lsb_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (lsb_bits[%u]): 0x%0*" PRIX32 " (%u bit(s)).\n",
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
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx,
  unsigned blk_idx
)
{
  (void) entry; // TODO

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "    Audio data block %u, block()\n",
    blk_idx
  );

  /* [b1 block_header_exists] */
  bool block_header_exists;
  READ_BITS_MLP_SUBSTR(&block_header_exists, reader, 1, return -1);
  bool restart_header_exists = block_header_exists;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "     Block header (block_header_exists): %s (0b%x).\n",
    BOOL_PRESENCE(block_header_exists),
    block_header_exists
  );

  if (block_header_exists) {

    /* [b1 restart_header_exists] */
    READ_BITS_MLP_SUBSTR(&restart_header_exists, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "      Restart header (restart_header_exists): %s (0b%x).\n",
      BOOL_PRESENCE(restart_header_exists),
      restart_header_exists
    );

    if (restart_header_exists) {
      /* [vn restart_header()] */
      if (_decodeMlpRestartHeader(reader, &ss_param->restart_header, ss_idx) < 0)
        return -1;
      _defaultMlpSubstreamParameters(ss_param); // Set defaults.
      ss_param->restart_header_seen = true;
    }

    /* [vn block_header()] */
    if (_decodeMlpBlockHeader(reader, ss_param) < 0)
      return -1;
  }

  if (!ss_param->restart_header_seen)
    LIBBLU_MLP_ERROR_RETURN(
      "Missing required substream block first restart header, "
      "decoder won't be able to decode audio.\n"
    );

  if (ss_param->restart_header.error_protect) {
    /* [v16 error_protect] */
    uint16_t error_protect;
    READ_BITS_MLP_SUBSTR(&error_protect, reader, 16, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (error_protect): 0x%04" PRIX16 ".\n",
      error_protect
    );
  }

  /* [vn block_data()] */
  if (_decodeMlpBlockData(reader, ss_param) < 0)
    return -1;

  if (ss_param->restart_header.error_protect) {
    /* [v8 block_header_CRC] */
    uint8_t block_header_CRC;
    READ_BITS_MLP_SUBSTR(&block_header_CRC, reader, 8, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (block_header_CRC): 0x%02" PRIX8 ".\n",
      block_header_CRC
    );
  }

  return 0;
}

int decodeMlpSubstreamSegment(
  BitstreamReaderPtr input,
  MlpSubstreamParameters * substream,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx
)
{
  MlpSubstreamBitsReader reader;
  uint8_t parity = 0x00;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Substream segment %u, substream_segment(%u)\n",
    ss_idx,
    ss_idx
  );

  /* Init bitstream reader */
  unsigned ss_size = 2 * entry->substream_size;
  if (_initMlpSubstreamBitsReader(&reader, input, ss_size, &parity) < 0)
    return -1;

  /* Clear substream changes counters */
  substream->matrix_parameters_nb_changes = 0;
  for (unsigned i = 0; i < MLP_MAX_NB_CHANNELS; i++) {
    substream->channels_parameters[i].fir_filter_nb_changes = 0;
    substream->channels_parameters[i].iir_filter_nb_changes = 0;
  }

  bool last_block_in_segment;
  unsigned block_idx = 0;
  do {
    /* [vn block()] */
    if (_decodeMlpBlock(&reader, substream, entry, ss_idx, block_idx++) < 0)
      goto free_return;

    /* [b1 last_block_in_segment] */
    READ_BITS_MLP_SUBSTR(&last_block_in_segment, &reader, 1, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "    TODO (last_block_in_segment): %s (0b%x).\n",
      BOOL_STR(last_block_in_segment),
      last_block_in_segment
    );
  } while (!last_block_in_segment);

  /* [vn padding] */
  _padBitsMlpSubstreamBitsReader(&reader);

  if (32 <= _remainingBitsMlpSubstreamBitsReader(&reader)) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "    End of stream extra parameters present.\n"
    );

    /* [v18 terminatorA] */
    uint32_t terminatorA;
    READ_BITS_MLP_SUBSTR(&terminatorA, &reader, 18, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (terminatorA): 0x%05" PRIX32 ".\n",
      terminatorA
    );

    /* [b1 zero_samples_indicated] */
    bool zero_samples_indicated;
    READ_BITS_MLP_SUBSTR(&zero_samples_indicated, &reader, 1, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (zero_samples_indicated): %s (0b%x).\n",
      BOOL_STR(zero_samples_indicated),
      zero_samples_indicated
    );

    if (zero_samples_indicated) {
      /* [u13 zero_samples] */
      unsigned zero_samples;
      READ_BITS_MLP_SUBSTR(&zero_samples, &reader, 13, goto free_return);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "     TODO (zero_samples): %u (0x%04X).\n",
        zero_samples,
        zero_samples
      );
    }
    else {
      /* [v13 terminatorB] */
      uint32_t terminatorB;
      READ_BITS_MLP_SUBSTR(&terminatorB, &reader, 13, goto free_return);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "     TODO (terminatorB): 0x%05" PRIX32 ".\n",
        terminatorB
      );
    }
  }

  if (entry->crc_present) {
    /* [v8 substream_parity[i]] */
    uint8_t substream_parity;
    READ_BITS_MLP_SUBSTR(&substream_parity, &reader, 8, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (substream_parity[%u]): 0x%02" PRIX8 ".\n",
      ss_idx,
      substream_parity
    );

    /* [v8 substream_CRC[i]] */
    uint8_t substream_CRC;
    READ_BITS_MLP_SUBSTR(&substream_CRC, &reader, 8, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (substream_CRC[%u]): 0x%02" PRIX8 ".\n",
      ss_idx,
      substream_CRC
    );
  }

  _cleanMlpSubstreamBitsReader(reader);
  return 0;

free_return:
  _cleanMlpSubstreamBitsReader(reader);
  return -1;
}

int decodeMlpExtraData(
  BitstreamReaderPtr input,
  unsigned au_remaining_length
)
{
  MlpSubstreamBitsReader reader;
  uint8_t parity = 0x00;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "  Extension data, EXTRA_DATA()\n"
  );

  /** [v16 EXTRA_DATA_length_word]
   * -> v4:  length_check_nibble
   * -> u12: EXTRA_DATA_length
   */
  uint16_t length_word;
  MLP_READ_BITS(&length_word, input, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   TODO (length_check_nibble): 0x%X.\n",
    length_word >> 12
  );

  for (int i = 3; 0 <= i; i--)
    parity ^= (length_word >> (i << 2)) & 0xFF;
  if (parity != 0xF)
    LIBBLU_MLP_ERROR_RETURN("Invalid EXTRA DATA length nibble check.\n");

  unsigned EXTRA_DATA_length = length_word & 0xFFF;
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   TODO (EXTRA_DATA_length): 0x%02X.\n",
    EXTRA_DATA_length
  );

  if (au_remaining_length < EXTRA_DATA_length)
    LIBBLU_MLP_ERROR_RETURN("EXTRA DATA length is out of access unit.\n");

  unsigned data_length = (EXTRA_DATA_length << 4) - 4;
  if (_initMlpSubstreamBitsReader(&reader, input, data_length, &parity) < 0)
    return -1;
  _cleanMlpSubstreamBitsReader(reader);

  /* [vn EXTRA_DATA_padding] */
  // Skipped

  /* [v8 EXTRA_DATA_parity] */
  uint8_t EXTRA_DATA_parity;
  MLP_READ_BITS(&EXTRA_DATA_parity, input, 8, return -1);

  if (parity != EXTRA_DATA_parity)
    LIBBLU_MLP_ERROR_RETURN("Invalid EXTRA DATA parity check.\n");

  return 0;
}
