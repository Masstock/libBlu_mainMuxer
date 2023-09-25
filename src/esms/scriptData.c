#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "../util/common.h"

#include "scriptData.h"

#define FREAD(fd, d, s, e)                                                    \
  do {                                                                        \
    uint8_t _buf[s];                                                          \
    size_t _s = (s);                                                          \
    if (!fread(_buf, _s, 1, fd))                                              \
      e;                                                                      \
    uint64_t _v = 0;                                                          \
    for (size_t _i = 0; _i < _s; _i++)                                        \
      _v = (_v << 8) | _buf[_i];                                              \
    *(d) = _v;                                                                \
  } while (0)

#define READ_VALUE(f, s, d, e)                                                \
  do {                                                                        \
    uint64_t _val;                                                            \
    if (readValue64BigEndian(f, s, &_val) < 0)                                \
      e;                                                                      \
    if (NULL != (d))                                                          \
      *(d) = _val;                                                            \
  } while (0)

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
  EsmsESSourceFileProperties properties
)
{
  if (dst->opened)
    LIBBLU_ERROR_RETURN(
      "ESMS source files list can no longer be edited after opening them.\n"
    );

  lbc * filepath_copy;
  if (NULL == (filepath_copy = checkAndDupFilepathEsmsESSourceFiles(filepath)))
    return -1;

  if (ESMS_MAX_SUPPORTED_NB_ES_SOURCE_FILES <= dst->nb_source_files)
    LIBBLU_ERROR_FRETURN("ESMS source files number limit reached.\n");

  dst->entries[dst->nb_source_files++] = (EsmsESSourceFilesEntry) {
    .filepath = filepath_copy,
    .properties = properties
  };

  return 0;

free_return:
  free(filepath_copy);
  return -1;
}

int openAllEsmsESSourceFiles(
  EsmsESSourceFiles * dst
)
{

  if (!dst->nb_source_files)
    LIBBLU_ERROR_RETURN("No ESMS source file to open.\n");
  if (dst->opened)
    LIBBLU_ERROR_RETURN("ESMS source files opening already done.\n");

  for (unsigned i = 0; i < dst->nb_source_files; i++) {
    EsmsESSourceFilesEntry * entry = &dst->entries[i];
    if (NULL == (entry->handle = createBitstreamReaderDefBuf(entry->filepath)))
      return -1;
  }

  return 0;
}

/* ### ESMS Directories : ################################################## */

/* ### ESMS Data Blocks : ################################################## */

int appendEsmsDataBlocks(
  EsmsDataBlocks * dst,
  EsmsDataBlockEntry entry,
  unsigned * data_block_id
)
{
  unsigned dst_id;

  assert(NULL != dst);

  if (ESMS_MAX_SUPPORTED_DATA_BLOCKS_ENTRIES <= dst->nb_data_blocks)
    LIBBLU_ERROR_RETURN("Too many Data Blocks definitions.\n");

  if (dst->nb_alloc_data_blocks <= dst->nb_data_blocks) {
    unsigned new_size = GROW_ALLOCATION(
      dst->nb_alloc_data_blocks,
      ESMS_DEFAULT_NB_DATA_BLOCKS_ENTRIES
    );

    if (lb_mul_overflow(new_size, sizeof(EsmsDataBlockEntry)))
      LIBBLU_ERROR_RETURN("Data Blocks definitions number overflow.\n");
    size_t alloc_size = new_size * sizeof(EsmsDataBlockEntry);

    EsmsDataBlockEntry * new_array = realloc(dst->entries, alloc_size);
    if (NULL == new_array)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    dst->entries = new_array;
    dst->nb_alloc_data_blocks = new_size;
  }

  dst_id = dst->nb_data_blocks++;
  dst->entries[dst_id] = entry;

  if (NULL != data_block_id)
    *data_block_id = dst_id;

  return 0;
}

int updateEsmsDataBlocks(
  EsmsDataBlocks * dst,
  EsmsDataBlockEntry updated_entry,
  unsigned dstIdx
)
{
  assert(NULL != dst);

  if (dst->nb_data_blocks <= dstIdx)
    LIBBLU_ERROR_RETURN("Out of range Data Block definition idx %u.\n", dstIdx);

  cleanEsmsDataBlockEntry(dst->entries[dstIdx]);
  dst->entries[dstIdx] = updated_entry;

  return 0;
}

/* ### ESMS Script commands : ############################################## */

