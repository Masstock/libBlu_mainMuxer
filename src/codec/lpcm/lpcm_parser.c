#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "lpcm_parser.h"

#define READ_BYTES(d, br, s, e)                                               \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readValue64LittleEndian(br, s, &_val) < 0)                            \
      e;                                                                      \
    *d = _val;                                                                \
  } while (0)

#define READ_ARRAY(d, br, s, e)                                               \
  do {                                                                        \
    if (readBytes(br, d, s) < 0)                                              \
      e;                                                                      \
  } while (0)

#define SKIP_BYTES(br, s, e)                                                  \
  do {                                                                        \
    if (skipBytes(br, s) < 0)                                                 \
      e;                                                                      \
  } while (0)

#define PAD_WORD(br, e)                                                       \
  do {                                                                        \
    if (paddingBoundary(br, 2))                                               \
      e;                                                                      \
  } while (0)

static int _parseRiffChunkHeader(
  BitstreamReaderPtr br,
  RiffChunkHeader * hdr
)
{
  int64_t offset = tellPos(br);

  /* [u32 ckID] */
  uint32_t ckID_value;
  READ_BYTES(&ckID_value, br, 4, return -1);

  /* [u32 ckSize] */
  uint32_t ckSize_value;
  READ_BYTES(&ckSize_value, br, 4, return -1);

  LIBBLU_LPCM_DEBUG(
    "0x%" PRIX64 " - %s chunk "
    "- ckID = 0x%08" PRIX32 ", ckSize = 0x%08" PRIX32 "\n",
    offset, WaveRIFFChunkIdStr(ckID_value), ckID_value, ckSize_value
  );

  *hdr = (RiffChunkHeader) {
    .ckID = ckID_value,
    .ckSize = ckSize_value
  };

  return 0;
}

static int _decodeWaveJunkChunk(
  BitstreamReaderPtr br,
  RiffChunkHeader header
)
{
  /* ckID == "JUNK" */
  assert(header.ckID == WAVE_JUNK);

  /* [u<ckSize> filler] */
  LIBBLU_LPCM_DEBUG(" Skipping %u bytes of 'filler'.\n", header.ckSize);
  SKIP_BYTES(br, header.ckSize, return -1);

  PAD_WORD(br, return -1);
  return 0;
}

static int _parseWaveFmtChunkCommonFields(
  BitstreamReaderPtr br,
  RiffChunkHeader header,
  WaveFmtChunkCommonFields * com_fmt,
  uint32_t * remaining_bytes
)
{
  /* ckID == "fmt " */
  assert(header.ckID == WAVE_FMT);

  if (header.ckSize < 14)
    LIBBLU_LPCM_ERROR_RETURN(
      "WAVE format chunk ('fmt') size error, shall be at least 18 bytes.\n"
    );
  *remaining_bytes = header.ckSize - 14;

  /* [u16 wFormatTag] */
  READ_BYTES(&com_fmt->wFormatTag, br, 2, return -1);
  LIBBLU_LPCM_DEBUG(
    " WAVE format category (wFormatTag): %s (0x%" PRIX16 ").\n",
    WaveFmtFormatTagStr(com_fmt->wFormatTag),
    com_fmt->wFormatTag
  );

  /* [u16 wChannels] */
  READ_BYTES(&com_fmt->wChannels, br, 2, return -1);
  LIBBLU_LPCM_DEBUG(
    " Number of channels (wChannels): %u (0x%" PRIX16 ").\n",
    com_fmt->wChannels,
    com_fmt->wChannels
  );

  /* [u32 nSamplesPerSec] */
  READ_BYTES(&com_fmt->nSamplesPerSec, br, 4, return -1);
  LIBBLU_LPCM_DEBUG(
    " Sampling rate (nSamplesPerSec): %u samples/s (0x%" PRIX32 ").\n",
    com_fmt->nSamplesPerSec,
    com_fmt->nSamplesPerSec
  );

  /* [u32 dwAvgBytesPerSec] */
  READ_BYTES(&com_fmt->dwAvgBytesPerSec, br, 4, return -1);
  LIBBLU_LPCM_DEBUG(
    " Average number of bytes per second (dwAvgBytesPerSec): "
    "%u Bps (0x%" PRIX32 ").\n",
    com_fmt->dwAvgBytesPerSec,
    com_fmt->dwAvgBytesPerSec
  );

  /* [u16 wBlockAlign] */
  READ_BYTES(&com_fmt->wBlockAlign, br, 2, return -1);
  LIBBLU_LPCM_DEBUG(
    " Block alignment (wBlockAlign): "
    "%u byte(s) (0x%" PRIX16 ").\n",
    com_fmt->wBlockAlign,
    com_fmt->wBlockAlign
  );

  return 0;
}

