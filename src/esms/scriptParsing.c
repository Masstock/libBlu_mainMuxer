#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "scriptParsing.h"

/* ### ESMS ES Properties section : ######################################## */

/** \~english
 * \brief
 *
 * \param f BitstreamReaderPtr source file.
 * \param s size_t size of the value in bytes (between 1 and 8 bytes).
 * \param d Numerical destination pointer.
 */
#define READ_VALUE(f, s, d, e)                                                \
  do {                                                                        \
    uint64_t value;                                                           \
                                                                              \
    if (readValue64BigEndian(f, s, &value) < 0)                               \
      e;                                                                      \
    if (NULL != (d))                                                          \
      *(d) = value;                                                           \
  } while (0)

#define READ_BIN_VALUE(f, s, d, e)                                            \
  do {                                                                        \
    uint64_t value;                                                           \
                                                                              \
    if (readBits64(f, &value, s) < 0)                                         \
      e;                                                                      \
    if (NULL != (d))                                                          \
      *(d) = value;                                                           \
  } while (0)

#define SKIP_VALUE(f, s)                                                      \
  do {                                                                        \
    if (skipBytes(f, s) < 0)                                                  \
      return -1;                                                              \
  } while (0)

int parseESPropertiesHeaderEsms(
  BitstreamReaderPtr script,
  LibbluESProperties * dst,
  uint64_t * refPts,
  uint64_t * endPts
)
{
  LibbluESType esType, esTypeExpected;

  /* [v32 esPropertiesHeader] */
  if (checkDirectoryMagic(script, ES_PROPERTIES_HEADER, 4) < 0)
    return -1;

  /* [u8 type] */
  READ_VALUE(script, 1, &esType, return -1);

  /* [u8 streamCodingType] */
  READ_VALUE(script, 1, &dst->codingType, return -1);

  if (!isSupportedStreamCodingType(dst->codingType))
    LIBBLU_ERROR_RETURN(
      "Unsupported stream coding type 0x%02" PRIX8 ".\n",
      dst->codingType
    );

  /* Check accordance between streamCodingType and type for security : */
  if ((esTypeExpected = determineLibbluESType(dst->codingType)) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to identify stream coding type 0x%02" PRIX8 ".\n",
      dst->codingType
    );
  if (esType != esTypeExpected)
    LIBBLU_ERROR_RETURN(
      "Stream type mismatch, expect %s, got %s.\n",
      libbluESTypeStr(esTypeExpected),
      libbluESTypeStr(esType)
    );
  dst->type = esType;

  /* [u64 PTSreference] */
  READ_VALUE(script, 8, refPts, return -1);

  /* [u32 bitrate] */
  READ_VALUE(script, 4, &dst->bitrate, return -1);

  /* [u64 endPts] */
  READ_VALUE(script, 8, endPts, return -1);

  /* [v64 scriptingFlags] */
  SKIP_VALUE(script, 8);

  return 0;
}

