#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "dts_dtshd_file.h"

const char * dtshdChunkIdStr(
  const DtsHdChunkMagic id
)
{
  switch (id) {
    case DTS_HD_DTSHDHDR:
      return "DTS-HD File Header Chunk";
    case DTS_HD_FILEINFO:
      return "DTS-HD File Info Data Chunk";
    case DTS_HD_CORESSMD:
      return "DTS-HD Core Substream Metadata Chunk";
    case DTS_HD_EXTSS_MD:
      return "DTS-HD Extension Substream Metadata Chunk";
    case DTS_HD_AUPR_HDR:
      return "DTS-HD Audio Presentation Header Metadata Chunk";
    case DTS_HD_AUPRINFO:
      return "DTS-HD Audio Presentation Information Text Chunk";
    case DTS_HD_NAVI_TBL:
      return "DTS-HD Navigation Metadata Chunk";
    case DTS_HD_BITSHVTB:
      return "DTS-HD Bit Sheving Metadata Chunk";
    case DTS_HD_STRMDATA:
      return "DTS-HD Encoded Stream Data Chunk";
    case DTS_HD_TIMECODE:
      return "DTS-HD Timecode Data Chunk";
    case DTS_HD_BUILDVER:
      return "DTS-HD BuildVer Data Chunk";
    case DTS_HD_BLACKOUT:
      return "DTS-HD Encoded Blackout Data Chunk";
    case DTS_HD_BRANCHPT:
      return "DTS-HD Branch Point Metadata Chunk";
  }

  return "DTS-HD Unknown Chunk";
}

