#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "scriptCreation.h"

EsmsFileHeaderPtr createEsmsFileHandler(
  LibbluESType type,
  const LibbluESSettingsOptions options,
  const LibbluESFmtSpecPropType fmtSpecPropType
)
{
  EsmsFileHeaderPtr handler;
  size_t fmtSpecPropAllocationSize;

  handler = (EsmsFileHeaderPtr) calloc(1, sizeof(EsmsFileHeader));
  if (NULL == handler)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  handler->streamType = type;
  handler->flags = computeFlagsLibbluESSettingsOptions(options);
  handler->fmtSpecProp.sharedPtr = NULL;

  /* Get the allocation size required by format specific properties */
  fmtSpecPropAllocationSize = sizeofLibbluESFmtSpecPropType(
    fmtSpecPropType
  );

  /* If fmtSpecPropType != FMT_SPEC_INFOS_NONE, allocate the required space. */
  if (0 < fmtSpecPropAllocationSize) {
    void * fmtSpecPropPtr;

    if (NULL == (fmtSpecPropPtr = malloc(fmtSpecPropAllocationSize)))
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
    handler->fmtSpecProp.sharedPtr = fmtSpecPropPtr;
  }

  return handler;

free_return:
  destroyEsmsFileHandler(handler);
  return NULL;
}

