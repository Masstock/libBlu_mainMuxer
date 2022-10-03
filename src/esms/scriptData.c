#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "../util/common.h"

#include "scriptData.h"

/* ### ESMS ES Source Files : ############################################## */

static lbc * checkAndDupFilepathEsmsESSourceFiles(
  const lbc * filepath
)
{
  size_t size;
  lbc * copy;

  size = lbc_strlen(filepath);
  if (!size || ESMS_MAX_FILENAME_LEN <= size)
    LIBBLU_ERROR_NRETURN(
      "Invalid source filepath size '%" PRI_LBCS "'.\n",
      filepath
    );

  if (NULL == (copy = lbc_strdup(filepath)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  return copy;
}

int appendEsmsESSourceFiles(
  EsmsESSourceFiles * dst,
  const lbc * filepath,
  EsmsESSourceFile properties
)
{
  unsigned idx;
  lbc * copy;

  if (NULL != dst->handles)
    LIBBLU_ERROR_RETURN(
      "ESMS source files list can no longer be edited after opening them.\n"
    );

  if (NULL == (copy = checkAndDupFilepathEsmsESSourceFiles(filepath)))
    return -1;

  if (dst->nbAllocatedFiles <= dst->nbUsedFiles) {
    lbc ** newPathsArray;
    EsmsESSourceFile * newPropArray;
    size_t newSize;

    if (ESMS_MAX_SUPPORTED_NB_ES_SOURCE_FILES <= dst->nbUsedFiles)
      LIBBLU_ERROR_FRETURN("ESMS source files number limit reached.\n");

    newSize = GROW_ALLOCATION(
      dst->nbAllocatedFiles,
      ESMS_DEFAULT_NB_ES_SOURCE_FILES
    );
    if (lb_mul_overflow(newSize, sizeof(lbc *)))
      LIBBLU_ERROR_FRETURN("ESMS source files number overflow.\n");
    if (lb_mul_overflow(newSize, sizeof(EsmsESSourceFile)))
      LIBBLU_ERROR_FRETURN("ESMS source files number overflow.\n");

    newPathsArray = (lbc **) realloc(dst->filepaths, newSize * sizeof(lbc *));
    newPropArray = (EsmsESSourceFile *) realloc(dst->properties, newSize * sizeof(EsmsESSourceFile));
    if (NULL == newPathsArray || NULL == newPropArray) {
      /* Set fields to avoid use-after free if one of the calls succeed */
      if (NULL != newPathsArray)
        dst->filepaths = newPathsArray;
      if (NULL != newPropArray)
        dst->properties = newPropArray;
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
    }

    dst->filepaths = newPathsArray;
    dst->properties = newPropArray;
    dst->nbAllocatedFiles = newSize;
  }

  idx = dst->nbUsedFiles++;
  dst->filepaths[idx] = copy;
  dst->properties[idx] = properties;

  return 0;

free_return:
  free(copy);
  return -1;
}

int openAllEsmsESSourceFiles(
  EsmsESSourceFiles * dst
)
{
  unsigned i;

  if (!dst->nbUsedFiles)
    LIBBLU_ERROR_RETURN("No ESMS source file to open.\n");
  if (NULL != dst->handles)
    LIBBLU_ERROR_RETURN("ESMS source files opening already done.\n");

  dst->handles = (BitstreamReaderPtr *) malloc(
    dst->nbUsedFiles * sizeof(BitstreamReaderPtr)
  );
  if (NULL == dst->handles)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  for (i = 0; i < dst->nbUsedFiles; i++)
    dst->handles[i] = NULL;
  for (i = 0; i < dst->nbUsedFiles; i++) {
    if (NULL == (dst->handles[i] = createBitstreamReaderDefBuf(dst->filepaths[i])))
      return -1;
  }

  return 0;
}

/* ### ESMS Directories : ################################################## */

/* ### ESMS Data Blocks : ################################################## */

int appendEsmsDataBlocks(
  EsmsDataBlocks * dst,
  EsmsDataBlockEntry entry,
  unsigned * idx
)
{
  unsigned dstIdx;

  assert(NULL != dst);

  if (ESMS_MAX_SUPPORTED_DATA_BLOCKS_ENTRIES <= dst->nbUsedEntries)
    LIBBLU_ERROR_RETURN("Too many Data Blocks definitions.\n");

  if (dst->nbAllocatedEntries <= dst->nbUsedEntries) {
    EsmsDataBlockEntry * newArray;
    unsigned newSize;

    newSize = GROW_ALLOCATION(
      dst->nbAllocatedEntries,
      ESMS_DEFAULT_NB_DATA_BLOCKS_ENTRIES
    );
    if (lb_mul_overflow(newSize, sizeof(EsmsDataBlockEntry)))
      LIBBLU_ERROR_RETURN("Data Blocks definitions number overflow.\n");

    newArray = (EsmsDataBlockEntry *) realloc(
      dst->entries,
      newSize * sizeof(EsmsDataBlockEntry)
    );
    if (NULL == newArray)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    dst->entries = newArray;
    dst->nbAllocatedEntries = newSize;
  }

  dstIdx = dst->nbUsedEntries++;
  dst->entries[dstIdx] = entry;

  if (NULL != idx)
    *idx = dstIdx;

  return 0;
}

int updateEsmsDataBlocks(
  EsmsDataBlocks * dst,
  EsmsDataBlockEntry newEntry,
  unsigned dstIdx
)
{
  assert(NULL != dst);

  if (dst->nbUsedEntries <= dstIdx)
    LIBBLU_ERROR_RETURN("Out of range Data Block definition idx %u.\n", dstIdx);

  cleanEsmsDataBlockEntry(dst->entries[dstIdx]);
  dst->entries[dstIdx] = newEntry;

  return 0;
}

/* ### ESMS Script commands : ############################################## */

/* ###### Add Data command : ############################################### */

int setEsmsAddDataCommand(
  EsmsAddDataCommand * dst,
  uint32_t offset,
  EsmsDataInsertionMode mode,
  const uint8_t * data,
  uint16_t dataLength
)
{
  uint8_t * newData;

  assert(NULL != dst);
  assert(NULL != data);
  assert(0 < dataLength);

  if (NULL == (newData = (uint8_t *) malloc(dataLength)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  memcpy(newData, data, dataLength);

  *dst = (EsmsAddDataCommand) {
    .offset = offset,
    .mode = mode,
    .data = newData,
    .dataLength = dataLength
  };

  return 0;
}

int applyEsmsAddDataCommand(
  EsmsAddDataCommand command,
  uint8_t * packetData,
  size_t packetSize
)
{
  assert(NULL != packetData);

  if (packetSize < command.offset + command.dataLength)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range add data insertion position "
      "(%zu < %" PRIu32 " + %" PRIu16 ").\n",
      packetSize,
      command.offset,
      command.dataLength
    );

  if (command.mode == INSERTION_MODE_INSERT) {
    /* Firstly shifting frame data, before data insertion. */
    memmove(
      packetData + command.offset,
      packetData + command.offset + command.dataLength,
      packetSize - command.offset - command.dataLength
    );
  }

  memcpy(packetData + command.offset, command.data, command.dataLength);

  return 0;
}

/* ###### ESMS Change Byte Order command : ################################# */

int applyEsmsChangeByteOrderCommand(
  EsmsChangeByteOrderCommand command,
  uint8_t * packetData,
  size_t packetSize
)
{
  uint8_t temp[32];
  size_t off, halfUnitSize;

  assert(NULL != packetData);

  if (packetSize < command.offset + command.length)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range byte order swapping domain "
      "(%zu < %" PRIu32 " + %" PRIu32 ").\n",
      packetSize,
      command.offset,
      command.length
    );

  if (32 < (halfUnitSize = command.unitSize >> 1))
    LIBBLU_ERROR_RETURN(
      "Broken script, unexpected byte order swapping value length "
      "of %zu bytes.",
      halfUnitSize
    );

  for (
    off = command.offset;
    off < command.offset + command.length;
    off += command.unitSize
  ) {
    memcpy(temp, packetData + off, halfUnitSize);
    memcpy(
      packetData + off,
      packetData + off + command.unitSize - halfUnitSize,
      halfUnitSize
    );
    memcpy(
      packetData + off + command.unitSize - halfUnitSize,
      temp,
      halfUnitSize
    );
  }

  return 0;
}

/* ###### ESMS Add PES Payload command : ################################### */

int applyEsmsAddPesPayloadCommand(
  EsmsAddPesPayloadCommand command,
  uint8_t * packetData,
  size_t packetSize,
  const BitstreamReaderPtr * sourceFilesHandles,
  unsigned nbSourceFiles
)
{
  BitstreamReaderPtr srcFileHandle;
  int64_t srcFileOff;

  assert(NULL != packetData);

  if (packetSize < command.dstOffset + command.size)
    LIBBLU_ERROR_RETURN(
      "Broken script, "
      "out of range data insertion domain (%zu < %zu + %zu).\n",
      packetSize,
      command.dstOffset,
      command.size
    );

  if (nbSourceFiles <= command.fileIdx)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range file idx handle (%u <= %zu).\n",
      nbSourceFiles,
      command.fileIdx
    );

  srcFileHandle = sourceFilesHandles[command.fileIdx];
  srcFileOff = tellPos(srcFileHandle);

  /* Reaching PES frame data start reading offset in source file */
  if (command.srcOffset < srcFileOff) {
    /* Lower position, use of seeking. */
    if (seekPos(srcFileHandle, command.srcOffset, SEEK_SET) < 0)
      LIBBLU_ERROR_RETURN("Broken script, invalid seeking offset.\n");
  }
  else if (srcFileOff < command.srcOffset) {
    /* Greater position, use of skipping. */
    if (skipBytes(srcFileHandle, command.srcOffset - srcFileOff) < 0)
      LIBBLU_ERROR_RETURN("Broken script, invalid seeking offset.\n");
  }

  return readBytes(
    srcFileHandle,
    packetData + command.dstOffset,
    command.size
  );
}

/* ###### ESMS Add Padding command : ####################################### */

int applyEsmsAddPaddingCommand(
  EsmsAddPaddingCommand command,
  uint8_t * packetData,
  size_t packetSize
)
{
  assert(NULL != packetData);

  if (packetSize < command.offset + command.length)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range padding insertion domain "
      "(%zu < %" PRIu32 " + %" PRIu32 ").\n",
      packetSize,
      command.offset,
      command.length
    );

  if (command.mode == INSERTION_MODE_INSERT) {
    /* Firstly shifting frame data, before data insertion. */
    memmove(
      packetData + command.offset + command.length,
      packetData + command.offset,
      packetSize - command.offset - command.length
    );
  }

  memset(packetData + command.offset, command.byte, command.length);

  return 0;
}