int decodeDtshdHeaderChunk(
  BitstreamReaderPtr file,
  DtshdFileHeaderChunk * param
)
{
  uint64_t value;
  int64_t startOff;

  static const unsigned refClockCodes[] = {
    32000,
    44100,
    48000,
    0
  };

  static const float tcFrameRateCodes[8][2] = {
    {           0., 0},
    {24000. / 1001, 0},
    {          24., 0},
    {          25., 0},
    {30000. / 1001, 1},
    {30000. / 1001, 0},
    {          30., 1},
    {          30., 0}
  };

  assert(NULL != param);

  /* [v64 chunkId] // 'DTSHDHDR' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_DTSHDHDR)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD File Header Chunk identifier (receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 Hdr_Byte_Size] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Header size (Hdr_Byte_Size): %" PRId64 " bytes.\n",
    param->length
  );

  startOff = tellPos(file);

  /* [u32 Hdr_Version] */
  if (readValue64BigEndian(file, 4, &value) < 0)
    return -1;
  param->version = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Header version (Hdr_Version): %u (%s).\n",
    param->version,
    (param->version == DTS_HD_HEADER_SUPPORTED_VER) ? "Valid" : "Unsupported"
  );

  if (param->version != DTS_HD_HEADER_SUPPORTED_VER)
    LIBBLU_DTS_ERROR_RETURN(
      "Unsupported DTS-HD file header version (%u).\n",
      param->version
    );

  /** [v40 Time_Code]
   * -> u2  - RefClockCode
   * -> v6  - reserved
   * -> u32 - TimeStamp
   */
  /* [u2 RefClockCode] */
  if (readBits64(file, &value, 2) < 0)
    return -1;
  param->refClockCode = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Reference clock code (RefClockCode): 0x%" PRIX8 ".\n",
    param->refClockCode
  );

  /* => RefClock */
  if (0 == (param->refClock = refClockCodes[param->refClockCode]))
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use ('refClockCode' == %u).\n",
      param->refClockCode
    );

  LIBBLU_DTS_DEBUG_DTSHD(
    "  => Reference clock: %u Hz.\n",
    param->refClock
  );

  /* [v6 reserved] */
  if (skipBits(file, 6) < 0)
    return -1;

  /* [u32 TimeStamp] */
  if (readBits64(file, &value, 32) < 0)
    return -1;
  param->timeStamp = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Timestamp (TimeStamp): %u samples (0x%x).\n",
    param->timeStamp,
    param->timeStamp
  );

  /** [v8 TC_Frame_Rate]
   * -> v4  - reserved
   * -> u4  - frameRateCode
   */
  if (skipBits(file, 4) < 0)
    return -1;
  if (readBits64(file, &value, 4) < 0)
    return -1;
  param->frameRateCode = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Timecode frame-rate code (TC_Frame_Rate): 0x%" PRIX8 ".\n",
    param->frameRateCode
  );

  if (param->frameRateCode <= 0x7) {
    param->frameRate = tcFrameRateCodes[param->frameRateCode][0];
    param->dropFrame = tcFrameRateCodes[param->frameRateCode][1];
  }
  else
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use ('frameRateCode' == 0x%" PRIX8 ").\n",
      param->frameRateCode
    );

  if (param->frameRateCode == 0x0)
    LIBBLU_DTS_DEBUG_DTSHD(" => Timecode rate: NOT_INDICATED (0x0).\n");
  else
    LIBBLU_DTS_DEBUG_DTSHD(
      " => Timecode rate: %.3f FPS %s(0x%" PRIx8 ").\n",
      param->frameRate,
      (param->dropFrame) ? "Drop " : "",
      param->frameRateCode
    );

  LIBBLU_DTS_DEBUG_DTSHD(" Bitstream Metadata (Bitw_Stream_Metadata):\n");

 /** [v16 Bitw_Stream_Metadata]
  * -> v11: reserved
  * -> b1 : extSubStreamPres
  * -> b1 : coreSubStreamPres
  * -> b1 : navigationTableIndicatorPres
  * -> b1 : peakBitRateSmoothingPerformed
  * -> b1 : constantBitRate
  */
  if (skipBits(file, 11) < 0)
    return -1;

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->extSubstreamPresent = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Extension Substream: %s (0x%x);\n",
    BOOL_PRESENCE(param->extSubstreamPresent),
    param->extSubstreamPresent
  );

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->coreSubstreamPresent = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Core Substream: %s (0x%x);\n",
    BOOL_PRESENCE(param->coreSubstreamPresent),
    param->coreSubstreamPresent
  );

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->navigationTableIndicatorPresent = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Navigation Table Indicator: "
    "%s (0x%x);\n",
    BOOL_PRESENCE(param->navigationTableIndicatorPresent),
    param->navigationTableIndicatorPresent
  );

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->peakBitRateSmoothingPerformed = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Peak Bit-rate (PBR) Smoothing: %s (0x%x);\n",
    (param->peakBitRateSmoothingPerformed) ? "Performed" : "Not performed",
    param->peakBitRateSmoothingPerformed
  );

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->variableBitRate = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "  -> Bitrate mode: %s (0x%x).\n",
    (param->variableBitRate) ? "Variable" : "Constant",
    param->variableBitRate
  );

  /* [u8 Num_Audio_Presentations] */
  if (readBits64(file, &value, 8) < 0)
    return -1;
  param->nbAudioPresentations = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Number of Audio Presentations (Num_Audio_Presentations): "
    "%u presentation(s) (0x%x).\n",
    param->nbAudioPresentations,
    param->nbAudioPresentations
  );

  /* [u8 Number_Of_Ext_Sub_Streams] */
  if (readBits64(file, &value, 8) < 0)
    return -1;

  if (param->extSubstreamPresent)
    param->nbExtensionSubstreams = value + 1;
  else
    param->nbExtensionSubstreams = 0;

  LIBBLU_DTS_DEBUG_DTSHD(
    " Number of Extension Substreams (Number_Of_Ext_Sub_Streams): "
    "%u substream(s) (0x%x).\n",
    param->nbExtensionSubstreams,
    param->nbExtensionSubstreams
  );

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;
  return 0;
}