static int _checkWaveFmtChunkCommonFields(
  const WaveFmtChunkCommonFields * com_fmt
)
{

  if (
    com_fmt->wFormatTag != WAVE_FORMAT_PCM
    && com_fmt->wFormatTag != WAVE_FORMAT_EXTENSIBLE
  ) {
    LIBBLU_LPCM_ERROR_RETURN(
      "Unsupported WAVE Format category (wFormatTag == 0x%04" PRIX16 ").\n",
      com_fmt->wFormatTag
    );
  }

  if (!com_fmt->wChannels || 8 < com_fmt->wChannels)
    LIBBLU_LPCM_ERROR_RETURN(
      "Out of range number of channels, expect a value between 1 and 8, "
      "not %u (wFormatTag == 0x%04" PRIX16 ").\n",
      com_fmt->wChannels,
      com_fmt->wChannels
    );

  if (
    com_fmt->nSamplesPerSec != 48000
    && com_fmt->nSamplesPerSec != 96000
    && com_fmt->nSamplesPerSec != 192000
  ) {
    LIBBLU_LPCM_ERROR_RETURN(
      "Unallowed sampling rate, valid values are 48000, 96000 and 192000, "
      "not %u (nSamplesPerSec == 0x%04" PRIX16 ").\n",
      com_fmt->nSamplesPerSec,
      com_fmt->nSamplesPerSec
    );
  }

  if (com_fmt->nSamplesPerSec == 192000 && 6 < com_fmt->wChannels) {
    LIBBLU_LPCM_ERROR_RETURN(
      "The maximum allowed number of channels for the 192 kHz sampling "
      "frequency is 6 (got %u).\n",
      com_fmt->wChannels
    );
  }

  return 0;
}

static int _parseWaveFmtPCMFormatSpecific(
  BitstreamReaderPtr br,
  WaveFmtPCMFormatSpecific * pcm_fmt
)
{

  /* [u16 wBitsPerSample] */
  READ_BYTES(&pcm_fmt->wBitsPerSample, br, 2, return -1);
  LIBBLU_LPCM_DEBUG(
    " Sample size (wBitsPerSample): %u bits (0x%" PRIX16 ").\n",
    pcm_fmt->wBitsPerSample,
    pcm_fmt->wBitsPerSample
  );

  return 0;
}