/* ###### ESMS Add Data Block command : #################################### */

int applyEsmsAddDataBlockCommand(
  EsmsAddDataBlockCommand command,
  uint8_t * packetData,
  size_t packetSize,
  EsmsDataBlocks dataBlocks
)
{
  EsmsDataBlockEntry entry;

  assert(NULL != packetData);

  if (dataBlocks.nbUsedEntries <= command.blockIdx)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range data block idx (%u <= %" PRIu8 ").\n",
      dataBlocks.nbUsedEntries,
      command.blockIdx
    );

  entry = dataBlocks.entries[command.blockIdx];
  if (packetSize < command.offset + entry.size)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range data block insertion domain "
      "(%zu < %" PRIu32 " + %" PRIu32 ").\n",
      packetSize,
      command.offset,
      entry.size
    );

  if (!entry.size)
    return 0; /* No data */

  if (command.mode == INSERTION_MODE_INSERT) {
    /* Firstly shifting frame data, before data insertion. */
    memmove(
      packetData + command.offset + entry.size,
      packetData + command.offset,
      packetSize - command.offset - entry.size
    );
  }

  memcpy(packetData + command.offset, entry.data, entry.size);

  return 0;
}

/* ###### ESMS script command node : ####################################### */

EsmsCommandNodePtr createEsmsCommandNode(
  EsmsCommandType type
)
{
  EsmsCommandNodePtr node;

  if (NULL == (node = (EsmsCommandNodePtr) malloc(sizeof(EsmsCommandNode))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  node->next = NULL;
  initEsmsCommand(&node->command, type);

  return node;
}

/* ### ESMS script PES packet node : ####################################### */

EsmsPesPacketNodePtr createEsmsPesPacketNode(
  void
)
{
  EsmsPesPacketNodePtr node;

  node = (EsmsPesPacketNodePtr) malloc(sizeof(EsmsPesPacketNode));
  if (NULL == node)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  node->next = NULL;
  node->extensionFrame = false;
  node->dtsPresent = false;
  node->pts = 0;
  node->dts = 0;
  node->length = 0;
  node->commands = NULL;

  return node;
}

/* ### ESMS files utilities : ############################################## */

const char * ESMSDirectoryIdStr(
  ESMSDirectoryId id
)
{
  static const char * dirs[] = {
    "unknown",
    "ES Properties",
    "ES PES Cutting",
    "ES Format Properties",
    "ES Data Blocks Definition"
  };

  if (id < ARRAY_SIZE(dirs))
    return dirs[id];
  return "unknown";
}

ESMSDirectoryFetcherErrorCodes getDirectoryOffset(
  const lbc * essFilename,
  ESMSDirectoryId id,
  uint64_t * offset
)
{
  ESMSDirectoryFetcherErrorCodes ret;
  FILE * essInputFile;
  uint64_t offsetValue;
  uint8_t i, directoryNb, directoryId;

  if (NULL == (essInputFile = lbc_fopen(essFilename, "rb")))
    return ESMS_DF_READ_ERROR; /* Opening error */

  /* [u32 esmsFileHeader] */
  /* [u8 formatVersion] */
  /* [u8 completedFile] */
  if (fseek(essInputFile, 0x6, SEEK_CUR) < 0) {
    ret = ESMS_DF_READ_ERROR;
    goto free_return;
  }

  /* [u8 directoryNb] */
  if (!RA_FILE(essInputFile, &directoryNb, 1)) {
    ret = ESMS_DF_READ_ERROR;
    goto free_return;
  }

  for (i = 0; i < directoryNb; i++) {
    /* [u8 directoryId] */
    if (!RA_FILE(essInputFile, &directoryId, 1)) {
      ret = ESMS_DF_READ_ERROR;
      goto free_return;
    }

    /* [u64 directoryOffset] */
    if (!RA_FILE(essInputFile, &offsetValue, 8)) {
      ret = ESMS_DF_READ_ERROR;
      goto free_return;
    }

    if (directoryId == id) {
      if (NULL != offset)
        *offset = offsetValue;
      ret = ESMS_DF_OK;
      goto free_return;
    }
  }

  ret = ESMS_DF_NOT_FOUND;

free_return:
  if (fclose(essInputFile) != 0)
    return ESMS_DF_READ_ERROR;
  return ret;
}

int isPresentDirectory(
  const lbc * essFilename,
  ESMSDirectoryId id
)
{
  switch (getDirectoryOffset(essFilename, id, NULL)) {
    case ESMS_DF_OK:
      return 1;

    case ESMS_DF_READ_ERROR:
      break;

    case ESMS_DF_NOT_FOUND:
      return 0;
  }

  return -1;
}

int seekDirectoryOffset(
  BitstreamReaderPtr essHandle,
  const lbc * essFilename,
  ESMSDirectoryId id
)
{
  uint64_t offset;

  switch (getDirectoryOffset(essFilename, id, &offset)) {
    case ESMS_DF_OK:
      break; /* OK */

    case ESMS_DF_READ_ERROR:
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', "
        "reading error.\n",
        essFilename
      );

    case ESMS_DF_NOT_FOUND:
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', "
        "unable to find directory \"%s\".\n",
        essFilename,
        ESMSDirectoryIdStr(id)
      );
  }

  if (seekPos(essHandle, offset, SEEK_SET) < 0)
    LIBBLU_ERROR_RETURN(
      "Broken script '%" PRI_LBCS "', "
      "offset pointing outside of file.\n",
      essFilename
    );

  return 0;
}