int decodeDtshdFileInfoChunk(
  BitstreamReaderPtr file,
  DtshdFileInfo * param
)
{
  int64_t startOff;
  uint64_t value;
  size_t stringLength;
  char cara, * writingPointer;

  /* [v64 chunkId] // 'FILEINFO' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_FILEINFO)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD File Info Chunk identifier (receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [vn buildVerString] */
  stringLength = param->length;
  param->string = NULL;
  if (NULL == (param->string = (char *) calloc(stringLength, sizeof(char))))
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  writingPointer = param->string;
  while ((size_t) (writingPointer - param->string) < stringLength) {
    if (readByte(file, (uint8_t *) &cara) < 0)
      return -1;

    if (cara == '\n') {
      *(writingPointer++) = '\\';
      *(writingPointer++) = 'n';
    }
    else
      *(writingPointer++) = cara;

    if (cara == '\0')
      break; /* End of string */
  }

  LIBBLU_DTS_DEBUG_DTSHD(
    "String: '%s'.\n",
    param->string
  );

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdStreamDataChunk(
  BitstreamReaderPtr file,
  DtshdStreamData * param,
  bool inStreamData
)
{
  uint64_t value;

  if (inStreamData) {
    /* lbc_printf("%" PRId64 "\n", param->endOffset - tellPos(file)); */
    if (param->endOffset <= tellPos(file)) {
      if (param->endOffset < tellPos(file))
        LIBBLU_DTS_ERROR_RETURN(
          "DTS-HD file chunk length error (longer than expected).\n"
        );

      /* Align to 32-bit boundary */
      while (tellPos(file) % 4) {
        if (skipBytes(file, 1) < 0)
          return -1;
      }

      return 0;
    }

    /* Read DTS audio frames from payload. */
    return 1;
  }

  /* [v64 chunkId] // 'STRMDATA' */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_STRMDATA)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Encoded Stream Data Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->endOffset = tellPos(file) + value;

  if (param->endOffset == tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (unexpected empty stream data chunk).\n"
    );

  /* [vn DtshdEncodedDataPayload] */
  /* Parsed outside of function. */
  return 1;
}