static int _checkWaveFmtPCMFormatSpecific(
  const WaveFmtChunk * fmt
)
{
  const WaveFmtChunkCommonFields * com_fmt = &fmt->common_fields;
  const WaveFmtPCMFormatSpecific * pcm_fmt = &fmt->fmt_spec.pcm;

  if (pcm_fmt->wBitsPerSample != 16 && pcm_fmt->wBitsPerSample != 24) {
    if (pcm_fmt->wBitsPerSample == 20)
      LIBBLU_LPCM_ERROR_RETURN(
        "Unexpected sample bit-depth, 20 bits audio samples shall be "
        "padded to 24 bits MSB first.\n"
      );

    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected sample bit-depth, supported sample sizes are 16 and 24 bits "
      "(got %u bits).\n",
      pcm_fmt->wBitsPerSample
    );
  }

  if (com_fmt->wChannels * pcm_fmt->wBitsPerSample != com_fmt->wBlockAlign * 8)
    LIBBLU_LPCM_ERROR_RETURN(
      "The number of bits per block does not match the number of channels "
      "and samples bit-depth values "
      "(wChannels * wBitsPerSample (= %u bits) != wBlockAlign (= %u bits)).\n",
      com_fmt->wChannels * pcm_fmt->wBitsPerSample,
      com_fmt->wBlockAlign * 8
    );

  if (com_fmt->nSamplesPerSec * com_fmt->wBlockAlign != com_fmt->dwAvgBytesPerSec)
    LIBBLU_LPCM_ERROR_RETURN(
      "The number of bits per second does not match the sample rate "
      "and audio block alignement values (nSamplesPerSec * wBlockAlign "
      "(= %u bits) != dwAvgBytesPerSec (= %u bits)).\n",
      com_fmt->nSamplesPerSec * com_fmt->wBlockAlign,
      com_fmt->dwAvgBytesPerSec
    );

  return 0;
}

typedef enum {
  KSDATAFORMAT_SUBTYPE_NULL,
  KSDATAFORMAT_SUBTYPE_PCM
} WaveFmtSubFormatType;

static const char * _WaveFmtSubFormatTypeStr(
  WaveFmtSubFormatType SubFormat_type
)
{
  const char * strings[] = {
    "Unknown",
    "PCM Extensible format"
  };

  return strings[SubFormat_type];
}

static WaveFmtSubFormatType _getSubFormatType(
  const LB_DECL_GUID(SubFormat)
)
{

  /* PCM */
  static const LB_DECL_GUID(pcm_sub_format) = WAVE_SUB_FORMAT_PCM;
  if (lb_guid_equal(SubFormat, pcm_sub_format))
    return KSDATAFORMAT_SUBTYPE_PCM;

  return KSDATAFORMAT_SUBTYPE_NULL;
}

static int _parseWaveFmtExtensibleSpecific(
  BitstreamReaderPtr br,
  WaveFmtExtensibleSpecific * ext_fmt
)
{

  /* PCM format specific common */
  if (_parseWaveFmtPCMFormatSpecific(br, &ext_fmt->pcm) < 0)
    return -1;

  /* [u16 cbSize] */
  READ_BYTES(&ext_fmt->cbSize, br, 2, return -1);

  if (0 < ext_fmt->cbSize && 22 != ext_fmt->cbSize)
    LIBBLU_ERROR_RETURN(
      "WAVE extensible format parsing error, "
      "invalid extension size (cbSize = %u).\n",
      ext_fmt->cbSize
    );

  if (0 < ext_fmt->cbSize) {
    /* [u16 wValidBitsPerSample/wSamplesPerBlock] */
    READ_BYTES(&ext_fmt->samples.wValidBitsPerSample, br, 2, return -1);

    if (0 != ext_fmt->pcm.wBitsPerSample)
      LIBBLU_LPCM_DEBUG(
        " Sample bit depth (wBitsPerSample): %u bits (0x%" PRIX16 ").\n",
        ext_fmt->samples.wValidBitsPerSample,
        ext_fmt->samples.wValidBitsPerSample
      );
    else
      LIBBLU_LPCM_DEBUG(
        " Number of samples per block (wSamplesPerBlock): %u (0x%" PRIX16 ").\n",
        ext_fmt->samples.wSamplesPerBlock,
        ext_fmt->samples.wSamplesPerBlock
      );

    /* [v32 dwChannelMask] */
    READ_BYTES(&ext_fmt->dwChannelMask, br, 4, return -1);

    LIBBLU_LPCM_DEBUG(
      " Channels speakers mapping (dwChannelMask): 0x%04" PRIX16 ").\n",
      ext_fmt->dwChannelMask,
      ext_fmt->dwChannelMask
    );

    /* [v128 SubFormat] */
    READ_ARRAY(ext_fmt->SubFormat, br, 16, return -1);

    LIBBLU_LPCM_DEBUG(
      " Audio data type GUID (SubFormat): %s (0x%" LB_PRIGUID ").\n",
      _WaveFmtSubFormatTypeStr(_getSubFormatType(ext_fmt->SubFormat)),
      LB_PRIGUID_EXTENDER(ext_fmt->SubFormat)
    );
  }

  return 0;
}