int checkDirectoryMagic(
  BitstreamReaderPtr script,
  const char * magic,
  size_t magicSize
)
{
  uint8_t buf[9] = {'\0'};

  assert(0 < magicSize && magicSize <= 8);

  if (readBytes(script, buf, magicSize) < 0)
    return -1;

  if (0 != memcmp(buf, (void *) magic, magicSize))
    LIBBLU_ERROR_RETURN(
      "Broken script, unexpected section header '%s', expect '%s'.\n",
      (char *) buf,
      magic
    );

  return 0;
}

const char * ESMSValidatorErrorStr(
  const ESMSFileValidatorRet code
)
{
  switch (code) {
    case ESMS_FV_OK:
      return "Using already builded script.";
    case ESMS_FV_NO_SCRIPT:
      return "No script found, generating script file...";
    case ESMS_FV_READ_ERROR:
      return "Reading error in script, re-generating script file...";
    case ESMS_FV_HEADER_ERROR:
      return "Error in script header, re-generating script file...";
    case ESMS_FV_VERSION_ERROR:
      return "Incompatible script version, re-generating compatible "
        "script file...";
    case ESMS_FV_INCOMPLETE_FILE:
      return "Unfinished script, generating script file...";
    case ESMS_FV_INCOMPATIBLE_FLAGS:
      return "Incompatible script, generating script file...";
    case ESMS_FV_INVALID_SOURCE_FILE:
      return "Incompatible source file checksum, generating script file...";
    case ESMS_FV_MEMORY_ERROR:
      return "Memory allocation error.";
  }

  return "Unknown error code.";
}