int updateEsmsHeader(const lbc * essFileName, const EsmsFileHeaderPtr script)
{
  FILE * scriptFile;
  EsmsFileDirectory dir;
  uint8_t dirIdx;

  static const uint8_t marker = 0x01;

  if (NULL == (scriptFile = lbc_fopen(essFileName, "rb+"))) {
    LIBBLU_ERROR_RETURN(
      "Unable to update ESMS Header: File opening error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  }

  /* [v32 esmsFileHeader] */
  /* [u8 formatVersion] */
  if (fseek(scriptFile, 0x5, SEEK_SET) < 0) {
    LIBBLU_ERROR_RETURN(
      "Unable to update ESMS Header: File seeking error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  }

  /* [u8 completedFile] */
  if (!fwrite(&marker, sizeof(uint8_t), 1, scriptFile))
    goto write_error;

  /* [u8 directoryNb] */
  if (!fwrite(&script->directories.nbUsedDirs, sizeof(uint8_t), 0x1, scriptFile))
    goto write_error;

  for (dirIdx = 0; dirIdx < script->directories.nbUsedDirs; dirIdx++) {
    dir = script->directories.dirs[dirIdx];

    /* [u8 directoryId] */
    if (!fwrite(&dir.id, sizeof(uint8_t), 0x1, scriptFile))
      goto write_error;

    /* [u64 directoryOffset] */
    if (!fwrite(&dir.offset, sizeof(uint64_t), 0x1, scriptFile))
      goto write_error;
  }

  fclose(scriptFile);
  return 0;

write_error:
  LIBBLU_ERROR_RETURN(
    "Unable to update ESMS Header: "
    "Unable to write on file, %s (errno: %d).\n",
    strerror(errno),
    errno
  );
}

int appendDirEsms(
  EsmsFileHeaderPtr script,
  uint8_t dirId,
  uint64_t dirOffset
)
{
  unsigned dirIdx;

  assert(NULL != script);

  for (dirIdx = 0; dirIdx < script->directories.nbUsedDirs; dirIdx++) {
    if (script->directories.dirs[dirIdx].id == dirId)
      LIBBLU_ERROR_RETURN("ESMS directory ID %u duplicated.\n", dirId);
  }

  if (ESMS_MAX_ALLOWED_DIR <= dirIdx)
    LIBBLU_ERROR_RETURN("Too many directories saved in ESMS file.\n");

  /* Adding values : */
  script->directories.dirs[dirIdx].id = dirId;
  script->directories.dirs[dirIdx].offset = dirOffset;
  script->directories.nbUsedDirs++;

  return 0;
}

int appendSourceFileEsmsWithCrc(
  EsmsFileHeaderPtr script,
  const lbc * filename,
  uint16_t crcCheckedBytes,
  uint32_t crc32,
  unsigned * idx
)
{
  EsmsESSourceFile prop;

  assert(NULL != script);
  assert(NULL != filename);

  if (isPresentEsmsESSourceFiles(script->sourceFiles, filename))
    LIBBLU_ERROR_RETURN("Duplicated source filename in ESMS file.\n");

  setEsmsESSourceFile(&prop, crc32, crcCheckedBytes);
  if (appendEsmsESSourceFiles(&script->sourceFiles, filename, prop) < 0)
    return -1;

  if (NULL != idx)
    *idx = nbUsedFilesEsmsESSourceFiles(script->sourceFiles) - 1;

  return 0;
}

int appendSourceFileEsms(
  EsmsFileHeaderPtr script,
  const lbc * filename,
  unsigned * newFileIdx
)
{
  BitstreamReaderPtr input;

  if (NULL == (input = createBitstreamReader(filename, READ_BUFFER_LEN)))
    return -1;

  int64_t inputSize = remainingSize(input);
  size_t crcSize = MIN((size_t) inputSize, CRC32_USED_BYTES);
  uint8_t crcBuf[CRC32_USED_BYTES];

  if (readBytes(input, crcBuf, crcSize) < 0)
    goto free_return;

  int ret = appendSourceFileEsmsWithCrc(
    script,
    filename,
    crcSize,
    lb_compute_crc32(crcBuf, 0, crcSize),
    newFileIdx
  );

  closeBitstreamReader(input);
  return ret;

free_return:
  closeBitstreamReader(input);
  return -1;
}

int appendDataBlockEsms(
  EsmsFileHeaderPtr script,
  const uint8_t * data,
  uint32_t size,
  unsigned * idx
)
{
  uint8_t * dataCopy = NULL;
  EsmsDataBlockEntry entry;

  assert(NULL != script);
  assert((0 == size) ^ (NULL != data));

  if (0 < size) {
    if (NULL == (dataCopy = (uint8_t *) malloc(size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    memcpy(dataCopy, data, size);
  }

  setEsmsDataBlockEntry(&entry, dataCopy, size);
  if (appendEsmsDataBlocks(&script->dataBlocks, entry, idx) < 0)
    goto free_return;

  return 0;

free_return:
  cleanEsmsDataBlockEntry(entry);
  return -1;
}

int updateDataBlockEsms(
  EsmsFileHeaderPtr script,
  const uint8_t * data,
  uint32_t size,
  unsigned idx
)
{
  uint8_t * dataCopy = NULL;
  EsmsDataBlockEntry entry;

  assert(NULL != script);
  assert((0 == size) ^ (NULL != data));

  if (0 < size) {
    if (NULL == (dataCopy = (uint8_t *) malloc(size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    memcpy(dataCopy, data, size);
  }

  setEsmsDataBlockEntry(&entry, dataCopy, size);
  if (updateEsmsDataBlocks(&script->dataBlocks, entry, idx) < 0)
    goto free_return;

  return 0;

free_return:
  cleanEsmsDataBlockEntry(entry);
  return -1;
}

int writeEsmsHeader(
  BitstreamWriterPtr esmsFile
)
{
  /* Version: 4 */
  size_t paddingLength;

  /* [u32 esmsFileHeader] */
  if (writeBytes(esmsFile, (uint8_t *) ESMS_FILE_HEADER, 4) < 0)
    return -1;

  /* [u8 formatVersion] */
  if (writeByte(esmsFile, CURRENT_ESMS_FORMAT_VER) < 0)
    return -1;

  /* [u8 completedFile] */
  if (writeByte(esmsFile, 0x00) < 0) /* Place Holder */
    return -1;

  /* [u8 directoryNb] */
  if (writeByte(esmsFile, 0x00) < 0) /* Place Holder */
    return -1;

  /* [v<7*ESMS_MAX_ALLOWED_DIR> reservedSpaceForDir] */
  paddingLength = ESMS_DIRECTORY_IDX_LENGTH * ESMS_MAX_ALLOWED_DIR;
  if (writeReservedBytes(esmsFile, 0x00, paddingLength) < 0) /* Place Holder */
    return -1;

  return 0;
}

int writeEsmsEsPropertiesSection(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
)
{
  unsigned i;

  assert(NULL != esmsFile);
  assert(NULL != script);

  /* [v32 esPropertiesHeader] */
  if (writeBytes(esmsFile, (uint8_t *) ES_PROPERTIES_HEADER, 4) < 0)
    return -1;

  script->streamType = determineLibbluESType(script->prop.codingType);
  if (0xFF == script->streamType)
    LIBBLU_ERROR_RETURN(
      "ESMS file is corrupted or stream type is unsupported.\n"
    );

  /* [u8 type] */
  if (writeByte(esmsFile, script->streamType) < 0)
    return -1;

  /* [u8 streamCodingType] */
  if (writeByte(esmsFile, script->prop.codingType) < 0)
    return -1;

  /* [u64 PTSreference] */
  if (writeUint64(esmsFile, script->ptsRef) < 0)
    return -1;

  /* [u32 bitrate] */
  if (writeUint32(esmsFile, script->bitrate) < 0)
    return -1;

  /* [u64 endPts] */
  if (writeUint64(esmsFile, script->endPts) < 0)
    return -1;

  /* [u64 scriptingFlags] */
  if (writeUint64(esmsFile, script->flags) < 0)
    return -1;

  /* [u8 nbSourceFiles] */
  if (writeByte(esmsFile, script->sourceFiles.nbUsedFiles) < 0)
    return -1;

  for (i = 0; i < script->sourceFiles.nbUsedFiles; i++) {
    /* TODO: Move to a function */
    const lbc * filepathOrig = script->sourceFiles.filepaths[i];
    const EsmsESSourceFile fileProp = script->sourceFiles.properties[i];
    char * filepathConv;
    size_t filepathSize;

    /* Write a UTF-8 conversion of filepath */
    if (NULL == (filepathConv = lbc_convfrom(filepathOrig)))
      goto free_return;

    if (INT16_MAX < (filepathSize = strlen(filepathConv)))
      LIBBLU_ERROR_FRETURN(
        "Filepath '%" PRI_LBCS "' UTF-8 conversion size overflow.\n",
        filepathOrig
      );

    /* [u16 sourceFileNameLength] */
    if (writeUint16(esmsFile, filepathSize) < 0)
      goto free_return;

    /* [u<sourceFileNameLength> sourceFileName] */
    if (writeBytes(esmsFile, (uint8_t *) filepathConv, filepathSize) < 0)
      goto free_return;

    /* [u16 crcCheckedBytes] */
    if (writeUint16(esmsFile, fileProp.crcCheckedBytes) < 0)
      goto free_return;

    /* [u32 crc] */
    if (writeUint32(esmsFile, fileProp.crc32) < 0)
      goto free_return;

    free(filepathConv);

    while (0) {
      /* Unaccessible error only code */
      free_return:
        free(filepathConv);
        return -1;
    }
  }

  return 0;
}

int writeEsmsPesCuttingHeader(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
)
{
  assert(NULL != esmsFile);
  assert(NULL != script);

  if (appendDirEsms(script, ESMS_DIRECTORY_ID_ES_PES_CUTTING, tellWritingPos(esmsFile)) < 0)
    return -1;

  /* [v32 pesCuttingHeader] */
  if (writeBytes(esmsFile, (uint8_t *) PES_CUTTING_HEADER, 4) < 0)
    return -1;

  return 0;
}

int writeEsmsPesPacketExtensionData(
  BitstreamWriterPtr esmsFile,
  EsmsPesPacketExtData extParam,
  LibbluStreamCodingType codec
)
{
  switch (codec) {
    case STREAM_CODING_TYPE_AVC: {
        bool largeTimeFields =
          (extParam.h264.cpbRemovalTime >> 32)
          | (extParam.h264.dpbOutputTime >> 32)
        ;

        /* [u16 extensionDataLen] */
        if (
          writeUint16(
            esmsFile,
            ESMS_PFH_EXT_PARAM_H264_LENGTH(largeTimeFields)
          ) < 0
        )
          return -1;

        /* [b1 largeTimeFields] [v7 reserved] */
        if (writeByte(esmsFile, largeTimeFields ? 0x80 : 0x00) < 0)
          return -1;

        if (largeTimeFields) {
          /* [u64 cpbRemovalTime] */
          if (writeUint64(esmsFile, extParam.h264.cpbRemovalTime) < 0)
            return -1;

          /* [u64 dpbOutputTime] */
          if (writeUint64(esmsFile, extParam.h264.dpbOutputTime) < 0)
            return -1;
        }
        else {
          /* [u32 cpbRemovalTime] */
          if (writeUint32(esmsFile, extParam.h264.cpbRemovalTime) < 0)
            return -1;

          /* [u32 dpbOutputTime] */
          if (writeUint32(esmsFile, extParam.h264.dpbOutputTime) < 0)
            return -1;
        }
      }
      break;

    default:
      /* [v16 extensionDataLen] */
      if (writeUint16(esmsFile, 0) < 0)
        return -1;
  }

  return 0;
}

static int writeEsmsAddDataCommand(
  BitstreamWriterPtr esmsFile,
  EsmsAddDataCommand command
)
{
  /* [u16 rawDataLength] */
  if (writeUint16(esmsFile, ADD_DATA_COM_LEN + command.dataLength) < 0)
    return -1;

  /* [u32 offset] */
  if (writeUint32(esmsFile, command.offset) < 0)
    return -1;

  /* [u8 insertingMode] */
  if (writeByte(esmsFile, command.mode) < 0)
    return -1;

  /* [v(n) insertingData] */
  if (writeBytes(esmsFile, command.data, command.dataLength) < 0)
    return -1;

  return 0;
}

static int writeEsmsChangeByteOrderCommand(
  BitstreamWriterPtr esmsFile,
  EsmsChangeByteOrderCommand command
)
{
  /* [u16 rawDataLength] */
  if (writeUint16(esmsFile, CHANGE_BYTEORDER_COM_LEN) < 0)
    return -1;

  /* [u8 valueLength] */
  if (writeByte(esmsFile, command.unitSize) < 0)
    return -1;

  /* [u32 valuesFieldOffset] */
  if (writeUint32(esmsFile, command.offset) < 0)
    return -1;

  /* [u32 valuesFieldLength] */
  if (writeUint32(esmsFile, command.length) < 0)
    return -1;

  return 0;
}

static int writeEsmsAddPesPayloadCommand(
  BitstreamWriterPtr esmsFile,
  EsmsAddPesPayloadCommand command
)
{
  bool payloadOffsetExtensionPres;
  bool payloadLengthExtensionPres;
  uint16_t commandSize;
  uint8_t propByte;

  payloadOffsetExtensionPres = (0x0 != (command.srcOffset >> 32));
  payloadLengthExtensionPres = (0x0 != (command.size >> 16));
  commandSize =
    ADD_PAYLOAD_DATA_COM_LEN
    + (payloadOffsetExtensionPres ? 4 : 0)
    + (payloadLengthExtensionPres ? 2 : 0)
  ;

  /* [u16 rawDataLength] */
  if (writeUint16(esmsFile, commandSize) < 0)
    return -1;

  /**
   * [b1 payloadOffsetExtensionPresent]
   * [b1 payloadLengthExtensionPresent]
   * [v6 reserved]
   */
  propByte =
    (payloadOffsetExtensionPres << 7)
    | (payloadLengthExtensionPres << 6)
  ;
  if (writeByte(esmsFile, propByte) < 0)
    return -1;

  /* [u8 sourceFileIdx] */
  if (writeByte(esmsFile, command.fileIdx) < 0)
    return -1;

  /* [u32 insertingOffset] */
  if (writeUint32(esmsFile, command.dstOffset) < 0)
    return -1;

  /* [u32 payloadOffset] */
  if (writeUint32(esmsFile, ((uint64_t) command.srcOffset)) < 0)
    return -1;

  if (payloadOffsetExtensionPres) {
    /* [u32 payloadOffsetExtension] */
    if (writeUint32(esmsFile, ((uint64_t) command.srcOffset) >> 32) < 0)
      return -1;
  }

  /* [u16 payloadLength] */
  if (writeUint16(esmsFile, command.size) < 0)
    return -1;

  if (payloadLengthExtensionPres) {
    /* [u16 payloadLengthExtension] */
    if (writeUint16(esmsFile, command.size >> 16) < 0)
      return -1;
  }

  return 0;
}

static int writeEsmsAddPaddingDataCommand(
  BitstreamWriterPtr esmsFile,
  EsmsAddPaddingCommand command
)
{
  /* [u16 rawDataLength] */
  if (writeUint16(esmsFile, ADD_PADD_COM_LEN) < 0)
    return -1;

  /* [u32 offset] */
  if (writeUint32(esmsFile, command.offset) < 0)
    return -1;

  /* [u8 insertingMode] */
  if (writeByte(esmsFile, command.mode) < 0)
    return -1;

  /* [u32 paddingLength] */
  if (writeUint32(esmsFile, command.length) < 0)
    return -1;

  /* [u8 paddingByte] */
  if (writeByte(esmsFile, command.byte) < 0)
    return -1;

  return 0;
}

static int writeEsmsAddDataBlockCommand(
  BitstreamWriterPtr esmsFile,
  EsmsAddDataBlockCommand command
)
{
  /* [u16 rawDataLength] */
  if (writeUint16(esmsFile, ADD_DATA_SECTION_COM_LEN) < 0)
    return -1;

  /* [u32 offset] */
  if (writeUint32(esmsFile, command.offset) < 0)
    return -1;

  /* [u8 insertingMode] */
  if (writeByte(esmsFile, command.mode) < 0)
    return -1;

  /* [u8 dataBlockId] */
  if (writeByte(esmsFile, command.blockIdx) < 0)
    return -1;

  return 0;
}

int writeEsmsPesPacket(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
)
{
  int ret;
  unsigned i;

  uint32_t pesFrameLength;
  uint8_t flagsByte;

  EsmsPesPacketHeader curFrame;

  assert(NULL != esmsFile);
  assert(NULL != script);

  if (!script->commandsPipeline.initFrame)
    LIBBLU_ERROR_RETURN("Attempt to write uninitialized ESMS PES frame.\n");

  if (script->commandsPipeline.nbFrames == 0) {
    if (writeEsmsPesCuttingHeader(esmsFile, script) < 0)
      return -1;
  }

  curFrame = script->commandsPipeline.curFrame;

  /* Write frame : */
  switch (script->streamType) {
    case ES_VIDEO:
      /* [u2 pictureType] [v6 reserved] */
      if (writeByte(esmsFile, curFrame.pictureType << 6) < 0)
        return -1;
      break;

    case ES_AUDIO:
      /* [b1 extensionFrame] [v7 reserved] */
      if (writeByte(esmsFile, curFrame.extensionFrame << 7) < 0)
        return -1;
      break;

    case ES_HDMV:
      /* [v8 reserved] */
      if (writeByte(esmsFile, 0x00) < 0)
        return -1;
      break;

    default:
      /* Error, unexpected type */
      LIBBLU_ERROR_RETURN(
        "Unexpected ESMS stream type %u at PES frame writing.\n",
        script->streamType
      );
  }

  pesFrameLength = computePesFrameLength(
    curFrame.commands,
    curFrame.nbCommands,
    script->dataBlocks
  );

  /* LIBBLU_DEBUG_COM("ESMS Frame length: %zu bytes.\n", pesFrameLength); */

  /**
   * [b1 ptsLongField] [b1 dtsPresent] [b1 dtsLongField]
   * [b1 lengthLongField] [v7 reserved]
   */
  flagsByte =
    ((curFrame.pts  >> 32)        ? ESMS_FFLAG_PTS_LONG_FIELD    : 0x0)
    | ((curFrame.dtsPresent)      ? ESMS_FFLAG_DTS_PRESENT       : 0x0)
    | ((curFrame.dts   >> 32)     ? ESMS_FFLAG_DTS_LONG_FIELD    : 0x0)
    | ((pesFrameLength >> 16)     ? ESMS_FFLAG_LENGTH_LONG_FIELD : 0x0)
    | ((curFrame.extParamPresent) ? ESMS_FFLAG_EXT_DATA_PRESENT  : 0x0)
  ;

  if (writeByte(esmsFile, flagsByte) < 0)
    return -1;

  if (flagsByte & ESMS_FFLAG_PTS_LONG_FIELD) {
    /* [u64 pts] */
    if (writeUint64(esmsFile, curFrame.pts) < 0)
      return -1;
  }
  else {
    /* [u32 pts] */
    if (writeUint32(esmsFile, curFrame.pts) < 0)
      return -1;
  }

  if (flagsByte & ESMS_FFLAG_DTS_PRESENT) {
    if (flagsByte & ESMS_FFLAG_DTS_LONG_FIELD) {
      /* [u64 dts] */
      if (writeUint64(esmsFile, curFrame.dts) < 0)
        return -1;
    }
    else {
      /* [u32 dts] */
      if (writeUint32(esmsFile, curFrame.dts) < 0)
        return -1;
    }
  }

  if (flagsByte & ESMS_FFLAG_EXT_DATA_PRESENT) {
    ret = writeEsmsPesPacketExtensionData(
      esmsFile, curFrame.extParam, script->prop.codingType
    );
    if (ret < 0)
      return -1;
  }

  if (flagsByte & ESMS_FFLAG_LENGTH_LONG_FIELD) {
    /* [u32 length] */
    if (writeUint32(esmsFile, pesFrameLength) < 0)
      return -1;
  }
  else {
    /* [u16 length] */
    if (writeUint16(esmsFile, pesFrameLength) < 0)
      return -1;
  }

  /* [u8 nbCommands] */
  if (writeByte(esmsFile, curFrame.nbCommands) < 0)
    return -1;

  /* Writing modification script commands : */
  for (i = 0; i < curFrame.nbCommands; i++) {
    EsmsCommand * command = curFrame.commands + i;

    /* [u8 commandId] */
    if (writeByte(esmsFile, command->type) < 0)
      return -1;

    switch (command->type) {
      case ESMS_ADD_DATA:
        if (writeEsmsAddDataCommand(esmsFile, command->data.addData) < 0)
          return -1;
        break;

      case ESMS_CHANGE_BYTEORDER:
        if (writeEsmsChangeByteOrderCommand(esmsFile, command->data.changeByteOrder) < 0)
          return -1;
        break;

      case ESMS_ADD_PAYLOAD_DATA:
        if (writeEsmsAddPesPayloadCommand(esmsFile, command->data.addPesPayload) < 0)
          return -1;
        break;

      case ESMS_ADD_PADDING_DATA:
        if (writeEsmsAddPaddingDataCommand(esmsFile, command->data.addPadding) < 0)
          return -1;
        break;

      case ESMS_ADD_DATA_SECTION:
        if (writeEsmsAddDataBlockCommand(esmsFile, command->data.addDataBlock) < 0)
          return -1;
        break;
    }
  }

  script->commandsPipeline.nbFrames++;
  script->commandsPipeline.initFrame = false;
  return 0;
}

int writeH264FmtSpecificInfos(
  BitstreamWriterPtr esmsFile,
  LibbluESH264SpecProperties * param
)
{
  assert(NULL != param);

  /* [v8 constraint_flags] */
  if (writeByte(esmsFile, param->constraintFlags) < 0)
    return -1;

  /* [u32 cpbSize] */
  if (writeUint32(esmsFile, param->cpbSize) < 0)
    return -1;

  /* [u32 bitrate] */
  if (writeUint32(esmsFile, param->bitrate) < 0)
    return -1;

  return 0;
}

int writeAc3FmtSpecificInfos(
  BitstreamWriterPtr esmsFile,
  LibbluESAc3SpecProperties * param
)
{
  int ret;

  /* [u3 subSampleRate] [u5 bsid] */
  ret = writeByte(
    esmsFile,
    ((param->subSampleRate  &  0x7) << 5)
    | ((param->bsid         & 0x1F)     )
  );
  if (ret < 0)
    return -1;

  /* [b1 bitrateMode] [u5 bitrateCode] [u2 surroundCode] */
  ret = writeByte(
    esmsFile,
    ((param->bitrateMode    &  0x1) << 7)
    | ((param->bitrateCode  & 0x1F) << 2)
    | ((param->surroundCode &  0x3)     )
  );
  if (ret < 0)
    return -1;

  /* [u3 bsmod] [u4 numChannels] [b1 fullSVC] */
  ret = writeByte(
    esmsFile,
    ((param->bitrateMode    &  0x7) << 5)
    | ((param->numChannels  &  0xF) << 1)
    | ((param->fullSVC      &  0x1)     )
  );
  if (ret < 0)
    return -1;

  return 0;
}

int writeEsmsEsCodecSpecParametersSection(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
)
{
  int ret;

  const uint8_t * specificCodecHeader;

  assert(NULL != esmsFile);
  assert(NULL != script);

  switch (script->prop.codingType) {
    case STREAM_CODING_TYPE_H262:
    case STREAM_CODING_TYPE_AVC:
      /* [v64 videoSpecificCodecParamHeader] */
      specificCodecHeader = (uint8_t *) ES_CODEC_SPEC_PARAM_HEADER_VIDEO;
      if (writeBytes(esmsFile, specificCodecHeader, 8) < 0)
        return -1;

      /* [u4 videoFormat] [u4 frameRate] */
      ret = writeByte(
        esmsFile,
        ((script->prop.videoFormat & 0xF) << 4)
        | (script->prop.frameRate  & 0xF)
      );
      if (ret < 0)
        return -1;

      /* [u8 profileIDC] */
      if (writeByte(esmsFile, script->prop.profileIDC) < 0)
        return -1;

      /* [u8 levelIDC] */
      if (writeByte(esmsFile, script->prop.levelIDC) < 0)
        return -1;

      /* [b1 stillPicture] [v7 reserved] */
      ret = writeByte(
        esmsFile,
        script->prop.stillPicture << 7
      );
      if (ret < 0)
        return -1;

      if (script->prop.codingType == STREAM_CODING_TYPE_AVC) {
        if (writeH264FmtSpecificInfos(esmsFile, script->fmtSpecProp.h264) < 0)
          return -1;
      }
      break;

    case STREAM_CODING_TYPE_LPCM:
    case STREAM_CODING_TYPE_AC3:
    case STREAM_CODING_TYPE_DTS:
    case STREAM_CODING_TYPE_TRUEHD:
    case STREAM_CODING_TYPE_EAC3:
    case STREAM_CODING_TYPE_HDHR:
    case STREAM_CODING_TYPE_HDMA:
    case STREAM_CODING_TYPE_EAC3_SEC:
    case STREAM_CODING_TYPE_DTSE_SEC:
      /* [v64 audioSpecificFormatParamHeader] */
      specificCodecHeader = (uint8_t *) ES_CODEC_SPEC_PARAM_HEADER_AUDIO;
      if (writeBytes(esmsFile, specificCodecHeader, 8) < 0)
        return -1;

      /* [u4 audioFormat] [u4 sampleRate] */
      ret = writeByte(
        esmsFile,
        ((script->prop.audioFormat & 0xF) << 4)
        | (script->prop.sampleRate & 0xF)
      );
      if (ret < 0)
        return -1;

      /* [u8 bitDepth] */
      if (writeByte(esmsFile, script->prop.bitDepth) < 0)
        return -1;

      /* [v8 reserved] // 0x00 */
      if (writeByte(esmsFile, 0x00) < 0)
        return -1;

      if (
        script->prop.codingType == STREAM_CODING_TYPE_AC3
        || script->prop.codingType == STREAM_CODING_TYPE_TRUEHD
        || script->prop.codingType == STREAM_CODING_TYPE_EAC3
        || script->prop.codingType == STREAM_CODING_TYPE_EAC3_SEC
      ) {
        if (writeAc3FmtSpecificInfos(esmsFile, script->fmtSpecProp.ac3) < 0)
          return -1;
      }
      break;

    case STREAM_CODING_TYPE_IG:
    case STREAM_CODING_TYPE_PG:
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unknown codec specific informations for stream coding type 0x%x.\n",
        script->prop.codingType
      );
  }

  return 0;
}

int writeEsmsDataBlocksDefSection(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
)
{
  unsigned i;

  assert(NULL != esmsFile);
  assert(NULL != script);

  /* [u32 dataBlocksDefHeader] // "DTBK" */
  if (writeBytes(esmsFile, (uint8_t *) DATA_BLOCKS_DEF_HEADER, 4) < 0)
    return -1;

  /* [u8 nbDataBlocks] */
  if (writeByte(esmsFile, nbUsedEntriesEsmsDataBlocks(script->dataBlocks)) < 0)
    return -1;

  for (i = 0; i < nbUsedEntriesEsmsDataBlocks(script->dataBlocks); i++) {
    EsmsDataBlockEntry entry;

    if (getEsmsDataBlocks(script->dataBlocks, &entry, i) < 0)
      return -1;

    /* [u32 dataBlockLength[i]] */
    if (writeUint32(esmsFile, entry.size) < 0)
      return -1;

    /* [v<dataBlockLength[i]> dataBlock[i]] */
    if (0 < entry.size) {
      if (writeBytes(esmsFile, entry.data, entry.size) < 0)
        return -1;
    }
  }

  return 0;
}

int addEsmsFileEnd(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
)
{

#define APPEND_DIR(id)  appendDirEsms(script, id, tellWritingPos(esmsFile))

  /* Complete ESMS PES Cutting section. */
  if (script->commandsPipeline.nbFrames == 0) {
    /* Init section if never initialized */
    if (writeEsmsPesCuttingHeader(esmsFile, script) < 0)
      return -1;
  }
  /* [u8 endMarker] // ESMS PES Cutting section loop end marker. */
  if (writeByte(esmsFile, ESMS_SCRIPT_END_MARKER) < 0)
    return -1;

  /* ES Properties */
  if (APPEND_DIR(ESMS_DIRECTORY_ID_ES_PROP) < 0)
    return -1;

  if (writeEsmsEsPropertiesSection(esmsFile, script) < 0)
    return -1;

  if (0 < nbUsedEntriesEsmsDataBlocks(script->dataBlocks)) {
    /* Data blocks definition */
    if (APPEND_DIR(ESMS_DIRECTORY_ID_ES_DATA_BLK_DEF) < 0)
      return -1;

    if (writeEsmsDataBlocksDefSection(esmsFile, script) < 0)
      return -1;
  }

  /* Codec specific parameters */
  if (APPEND_DIR(ESMS_DIRECTORY_ID_ES_FMT_PROP) < 0)
    return -1;

  if (writeEsmsEsCodecSpecParametersSection(esmsFile, script) < 0)
    return -1;

#undef APPEND_DIR

  return 0;
}

static int initEsmsPesPacket(
  EsmsFileHeaderPtr script,
  const uint8_t pictType,
  bool extFrame,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
)
{
  EsmsPesPacketHeader * curFrame;

  assert(NULL != script);

  if (script->commandsPipeline.initFrame)
    LIBBLU_ERROR_RETURN("Attempt to double initialize ESMS PES frame.\n");

  curFrame = &script->commandsPipeline.curFrame;
  curFrame->pictureType = pictType;
  curFrame->extensionFrame = extFrame;
  curFrame->dtsPresent = dtsPres;
  curFrame->extParamPresent = false;
  curFrame->pts = pts;
  curFrame->dts = dts;
  curFrame->nbCommands = 0;

  script->commandsPipeline.initFrame = true;
  return 0;
}

int initEsmsVideoPesFrame(
  EsmsFileHeaderPtr script,
  const uint8_t pictType,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
)
{
  if (script->streamType != ES_VIDEO)
    LIBBLU_ERROR_RETURN("Can't create video PES frame in non-video stream.\n");

  return initEsmsPesPacket(script, pictType, false, dtsPres, pts, dts);
}

int initEsmsAudioPesFrame(
  EsmsFileHeaderPtr script,
  bool extFrame,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
)
{
  if (script->streamType != ES_AUDIO)
    LIBBLU_ERROR_RETURN("Can't create audio PES frame in non-audio stream.\n");

  return initEsmsPesPacket(script, false, extFrame, dtsPres, pts, dts);
}

int initEsmsHdmvPesFrame(
  EsmsFileHeaderPtr script,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
)
{
  if (script->streamType != ES_HDMV)
    LIBBLU_ERROR_RETURN("Can't create HDMV PES frame in non-HDMV stream.\n");

  return initEsmsPesPacket(script, false, false, dtsPres, pts, dts);
}

bool isEsmsPesPacketExtensionDataSupported(
  EsmsFileHeaderPtr script
)
{
  switch (script->prop.codingType) {
    case STREAM_CODING_TYPE_AVC:
      return true;

    default:
      break;
  }

  return false;
}

int setEsmsPesPacketExtensionData(
  EsmsFileHeaderPtr script,
  EsmsPesPacketExtData data
)
{
  if (!isEsmsPesPacketExtensionDataSupported(script))
    LIBBLU_ERROR_RETURN(
      "No codec specific ESMS PES frame extension data available.\n"
    );

  if (!script->commandsPipeline.initFrame)
    LIBBLU_ERROR_RETURN(
      "Missing a pending destination initialized PES frame "
      "for the extension data.\n"
    );

  if (script->commandsPipeline.curFrame.extParamPresent)
    LIBBLU_ERROR_RETURN(
      "Extension data as already been defined for the pending PES frame.\n"
    );

  script->commandsPipeline.curFrame.extParam = data;
  script->commandsPipeline.curFrame.extParamPresent = true;

  return 0;
}

static EsmsCommand * newCommand(
  EsmsFileHeaderPtr script,
  EsmsCommandType type
)
{
  EsmsPesPacketHeader * curFrame;

  curFrame = &script->commandsPipeline.curFrame;

  if (MAX_SUPPORTED_COMMANDS <= curFrame->nbCommands)
    LIBBLU_ERROR_NRETURN(
      "Too many scripting commands in current ESMS PES frame.\n"
    );

  return initEsmsCommand(curFrame->commands + curFrame->nbCommands++, type);
}

int appendAddDataCommand(
  EsmsFileHeaderPtr script,
  const uint32_t offset,
  const EsmsDataInsertionMode mode,
  const uint16_t dataLength,
  const uint8_t * data
)
{
  EsmsCommand * command;

#if ENABLE_MAX_ADD_DATA_BYTES
  if (MAX_ADD_DATA_BYTES < dataLength)
    LIBBLU_ERROR_RETURN(
      "\"Add data\" command insertingData field length exceed maximum "
      "supported length (%u < %" PRIu16 ").\n",
      MAX_ADD_DATA_BYTES,
      dataLength
    );
#endif

  if (NULL == (command = newCommand(script, ESMS_ADD_DATA)))
    return -1;

  return setEsmsAddDataCommand(
    &command->data.addData,
    offset,
    mode,
    data,
    dataLength
  );
}

int appendChangeByteOrderCommand(
  EsmsFileHeaderPtr script,
  const uint8_t valueLength,
  const uint32_t swappingDomainStartOffset,
  const uint32_t swappingDomainLength
)
{
  EsmsCommand * command;

  if (swappingDomainLength % valueLength) {
    /* Data field is not composed of complete blocks of valueLength bytes,
    swapping cannot be processed. */
    LIBBLU_ERROR_RETURN(
      "Byte order change command domain length is invalid and shall be a "
      "modulo of valueLength %" PRIu8 ".\n", valueLength
    );
  }

  if (NULL == (command = newCommand(script, ESMS_CHANGE_BYTEORDER)))
    return -1;

  return setEsmsChangeByteOrderCommand(
    &command->data.changeByteOrder,
    valueLength,
    swappingDomainStartOffset,
    swappingDomainLength
  );
}

int appendAddPesPayloadCommand(
  EsmsFileHeaderPtr script,
  const unsigned fileIdx,
  const uint32_t offset,
  const uint64_t fileOffset,
  const uint32_t payloadLength
)
{
  EsmsCommand * command;

  assert(NULL != script);
  assert(0 < payloadLength);

  if (script->sourceFiles.nbUsedFiles <= fileIdx)
    LIBBLU_ERROR_RETURN("Unknown source file index %u.\n", fileIdx);

  if (NULL == (command = newCommand(script, ESMS_ADD_PAYLOAD_DATA)))
    return -1;

  return setEsmsAddPesPayloadCommand(
    &command->data.addPesPayload,
    fileIdx,
    offset,
    fileOffset,
    payloadLength
  );
}

int appendPaddingDataCommand(
  EsmsFileHeaderPtr script,
  const uint32_t offset,
  const EsmsDataInsertionMode mode,
  const uint16_t length,
  const uint8_t paddingByte
)
{
  EsmsCommand * command;

  assert(NULL != script);
  assert(0 < length);

  if (NULL == (command = newCommand(script, ESMS_ADD_PADDING_DATA)))
    return -1;

  return setEsmsAddPaddingCommand(
    &command->data.addPadding,
    offset,
    mode,
    length,
    paddingByte
  );
}

int appendAddDataBlockCommand(
  EsmsFileHeaderPtr script,
  const uint32_t offset,
  const EsmsDataInsertionMode mode,
  const uint8_t dataSectBlockIdx
)
{
  EsmsCommand * command;

  assert(NULL != script);

  if (NULL == (command = newCommand(script, ESMS_ADD_DATA_SECTION)))
    return -1;

  return setEsmsAddDataBlockCommand(
    &command->data.addDataBlock,
    offset,
    mode,
    dataSectBlockIdx
  );
}

uint32_t computePesFrameLength(
  const EsmsCommand * commands,
  int nbCommands,
  EsmsDataBlocks dataBlocks
)
{
  uint32_t length = 0;
  int i;

  for (i = 0; i < nbCommands; i++) {
    switch (commands[i].type) {
      case ESMS_ADD_DATA:
        switch (commands[i].data.addData.mode) {
          case INSERTION_MODE_ERASE:
            length = MAX(
              length,
              commands[i].data.addData.offset
              + commands[i].data.addData.dataLength
            );
            break;

          case INSERTION_MODE_INSERT:
            length += commands[i].data.addData.dataLength;
        }
        break;

      case ESMS_ADD_PAYLOAD_DATA:
        length = MAX(
          length,
          commands[i].data.addPesPayload.dstOffset
          + commands[i].data.addPesPayload.size
        );
        break;

      case ESMS_ADD_PADDING_DATA:
        length = MAX(
          length,
          commands[i].data.addPadding.offset
          + commands[i].data.addPadding.length
        );
        break;

      case ESMS_CHANGE_BYTEORDER:
        break;

      case ESMS_ADD_DATA_SECTION:
        switch (commands[i].data.addDataBlock.mode) {
          case INSERTION_MODE_ERASE:
            length = MAX(
              length,
              commands[i].data.addDataBlock.offset
              + dataBlocks.entries[
                commands[i].data.addDataBlock.blockIdx
              ].size
            );
            break;

          case INSERTION_MODE_INSERT:
            length += dataBlocks.entries[
              commands[i].data.addDataBlock.blockIdx
            ].size;
        }
    }
  }

  return length;
}