static int checkCrcEntryESPropertiesSourceFilesEsms(
  const lbc * filepath,
  EsmsESSourceFile prop
)
{
  uint8_t * buf;
  BitstreamReaderPtr file;
  uint32_t crcResult;

  if (NULL == (buf = (uint8_t *) malloc(prop.crcCheckedBytes)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  if (NULL == (file = createBitstreamReaderDefBuf(filepath)))
    LIBBLU_ERROR_FRETURN("Unable to open source file.\n");

  if (readBytes(file, buf, prop.crcCheckedBytes) < 0)
    return -1;

  crcResult = lb_compute_crc32(buf, 0, prop.crcCheckedBytes);

  closeBitstreamReader(file);
  free(buf);

  if (crcResult != prop.crc32)
    LIBBLU_ERROR_RETURN(
      "Source file '%" PRI_LBCS "' checksum error.\n",
      filepath
    );

  return 0;

free_return:
  closeBitstreamReader(file);
  free(buf);
  return -1;
}

static int parseEntryESPropertiesSourceFilesEsms(
  BitstreamReaderPtr script,
  EsmsESSourceFiles * dst
)
{
  size_t srcFilepathSize;
  uint8_t srcFilepath[PATH_BUFSIZE];

  uint16_t crcCoveredSize;
  uint32_t crcValue;

  lbc * convFilepath;
  EsmsESSourceFile prop;

  /* [u16 sourceFilepathSize] */
  READ_VALUE(script, 2, &srcFilepathSize, return -1);
  if (!srcFilepathSize || PATH_BUFSIZE <= srcFilepathSize)
    LIBBLU_ERROR_RETURN(
      "Invalid source filepath size (%zu characters).\n",
      srcFilepathSize
    );

  /* [v<sourceFileNameLength> sourceFileName] */
  if (readBytes(script, srcFilepath, srcFilepathSize) < 0)
    return -1;
  srcFilepath[srcFilepathSize] = '\0';

  /* Convert from UTF-8 to internal char repr. type (if required) */
  if (NULL == (convFilepath = lbc_utf8_convto(srcFilepath)))
    return -1;

  /* [u16 crcCheckedBytes] */
  READ_VALUE(script, 2, &crcCoveredSize, goto free_return);

  /* [u32 crc] */
  READ_VALUE(script, 4, &crcValue, goto free_return);

  setEsmsESSourceFile(&prop, crcValue, crcCoveredSize);

  /* Check CRC-32 */
  if (checkCrcEntryESPropertiesSourceFilesEsms(convFilepath, prop) < 0)
    goto free_return;

  /* Save file */
  if (appendEsmsESSourceFiles(dst, convFilepath, prop) < 0)
    goto free_return;

  free(convFilepath);
  return 0;

free_return:
  free(convFilepath);
  return -1;
}

int parseESPropertiesSourceFilesEsms(
  BitstreamReaderPtr script,
  EsmsESSourceFiles * dst
)
{
  unsigned i, nbSourceFiles;

  assert(0 == dst->nbUsedFiles);

  /* [u8 nbSourceFiles] */
  READ_VALUE(script, 1, &nbSourceFiles, return -1);

  for (i = 0; i < nbSourceFiles; i++) {
    if (parseEntryESPropertiesSourceFilesEsms(script, dst) < 0)
      return -1;
  }

  return 0;
}

/* ### ESMS ES Format Properties section : ################################# */

static int parseH264VideoESFmtPropertiesEsms(
  BitstreamReaderPtr script,
  LibbluESFmtSpecProp * dst
)
{
  LibbluESH264SpecProperties * prop = dst->h264;

  /* [v8 constraint_flags] */
  READ_VALUE(script, 1, &prop->constraintFlags, return -1);

  /* [u32 cpbSize] */
  READ_VALUE(script, 4, &prop->cpbSize, return -1);

  /* [u32 bitrate] */
  READ_VALUE(script, 4, &prop->bitrate, return -1);

  return 0;
}

static int parseVideoESFmtPropertiesEsms(
  BitstreamReaderPtr script,
  LibbluESProperties * dst,
  LibbluESFmtSpecProp * fmtSpecDst
)
{
  uint8_t flags;

  /* [v64 videoSpecificFormatParamHeader] */
  if (checkDirectoryMagic(script, ES_CODEC_SPEC_PARAM_HEADER_VIDEO, 8) < 0)
    return -1;

  /* [u4 videoFormat] */
  READ_BIN_VALUE(script, 4, &dst->videoFormat, return -1);

  /* [u4 frameRate] */
  READ_BIN_VALUE(script, 4, &dst->frameRate, return -1);

  /* [u8 profileIDC] */
  READ_VALUE(script, 1, &dst->profileIDC, return -1);

  /* [u8 levelIDC] */
  READ_VALUE(script, 1, &dst->levelIDC, return -1);

  /* [b1 stillPicture] [v7 reserved] */
  READ_VALUE(script, 1, &flags, return -1);
  dst->stillPicture = (flags & 0x80);

  if (dst->codingType == STREAM_CODING_TYPE_AVC) {
    if (initLibbluESFmtSpecProp(fmtSpecDst, FMT_SPEC_INFOS_H264) < 0)
      return -1;

    if (parseH264VideoESFmtPropertiesEsms(script, fmtSpecDst) < 0)
      return -1;
  }

  return 0;
}

static int parseAc3AudioESFmyPropertiesEsms(
  BitstreamReaderPtr script,
  LibbluESFmtSpecProp * dst
)
{
  LibbluESAc3SpecProperties * prop = dst->ac3;

  /* [u3 sample_rate_code] */
  READ_BIN_VALUE(script, 3, &prop->sample_rate_code, return -1);

  /* [u5 bsid] */
  READ_BIN_VALUE(script, 5, &prop->bsid, return -1);

  /* [u6 bit_rate_code] */
  READ_BIN_VALUE(script, 6, &prop->bit_rate_code, return -1);

  /* [u2 surround_mode] */
  READ_BIN_VALUE(script, 2, &prop->surround_mode, return -1);

  /* [u3 bsmod] */
  READ_BIN_VALUE(script, 3, &prop->bsmod, return -1);

  /* [u4 num_channels] */
  READ_BIN_VALUE(script, 4, &prop->num_channels, return -1);

  /* [b1 full_svc] */
  READ_BIN_VALUE(script, 1, &prop->full_svc, return -1);

  return 0;
}

static int parseAudioESFmtPropertiesEsms(
  BitstreamReaderPtr script,
  LibbluESProperties * dst,
  LibbluESFmtSpecProp * fmtSpecDst
)
{

  /* [v64 audioSpecificFormatParamHeader] */
  if (checkDirectoryMagic(script, ES_CODEC_SPEC_PARAM_HEADER_AUDIO, 8) < 0)
    return -1;

  /* [u4 audioFormat] */
  READ_BIN_VALUE(script, 4, &dst->audioFormat, return -1);

  /* [u4 sampleRate] */
  READ_BIN_VALUE(script, 4, &dst->sampleRate, return -1);

  /* [u8 bitDepth] */
  READ_VALUE(script, 1, &dst->bitDepth, return -1);

  /* [v8 reserved] */
  SKIP_VALUE(script, 1);

  if (isAc3AudioStreamCodingType(dst->codingType)) {
    if (initLibbluESFmtSpecProp(fmtSpecDst, FMT_SPEC_INFOS_AC3) < 0)
      return -1;

    if (parseAc3AudioESFmyPropertiesEsms(script, fmtSpecDst) < 0)
      return -1;
  }

  return 0;
}

int parseESFmtPropertiesEsms(
  BitstreamReaderPtr script,
  LibbluESProperties * dst,
  LibbluESFmtSpecProp * fmtSpecDst
)
{

  switch (dst->type) {
    case ES_VIDEO:
      return parseVideoESFmtPropertiesEsms(script, dst, fmtSpecDst);

    case ES_AUDIO:
      return parseAudioESFmtPropertiesEsms(script, dst, fmtSpecDst);

    default: /* ES_HDMV */
      break; /* No ES Format Properties section defined. */
  }

  return 0;
}

/* ### ESMS ES Data Blocks Definition section : ############################ */

static int parseEntryESDataBlocksDefinitionEsms(
  BitstreamReaderPtr script,
  EsmsDataBlocks * dst
)
{
  uint32_t size;
  uint8_t * data = NULL;

  EsmsDataBlockEntry entry;

  /* [u32 dataBlockLength] */
  READ_VALUE(script, 4, &size, return -1);

  if (0 < size) {
    if (NULL == (data = (uint8_t *) malloc(size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    /* [v<dataBlockLength> dataBlock] */
    if (readBytes(script, data, size) < 0)
      goto free_return;
  }

  setEsmsDataBlockEntry(&entry, data, size);
  if (appendEsmsDataBlocks(dst, entry, NULL) < 0)
    goto free_return;

  return 0;

free_return:
  free(data);
  return -1;
}

int parseESDataBlocksDefinitionEsms(
  BitstreamReaderPtr script,
  EsmsDataBlocks * dst
)
{
  unsigned i, nbDataBlocks;

  /* [v32 dataBlocksDefHeader] */
  if (checkDirectoryMagic(script, DATA_BLOCKS_DEF_HEADER, 4) < 0)
    return -1;

  /* [u8 nbDataBlocks] */
  READ_VALUE(script, 1, &nbDataBlocks, return -1);

  for (i = 0; i < nbDataBlocks; i++) {
    if (parseEntryESDataBlocksDefinitionEsms(script, dst) < 0)
      return -1;
  }

  return 0;
}

/* ### ESMS PES Cutting section : ########################################## */
/* ###### PES packet properties : ########################################## */

#define CHECK_DATA_SIZE(c, s, e)                                              \
  do {                                                                        \
    if ((s) < (e))                                                            \
      LIBBLU_ERROR_RETURN(                                                    \
        "Broken script, unexpected " c " extension size.\n"                   \
      );                                                                      \
    (s) -= (e);                                                               \
  } while (0)

#define SKIP_REMAINING_DATA(scrt, s)                                          \
  skipBytes(scrt, s)

static int parseEsmsPesPacketH264ExtData(
  EsmsPesPacketH264ExtData * dst,
  BitstreamReaderPtr script,
  size_t size
)
{
  /* H.264 AVC Extension data */

  uint8_t flags;
  unsigned cpbDpbFieldsSize;

  /* [b1 largeTimeFields] [v7 reserved] */
  CHECK_DATA_SIZE("H.264 AVC", size, 1);
  READ_VALUE(script, 1, &flags, return -1);
  cpbDpbFieldsSize = (flags & 0x80) ? 8 : 4;

  /* [un cpbRemovalTime] */
  CHECK_DATA_SIZE("H.264 AVC", size, cpbDpbFieldsSize);
  READ_VALUE(script, cpbDpbFieldsSize, &dst->cpbRemovalTime, return -1);

  /* [un dpbOutputTime] */
  CHECK_DATA_SIZE("H.264 AVC", size, cpbDpbFieldsSize);
  READ_VALUE(script, cpbDpbFieldsSize, &dst->dpbOutputTime, return -1);

  return SKIP_REMAINING_DATA(script, size);
}

#undef CHECK_DATA_SIZE
#undef SKIP_REMAINING_DATA

static int parseEsmsPesPacketExtData(
  EsmsPesPacketExtData * dst,
  BitstreamReaderPtr script,
  LibbluStreamCodingType codingType
)
{
  size_t extSize;

  assert(NULL != dst);

  /* [u16 extensionDataLen] */
  READ_VALUE(script, 2, &extSize, return -1);

  switch (codingType) {
    case STREAM_CODING_TYPE_AVC:
      return parseEsmsPesPacketH264ExtData(&dst->h264, script, extSize);

    default:
      break; /* Unsupported Extension data */
  }

  /* [vn reserved] */
  return skipBytes(script, extSize);
}

static int parsePropertiesEsmsPesPacketNode(
  EsmsPesPacketNodePtr dst,
  BitstreamReaderPtr script,
  LibbluStreamCodingType codingType
)
{
  uint8_t flags;
  size_t ptsFieldSize, dtsFieldSize, lengthFieldSize;

  if (isAudioStreamCodingType(codingType)) {
    /* [b1 extensionFrame] [v7 reserved] */
    READ_VALUE(script, 1, &flags, return -1);
    dst->extensionFrame = (flags & 0x80);
  }
  else {
    /* [v8 reserved] */
    SKIP_VALUE(script, 1);
  }

  /** [v8 fieldsProperties]
   * -> b1  : ptsLongField
   * -> b1  : dtsPresent
   * -> b1  : dtsLongField
   * -> b1  : lengthLongField
   * -> b1  : extensionDataPres
   * -> v3  : reserved
   */
  READ_VALUE(script, 1, &flags, return -1);
  ptsFieldSize              = (flags & 0x80) ? 8 : 4;
  dst->dtsPresent           = (flags & 0x40);
  dtsFieldSize              = (flags & 0x20) ? 8 : 4;
  lengthFieldSize           = (flags & 0x10) ? 4 : 2;
  dst->extensionDataPresent = (flags & 0x08);

  /* [u32/64 pts] */
  READ_VALUE(script, ptsFieldSize, &dst->pts, return -1);
  dst->dts = dst->pts;

  if (dst->dtsPresent) {
    /* [u32/64 dts] */
    READ_VALUE(script, dtsFieldSize, &dst->dts, return -1);
  }

  if (dst->extensionDataPresent) {
    if (parseEsmsPesPacketExtData(&dst->extensionData, script, codingType) < 0)
      return -1;
  }

  /* [u16/32 length] */
  READ_VALUE(script, lengthFieldSize, &dst->length, return -1);

  return 0;
}

/* ###### Script commands utilities : ###################################### */

static int allocateEsmsCommandParsingData(
  EsmsCommandParsingData * dst,
  size_t requiredSize
)
{
  size_t newSize;
  uint8_t * newData;

  if (requiredSize <= dst->dataAllocatedSize)
    return 0;

  for (
    newSize = dst->dataAllocatedSize;
    newSize < requiredSize;
    newSize = GROW_ALLOCATION(newSize, 64)
  ) {
    if (newSize < dst->dataAllocatedSize)
      LIBBLU_ERROR_RETURN("Broken script, command size overflow.\n");
  }

  if (NULL == (newData = (uint8_t *) realloc(dst->data, newSize)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  dst->data = newData;
  dst->dataAllocatedSize = newSize;

  return 0;
}

static int readRawDataEsmsCommandParsingData(
  EsmsCommandParsingData * dst,
  BitstreamReaderPtr script
)
{
  uint64_t rawDataSize;

  /* [u16 commandRawDataSize] */
  if (readValue64BigEndian(script, 2, &rawDataSize) < 0)
    return -1;

  if (allocateEsmsCommandParsingData(dst, rawDataSize) < 0)
    return -1;

  /* [vn rawData] */
  if (readBytes(script, dst->data, rawDataSize) < 0)
    return -1;
  dst->dataUsedSize = rawDataSize;

  return 0;
}

/* ###### Script commands data : ########################################### */

#define RB_COM(dat, off)                                                      \
  ((uint64_t) RB_ARRAY(dat->data, off))

static int parseScriptAddDataCommandEsmsPesPacketNode(
  EsmsCommandNodePtr dst,
  const EsmsCommandParsingData * data
)
{
  size_t off = 0;

  uint32_t offset;
  EsmsDataInsertionMode mode;

  /* [u32 offset] */
  offset  = RB_COM(data, off) << 24;
  offset |= RB_COM(data, off) << 16;
  offset |= RB_COM(data, off) <<  8;
  offset |= RB_COM(data, off);

  /* [u8 mode] */
  mode = RB_COM(data, off);
  if (!isValidEsmsDataInsertionMode(mode))
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add data\" command unknown insertion mode %u.\n",
      mode
    );

  /* [vn data] */
  if (
    data->dataUsedSize <= ADD_DATA_COM_LEN
    || UINT16_MAX < data->dataUsedSize - ADD_DATA_COM_LEN
  )
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add data\" command invalid size %zu.\n",
      data->dataUsedSize
    );

  return setEsmsAddDataCommand(
    &dst->command.data.addData,
    offset,
    mode,
    data->data + ADD_DATA_COM_LEN,
    data->dataUsedSize - ADD_DATA_COM_LEN
  );
}

static int parseScriptChangeByteOrderCommandEsmsPesPacketNode(
  EsmsCommandNodePtr dst,
  const EsmsCommandParsingData * data
)
{
  size_t off = 0;

  uint8_t unitSize;
  uint32_t offset;
  uint32_t length;

  /* [u8 valueLength] */
  unitSize = RB_COM(data, off);

  /* [u32 valuesFieldOffset] */
  offset  = RB_COM(data, off) << 24;
  offset |= RB_COM(data, off) << 16;
  offset |= RB_COM(data, off) <<  8;
  offset |= RB_COM(data, off);

  /* [u32 valuesFieldLength] */
  length  = RB_COM(data, off) << 24;
  length |= RB_COM(data, off) << 16;
  length |= RB_COM(data, off) <<  8;
  length |= RB_COM(data, off);

  return setEsmsChangeByteOrderCommand(
    &dst->command.data.changeByteOrder,
    unitSize,
    offset,
    length
  );
}

static int parseScriptAddPesPayloadCommandEsmsPesPacketNode(
  EsmsCommandNodePtr dst,
  const EsmsCommandParsingData * data
)
{
  size_t off = 0;

  uint8_t flags;
  unsigned fileIdx;
  uint32_t dstOffset;
  uint64_t srcOffset;
  uint32_t size;

  /** [v8 flags]
   * -> b1 : payloadOffsetExtensionPresent;
   * -> b1 : payloadLengthExtensionPresent;
   * -> v6 : reserved.
   */
  flags = RB_COM(data, off);

  /* [u8 sourceFileIdx] */
  fileIdx = RB_COM(data, off);

  /* [u32 insertingOffset] */
  dstOffset  = RB_COM(data, off) << 24;
  dstOffset |= RB_COM(data, off) << 16;
  dstOffset |= RB_COM(data, off) <<  8;
  dstOffset |= RB_COM(data, off);

  /* [u32 payloadOffset] */
  srcOffset  = RB_COM(data, off) << 24;
  srcOffset |= RB_COM(data, off) << 16;
  srcOffset |= RB_COM(data, off) <<  8;
  srcOffset |= RB_COM(data, off);

  if (flags & 0x80) {
    /* if (payloadOffsetExtensionPresent) */

    /* [u32 payloadOffsetExtension] */
    srcOffset |= ((uint64_t) RB_COM(data, off)) << 56;
    srcOffset |= ((uint64_t) RB_COM(data, off)) << 48;
    srcOffset |= ((uint64_t) RB_COM(data, off)) << 40;
    srcOffset |= ((uint64_t) RB_COM(data, off)) << 32;
  }

  /* [u16 payloadLength] */
  size  = RB_COM(data, off) << 8;
  size |= RB_COM(data, off);

  if (flags & 0x40) {
    /* if (payloadLengthExtensionPresent) */

    /* [u16 payloadLengthExtension] */
    size |= RB_COM(data, off) << 24;
    size |= RB_COM(data, off) << 16;
  }

  return setEsmsAddPesPayloadCommand(
    &dst->command.data.addPesPayload,
    fileIdx,
    dstOffset,
    srcOffset,
    size
  );
}

static int parseScriptAddPaddingCommandEsmsPesPacketNode(
  EsmsCommandNodePtr dst,
  const EsmsCommandParsingData * data
)
{
  size_t off = 0;

  uint32_t offset;
  EsmsDataInsertionMode mode;
  uint32_t length;
  uint8_t byte;

  /* [u32 insertingOffset] */
  offset  = RB_COM(data, off) << 24;
  offset |= RB_COM(data, off) << 16;
  offset |= RB_COM(data, off) <<  8;
  offset |= RB_COM(data, off);

  /* [u8 insertingMode] */
  mode = RB_COM(data, off);
  if (!isValidEsmsDataInsertionMode(mode))
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add Padding Data\" command unknown "
      "insertion mode %u.\n",
      mode
    );

  /* [u32 paddingLength] */
  length  = RB_COM(data, off) << 24;
  length |= RB_COM(data, off) << 16;
  length |= RB_COM(data, off) <<  8;
  length |= RB_COM(data, off);

  /* [v8 paddingByte] */
  byte = RB_COM(data, off);

  return setEsmsAddPaddingCommand(
    &dst->command.data.addPadding,
    offset,
    mode,
    length,
    byte
  );
}

static int parseScriptAddDataBlockCommandEsmsPesPacketNode(
  EsmsCommandNodePtr dst,
  const EsmsCommandParsingData * data
)
{
  size_t off = 0;

  uint32_t offset;
  EsmsDataInsertionMode mode;
  uint8_t blockIdx;

  /* [u32 insertingOffset] */
  offset  = RB_COM(data, off) << 24;
  offset |= RB_COM(data, off) << 16;
  offset |= RB_COM(data, off) <<  8;
  offset |= RB_COM(data, off);

  /* [u8 insertingMode] */
  mode = RB_COM(data, off);
  if (!isValidEsmsDataInsertionMode(mode))
    LIBBLU_ERROR_RETURN(
      "Broken script, \"Add Data Block\" command unknown "
      "insertion mode %u.\n",
      mode
    );

  /* [u8 dataBlockId] */
  blockIdx = RB_COM(data, off);

  return setEsmsAddDataBlockCommand(
    &dst->command.data.addDataBlock,
    offset,
    mode,
    blockIdx
  );
}

#undef RB_COM

/* ###### Script commands parsing : ######################################## */

static int parseScriptCommandsEsmsPesPacketNode(
  EsmsPesPacketNodePtr dst,
  BitstreamReaderPtr script,
  EsmsCommandParsingData * data
)
{
  unsigned i, nbScriptCommands;
  EsmsCommandNodePtr lastCommand, curCommand;

  assert(NULL != dst);
  assert(NULL == dst->commands);

  /* [u8 nbCommands] */
  READ_VALUE(script, 1, &nbScriptCommands, return -1);

  /* Read modification and save script commands list : */
  lastCommand = NULL;
  for (i = 0; i < nbScriptCommands; i++) {
    uint8_t commandId;
    int ret;

    /* [u8 commandType] */
    READ_VALUE(script, 1, &commandId, return -1);

    if (NULL == (curCommand = createEsmsCommandNode(commandId)))
      return -1;

    /* [u16 commandRawDataSize] [vn rawData] */
    if (readRawDataEsmsCommandParsingData(data, script) < 0)
      return -1;

    switch (commandId) {
      case ESMS_ADD_DATA: /* 0x00 : Add data */
        ret = parseScriptAddDataCommandEsmsPesPacketNode(curCommand, data);
        break;

      case ESMS_CHANGE_BYTEORDER: /* 0x01 : Change byte order */
        /* Switch byte-ordering of the current frame. */
        ret = parseScriptChangeByteOrderCommandEsmsPesPacketNode(curCommand, data);
        break;

      case ESMS_ADD_PAYLOAD_DATA: /* 0x02 : Add payload data */
        /* Reading source file PES data. */
        ret = parseScriptAddPesPayloadCommandEsmsPesPacketNode(curCommand, data);
        break;

      case ESMS_ADD_PADDING_DATA: /* 0x03 : Add padding data */
        ret = parseScriptAddPaddingCommandEsmsPesPacketNode(curCommand, data);
        break;

      case ESMS_ADD_DATA_SECTION: /* 0x04 : Add data section */
        ret = parseScriptAddDataBlockCommandEsmsPesPacketNode(curCommand, data);
        break;

      default:
        LIBBLU_ERROR_RETURN(
          "Unknown command type value 0x%x.\n",
          commandId
        );
    }
    if (ret < 0)
      return -1;

    /* Adding frame node : */
    if (NULL == dst->commands)
      dst->commands = curCommand;
    else
      attachEsmsCommandNode(lastCommand, curCommand);
    lastCommand = curCommand;
  }

  return 0;
}

#undef READ_VALUE
#undef SKIP_VALUE

/* ######################################################################### */

EsmsPesPacketNodePtr parseFrameNodeESPesCuttingEsms(
  BitstreamReaderPtr script,
  LibbluStreamCodingType codingType,
  EsmsCommandParsingData * data
)
{
  EsmsPesPacketNodePtr node;

  if (NULL == (node = createEsmsPesPacketNode()))
    return NULL;

  if (parsePropertiesEsmsPesPacketNode(node, script, codingType) < 0)
    return NULL;

  if (parseScriptCommandsEsmsPesPacketNode(node, script, data) < 0)
    return NULL;
  return node;
}