void printESMSValidatorError(ESMSFileValidatorRet code)
{
  lbc_printf("%s\n", ESMSValidatorErrorStr(code));
}

static ESMSFileValidatorRet isValidESMSFileSourceFile(
  const lbc * sourceFilepath,
  unsigned crcControlledSize,
  uint32_t crcExpectedResult
)
{
  ESMSFileValidatorRet ret;

  BitstreamReaderPtr file;
  uint8_t * crcBuffer;

  if (lbc_access_fp(sourceFilepath, "rb") < 0)
    return ESMS_FV_INVALID_SOURCE_FILE;

  file = createBitstreamReader(sourceFilepath, READ_BUFFER_LEN);
  if (NULL == file)
    return ESMS_FV_INVALID_SOURCE_FILE;

  if (NULL == (crcBuffer = (uint8_t *) malloc(crcControlledSize))) {
    ret = ESMS_FV_MEMORY_ERROR;
    goto free_return;
  }

  if (
    readBytes(file, crcBuffer, crcControlledSize) < 0
    || crcExpectedResult != lb_compute_crc32(crcBuffer, 0, crcControlledSize)
  ) {
    /* CRC-32 values mismatch */
    ret = ESMS_FV_INVALID_SOURCE_FILE;
    goto free_return;
  }

  free(crcBuffer);
  closeBitstreamReader(file);

  return ESMS_FV_OK;

free_return:
  free(crcBuffer);
  closeBitstreamReader(file);

  return ret;
}