/* ###### Add Data command : ############################################### */

int applyEsmsAddDataCommand(
  EsmsAddDataCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size
)
{
  assert(NULL != payload_dst);

  if (payload_size < command.insert_offset + command.data_size)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range add data insertion position "
      "(%" PRIu32 " < %" PRIu32 " + %" PRIu16 ").\n",
      payload_size,
      command.insert_offset,
      command.data_size
    );

  if (command.insert_mode == INSERTION_MODE_INSERT) {
    /* Firstly shifting frame data, before data insertion. */
    memmove(
      payload_dst + command.insert_offset,
      payload_dst + command.insert_offset + command.data_size,
      payload_size - command.insert_offset - command.data_size
    );
  }

  memcpy(payload_dst + command.insert_offset, command.data, command.data_size);

  return 0;
}

/* ###### ESMS Change Byte Order command : ################################# */

int applyEsmsChangeByteOrderCommand(
  EsmsChangeByteOrderCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size
)
{
  uint8_t temp[32];
  size_t off, halfUnitSize;

  assert(NULL != payload_dst);

  if (payload_size < command.section_offset + command.section_size)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range byte order swapping domain "
      "(%" PRIu32 " < %" PRIu32 " + %" PRIu32 ").\n",
      payload_size,
      command.section_offset,
      command.section_size
    );

  if (32 < (halfUnitSize = command.unit_size >> 1))
    LIBBLU_ERROR_RETURN(
      "Broken script, unexpected byte order swapping value length "
      "of %zu bytes.",
      halfUnitSize
    );

  for (
    off = command.section_offset;
    off < command.section_offset + command.section_size;
    off += command.unit_size
  ) {
    memcpy(temp, payload_dst + off, halfUnitSize);
    memcpy(
      payload_dst + off,
      payload_dst + off + command.unit_size - halfUnitSize,
      halfUnitSize
    );
    memcpy(
      payload_dst + off + command.unit_size - halfUnitSize,
      temp,
      halfUnitSize
    );
  }

  return 0;
}

/* ###### ESMS Add PES Payload command : ################################### */

int applyEsmsAddPesPayloadCommand(
  EsmsAddPesPayloadCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size,
  const EsmsESSourceFiles * src_files
)
{
  assert(NULL != payload_dst);

  if (payload_size < command.insert_offset + command.size)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range data insertion domain "
      "(%" PRIu32 " < %" PRIu32 " + %" PRIu32 ").\n",
      payload_size,
      command.insert_offset,
      command.size
    );

  if (src_files->nb_source_files <= command.src_file_id)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range file idx handle (%u <= %u).\n",
      src_files->nb_source_files,
      command.src_file_id
    );

  BitstreamReaderPtr esfile_bs = src_files->entries[command.src_file_id].handle;
  int64_t esfile_off = tellPos(esfile_bs);

  /* Reaching PES frame data start reading offset in source file */
  if (command.src_offset < esfile_off) {
    /* Lower position, use of seeking. */
    if (seekPos(esfile_bs, command.src_offset, SEEK_SET) < 0)
      LIBBLU_ERROR_RETURN("Broken script, invalid seeking offset.\n");
  }
  else if (esfile_off < command.src_offset) {
    /* Greater position, use of skipping. */
    if (skipBytes(esfile_bs, command.src_offset - esfile_off) < 0)
      LIBBLU_ERROR_RETURN("Broken script, invalid seeking offset.\n");
  }

  if (readBytes(esfile_bs, &payload_dst[command.insert_offset], command.size) < 0)
    return -1;

  return 0;
}

/* ###### ESMS Add Padding command : ####################################### */

int applyEsmsAddPaddingCommand(
  EsmsAddPaddingCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size
)
{
  assert(NULL != payload_dst);

  if (payload_size < command.insert_offset + command.size)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range padding insertion domain "
      "(%" PRIu32 " < %" PRIu32 " + %" PRIu32 ").\n",
      payload_size,
      command.insert_offset,
      command.size
    );

  if (command.insert_mode == INSERTION_MODE_INSERT) {
    /* Firstly shifting frame data, before data insertion. */
    memmove(
      &payload_dst[command.insert_offset + command.size],
      &payload_dst[command.insert_offset],
      payload_size - command.insert_offset - command.size
    );
  }

  memset(&payload_dst[command.insert_offset], command.filling_byte, command.size);

  return 0;
}

