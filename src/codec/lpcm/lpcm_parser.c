#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "lpcm_parser.h"

static int decodeWaveJunkChunk(
  BitstreamReaderPtr waveInput
)
{
  uint64_t value;

  /* [u32 ckID] // "JUNK" */
  if (readValue64BigEndian(waveInput, 4, &value) < 0)
    return -1;
  if (value != WAVE_JUNK)
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected chunk type identifier for a JUNK chunk "
      "(expect 0x%08X, got 0x%08" PRIX64 ").\n",
      WAVE_JUNK, value
    );

  /* [u32 ckSize] */
  if (readValue64LittleEndian(waveInput, 4, &value) < 0)
    return -1;

  /* [u<ckSize> filler] */
  if (skipBytes(waveInput, value) < 0)
    return -1;
  return 0;
}

static int decodeWaveFmtChunk(
  BitstreamReaderPtr waveInput,
  WaveFmtChunk * param
)
{
  uint64_t value;
  int64_t startOff, parsedCkSize;

  /* [u32 ckID] // "fmt " */
  if (readValue64BigEndian(waveInput, 4, &value) < 0)
    return -1;
  if (value != WAVE_FMT)
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected chunk type identifier for a WAVE Format chunk "
      "(expect 0x%08X, got 0x%08" PRIX64 ").\n",
      WAVE_FMT, value
    );

  /* [u32 ckSize] */
  if (readValue64LittleEndian(waveInput, 4, &value) < 0)
    return -1;
  param->ckSize = value;

  if ((startOff = tellPos(waveInput)) < 0)
    return -1;

  /* [u16 wFormatTag] */
  if (readValue64LittleEndian(waveInput, 2, &value) < 0)
    return -1;
  param->formatTag = value;

  if (
    param->formatTag != WAVE_FORMAT_PCM
    && param->formatTag != WAVE_FORMAT_EXTENSIBLE
  )
    LIBBLU_LPCM_ERROR_RETURN(
      "Unsupported Format category (wFormatTag == 0x%04" PRIX16 ").\n",
      param->formatTag
    );

  /* [u16 wChannels] */
  if (readValue64LittleEndian(waveInput, 2, &value) < 0)
    return -1;
  param->nbChannels = value;

  if (!param->nbChannels || 8 < param->nbChannels)
    LIBBLU_LPCM_ERROR_RETURN(
      "Out of range number of channels, expect a value between 1 and 8, "
      "not %u (wFormatTag == 0x%04" PRIX16 ").\n",
      param->nbChannels,
      param->nbChannels
    );

  /* [u32 nSamplesPerSec] */
  if (readValue64LittleEndian(waveInput, 4, &value) < 0)
    return -1;
  param->sampleRate = value;

  if (
    param->sampleRate != 48000
    && param->sampleRate != 96000
    && param->sampleRate != 192000
  )
    LIBBLU_LPCM_ERROR_RETURN(
      "Unallowed sampling rate, valid values are 48000, 96000 and 192000, "
      "not %u (nSamplesPerSec == 0x%04" PRIX16 ").\n",
      param->sampleRate,
      param->sampleRate
    );

  if (param->sampleRate == 192000 && 6 < param->nbChannels)
    LIBBLU_LPCM_ERROR_RETURN(
      "The maximum allowed number of channels for the 192 kHz sampling "
      "frequency is 6 (got %u).\n",
      param->nbChannels
    );

  /* [u32 dwAvgBytesPerSec] */
  if (readValue64LittleEndian(waveInput, 4, &value) < 0)
    return -1;
  param->bytesPerSec = value;

  /* [u16 wBlockAlign] */
  if (readValue64LittleEndian(waveInput, 2, &value) < 0)
    return -1;
  param->bytesPerBlock = value;

  /* PCM format-specific-fields */
  /* [u16 wBitsPerSample] */
  if (readValue64LittleEndian(waveInput, 2, &value) < 0)
    return -1;
  param->bitsPerSample = value;

  if (param->bitsPerSample != 16 && param->bitsPerSample != 24) {
    if (param->bitsPerSample == 20)
      LIBBLU_LPCM_ERROR_RETURN(
        "Unexpected sample bit-depth, 20 bits audio samples shall be "
        "padded to 24 bits.\n"
      );

    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected sample bit-depth, supported sample sizes are 16 and 24 bits "
      "(got %u bits).\n",
      param->bitsPerSample
    );
  }

  if (param->nbChannels * param->bitsPerSample != param->bytesPerBlock * 8)
    LIBBLU_LPCM_ERROR_RETURN(
      "The number of bits per block does not match the number of channels "
      "and samples bit-depth values "
      "(wChannels * wBitsPerSample (= %u bits) != wBlockAlign (= %u bits)).\n",
      param->nbChannels * param->bitsPerSample,
      param->bytesPerBlock * 8
    );

  if (param->sampleRate * param->bytesPerBlock != param->bytesPerSec)
    LIBBLU_LPCM_ERROR_RETURN(
      "The number of bits per second does not match the sample rate "
      "and audio block alignement values (nSamplesPerSec * wBlockAlign "
      "(= %u bits) != dwAvgBytesPerSec (= %u bits)).\n",
      param->sampleRate * param->bytesPerBlock,
      param->bytesPerSec
    );

  if (param->formatTag == WAVE_FORMAT_EXTENSIBLE) {
    /* PCM Extensible format */
    /* Described in [3] */
    WaveFmtExtensible * ext = &param->extension;

    /* [u16 cbSize] */
    if (readValue64LittleEndian(waveInput, 2, &value) < 0)
      return -1;
    ext->size = value;

    if (0 < ext->size) {
      unsigned nbChFromLayout;

      static const uint8_t waveSubFormat[16] = WAVE_SUB_FORMAT_PCM;

      if (ext->size < 22)
        LIBBLU_LPCM_ERROR_RETURN(
          "Wave format extensible extension size is under the minimum "
          "supported size, extension content is unknown "
          "(expect at least 22 bytes, got %u bytes).\n",
          ext->size
        );

      /* [u16 wValidBitsPerSample] */
      if (readValue64LittleEndian(waveInput, 2, &value) < 0)
        return -1;
      ext->content.validBitsPerSample = value;

      if (param->bitsPerSample < ext->content.validBitsPerSample)
        LIBBLU_LPCM_ERROR_RETURN(
          "Unexpected number of precision bits, value shall not exceed "
          "samples bit-depth "
          "(wValidBitsPerSample (= %u bits) > wBitsPerSample (= %u bits)).\n",
          ext->content.validBitsPerSample,
          param->bitsPerSample
        );

      if (
        ext->content.validBitsPerSample != 16
        && ext->content.validBitsPerSample != 20
        && ext->content.validBitsPerSample != 24
      )
        LIBBLU_LPCM_ERROR_RETURN(
          "Unexpected number of precision bits, supported values are "
          "16, 20 and 24 bits (wValidBitsPerSample == %u).\n",
          ext->content.validBitsPerSample
        );

      /* [u32 dwChannelMask] */
      if (readValue64LittleEndian(waveInput, 4, &value) < 0)
        return -1;
      ext->content.speakersLayoutMask = value;

      nbChFromLayout = nbChannelsWaveChannelMask(
        ext->content.speakersLayoutMask
      );

      if (nbChFromLayout != param->nbChannels)
        LIBBLU_LPCM_ERROR_RETURN(
          "Unexpected speakers layout, the number of channels described in "
          "in the mask is different from the number of audio channels "
          "(nbCh from dwChannelMask (= %u) != nChannels (= %u)).\n",
          nbChFromLayout,
          param->nbChannels
        );

      /* [u128 SubFormat] */
      if (readBytes(waveInput, ext->content.subFormatTag, 16) < 0)
        return -1;

      /* Check for SubFormat == KSDATAFORMAT_SUBTYPE_PCM */
      if (!lb_data_equal(ext->content.subFormatTag, waveSubFormat, 16)) {
        unsigned i;

        LIBBLU_LPCM_ERROR(
          "Unexpected data type GUID, expect PCM (SubFormat == 0x"
        );
        for (i = 0; i < 16; i++)
          LIBBLU_LPCM_ERROR_NH("%02" PRIX8, ext->content.subFormatTag[i]);
        LIBBLU_LPCM_ERROR_RETURN(
          ").\n"
        );
      }
    }
  }

  if ((parsedCkSize = tellPos(waveInput) - startOff) < 0)
    return -1;

  if (param->ckSize < (size_t) parsedCkSize)
    LIBBLU_LPCM_ERROR_RETURN(
      "WAVE format chunk size error, parsed size is greater than "
      "expected one (ckSize (= %zu) < parsed size (= %" PRId64 ").\n",
      param->ckSize,
      parsedCkSize
    );

  return 0;
}