ESMSFileValidatorRet isAValidESMSFile(
  const lbc * essFileName,
  const uint64_t flags,
  unsigned * version
)
{
  ESMSFileValidatorRet ret;

  FILE * scriptFile;
  uint8_t buf[8];

  ESMSDirectoryFetcherErrorCodes dirErrCode;
  uint64_t dirOff;

  unsigned i, nbStreams;

  unsigned scriptVersion;
  uint64_t calculatedFlags;

  assert(NULL != essFileName);

  if (NULL == essFileName)
    return ESMS_FV_NO_SCRIPT;

  if (NULL == (scriptFile = lbc_fopen(essFileName, "rb"))) {
    ret = (errno == ENOENT) ? ESMS_FV_NO_SCRIPT : ESMS_FV_READ_ERROR;
    errno = 0; /* Clear errno */

    return ret;
  }

  /* [v32 esmsFileHeader] */
  if (!RA_FILE(scriptFile, buf, 4))
    goto read_error;

  if (!lb_strn_equal((char *) buf, (char *) ESMS_FILE_HEADER, 4)) {
    ret = ESMS_FV_HEADER_ERROR;
    goto free_return;
  }

  /* [u8 formatVersion] */
  if (!RA_FILE(scriptFile, buf, 1))
    goto read_error;
  scriptVersion = buf[0];

  if (CURRENT_ESMS_FORMAT_VER != scriptVersion) {
    ret = ESMS_FV_VERSION_ERROR;
    goto free_return;
  }

  if (NULL != version)
    *version = scriptVersion;

  /* [u8 completedFile] */
  if (!RA_FILE(scriptFile, buf, 1))
    goto read_error;

  if (buf[0] != 0x01) {
    ret = ESMS_FV_INCOMPLETE_FILE;
    goto free_return;
  }

  dirErrCode = getDirectoryOffset(
    essFileName,
    ESMS_DIRECTORY_ID_ES_PROP,
    &dirOff
  );
  if (dirErrCode < 0)
    return ESMS_FV_READ_ERROR;

  /* Check 'scriptingFlags' */
  if (fseek(scriptFile, dirOff + ES_SCRIPTING_FLAGS_FIELD_POS, SEEK_SET) < 0)
    goto read_error;

  /* [v64 scriptingFlags] */
  if (!RA_FILE(scriptFile, buf, 8))
    goto read_error;
  calculatedFlags = UINT8A_TO_UINT64(buf);

  if (calculatedFlags != flags) {
    ret = ESMS_FV_INCOMPATIBLE_FLAGS;
    goto free_return;
  }

  /* Check input files checksums */
  /* [u8 nbSourceFiles] */
  if (!RA_FILE(scriptFile, buf, 1))
    goto read_error;
  nbStreams = buf[0];

  if (ESMS_MAX_ALLOWED_DIR < nbStreams) {
    ret = ESMS_FV_HEADER_ERROR;
    goto free_return;
  }

  for (i = 0; i < nbStreams; i++) {
    unsigned utf8FilepathSize;
    uint8_t * utf8Filepath;
    lbc * filepath;

    unsigned crcControlledSize;
    uint32_t crcExpectedResult;

    /* [u16 sourceFileNameLength] */
    if (!RA_FILE(scriptFile, buf, 2))
      goto read_error;
    utf8FilepathSize = UINT8A_TO_UINT16(buf);

    if (!utf8FilepathSize) {
      ret = ESMS_FV_HEADER_ERROR;
      goto free_return;
    }

    if (NULL == (utf8Filepath = (uint8_t *) malloc(utf8FilepathSize + 1))) {
      ret = ESMS_FV_MEMORY_ERROR;
      goto free_return;
    }

    /* [v<sourceFileNameLength> sourceFileName] */
    if (!RA_FILE(scriptFile, utf8Filepath, utf8FilepathSize)) {
      free(utf8Filepath);
      goto read_error;
    }
    utf8Filepath[utf8FilepathSize] = '\0'; /* Add the terminating NUL */

    if (NULL == (filepath = lbc_utf8_convto(utf8Filepath))) {
      ret = ESMS_FV_INVALID_SOURCE_FILE;
      goto free_return;
    }
    free(utf8Filepath);

    /* [u16 crcCheckedBytes] */
    if (!RA_FILE(scriptFile, buf, 2)) {
      free(filepath);
      goto read_error;
    }
    crcControlledSize = UINT8A_TO_UINT16(buf);

    /* [u32 crc] */
    if (!RA_FILE(scriptFile, buf, 4)) {
      free(filepath);
      goto read_error;
    }
    crcExpectedResult = UINT8A_TO_UINT32(buf);

    ret = isValidESMSFileSourceFile(
      filepath,
      crcControlledSize,
      crcExpectedResult
    );

    free(filepath);

    if (ret < 0)
      goto free_return;
  }

  fclose(scriptFile);
  return ESMS_FV_OK;

read_error:
  ret = ESMS_FV_READ_ERROR;
free_return:
  fclose(scriptFile);
  errno = 0; /* Clear errno */

  return ret;
}