static int _checkWaveFmtExtensibleSpecific(
  const WaveFmtChunk * fmt
)
{
  const WaveFmtChunkCommonFields  * com_fmt = &fmt->common_fields;
  const WaveFmtExtensibleSpecific * ext_fmt = &fmt->fmt_spec.extensible;

  /* Check PCM common fields */
  if (_checkWaveFmtPCMFormatSpecific(fmt) < 0)
    return -1;

  if (0 < ext_fmt->cbSize) {
    if (0 != ext_fmt->pcm.wBitsPerSample) {
      if (ext_fmt->pcm.wBitsPerSample < ext_fmt->samples.wValidBitsPerSample)
        LIBBLU_LPCM_ERROR_RETURN(
          "Unexpected sample bit depth, value shall not exceed samples size "
          "(wValidBitsPerSample (= %u bits) > wBitsPerSample (= %u bits)).\n",
          ext_fmt->samples.wValidBitsPerSample,
          ext_fmt->pcm.wBitsPerSample
        );

      if (
        ext_fmt->samples.wValidBitsPerSample != 16
        && ext_fmt->samples.wValidBitsPerSample != 20
        && ext_fmt->samples.wValidBitsPerSample != 24
      ) {
        LIBBLU_LPCM_ERROR_RETURN(
          "Unexpected number of precision bits, supported values are "
          "16, 20 and 24 bits (wValidBitsPerSample == %u).\n",
          ext_fmt->samples.wValidBitsPerSample
        );
      }
    }

    unsigned nb_ch_layout = nbChannelsWaveChannelMask(ext_fmt->dwChannelMask);
    if (nb_ch_layout != com_fmt->wChannels)
      LIBBLU_LPCM_ERROR_RETURN(
        "Unexpected speakers layout, the number of channels described in "
        "in the mask is different from the number of audio channels "
        "(nb channels from dwChannelMask (= %u) != wChannels (= %u)).\n",
        nb_ch_layout,
        com_fmt->wChannels
      );

    WaveFmtSubFormatType sf_type = _getSubFormatType(ext_fmt->SubFormat);
    if (KSDATAFORMAT_SUBTYPE_PCM != sf_type)
      LIBBLU_LPCM_ERROR_RETURN(
        "Unexpected WAVE extensible format (SubFormat), only PCM is allowed. "
        "Got %s (GUID = %" LB_PRIGUID ").\n",
        _WaveFmtSubFormatTypeStr(sf_type),
        LB_PRIGUID_EXTENDER(ext_fmt->SubFormat)
      );
  }

  return 0;
}

static int _decodeWaveFmtChunk(
  BitstreamReaderPtr br,
  RiffChunkHeader header,
  WaveFmtChunk * fmt
)
{
  uint32_t rem_bytes;
  if (_parseWaveFmtChunkCommonFields(br, header, &fmt->common_fields, &rem_bytes) < 0)
    return -1;
  if (_checkWaveFmtChunkCommonFields(&fmt->common_fields) < 0)
    return -1;

  switch (fmt->common_fields.wFormatTag) {
    case WAVE_FORMAT_PCM:
      if (rem_bytes != 2)
        LIBBLU_LPCM_ERROR_RETURN(
          "WAVE format chunk ('fmt') PCM format category size error, "
          "shall be equal to 2 bytes.\n"
        );
      if (_parseWaveFmtPCMFormatSpecific(br, &fmt->fmt_spec.pcm) < 0)
        return -1;
      if (_checkWaveFmtPCMFormatSpecific(fmt) < 0)
        return -1;
      break;

    case WAVE_FORMAT_EXTENSIBLE:
      if (rem_bytes < 2)
        LIBBLU_LPCM_ERROR_RETURN(
          "WAVE format chunk ('fmt') Extensible format category size error, "
          "shall be at least 2 bytes.\n"
        );
      if (_parseWaveFmtExtensibleSpecific(br, &fmt->fmt_spec.extensible) < 0)
        return -1;
      if (_checkWaveFmtExtensibleSpecific(fmt) < 0)
        return -1;
      break;
  }

  PAD_WORD(br, return -1);
  return 0;
}