/* ###### ESMS Add Data Block command : #################################### */

int applyEsmsAddDataBlockCommand(
  EsmsAddDataBlockCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size,
  EsmsDataBlocks data_blocks
)
{
  assert(NULL != payload_dst);

  if (data_blocks.nb_data_blocks <= command.data_block_id)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range data block idx (%u <= %" PRIu8 ").\n",
      data_blocks.nb_data_blocks,
      command.data_block_id
    );

  const EsmsDataBlockEntry * entry = &data_blocks.entries[command.data_block_id];
  if (payload_size < command.insert_offset + entry->data_block_size)
    LIBBLU_ERROR_RETURN(
      "Broken script, out of range data block insertion domain "
      "(%" PRIu32 " < %" PRIu32 " + %" PRIu32 ").\n",
      payload_size,
      command.insert_offset,
      entry->data_block_size
    );

  if (0 == entry->data_block_size)
    return 0; /* No data */

  if (command.insert_mode == INSERTION_MODE_INSERT) {
    /* Firstly shifting frame data, before data insertion. */
    memmove(
      &payload_dst[command.insert_offset + entry->data_block_size],
      &payload_dst[command.insert_offset],
      payload_size - command.insert_offset - entry->data_block_size
    );
  }

  memcpy(&payload_dst[command.insert_offset], entry->data_block, entry->data_block_size);

  return 0;
}

/* ###### ESMS script command node : ####################################### */

#if 0

/** \~english
 * \brief
 *
 * \param node
 * \param recursive
 *
 * \todo iterative ?
 */
void destroyEsmsCommandNode(
  EsmsCommandNodePtr node,
  bool recursive
)
{
  if (NULL == node)
    return;

  if (recursive)
    destroyEsmsCommandNode(node->next, true);

  cleanEsmsCommand(&node->command);
  free(node);
}

/* ### ESMS script PES packet node : ####################################### */