int decodeDtshdCoreSubStreamMetaChunk(
  BitstreamReaderPtr file,
  DtshdCoreSubStrmMeta * param
)
{
  int64_t startOff;
  uint64_t value;

  /* [v64 chunkId] // 'CORESSMD' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_CORESSMD)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Core Substream Metadata Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [u24 sampleRateValue] */
  if (readValue64BigEndian(file, 3, &value) < 0)
    return -1;
  param->sampleRate = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Sample rate: %u Hz.\n",
    param->sampleRate
  );

  /* [u16 bitRateValueKbps] */
  if (readValue64BigEndian(file, 2, &value) < 0)
    return -1;
  param->bitRateKbps = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Bit-rate: %u kbits/s.\n",
    param->bitRateKbps
  );

  /* [v16 channelMask] */
  if (readValue64BigEndian(file, 2, &value) < 0)
    return -1;
  param->channelMask = value;

  LIBBLU_DTS_DEBUG_DTSHD("Channel mask:");
  if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_DTSHD))
    dcaExtChMaskStrPrintFun(param->channelMask, lbc_deb_printf);
  LIBBLU_DTS_DEBUG_DTSHD_NH(" (0x%" PRIx16 ").\n", param->channelMask);

  /* [u32 framePayloadInBytes] */
  if (readValue64BigEndian(file, 4, &value) < 0)
    return -1;
  param->framePayloadLength = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Frame payload size: %zu byte(s).\n",
    param->framePayloadLength
  );

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdExtSubStreamMetaChunk(
  BitstreamReaderPtr file,
  DtshdExtSubStrmMeta * param,
  bool isVariableBitRate
)
{
  int64_t startOff;
  uint64_t value;

  /* [v64 chunkId] // 'EXTSS_MD' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_EXTSS_MD)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Extension Substream Metadata Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [u24 averageBitRateKbps] */
  if (readValue64BigEndian(file, 3, &value) < 0)
    return -1;
  param->avgBitRateKbps = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Average bit-rate: %u kbits/s.\n",
    param->avgBitRateKbps
  );

  if (isVariableBitRate) {
    /* [u24 peakBitRateKbps] */
    if (readValue64BigEndian(file, 3, &value) < 0)
      return -1;
    param->peakBitRateKbps = value;

    LIBBLU_DTS_DEBUG_DTSHD(
      "Peak bit-rate: %u kbits/s.\n",
      param->peakBitRateKbps
    );

    /* [u16 peakBitRateSmoothingBufSizeKb] */
    if (readValue64BigEndian(file, 2, &value) < 0)
      return -1;
    param->peakBitRateSmoothingBufSizeKb = value;

    LIBBLU_DTS_DEBUG_DTSHD(
      "Peak bit-rate Smoothing Buffer size: %u kbits.\n",
      param->peakBitRateSmoothingBufSizeKb
    );

    param->framePayloadLength = 0;
  }
  else {
    /* [u32 framePayloadInBytes] */
    if (readValue64BigEndian(file, 4, &value) < 0)
      return -1;
    param->framePayloadLength = value;

    LIBBLU_DTS_DEBUG_DTSHD(
      "Frame payload size: %zu byte(s).\n",
      param->framePayloadLength
    );

    param->peakBitRateKbps = 0;
    param->peakBitRateSmoothingBufSizeKb = 0;
  }

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdAudioPresHeaderMetaChunk(
  BitstreamReaderPtr file,
  DtshdAudioPresPropHeaderMeta * param
)
{
  int64_t startOff;
  uint64_t value;

  /* [v64 chunkId] // 'AUPR-HDR' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_AUPR_HDR)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Audio Presentation Header Metadata Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [u8 presentationIdx] */
  if (readBits64(file, &value, 8) < 0)
    return -1;
  param->presentationIndex = value;

 /** [v16 audioPresMetadata]
  * -> v12: reserved
  * -> b1 : lowBitRateCodingComponentPres
  * -> b1 : losslessCodingComponentPres
  * -> b1 : backwardCompatibleCoreLocation
  * -> b1 : backwardCompatibleCorePres
  */
  if (skipBits(file, 12) < 0)
    return -1;

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->lbrComponentPresent = value;

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->losslessComponentPresent = value;

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->backwardCompatibleCoreLocation = value;

  if (readBits64(file, &value, 1) < 0)
    return -1;
  param->backwardCompatibleCorePresent = value;

  LIBBLU_DTS_DEBUG_DTSHD("Audio Presentation meta-data:\n");

  LIBBLU_DTS_DEBUG_DTSHD(
    " -> Backward compatible Core: %s (0x%x);\n",
    BOOL_PRESENCE(param->backwardCompatibleCorePresent),
    param->backwardCompatibleCorePresent
  );

  if (param->backwardCompatibleCorePresent) {
    LIBBLU_DTS_DEBUG_DTSHD(
      " -> Backward compatible Core location: %s (0x%x);\n",
      (param->backwardCompatibleCoreLocation) ? "Extension Substream" : "Core Substream",
      param->backwardCompatibleCoreLocation
    );
  }

  LIBBLU_DTS_DEBUG_DTSHD(
    " -> Lossless Component: %s (0x%x);\n",
    BOOL_PRESENCE(param->losslessComponentPresent),
    param->losslessComponentPresent
  );

  LIBBLU_DTS_DEBUG_DTSHD(
    " -> LBR Component: %s (0x%x);\n",
    BOOL_PRESENCE(param->lbrComponentPresent),
    param->lbrComponentPresent
  );

  /* [u24 maxSampleRate] */
  if (readValue64BigEndian(file, 3, &value) < 0)
    return -1;
  param->maxSampleRate = value;

  LIBBLU_DTS_DEBUG_DTSHD("Max Sample Rate: %u Hz.\n", param->maxSampleRate);

  /* [u32 totalNbFrames] */
  if (readValue64BigEndian(file, 4, &value) < 0)
    return -1;
  param->nbFrames = value;

  LIBBLU_DTS_DEBUG_DTSHD("Number of Frames in Total: %u frame(s).\n", param->nbFrames);

  /* [u24 maxNbSamplesPerFrame] */
  if (readValue64BigEndian(file, 2, &value) < 0)
    return -1;
  param->maxNbSamplesPerFrame = value;

  /* [u40 maxNbSamplesOrigAudioPerFrame] */
  if (readValue64BigEndian(file, 5, &value) < 0)
    return -1;
  param->maxNbSamplesOrigAudioPerFrame = value;

  LIBBLU_DTS_DEBUG_DTSHD("Max Number of Samples per frame:\n");
  LIBBLU_DTS_DEBUG_DTSHD(" -> In stream: %u sample(s)/frame.\n", param->maxNbSamplesPerFrame);
  LIBBLU_DTS_DEBUG_DTSHD(" -> In original audio: %u sample(s)/frame.\n", param->maxNbSamplesOrigAudioPerFrame);

  /* [v16 channelsMask] */
  if (readValue64BigEndian(file, 2, &value) < 0)
    return -1;
  param->channelMask = value;

  LIBBLU_DTS_DEBUG_DTSHD("Channel mask:");
  if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_DTSHD))
    dcaExtChMaskStrPrintFun(param->channelMask, lbc_deb_printf);
  LIBBLU_DTS_DEBUG_DTSHD_NH(" (0x%" PRIx16 ").\n", param->channelMask);

  /* [u16 codecDelay] */
  if (readValue64BigEndian(file, 2, &value) < 0)
    return -1;
  param->codecDelay = value;

  param->nbSkippedFrames =
    (param->codecDelay + (param->maxNbSamplesPerFrame / 2))
    / param->maxNbSamplesPerFrame
  ;

  LIBBLU_DTS_DEBUG_DTSHD("Codec delay: %u samples.\n", param->codecDelay);
  LIBBLU_DTS_DEBUG_DTSHD(" -> %u audio frame(s).\n", param->nbSkippedFrames);

  param->extCorePresent = (
    param->backwardCompatibleCorePresent &&
    param->backwardCompatibleCoreLocation
  );

  if (param->extCorePresent) {
    LIBBLU_DTS_DEBUG_DTSHD("Core present in Extension Substream:\n");

    /* [u24 coreMaxSampleRate] */
    if (readValue64BigEndian(file, 3, &value) < 0)
      return -1;
    param->extCore.sampleRate = value;

    LIBBLU_DTS_DEBUG_DTSHD(" -> Sample Rate: %u Hz.\n", param->extCore.sampleRate);

    /* [u16 coreBitRate] */
    if (readValue64BigEndian(file, 2, &value) < 0)
      return -1;
    param->extCore.bitRateKbps = value;

    LIBBLU_DTS_DEBUG_DTSHD(" -> Core Bit-rate: %u k.\n", param->extCore.bitRateKbps);

    /* [u16 coreChannelMask] */
    if (readValue64BigEndian(file, 2, &value) < 0)
      return -1;
    param->extCore.channelMask = value;

    LIBBLU_DTS_DEBUG_DTSHD("Channel mask:");
    if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_DTSHD))
      dcaExtChMaskStrPrintFun(param->extCore.channelMask, lbc_deb_printf);
    LIBBLU_DTS_DEBUG_DTSHD_NH(" (0x%" PRIx16 ").\n", param->extCore.channelMask);
  }

  if (param->losslessComponentPresent) {
    LIBBLU_DTS_DEBUG_DTSHD("Lossless Extension present in Extension Substream:\n");

    /* [u8 lsbTrimPercent] */
    if (readValue64BigEndian(file, 1, &value) < 0)
      return -1;
    param->losslessLsbTrim = value;

    LIBBLU_DTS_DEBUG_DTSHD(" -> LSB Trim: %u bit(s).\n", param->losslessLsbTrim);
  }

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdAudioPresInfoChunk(
  BitstreamReaderPtr file,
  DtshdAudioPresText * param
)
{
  int64_t startOff;
  uint64_t value;
  size_t stringLength;
  char cara, * writingPointer;

  /* [v64 chunkId] // 'AUPRINFO' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_AUPRINFO)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Audio Presentation Information Text Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [u8 audioPresTextIndex] */
  if (readValue64BigEndian(file, 1, &value) < 0)
    return -1;
  param->index = value;

  /* [vn audioPresTextString] */
  stringLength = param->length;
  param->string = NULL;
  if (NULL == (param->string = (char *) calloc(stringLength, sizeof(char))))
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  writingPointer = param->string;
  while ((size_t) (writingPointer - param->string) < stringLength) {
    if (readByte(file, (uint8_t *) &cara) < 0)
      return -1;

    if (cara == '\n') {
      *(writingPointer++) = '\\';
      *(writingPointer++) = 'n';
    }
    else
      *(writingPointer++) = cara;

    if (cara == '\0')
      break; /* End of string */
  }

  LIBBLU_DTS_DEBUG_DTSHD(
    "String: '%s'.\n",
    param->string
  );

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdNavigationMetaChunk(
  BitstreamReaderPtr file,
  DtshdNavMeta * param
)
{
  int64_t startOff;
  uint64_t value;

  /* [v64 chunkId] // 'NAVI_TBL' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_NAVI_TBL)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Navigation Metadata Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* Not used */

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdTimecodeChunk(
  BitstreamReaderPtr file,
  DtshdTimecode * param
)
{
  int64_t startOff;
  uint64_t value;

  /* [v64 chunkId] // 'TIMECODE' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_TIMECODE)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Timecode Data Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [u32 timeCodeClock] */
  if (readValue64BigEndian(file, 4, &value) < 0)
    return -1;
  param->clockFreq = value;

  LIBBLU_DTS_DEBUG_DTSHD("Clock: %u Hz.\n", param->clockFreq);

  /* [v4 reserved] */
  if (skipBits(file, 4) < 0)
    return -1;

  /* [u4 timeCodeFrameRateCode] */
  if (readBits64(file, &value, 4) < 0)
    return -1;
  param->frameRateCode = value;

  param->dropFrame = false;
  switch (param->frameRateCode) {
    case 0x0:
      param->frameRate = 0; /* NOT_INDICATED */
      break;

    case 0x1:
      param->frameRate = (float) 24000 / 1001;
      break;

    case 0x2:
      param->frameRate = 24;
      break;

    case 0x3:
      param->frameRate = 25;
      break;

    case 0x4:
      param->dropFrame = true;
      param->frameRate = (float) 30000 / 1001;
      break;

    case 0x5:
      param->frameRate = (float) 30000 / 1001;
      break;

    case 0x6:
      param->dropFrame = true;
      param->frameRate = 30;
      break;

    case 0x7:
      param->frameRate = 30;
      break;

    default:
      LIBBLU_DTS_DEBUG_DTSHD(
        "Frame rate: Reserved value (0x%" PRIx8 ").\n",
        param->frameRateCode
      );

      LIBBLU_DTS_ERROR_RETURN(
        "Reserved value in use ('timeCodeFrameRateCode' == 0x%" PRIX8 ").\n",
        param->frameRateCode
      );
  }

  /* TODO */

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdBuildVerChunk(
  BitstreamReaderPtr file,
  DtshdBuildVer * param
)
{
  int64_t startOff;
  uint64_t value;
  size_t stringLength;
  char cara, * writingPointer;

  /* [v64 chunkId] // 'BUILDVER' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_BUILDVER)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD BuildVer Data Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [vn buildVerString] */
  stringLength = param->length;
  param->string = NULL;
  if (NULL == (param->string = (char *) calloc(stringLength, sizeof(char))))
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  writingPointer = param->string;
  while ((size_t) (writingPointer - param->string) < stringLength) {
    if (readByte(file, (uint8_t *) &cara) < 0)
      return -1;

    if (cara == '\n') {
      *(writingPointer++) = '\\';
      *(writingPointer++) = 'n';
    }
    else
      *(writingPointer++) = cara;

    if (cara == '\0')
      break; /* End of string */
  }

  LIBBLU_DTS_DEBUG_DTSHD(
    "String: '%s'.\n",
    param->string
  );

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdBlackoutChunk(
  BitstreamReaderPtr file,
  DtshdBlackout * param
)
{
  uint64_t value;
  int64_t startOff;

  /* [v64 chunkId] // 'BLACKOUT' Header */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  if (value != DTS_HD_BLACKOUT)
    LIBBLU_DTS_ERROR_RETURN(
      "Expected a DTS-HD Encoded Blackout Data Chunk identifier "
      "(receive: %s).\n",
      dtshdChunkIdStr(value)
    );

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_DTS_DEBUG_DTSHD(
    "Chunk length: 0x%" PRIx64 ".\n",
    param->length
  );

  startOff = tellPos(file);

  /* [vn blackoutFrame] */
  param->frame = (uint8_t *) malloc(param->length * sizeof(uint8_t));
  if (NULL == param->frame)
    LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

  if (readBytes(file, param->frame, param->length) < 0)
    return -1;

  /* [vn reserved/padding] */
  if (startOff + param->length < tellPos(file))
    LIBBLU_DTS_ERROR_RETURN(
      "DTS-HD file chunk length error (shorter than expected).\n"
    );

  if (skipBytes(file, param->length - (tellPos(file) - startOff)) < 0)
    return -1;

  return 0;
}

int decodeDtshdUnsupportedChunk(
  BitstreamReaderPtr file
)
{
  uint64_t value;

  /* [v64 chunkId] // Unknown */
  if (skipBytes(file, 8) < 0)
    return -1;

  /* [u64 chunkLength] */
  if (readValue64BigEndian(file, 8, &value) < 0)
    return -1;

  LIBBLU_DTS_DEBUG_DTSHD("Chunk length: " PRIu64 " bytes.\n", value);

  /* [vn reserved/padding] */
  return skipBytes(file, (size_t) value);
}

bool isDtshdFile(
  BitstreamReaderPtr file
)
{
  return nextUint64(file) == DTS_HD_DTSHDHDR;
}

DtshdFileHandlerPtr createDtshdFileHandler(
  void
)
{
  return (DtshdFileHandlerPtr) calloc(
    1, sizeof(DtshdFileHandler)
  );
}

void destroyDtshdFileHandler(
  DtshdFileHandlerPtr handle
)
{
  if (NULL == handle)
    return;

  if (handle->fileInfoPresent)
    free(handle->fileInfo.string);
  if (handle->audioPresTextPresent)
    free(handle->audioPresText.string);
  if (handle->buildVerPresent)
    free(handle->buildVer.string);
  if (handle->blackoutPresent)
    free(handle->blackout.frame);
  free(handle);
}

int decodeDtshdFileChunk(
  BitstreamReaderPtr file,
  DtshdFileHandlerPtr handle,
  bool skipChecks
)
{
  int ret;

  uint64_t magic;
  bool * presenceFlag;

  assert(NULL != file);
  assert(NULL != handle);

  if (handle->inStreamData) {
    ret = decodeDtshdStreamDataChunk(
      file,
      &handle->streamData,
      true
    );

    if (ret == 0)
      handle->inStreamData = false;
    return ret;
  }

  magic = nextUint64(file);

  LIBBLU_DTS_DEBUG_DTSHD(
    "0x%08" PRIX64 " === %s (0x%016" PRIX64 ") ===\n",
    tellPos(file), dtshdChunkIdStr(magic), magic
  );

  switch (magic) {
    case DTS_HD_DTSHDHDR:
      presenceFlag = &handle->headerPresent;
      ret = decodeDtshdHeaderChunk(file, &handle->header);
      break;

    case DTS_HD_FILEINFO:
      presenceFlag = &handle->fileInfoPresent;
      ret = decodeDtshdFileInfoChunk(file, &handle->fileInfo);
      break;

    case DTS_HD_CORESSMD:
      presenceFlag = &handle->coreMetadataPresent;
      ret = decodeDtshdCoreSubStreamMetaChunk(file, &handle->coreMetadata);
      break;

    case DTS_HD_EXTSS_MD:
      if (!handle->headerPresent)
        LIBBLU_DTS_ERROR_RETURN(
          "Expect presence of DTS-HD file header before EXTSS_MD chunk.\n"
        );
      presenceFlag = &handle->extMetadataPresent;
      ret = decodeDtshdExtSubStreamMetaChunk(
        file,
        &handle->extMetadata,
        handle->header.variableBitRate
      );
      break;

    case DTS_HD_AUPR_HDR:
      presenceFlag = &handle->audioPresHeaderMetadataPresent;
      ret = decodeDtshdAudioPresHeaderMetaChunk(
        file, &handle->audioPresHeaderMetadata
      );
      break;

    case DTS_HD_AUPRINFO:
      presenceFlag = &handle->audioPresTextPresent;
      ret = decodeDtshdAudioPresInfoChunk(file, &handle->audioPresText);
      break;

    case DTS_HD_NAVI_TBL:
      presenceFlag = &handle->navigationMetadataPresent;
      ret = decodeDtshdNavigationMetaChunk(
        file, &handle->navigationMetadata
      );
      break;

    case DTS_HD_STRMDATA:
      presenceFlag = &handle->streamDataPresent;
      ret = decodeDtshdStreamDataChunk(
        file, &handle->streamData, false
      );
      handle->inStreamData = true;
      break;

    case DTS_HD_TIMECODE:
      presenceFlag = &handle->timecodePresent;
      ret = decodeDtshdTimecodeChunk(
        file, &handle->timecode
      );
      break;

    case DTS_HD_BUILDVER:
      presenceFlag = &handle->buildVerPresent;
      ret = decodeDtshdBuildVerChunk(
        file, &handle->buildVer
      );
      break;

    case DTS_HD_BLACKOUT:
      presenceFlag = &handle->blackoutPresent;
      ret = decodeDtshdBlackoutChunk(
        file, &handle->blackout
      );
      break;

    case DTS_HD_BITSHVTB:
    case DTS_HD_BRANCHPT:
    default:
      presenceFlag = NULL;
      ret = decodeDtshdUnsupportedChunk(file);
      break;
  }

  if (skipChecks)
    presenceFlag = NULL;

  if (0 <= ret && NULL != presenceFlag) {
    /* No error */
    if (*presenceFlag)
      LIBBLU_DTS_ERROR_RETURN(
        "Presence of duplicated %s.\n", dtshdChunkIdStr(magic)
      );
    *presenceFlag = true;
  }

  return ret;
}

bool getDtshdInitialDelay(
  DtshdFileHandlerPtr handle,
  unsigned * skippedFramesNumber
)
{
  if (!handle->audioPresHeaderMetadataPresent)
    return false; /* No data */

  if (NULL != skippedFramesNumber)
    *skippedFramesNumber = handle->audioPresHeaderMetadata.nbSkippedFrames;
  return true;
}