static int _parseWaveFormHeader(
  BitstreamReaderPtr br,
  RiffFormHeader * hdr
)
{
  RiffChunkHeader header;

  if (_parseRiffChunkHeader(br, &header) < 0)
    return -1;

  if (WAVE_RIFF != header.ckID)
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected file magic for a WAVE RIFF file "
      "(got %s, 0x%08" PRIX32 ").\n",
      WaveRIFFChunkIdStr(header.ckID),
      header.ckID
    );

  /* ckID == "RIFF" */
  assert(WAVE_RIFF == header.ckID);

  uint32_t formType;
  READ_BYTES(&formType, br, 4, return -1);

  LIBBLU_LPCM_DEBUG(
    " RIFF form type (formType): %s (0x%08" PRIX32 ").\n",
    RiffFormTypeStr(formType),
    formType
  );

  *hdr = (RiffFormHeader) {
    .header = header,
    .type = formType
  };

  return 0;
}

typedef struct {
  RiffChunkHeader header;
  union {
    WaveFmtChunk fmt;
  };
} WaveChunk;

static int _decodeWaveChunk(
  BitstreamReaderPtr br,
  WaveChunk * chunk,
  uint32_t * allowed_chunks
)
{
  RiffChunkHeader header;

  if (_parseRiffChunkHeader(br, &header) < 0)
    return -1;
  chunk->header = header;

  for (; 0x0 != *allowed_chunks; allowed_chunks++) {
    if (*allowed_chunks == header.ckID)
      break;
  }
  if (0x0 == *allowed_chunks)
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected RIFF chunk type %s (0x%08" PRIX32 ").\n",
      WaveRIFFChunkIdStr(header.ckID),
      header.ckID
    );

  switch (header.ckID) {
    case WAVE_RIFF:
      assert(0);
    case WAVE_FMT:
      return _decodeWaveFmtChunk(br, header, &chunk->fmt);
    case WAVE_JUNK:
      return _decodeWaveJunkChunk(br, header);
    case WAVE_DATA:
      return 0;
  }

  LIBBLU_LPCM_ERROR_RETURN(
    "Broken program, unmanaged RIFF chunk type %s (0x%08" PRIX32 ").\n",
    WaveRIFFChunkIdStr(header.ckID),
    header.ckID
  );
}

static int decodeWaveHeaders(
  BitstreamReaderPtr br,
  WaveFile * chunks
)
{
  uint32_t allowed_chunks[] = {WAVE_DATA, WAVE_JUNK, WAVE_FMT, 0x0};

  RiffFormHeader form;
  if (_parseWaveFormHeader(br, &form) < 0)
    return -1;

  if (form.type != RIFF_WAVE_FORM)
    LIBBLU_LPCM_ERROR_RETURN(
      "RIFF file form type shall be Waveform Audio Format "
      "(got %s, 0x%08" PRIX32 ").\n",
      RiffFormTypeStr(form.type),
      form.type
    );

  WaveChunk chunk;
  do {
    if (_decodeWaveChunk(br, &chunk, allowed_chunks) < 0)
      return -1;
    switch (chunk.header.ckID) {
      case WAVE_FMT:
        chunks->fmt = chunk.fmt;
        chunks->fmt_present = true;
        allowed_chunks[ARRAY_SIZE(allowed_chunks)-2] = 0x0;
        break;
      case WAVE_DATA:
        chunks->data = chunk.header;
        break;
      default:
        break;
    }
  } while (chunk.header.ckID != WAVE_DATA);

  if (!chunks->fmt_present)
    LIBBLU_LPCM_ERROR_RETURN(
      "Invalid WAVE file, missing fmt chunk.\n"
    );

  return 0;
}