EsmsPesPacketNodePtr createEsmsPesPacketNode(
  void
)
{
  EsmsPesPacketNodePtr node;
  if (NULL == (node = calloc(1, sizeof(EsmsPesPacketNode))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  return node;
}

void destroyEsmsPesPacketNode(
  EsmsPesPacketNodePtr node,
  bool recursive
)
{
  if (NULL == node)
    return;

  if (recursive)
    destroyEsmsPesPacketNode(node->next, true);

  destroyEsmsCommandNode(node->commands, true);
  free(node);
}

#endif

/* ### ESMS files utilities : ############################################## */

ESMSDirectoryFetcherErrorCodes getDirectoryOffset(
  const lbc * esms_filepath,
  ESMSDirectoryId looked_dir_ID,
  int64_t * offset
)
{
  ESMSDirectoryFetcherErrorCodes ret;

  FILE * esms_fd = lbc_fopen(esms_filepath, "rb");
  if (NULL == esms_fd)
    return ESMS_DF_READ_ERROR; // Opening error

  /* [u32 ESMS_magic] */
  /* [u16 ESMS_version] */
  /* [u8 flags_byte] */
  if (fseek(esms_fd, 0x7, SEEK_CUR) < 0)
    goto read_error;

  /* [u8 nb_directory] */
  uint8_t nb_directory;
  FREAD(esms_fd, &nb_directory, 1, goto read_error);

  for (unsigned i = 0; i < nb_directory; i++) {
    /* [u8 dir_ID] */
    uint8_t dir_ID;
    FREAD(esms_fd, &dir_ID, 1, goto read_error);

    /* [u64 dir_offset] */
    uint64_t dir_offset;
    FREAD(esms_fd, &dir_offset, 8, goto read_error);

    if (INT64_MAX < dir_offset) {
      ret = ESMS_DF_INVALID;
      goto free_return;
    }

    if (dir_ID == looked_dir_ID) {
      if (NULL != offset)
        *offset = (int64_t) dir_offset;
      ret = ESMS_DF_OK;
      goto free_return;
    }
  }

  ret = ESMS_DF_NOT_FOUND;

free_return:
  if (fclose(esms_fd) != 0)
    return ESMS_DF_READ_ERROR;
  return ret;

read_error:
  ret = ESMS_DF_READ_ERROR;
  goto free_return;
}

int isPresentDirectory(
  const lbc * essFilename,
  ESMSDirectoryId id
)
{
  switch (getDirectoryOffset(essFilename, id, NULL)) {
    case ESMS_DF_OK:
      return 1;
    case ESMS_DF_NOT_FOUND:
      return 0;
    default: // Error
      return -1;
  }
}

int seekDirectoryOffset(
  BitstreamReaderPtr esms_bs,
  const lbc * essFilename,
  ESMSDirectoryId id
)
{

  int64_t dir_offset;
  switch (getDirectoryOffset(essFilename, id, &dir_offset)) {
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

    case ESMS_DF_INVALID:
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', "
        "invalid directory \"%s\".\n",
        essFilename,
        ESMSDirectoryIdStr(id)
      );
  }

  if (seekPos(esms_bs, dir_offset, SEEK_SET) < 0)
    LIBBLU_ERROR_RETURN(
      "Broken script '%" PRI_LBCS "', offset pointing outside of file.\n",
      essFilename
    );

  return 0;
}

int checkDirectoryMagic(
  BitstreamReaderPtr esms_bs,
  uint64_t expected,
  unsigned size
)
{
  assert(0 < size && size <= 8);

  uint64_t magic;
  READ_VALUE(esms_bs, size, &magic, return -1);

  if (expected != magic)
    LIBBLU_ERROR_RETURN(
      "Broken script, unexpected section header 0x%0*X, expect 0x%0*X.\n",
      2*size, magic,
      2*size, expected
    );

  return 0;
}

const char * ESMSValidatorErrorStr(
  const ESMSFileValidatorRet code
)
{
  switch (code) {
    case ESMS_FV_OK:
      return "OK";
    case ESMS_FV_NO_SCRIPT:
      return "Unable to found script";
    case ESMS_FV_READ_ERROR:
      return "Reading error in script";
    case ESMS_FV_HEADER_ERROR:
      return "Error in script header";
    case ESMS_FV_VERSION_ERROR:
      return "Incompatible script version";
    case ESMS_FV_INCOMPLETE_FILE:
      return "Unfinished script.";
    case ESMS_FV_INCOMPATIBLE_FLAGS:
      return "Incompatible flags in script";
    case ESMS_FV_INVALID_SOURCE_FILE:
      return "Incompatible source file checksum";
    case ESMS_FV_MEMORY_ERROR:
      return "Memory allocation error";
  }

  return "Unknown error code";
}

static ESMSFileValidatorRet isValidESMSFileSourceFile(
  const lbc * sourceFilepath,
  unsigned crcControlledSize,
  uint32_t crcExpectedResult
)
{
  ESMSFileValidatorRet ret;

  BitstreamReaderPtr esms_bs;
  uint8_t * crcBuffer;

  if (lbc_access_fp(sourceFilepath, "rb") < 0)
    return ESMS_FV_INVALID_SOURCE_FILE;

  esms_bs = createBitstreamReader(sourceFilepath, READ_BUFFER_LEN);
  if (NULL == esms_bs)
    return ESMS_FV_INVALID_SOURCE_FILE;

  if (NULL == (crcBuffer = (uint8_t *) malloc(crcControlledSize))) {
    ret = ESMS_FV_MEMORY_ERROR;
    goto free_return;
  }

  if (
    readBytes(esms_bs, crcBuffer, crcControlledSize) < 0
    || crcExpectedResult != lb_compute_crc32(crcBuffer, 0, crcControlledSize)
  ) {
    /* CRC-32 values mismatch */
    ret = ESMS_FV_INVALID_SOURCE_FILE;
    goto free_return;
  }

  free(crcBuffer);
  closeBitstreamReader(esms_bs);

  return ESMS_FV_OK;

free_return:
  free(crcBuffer);
  closeBitstreamReader(esms_bs);

  return ret;
}

/** \~english
 * \brief ESMS "ES properties" scripting_flags field relative offset in bytes.
 *
 * Relative offset of the "scripting_flags" field first byte from the "ES
 * properties" section's field "ESPR_magic" first byte.
 */
#define ES_SCRIPTING_FLAGS_FIELD_POS  0x13

ESMSFileValidatorRet isAValidESMSFile(
  const lbc * esms_fp,
  uint64_t expected_flags,
  uint16_t * ESMS_version_ret
)
{
  ESMSFileValidatorRet ret = ESMS_FV_OK;

  assert(NULL != esms_fp);
  if (NULL == esms_fp)
    return ESMS_FV_NO_SCRIPT;

  FILE * esms_fd;
  if (NULL == (esms_fd = lbc_fopen(esms_fp, "rb"))) {
    ret = (errno == ENOENT) ? ESMS_FV_NO_SCRIPT : ESMS_FV_READ_ERROR;
    errno = 0; /* Clear errno */
    return ret;
  }

  /* [v32 ESMS_magic] */
  uint32_t ESMS_magic;
  FREAD(esms_fd, &ESMS_magic, 4, goto read_error);

  if (ESMS_FILE_HEADER_MAGIC != ESMS_magic) {
    ret = ESMS_FV_HEADER_ERROR;
    goto free_return;
  }

  /* [u16 ESMS_version] */
  uint16_t ESMS_version;
  FREAD(esms_fd, &ESMS_version, 2, goto read_error);

  if (ESMS_FORMAT_VER != ESMS_version) {
    ret = ESMS_FV_VERSION_ERROR;
    goto free_return;
  }

  if (NULL != ESMS_version_ret)
    *ESMS_version_ret = ESMS_version;

  /* [v8 flags_byte] */
  uint8_t flags_byte;
  FREAD(esms_fd, &flags_byte, 1, goto read_error);

  if ((flags_byte & 0x1) != 0x1) {
    ret = ESMS_FV_INCOMPLETE_FILE;
    goto free_return;
  }

  int64_t dir_offset;
  if (getDirectoryOffset(esms_fp, ESMS_DIRECTORY_ID_ES_PROP, &dir_offset) < 0)
    return ESMS_FV_READ_ERROR;

  /* Check 'scripting_flags' */
  if (fseek(esms_fd, dir_offset + ES_SCRIPTING_FLAGS_FIELD_POS, SEEK_SET) < 0)
    goto read_error;

  /* [v64 scripting_flags] */
  uint64_t scripting_flags;
  FREAD(esms_fd, &scripting_flags, 8, goto read_error);

  if (scripting_flags != expected_flags) {
    ret = ESMS_FV_INCOMPATIBLE_FLAGS;
    goto free_return;
  }

  /* Check input files checksums */
  /* [u8 nbSourceFiles] */
  uint8_t nb_source_files;
  FREAD(esms_fd, &nb_source_files, 1, goto read_error);

  if (ESMS_MAX_SUPPORTED_NB_ES_SOURCE_FILES < nb_source_files) {
    ret = ESMS_FV_HEADER_ERROR;
    goto free_return;
  }

  for (unsigned i = 0; i < nb_source_files; i++) {
    /* [u16 src_filepath_size] */
    uint16_t src_filepath_size;
    FREAD(esms_fd, &src_filepath_size, 2, goto read_error);

    if (!src_filepath_size) {
      ret = ESMS_FV_HEADER_ERROR;
      goto free_return;
    }

    uint8_t * src_filepath_utf8;
    if (NULL == (src_filepath_utf8 = calloc(src_filepath_size + 1u, 1))) {
      ret = ESMS_FV_MEMORY_ERROR;
      goto free_return;
    }

    /* [u<8*src_filepath_size> src_filepath] */
    if (!fread(src_filepath_utf8, src_filepath_size, 1, esms_fd)) {
      free(src_filepath_utf8);
      goto free_return;
    }

    lbc * src_filepath;
    if (NULL == (src_filepath = lbc_utf8_convto(src_filepath_utf8))) {
      ret = ESMS_FV_INVALID_SOURCE_FILE;
      goto free_return;
    }
    free(src_filepath_utf8);

#define FRETURN  do {free(src_filepath); goto free_return;} while (0)
    /* [u16 crc_checked_bytes] */
    uint16_t crc_checked_bytes;
    FREAD(esms_fd, &crc_checked_bytes, 2, FRETURN);

    /* [u32 crc] */
    uint32_t crc;
    FREAD(esms_fd, &crc, 4, FRETURN);

    ret = isValidESMSFileSourceFile(src_filepath, crc_checked_bytes, crc);
#undef FRETURN
    free(src_filepath);

    if (ret < 0)
      goto free_return;
  }

  fclose(esms_fd);
  return ESMS_FV_OK;

read_error:
  LIBBLU_SCRIPT_DEBUG(
    " Reading error, %s (errno: %d).\n",
    strerror(errno),
    errno
  );
  ret = ESMS_FV_READ_ERROR;
free_return:
  fclose(esms_fd);
  errno = 0; /* Clear errno */

  return ret;
}