static int decodeWaveHeaders(
  BitstreamReaderPtr waveInput,
  WaveChunksParameters * param
)
{
  uint64_t value;

  /* [v32 ckID] // "RIFF" */
  if (readValue64BigEndian(waveInput, 4, &value) < 0)
    return -1;

  if (value != WAVE_RIFF)
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected chunk type identifier for a RIFF chunk "
      "(expect 0x%08X, got 0x%08" PRIX64 ").\n",
      WAVE_RIFF, value
    );

  /* [u32 cksize] */
  if (readValue64LittleEndian(waveInput, 4, &value) < 0)
    return -1;
  param->fileSize = value;

  /* [u32 WAVEID] // "WAVE" */
  if (readValue64BigEndian(waveInput, 4, &value) < 0)
    return -1;

  if (value != WAVE_WAVE)
    LIBBLU_LPCM_ERROR_RETURN(
      "RIFF file is not a WAVE audio file "
      "(expect 0x%08X, got 0x%08" PRIX64 ").\n",
      WAVE_WAVE, value
    );

  while (nextUint32(waveInput) != WAVE_FMT) {
    /* [u32 ckID] // Junk */
    if (nextUint32(waveInput) == WAVE_JUNK) {
      /* Junk block */
      if (decodeWaveJunkChunk(waveInput) < 0)
        return -1;
      continue;
    }

    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected chunk type identifier "
      "(ckID == 0x%08" PRIX32 ").\n",
      nextUint32(waveInput)
    );
  }

  if (decodeWaveFmtChunk(waveInput, &(param->fmt)) < 0)
    return -1;

  while (nextUint32(waveInput) != WAVE_DATA) {

    /* Optional blockId not defined */
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected chunk type identifier "
      "(ckID == 0x%08" PRIX32 ").\n",
      nextUint32(waveInput)
    );
  }

  /* [v32 ckID] // "DATA" */
  if (readValue64BigEndian(waveInput, 4, &value) < 0)
    return -1;

  if (value != WAVE_DATA)
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected chunk type identifier for a WAVE Data chunk "
      "(expect 0x%08X, got 0x%08" PRIX64 ").\n",
      WAVE_DATA, value
    );

  /* [u32 cksize] */
  if (readValue64LittleEndian(waveInput, 4, &value) < 0)
    return -1;
  param->dataSize = value;

  param->pesPacketLength = param->fmt.bytesPerBlock * (param->fmt.sampleRate / LPCM_PES_FRAMES_PER_SECOND);

  return 0;
}