static int _getChannelAssignment(
  const WaveFmtChunk * fmt,
  uint8_t * result
)
{
  uint8_t assignment = 0x00;

  /* Assignment according to the number of channels */
  if (2 == fmt->common_fields.wChannels)
    assignment = 0x3; // 2ch, Stereo
  else if (4 == fmt->common_fields.wChannels)
    assignment = 0x4; // 4ch, L,C,R (3/0)
  else if (6 == fmt->common_fields.wChannels)
    assignment = 0x9; // 6ch, L,C,R,Ls,Rs,LFE (3/2 + LFE)
  else if (8 == fmt->common_fields.wChannels)
    assignment = 0xB; // 8ch, L,C,R,Ls,Cs_1,Cs_2,Rs,LFE (3/4 + LFE)

  /* Assignment according to format extensions */
  // TODO

  if (0x00 != assignment) {
    *result = assignment;
    return 0;
  }

  LIBBLU_LPCM_ERROR_RETURN(
    "Unable to determine channel assignment (wChannels = %u).\n",
    fmt->common_fields.wChannels
  );
}

static int _getSamplingFrequency(
  const WaveFmtChunk * fmt,
  uint8_t * result
)
{
  uint8_t sampling_frequency = 0x00;

  if (48000 == fmt->common_fields.nSamplesPerSec)
    sampling_frequency = 0x1; // 48kHz
  if (96000 == fmt->common_fields.nSamplesPerSec)
    sampling_frequency = 0x2; // 96kHz
  if (192000 == fmt->common_fields.nSamplesPerSec)
    sampling_frequency = 0x3; // 192kHz

  if (0x00 != sampling_frequency) {
    *result = sampling_frequency;
    return 0;
  }

  LIBBLU_LPCM_ERROR_RETURN(
    "Unable to determine sampling frequency (nSamplesPerSec = %u).\n",
    fmt->common_fields.nSamplesPerSec
  );
}

static int _getBitsPerSample(
  const WaveFmtChunk * fmt,
  uint8_t * result
)
{
  uint8_t bits_per_sample;

  if (16 == fmt->fmt_spec.pcm.wBitsPerSample)
    bits_per_sample = 0x1; // 16 bits/sample
  if (20 == fmt->fmt_spec.pcm.wBitsPerSample)
    bits_per_sample = 0x2; // 20 bits/sample
  if (24 == fmt->fmt_spec.pcm.wBitsPerSample)
    bits_per_sample = 0x3; // 24 bits/sample

  if (0x00 != bits_per_sample) {
    *result = bits_per_sample;
    return 0;
  }

  LIBBLU_LPCM_ERROR_RETURN(
    "Unable to determine bit depth (wBitsPerSample = %u).\n",
    fmt->fmt_spec.pcm.wBitsPerSample
  );
}

#define LPCM_AUDIO_DATA_HEADER_SIZE  4

static int _setLPCMAudioDataHeader(
  uint8_t header[static LPCM_AUDIO_DATA_HEADER_SIZE],
  const WaveFile * chunks
)
{
  const WaveFmtChunk * fmt = &chunks->fmt;

  uint16_t audio_data_payload_size = (
    (fmt->common_fields.wBlockAlign * fmt->common_fields.nSamplesPerSec)
    / LPCM_PES_FRAMES_PER_SECOND
  );

  uint8_t channel_assignment;
  if (_getChannelAssignment(fmt, &channel_assignment) < 0)
    return -1;

  uint8_t sampling_frequency;
  if (_getSamplingFrequency(fmt, &sampling_frequency) < 0)
    return -1;

  uint8_t bits_per_sample;
  if (_getBitsPerSample(fmt, &bits_per_sample) < 0)
    return -1;

  header[0] = audio_data_payload_size >> 8;
  header[1] = audio_data_payload_size & 0xFF;
  header[2] = (channel_assignment << 4) | sampling_frequency;
  header[3] = bits_per_sample << 6;
  return 0;
}

static int _setPesHeader(
  EsmsHandlerPtr script,
  const WaveFile * chunks,
  unsigned * header_block_idx
)
{
  uint8_t header[LPCM_AUDIO_DATA_HEADER_SIZE]; // LPCM_audio_data_header

  if (_setLPCMAudioDataHeader(header, chunks) < 0)
    return -1;

  if (appendDataBlockEsmsHandler(script, header, 4, header_block_idx) < 0)
    return -1;
  return 0;
}

int analyzeLpcm(
  LibbluESParsingSettings * settings
)
{
  EsmsHandlerPtr esms;
  if (NULL == (esms = createEsmsHandler(ES_AUDIO, settings->options, FMT_SPEC_INFOS_NONE)))
    return -1;

  uint8_t src_file_idx;
  if (appendSourceFileEsmsHandler(esms, settings->esFilepath, &src_file_idx) < 0)
    return -1;

  /* Open input/output files : */
  BitstreamReaderPtr waveInput;
  if (NULL == (waveInput = createBitstreamReaderDefBuf(settings->esFilepath)))
    return -1;

  BitstreamWriterPtr essOutput;
  if (NULL == (essOutput = createBitstreamWriterDefBuf(settings->scriptFilepath)))
    return -1;

  if (writeEsmsHeader(essOutput) < 0)
    return -1;

  /* WAVE RIFF File */
  WaveFile chunks;
  if (decodeWaveHeaders(waveInput, &chunks) < 0)
    return -1;
  const WaveFmtChunk * fmt = &chunks.fmt;

  esms->prop.coding_type = STREAM_CODING_TYPE_LPCM; /* LPCM */
  if (1 == chunks.fmt.common_fields.wChannels)
    esms->prop.audio_format = AUDIO_FORMAT_MONO;
  else if (2 == chunks.fmt.common_fields.wChannels)
    esms->prop.audio_format = AUDIO_FORMAT_STEREO;
  else
    esms->prop.audio_format = AUDIO_FORMAT_MULTI_CHANNEL;

  if (48000 == chunks.fmt.common_fields.nSamplesPerSec)
    esms->prop.sample_rate = SAMPLE_RATE_CODE_48000;
  else if (96000 == chunks.fmt.common_fields.nSamplesPerSec)
    esms->prop.sample_rate = SAMPLE_RATE_CODE_96000;
  else if (192000 == chunks.fmt.common_fields.nSamplesPerSec)
    esms->prop.sample_rate = SAMPLE_RATE_CODE_192000;
  else
    LIBBLU_LPCM_ERROR_RETURN(
      "Invalid audio sampling rate value %u, "
      "shall be one of 48000, 96000 or 1920000.\n",
      chunks.fmt.common_fields.nSamplesPerSec
    );

  uint16_t wBitsPerSample = chunks.fmt.fmt_spec.pcm.wBitsPerSample;
  if (16 != wBitsPerSample && 20 != wBitsPerSample && 24 != wBitsPerSample)
    LIBBLU_LPCM_ERROR_RETURN(
      "Invalid audio bit depth value %u, "
      "shall be one of 16, 20 or 24.\n",
      chunks.fmt.fmt_spec.pcm.wBitsPerSample
    );

  esms->prop.bit_depth = ((wBitsPerSample - 12) >> 2);

  /* Prepare Script Parameters : */

  /* Build PES Header : */
  unsigned pes_hdr_blk_idx;
  if (_setPesHeader(esms, &chunks, &pes_hdr_blk_idx) < 0)
    return -1;

  uint64_t pes_duration = MAIN_CLOCK_27MHZ / LPCM_PES_FRAMES_PER_SECOND;
  uint64_t pts = 0;

  uint8_t sample_size = DIV_ROUND_UP(fmt->fmt_spec.pcm.wBitsPerSample, 8);
  unsigned stream_duration = (
    chunks.data.ckSize / fmt->common_fields.wBlockAlign
  ) / fmt->common_fields.nSamplesPerSec;
  unsigned pes_size = (
    (fmt->common_fields.wBlockAlign * fmt->common_fields.nSamplesPerSec)
    / LPCM_PES_FRAMES_PER_SECOND
  );

  for (size_t remaining_size = chunks.data.ckSize; 0 < remaining_size; ) {
    /* Writing PES frames cutting commands : */
    printFileParsingProgressionBar(waveInput);
    int64_t start_off = tellPos(waveInput);
    // fprintf(stderr, "0x%llX\n", start_off);

    if (
      initAudioPesPacketEsmsHandler(
        esms, false, false, pts, 0
      ) < 0

      || appendAddDataBlockCommandEsmsHandler(
        esms, 0x0, INSERTION_MODE_OVERWRITE, pes_hdr_blk_idx
      ) < 0

      || appendAddPesPayloadCommandEsmsHandler(
        esms,
        src_file_idx, LPCM_AUDIO_DATA_HEADER_SIZE, start_off,
        MIN(pes_size, remaining_size)
      ) < 0

      || appendChangeByteOrderCommandEsmsHandler(
        esms,
        sample_size,
        LPCM_AUDIO_DATA_HEADER_SIZE,
        pes_size
      ) < 0
    )
      return -1;

    if (remaining_size < pes_size) {
      /* Padding LPCM (Addition of a '0' padding at the end of the frame to
        maintain PES fixed length) */
      LIBBLU_INFO("Addition of silence at the end of the stream for packets padding.\n");

      if (
        appendPaddingDataCommandEsmsHandler(
          esms,
          LPCM_AUDIO_DATA_HEADER_SIZE + remaining_size,
          INSERTION_MODE_OVERWRITE,
          pes_size - remaining_size,
          0x00
        )
      )
        return -1;
    }

    if (writePesPacketEsmsHandler(essOutput, esms) < 0)
      return -1;

    size_t frame_size = MIN(remaining_size, pes_size);
    if (skipBytes(waveInput, frame_size) < 0)
      return -1;
    remaining_size -= frame_size;
    pts += pes_duration;
  }

  closeBitstreamReader(waveInput);

  esms->bitrate = (
    fmt->common_fields.nSamplesPerSec
    * fmt->fmt_spec.pcm.wBitsPerSample
    * fmt->common_fields.wChannels
#if 0
    + (LPCM_AUDIO_DATA_HEADER_SIZE * 8 * LPCM_PES_FRAMES_PER_SECOND)
#endif
  );

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf(
    "Codec: %" PRI_LBCS ", %s (%u channels), Sample rate: %u Hz, Bits per sample: %u bits.\n",
    LibbluStreamCodingTypeStr(esms->prop.coding_type),
    AudioFormatCodeStr(esms->prop.audio_format),
    fmt->common_fields.wChannels,
    valueSampleRateCode(esms->prop.sample_rate),
    valueBitDepthCode(esms->prop.bit_depth)
  );
  lbc_printf(
    "Stream Duration: %02u:%02u:%02u\n",
    stream_duration / 60 / 60,
    stream_duration / 60 % 60,
    stream_duration % 60
  );
  lbc_printf("=======================================================================================\n");

  esms->PTS_final = pts;

  if (completePesCuttingScriptEsmsHandler(essOutput, esms) < 0)
    return -1;
  closeBitstreamWriter(essOutput);

  if (updateEsmsFile(settings->scriptFilepath, esms) < 0)
    return -1;
  destroyEsmsHandler(esms);
  return 0;
}