int analyzeLpcm(
  LibbluESParsingSettings * settings
)
{
  EsmsFileHeaderPtr lpcmInfos = NULL;
  unsigned lpcmHeaderBckIdx, lpcmSourceFileIdx;

  BitstreamReaderPtr waveInput = NULL;
  BitstreamWriterPtr essOutput = NULL;

  WaveChunksParameters waveChunks = {0};

  uint64_t startOff, remainingLength;
  uint64_t lpcmPts, frameDuration;
  long duration;

  uint8_t lpcmPesHeaderData[LPCM_PES_HEADER_LENGTH];

  if (NULL == (lpcmInfos = createEsmsFileHandler(ES_AUDIO, settings->options, FMT_SPEC_INFOS_NONE)))
    return -1;

  /* Pre-gen CRC-32 */
  if (appendSourceFileEsms(lpcmInfos, settings->esFilepath, &lpcmSourceFileIdx) < 0)
    return -1;

  /* Open input/output files : */
  if (NULL == (waveInput = createBitstreamReader(settings->esFilepath, (size_t) READ_BUFFER_LEN)))
    return -1;

  if (NULL == (essOutput = createBitstreamWriter(settings->scriptFilepath, (size_t) WRITE_BUFFER_LEN)))
    return -1;

  if (writeEsmsHeader(essOutput) < 0)
    return -1;

  if (nextUint32(waveInput) == WAVE_RIFF) {
    /* WAVE RIFF File */
    if (decodeWaveHeaders(waveInput, &waveChunks) < 0)
      return -1;
  }
  else
    LIBBLU_LPCM_ERROR_RETURN(
      "Unexpected file magic for a WAVE RIFF file "
      "(expect 0x%08X, got 0x%08" PRIX32 ").\n",
      WAVE_RIFF, nextUint32(waveInput)
    );

  lpcmInfos->prop.codingType = STREAM_CODING_TYPE_LPCM; /* LPCM */

  switch (waveChunks.fmt.nbChannels) {
    case 1:
      lpcmInfos->prop.audioFormat = 0x01;
      break;

    case 2:
      lpcmInfos->prop.audioFormat = 0x03;
      break;

    default:
      lpcmInfos->prop.audioFormat = 0x06;
  }

  switch (waveChunks.fmt.sampleRate) {
    case 48000:
      lpcmInfos->prop.sampleRate = SAMPLE_RATE_CODE_48000;
      break;

    case 96000:
      lpcmInfos->prop.sampleRate = SAMPLE_RATE_CODE_96000;
      break;

    default:
      lpcmInfos->prop.sampleRate = SAMPLE_RATE_CODE_192000;
  }

  lpcmInfos->prop.bitDepth = ((waveChunks.fmt.bitsPerSample - 12) / 4);

  /* Prepare Script Parameters : */

  /* Build PES Header : */
  /* [u16 Audio_data_payload_size] */
  lpcmPesHeaderData[0] = (waveChunks.pesPacketLength >> 8) & 0xFF;
  lpcmPesHeaderData[1] = (waveChunks.pesPacketLength     ) & 0xFF;
  /* [u4 AudioFormat] [u4 SampleRate] */
  lpcmPesHeaderData[2] =
   (((lpcmInfos->prop.audioFormat)  & 0xF) << 4) +
    ((lpcmInfos->prop.sampleRate)   & 0xF);
  /* [u2 BitDepth] [u6 RESERVED] */
  lpcmPesHeaderData[3] =
    ((lpcmInfos->prop.bitDepth      & 0x3) << 6);

  if (
    appendDataBlockEsms(
      lpcmInfos, lpcmPesHeaderData, LPCM_PES_HEADER_LENGTH, &lpcmHeaderBckIdx
    ) < 0
  )
    return -1;

  lpcmPts = 0;
  frameDuration = MAIN_CLOCK_27MHZ / LPCM_PES_FRAMES_PER_SECOND;

  remainingLength = waveChunks.dataSize;
  startOff = tellPos(waveInput);
  /* Define common PES frames cutting parameters : */

  while (0 < remainingLength) {
    /* Writing PES frames cutting commands : */

    if (
      initEsmsAudioPesFrame(
        lpcmInfos, false, false, lpcmPts, 0
      ) < 0

      || appendAddDataBlockCommand(
        lpcmInfos, 0x0, INSERTION_MODE_ERASE, lpcmHeaderBckIdx
      ) < 0

      || appendAddPesPayloadCommand(
        lpcmInfos,
        lpcmSourceFileIdx, LPCM_PES_HEADER_LENGTH, startOff,
        MIN(waveChunks.pesPacketLength, remainingLength)
      ) < 0

      || appendChangeByteOrderCommand(
        lpcmInfos,
        (uint8_t) ceil((float) waveChunks.fmt.bitsPerSample / 8),
        LPCM_PES_HEADER_LENGTH, waveChunks.pesPacketLength
      ) < 0
    )
      return -1;

    if (remainingLength < waveChunks.pesPacketLength) {
      /* Padding LPCM (Addition of a '0' padding at the end of the frame to maintain PES fixed length) */
      LIBBLU_INFO("Addition of silence at the end of the stream for packets padding.\n");

      /* Setup EsmsAddPaddingCommand : */
      if (
        appendPaddingDataCommand(
          lpcmInfos,
          LPCM_PES_HEADER_LENGTH + remainingLength,
          INSERTION_MODE_ERASE,
          waveChunks.pesPacketLength - remainingLength,
          0x00
        )
      )
        return -1;
    }

    if (writeEsmsPesPacket(essOutput, lpcmInfos) < 0)
      return -1;

    lpcmPts += frameDuration;

    /* if (skipBytes(waveInput, MIN(waveChunks.pesPacketLength, remainingLength)) < 0) */
      /* return -1; */

    remainingLength -= MIN(remainingLength, waveChunks.pesPacketLength);
    startOff += waveChunks.pesPacketLength;
  }

  closeBitstreamReader(waveInput);

  /* [u8 endMarker] */
  if (writeByte(essOutput, ESMS_SCRIPT_END_MARKER) < 0)
    return -1;

  duration = (waveChunks.dataSize / waveChunks.fmt.bytesPerBlock) / waveChunks.fmt.sampleRate;

  lpcmInfos->bitrate = (
    waveChunks.fmt.sampleRate
    * waveChunks.fmt.bitsPerSample
    * waveChunks.fmt.nbChannels
#if 0
    + (LPCM_PES_HEADER_LENGTH * 8 * LPCM_PES_FRAMES_PER_SECOND)
#endif
  );

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf(
    "Codec: Audio/LPCM, Nb channels: %u, Sample rate: %u kHz, "
    "Bits per sample: %u bits.\n",
    waveChunks.fmt.nbChannels,
    waveChunks.fmt.sampleRate / 1000,
    (
      (!waveChunks.fmt.extension.size) ?
        waveChunks.fmt.bitsPerSample
      :
        waveChunks.fmt.extension.content.validBitsPerSample
    )
  );
  lbc_printf("Stream Duration: %02ld:%02ld:%02ld\n", duration / 60 / 60, duration / 60 % 60, duration % 60);
  lbc_printf("=======================================================================================\n");

  lpcmInfos->endPts = lpcmPts;

  if (addEsmsFileEnd(essOutput, lpcmInfos) < 0)
    return -1;
  closeBitstreamWriter(essOutput);

  if (updateEsmsHeader(settings->scriptFilepath, lpcmInfos) < 0)
    return -1;
  destroyEsmsFileHandler(lpcmInfos);
  return 0;
}
