/** \~english
 * \file scriptData.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Elementary Stream Modification Script data structures and utilities.
 */

#ifndef __LIBBLU_MUXER__ESMS__SCRIPT_DATA_H__
#define __LIBBLU_MUXER__ESMS__SCRIPT_DATA_H__

#include "../util.h"

/** \~english
 * \brief Elementary Stream Manipulation Script file header string.
 */
#define ESMS_FILE_HEADER  "ESMS"

/** \~english
 * \brief Currently defined ESMS format version code.
 *
 * Parser syntax version.
 */
#define CURRENT_ESMS_FORMAT_VER  5

/** \~english
 * \brief ESMS directory index length in bytes.
 *
 * Composed of:
 *  - u8  : directoryId;
 *  - u64 : directoryOffset.
 *
 * => 9 bytes.
 */
#define ESMS_DIRECTORY_IDX_LENGTH  9

/** \~english
 * \brief ESMS source file max allowed filename length in characters including
 * NUL character.
 */
#define ESMS_MAX_FILENAME_LEN  4096

/** \~english
 * \brief ESMS max allowed number of data blocks in Data blocks definition
 * section.
 */
#define ESMS_MAX_NB_DATA_BLOCKS  32

/** \~english
 * \brief ESMS "ES properties" section header string.
 */
#define ES_PROPERTIES_HEADER  "ESPR"

/** \~english
 * \brief ESMS "PES cutting" section header string.
 */
#define PES_CUTTING_HEADER  "PESC"

/** \~english
 * \brief ESMS audio "Codec specific parameters" section header string.
 */
#define ES_CODEC_SPEC_PARAM_HEADER_AUDIO  "CSPMAUDO"

/** \~english
 * \brief ESMS video "Codec specific parameters" section header string.
 */
#define ES_CODEC_SPEC_PARAM_HEADER_VIDEO  "CSPMVIDO"

/** \~english
 * \brief ESMS "Data blocks definition" section header string.
 */
#define DATA_BLOCKS_DEF_HEADER  "DTBK"

/** \~english
 * \brief ESMS "ES properties" flags fiels relative offset in bytes.
 *
 * Relative offset of the "scriptingFlags" field first byte from the "ES
 * properties" section's field "esPropertiesHeader" first byte.
 */
#define ES_SCRIPTING_FLAGS_FIELD_POS  0x1A

/* Commands : */

/** \~english
 * \brief ESMS PES cutting script "Add data" command minimal length in bytes.
 */
#define ADD_DATA_COM_LEN  5 /* + insertingData len */

/** \~english
 * \brief ESMS PES cutting script "Change byte order" command length in bytes.
 */
#define CHANGE_BYTEORDER_COM_LEN  9

/** \~english
 * \brief ESMS PES cutting script "Add payload data" command minimal length
 * in bytes.
 */
#define ADD_PAYLOAD_DATA_COM_LEN  12 /* + Ext */

/** \~english
 * \brief ESMS PES cutting script "Add padding" command length in bytes.
 */
#define ADD_PADD_COM_LEN  10

/** \~english
 * \brief ESMS PES cutting script "Add data section" command length in bytes.
 */
#define ADD_DATA_SECTION_COM_LEN  6

/** \~english
 * \brief ESMS "PES cutting" section end marker byte.
 *
 * Byte used to mark the end of PES frames definition loop.
 */
#define ESMS_SCRIPT_END_MARKER  0xFF

/** \~english
 * \brief ESMS PES frame script max allowed number of commands.
 */
#define MAX_SUPPORTED_COMMANDS  32

/** \~english
 * \brief ESMS PES frame script "Add data" command max allowed data field
 * length in bytes.
 *
 * \note This value is only used if #ENABLE_MAX_ADD_DATA_BYTES parameter is
 * enabled.
 */
#define MAX_ADD_DATA_BYTES  50

/** \~english
 * \brief Define if #MAX_ADD_DATA_BYTES constraint shall be applied.
 *
 * Set to 0 if not, 1 if true.
 */
#define ENABLE_MAX_ADD_DATA_BYTES  0

typedef enum {
  FLAG_SHFT_SEC_STREAM                  =  0,  /**< Secondary stream type.   */
  FLAG_SHFT_EXTRACT_CORE                =  1,  /**< Extract audio core
    (only DTS-HD and Dolby TrueHD formats).                                  */
  FLAG_SHFT_DVD_OUTPUT                  =  2,  /**< Target DVD media output. */
  FLAG_SHFT_DISABLE_FIXES               =  4,  /**< Disable possible
    compliance corrections.                                                  */
  FLAG_SHFT_FORCE_REBUILD_SEI           =  5,  /**< Force rebuilding of ESMS
    files at every muxing.                                                   */
  FLAG_SHFT_CHANGE_FPS                  =  6,  /**< Change FPS value
    (only H.264 video format).                                               */
  FLAG_SHFT_CHANGE_AR                   =  7,  /**< Change aspect ratio
    value (only H.264 video format).                                         */
  FLAG_SHFT_CHANGE_LEVEL                =  8,  /**< Change encoding level
    value (only H.264 video format).                                         */
  FLAG_SHFT_REMOVE_SEI                  =  9,  /**< Remove SEI messages from
    H.264 streams.                                                           */

#if 0
  FLAG_SHFT_PBR_FILE                    =  3,  /**< DTS-HDMA PBR file
    presence.                                                                */
  FLAG_SHFT_DISABLE_HRD_VER             = 10,  /**< Disable H.264 HRD
    verifier.                                                                */
  FLAG_SHFT_DOUBLE_FRAME_TIMING_VALUES  = 11   /**< Double H.264 frames
    timing values.                                                           */
  FLAG_SHFT_ECHO_HRD_CPB                = 12,  /**< Enable display of
    H.264 HRD CPB buffering model updates.                                   */
  FLAG_SHFT_ECHO_HRD_DPB                = 13   /**< Enable display of
    H.264 HRD DPB buffering model updates.                                   */
#endif
} LibbluEsmsFlagsShiftingValues;

/** \~english
 * \brief ESMS "ES properties" section, scriptingFlags field values.
 */
typedef enum {
  FLAG_SEC_STREAM                  = 1 << FLAG_SHFT_SEC_STREAM,
  FLAG_EXTRACT_CORE                = 1 << FLAG_SHFT_EXTRACT_CORE,
  FLAG_DVD_OUTPUT                  = 1 << FLAG_SHFT_DVD_OUTPUT,
  FLAG_DISABLE_FIXES               = 1 << FLAG_SHFT_DISABLE_FIXES,
  FLAG_FORCE_REBUILD_SEI               = 1 << FLAG_SHFT_FORCE_REBUILD_SEI,
  FLAG_CHANGE_FPS                  = 1 << FLAG_SHFT_CHANGE_FPS,
  FLAG_CHANGE_AR                   = 1 << FLAG_SHFT_CHANGE_AR,
  FLAG_CHANGE_LEVEL                = 1 << FLAG_SHFT_CHANGE_LEVEL,
  FLAG_REMOVE_SEI                  = 1 << FLAG_SHFT_REMOVE_SEI,

#if 0
  FLAG_PBR_FILE                    = 1 << FLAG_SHFT_PBR_FILE,
  FLAG_DISABLE_HRD_VER             = 1 << FLAG_SHFT_DISABLE_HRD_VER,
  FLAG_DOUBLE_FRAME_TIMING_VALUES  = 1 << FLAG_SHFT_DOUBLE_FRAME_TIMING_VALUES
  FLAG_ECHO_HRD_CPB                = 1 << FLAG_SHFT_ECHO_HRD_CPB,
  FLAG_ECHO_HRD_DPB                = 1 << FLAG_SHFT_ECHO_HRD_DPB
#endif
} LibbluEsmsFlags;

static inline void printEnabledLibbluEsmsFlags(
  uint64_t flags
)
{
  printf("Enabled flags:\n");
  if (flags & FLAG_SEC_STREAM)
    printf(" - Secondary stream;\n");
  if (flags & FLAG_EXTRACT_CORE)
    printf(" - Extract core;\n");
  if (flags & FLAG_DVD_OUTPUT)
    printf(" - DVD media target;\n");
  if (flags & FLAG_DISABLE_FIXES)
    printf(" - Disable auto-fixes;\n");
  if (flags & FLAG_FORCE_REBUILD_SEI)
    printf(" - Force rebuild;\n");
  if (flags & FLAG_CHANGE_FPS)
    printf(" - Change FPS;\n");
  if (flags & FLAG_CHANGE_AR)
    printf(" - Change AR;\n");
  if (flags & FLAG_CHANGE_LEVEL)
    printf(" - Change level;\n");
  if (flags & FLAG_REMOVE_SEI)
    printf(" - Remove SEI messages;\n");

#if 0
  if (flags & FLAG_PBR_FILE)
    printf(" - DTS PBR file;\n");
  if (flags & FLAG_DISABLE_HRD_VER)
    printf(" - Disable HRD Buffer verifier;\n");
  if (flags & FLAG_DOUBLE_FRAME_TIMING_VALUES)
    printf(" - Double frames timing values;\n");
  if (flags & FLAG_ECHO_HRD_CPB)
    printf(" - Print HRD CPB states;\n");
  if (flags & FLAG_ECHO_HRD_DPB)
    printf(" - Print HRD DPB states;\n");
#endif
}

/* Frame flags : */
#define ESMS_FFLAG_PTS_LONG_FIELD                               0x80
#define ESMS_FFLAG_DTS_PRESENT                                  0x40
#define ESMS_FFLAG_DTS_LONG_FIELD                               0x20
#define ESMS_FFLAG_LENGTH_LONG_FIELD                            0x10
#define ESMS_FFLAG_EXT_DATA_PRESENT                             0x08

/* ### ESMS ES Source Files : ############################################## */

typedef struct {
  uint32_t crc32;
  uint16_t crcCheckedBytes;
} EsmsESSourceFile;

static inline void setEsmsESSourceFile(
  EsmsESSourceFile * dst,
  uint32_t crc32,
  uint16_t crcCheckedBytes
)
{
  *dst = (EsmsESSourceFile) {
    .crc32 = crc32,
    .crcCheckedBytes = crcCheckedBytes
  };
}

#define ESMS_DEFAULT_NB_ES_SOURCE_FILES 2
#define ESMS_MAX_SUPPORTED_NB_ES_SOURCE_FILES 255

typedef struct {
  lbc ** filepaths;
  EsmsESSourceFile * properties;
  BitstreamReaderPtr * handles;

  unsigned nbUsedFiles;
  unsigned nbAllocatedFiles;
} EsmsESSourceFiles;

static inline void initEsmsESSourceFiles(
  EsmsESSourceFiles * dst
)
{
  *dst = (EsmsESSourceFiles) {0};
}

static inline void cleanEsmsESSourceFiles(
  EsmsESSourceFiles srcFiles
)
{
  unsigned i;

  for (i = 0; i < srcFiles.nbUsedFiles; i++) {
    free(srcFiles.filepaths[i]);
    if (NULL != srcFiles.handles)
      closeBitstreamReader(srcFiles.handles[i]);
  }

  free(srcFiles.filepaths);
  free(srcFiles.properties);
  free(srcFiles.handles);
}

static inline unsigned nbUsedFilesEsmsESSourceFiles(
  EsmsESSourceFiles srcFiles
)
{
  return srcFiles.nbUsedFiles;
}

static inline lbc ** filepathsEsmsESSourceFiles(
  EsmsESSourceFiles srcFiles
)
{
  return srcFiles.filepaths;
}

static inline const BitstreamReaderPtr * handlesEsmsESSourceFiles(
  EsmsESSourceFiles srcFiles
)
{
  return srcFiles.handles;
}

static inline bool isPresentEsmsESSourceFiles(
  EsmsESSourceFiles srcFiles,
  const lbc * filepath
)
{
  unsigned i;

  for (i = 0; i < nbUsedFilesEsmsESSourceFiles(srcFiles); i++) {
    if (lbc_equal(srcFiles.filepaths[i], filepath))
      return true;
  }

  return false;
}

int appendEsmsESSourceFiles(
  EsmsESSourceFiles * dst,
  const lbc * filepath,
  EsmsESSourceFile properties
);

int openAllEsmsESSourceFiles(
  EsmsESSourceFiles * dst
);

/* ### ESMS Directories : ################################################## */

typedef struct {
  uint64_t offset;
  uint8_t id;
} EsmsFileDirectory;

/** \~english
 * \brief ESMS max allowed number of directories.
 *
 * Corresponding therefore the maximum number of allowed indexed sections
 * in a ESMS file.
 */
#define ESMS_MAX_ALLOWED_DIR  10

typedef struct {
  EsmsFileDirectory dirs[ESMS_MAX_ALLOWED_DIR];
  uint8_t nbUsedDirs;
} EsmsFileDirectories;

static inline void initEsmsFileDirectories(
  EsmsFileDirectories * dst
)
{
  *dst = (EsmsFileDirectories) {
    .nbUsedDirs = 0
  };
}

/* ### ESMS Data Blocks : ################################################## */

typedef struct {
  uint8_t * data;
  uint32_t size;
} EsmsDataBlockEntry;

static inline void setEsmsDataBlockEntry(
  EsmsDataBlockEntry * dst,
  uint8_t * data,
  uint32_t size
)
{
  *dst = (EsmsDataBlockEntry) {
    .data = data,
    .size = size
  };
}

static inline void cleanEsmsDataBlockEntry(
  EsmsDataBlockEntry entry
)
{
  free(entry.data);
}

#define ESMS_DEFAULT_NB_DATA_BLOCKS_ENTRIES 2
#define ESMS_MAX_SUPPORTED_DATA_BLOCKS_ENTRIES 255

typedef struct {
  EsmsDataBlockEntry * entries;

  unsigned nbUsedEntries;
  unsigned nbAllocatedEntries;
} EsmsDataBlocks;

static inline void initEsmsDataBlocks(
  EsmsDataBlocks * dst
)
{
  *dst = (EsmsDataBlocks) {
    .entries = NULL,
    .nbUsedEntries = 0,
    .nbAllocatedEntries = 0
  };
}

static inline void cleanEsmsDataBlocks(
  EsmsDataBlocks dataBlocks
)
{
  unsigned i;

  for (i = 0; i < dataBlocks.nbUsedEntries; i++)
    cleanEsmsDataBlockEntry(dataBlocks.entries[i]);
  free(dataBlocks.entries);
}

static inline unsigned nbUsedEntriesEsmsDataBlocks(
  EsmsDataBlocks dataBlocks
)
{
  return dataBlocks.nbUsedEntries;
}

static inline bool nbUsedEntriesLimitReachedEsmsDataBlocks(
  EsmsDataBlocks dataBlocks
)
{
  return
    ESMS_MAX_SUPPORTED_DATA_BLOCKS_ENTRIES
    <= nbUsedEntriesEsmsDataBlocks(dataBlocks)
  ;
}

int appendEsmsDataBlocks(
  EsmsDataBlocks * dst,
  EsmsDataBlockEntry entry,
  unsigned * idx
);

int updateEsmsDataBlocks(
  EsmsDataBlocks * dst,
  EsmsDataBlockEntry newEntry,
  unsigned dstIdx
);

static inline int getEsmsDataBlocks(
  EsmsDataBlocks dataBlocks,
  EsmsDataBlockEntry * dst,
  unsigned idx
)
{
  if (nbUsedEntriesEsmsDataBlocks(dataBlocks) < idx)
    LIBBLU_ERROR_RETURN("Unknown Data Block entry idx %u.\n", idx);

  if (NULL != dst)
    *dst = dataBlocks.entries[idx];
  return 0;
}

/* ### ESMS Script commands : ############################################## */

/** \~english
 * \brief #ScriptCommand type.
 */
typedef enum {
  ESMS_ADD_DATA,          /**< Add data bytes.                               */
  ESMS_CHANGE_BYTEORDER,  /**< Change byte-order.                            */
  ESMS_ADD_PAYLOAD_DATA,  /**< Add data from a script linked source file.    */
  ESMS_ADD_PADDING_DATA,  /**< Add padding bytes.                            */
  ESMS_ADD_DATA_SECTION   /**< Add bytes from a script data section.         */
} EsmsCommandType;

static inline bool isValidEsmsCommandType(
  EsmsCommandType type
)
{
  return
    type == ESMS_ADD_DATA
    || type == ESMS_CHANGE_BYTEORDER
    || type == ESMS_ADD_PAYLOAD_DATA
    || type == ESMS_ADD_PADDING_DATA
    || type == ESMS_ADD_DATA_SECTION
  ;
}

typedef enum {
  INSERTION_MODE_ERASE   = 0x0,
  INSERTION_MODE_INSERT  = 0x1
} EsmsDataInsertionMode;

static inline bool isValidEsmsDataInsertionMode(
  uint8_t value
)
{
  return
    value == INSERTION_MODE_ERASE
    || value == INSERTION_MODE_INSERT
  ;
}

/* ###### ESMS Add Data command : ########################################## */

typedef struct {
  uint32_t offset;
  EsmsDataInsertionMode mode;

  uint8_t * data;
  uint16_t dataLength;
} EsmsAddDataCommand;

int setEsmsAddDataCommand(
  EsmsAddDataCommand * dst,
  uint32_t offset,
  EsmsDataInsertionMode mode,
  const uint8_t * data,
  uint16_t dataLength
);

static inline void cleanEsmsAddDataCommand(
  EsmsAddDataCommand command
)
{
  free(command.data);
}

int applyEsmsAddDataCommand(
  EsmsAddDataCommand command,
  uint8_t * packetData,
  size_t packetSize
);

/* ###### ESMS Change Byte Order command : ################################# */

typedef struct {
  uint8_t unitSize;  /**< Integer unit size in bytes.                        */
  uint32_t offset;   /**< Swapped region start offset in bytes.              */
  uint32_t length;   /**< Swapped region size in bytes.                      */
} EsmsChangeByteOrderCommand;

static inline int setEsmsChangeByteOrderCommand(
  EsmsChangeByteOrderCommand * dst,
  uint8_t unitSize,
  uint32_t offset,
  uint32_t length
)
{
  *dst = (EsmsChangeByteOrderCommand) {
    .unitSize = unitSize,
    .offset = offset,
    .length = length
  };

  return 0;
}

int applyEsmsChangeByteOrderCommand(
  EsmsChangeByteOrderCommand command,
  uint8_t * packetData,
  size_t packetSize
);

/* ###### ESMS Add PES Payload command : ################################### */

typedef struct {
  unsigned fileIdx;    /**< Source file index.                               */
  uint32_t dstOffset;  /**< Destination offset in PES packet.                */
  int64_t srcOffset;   /**< Source offset in file.                           */
  uint32_t size;       /**< Size of the copied field.                        */
} EsmsAddPesPayloadCommand;

static inline int setEsmsAddPesPayloadCommand(
  EsmsAddPesPayloadCommand * dst,
  unsigned fileIdx,
  uint32_t dstOffset,
  int64_t srcOffset,
  uint32_t size
)
{
  *dst = (EsmsAddPesPayloadCommand) {
    .fileIdx = fileIdx,
    .dstOffset = dstOffset,
    .srcOffset = srcOffset,
    .size = size
  };

  return 0;
}

int applyEsmsAddPesPayloadCommand(
  EsmsAddPesPayloadCommand command,
  uint8_t * packetData,
  size_t packetSize,
  const BitstreamReaderPtr * sourceFilesHandles,
  unsigned nbSourceFiles
);

/* ###### ESMS Add Padding command : ####################################### */

typedef struct {
  uint32_t offset;
  EsmsDataInsertionMode mode;

  uint32_t length;
  uint8_t byte;
} EsmsAddPaddingCommand;

static inline int setEsmsAddPaddingCommand(
  EsmsAddPaddingCommand * dst,
  uint32_t offset,
  EsmsDataInsertionMode mode,
  uint32_t length,
  uint8_t byte
)
{
  *dst = (EsmsAddPaddingCommand) {
    .offset = offset,
    .mode = mode,
    .length = length,
    .byte = byte
  };

  return 0;
}

int applyEsmsAddPaddingCommand(
  EsmsAddPaddingCommand command,
  uint8_t * packetData,
  size_t packetSize
);

/* ###### ESMS Add Data Block command : #################################### */

typedef struct {
  uint32_t offset;
  EsmsDataInsertionMode mode;

  uint8_t blockIdx;
} EsmsAddDataBlockCommand;

static inline int setEsmsAddDataBlockCommand(
  EsmsAddDataBlockCommand * dst,
  uint32_t offset,
  EsmsDataInsertionMode mode,
  uint8_t blockIdx
)
{
  *dst = (EsmsAddDataBlockCommand) {
    .offset = offset,
    .mode = mode,
    .blockIdx = blockIdx
  };

  return 0;
}

/** \~english
 * \brief
 *
 * \param command
 * \param packetData
 * \param packetSize
 * \param dataSections
 * \return int
 */
int applyEsmsAddDataBlockCommand(
  EsmsAddDataBlockCommand command,
  uint8_t * packetData,
  size_t packetSize,
  EsmsDataBlocks dataSections
);

/* ###### ESMS script commands common : #################################### */

typedef union {
  EsmsAddDataCommand addData;
  EsmsChangeByteOrderCommand changeByteOrder;
  EsmsAddPesPayloadCommand addPesPayload;
  EsmsAddPaddingCommand addPadding;
  EsmsAddDataBlockCommand addDataBlock;
} EsmsCommandData;

typedef struct {
  EsmsCommandType type;
  EsmsCommandData data;
} EsmsCommand;

static inline EsmsCommand * initEsmsCommand(
  EsmsCommand * dst,
  EsmsCommandType type
)
{
  *dst = (EsmsCommand) {
    .type = type
  };

  return dst;
}

static inline void cleanEsmsCommand(
  EsmsCommand command
)
{
  switch (command.type) {
    case ESMS_ADD_DATA:
      return cleanEsmsAddDataCommand(command.data.addData);

    case ESMS_CHANGE_BYTEORDER:
    case ESMS_ADD_PAYLOAD_DATA:
    case ESMS_ADD_PADDING_DATA:
    case ESMS_ADD_DATA_SECTION:
      break;
  }
}

/* ###### ESMS script command node : ####################################### */

typedef struct EsmsCommandNode {
  struct EsmsCommandNode * next;
  EsmsCommand command;
} EsmsCommandNode, *EsmsCommandNodePtr;

EsmsCommandNodePtr createEsmsCommandNode(
  EsmsCommandType type
);

void destroyEsmsCommandNode(
  EsmsCommandNodePtr node,
  bool recursive
);

static inline void attachEsmsCommandNode(
  EsmsCommandNodePtr root,
  const EsmsCommandNodePtr sibling
)
{
  assert(NULL != root);

  root->next = sibling;
}

/* ### ESMS script PES packet node : ####################################### */

#define ESMS_PFH_EXT_PARAM_H264_LENGTH(largeTimeFields)                       \
  (                                                                           \
    (largeTimeFields) ?                                                       \
      17                                                                      \
    :                                                                         \
      9                                                                       \
  )

typedef struct {
  uint64_t cpbRemovalTime;
  uint64_t dpbOutputTime;
} EsmsPesPacketH264ExtData;

typedef union {
  EsmsPesPacketH264ExtData h264;
} EsmsPesPacketExtData;

typedef struct EsmsPesPacketNode {
  struct EsmsPesPacketNode * next;

  bool extensionFrame;
  bool dtsPresent;
  bool extensionDataPresent;

  uint64_t pts;                        /**< Frame Presentation Time Stamp
    value in 27MHz clock ticks.                                              */
  uint64_t dts;                        /**< Frame Decoding Time Stamp value
    in 27MHz clock ticks.                                                    */

  EsmsPesPacketExtData extensionData;

  unsigned length;                     /**< Output PES frame length in
    bytes.                                                                   */

  EsmsCommandNodePtr commands;
} EsmsPesPacketNode, *EsmsPesPacketNodePtr;

EsmsPesPacketNodePtr createEsmsPesPacketNode(
  void
);

void destroyEsmsPesPacketNode(
  EsmsPesPacketNodePtr node,
  bool recursive
);

static inline void attachEsmsPesPacketNode(
  EsmsPesPacketNodePtr root,
  const EsmsPesPacketNodePtr sibling
)
{
  assert(NULL != root);

  root->next = sibling;
}

static inline EsmsCommandNodePtr claimCommandsEsmsPesPacketNode(
  EsmsPesPacketNodePtr node
)
{
  EsmsCommandNodePtr commands;

  assert(NULL != node);

  commands = node->commands;
  node->commands = NULL;

  return commands;
}

/* ### ESMS files utilities : ############################################## */

typedef enum {
  ESMS_DIRECTORY_ID_ES_PROP          = 0x01,
  ESMS_DIRECTORY_ID_ES_PES_CUTTING   = 0x02,
  ESMS_DIRECTORY_ID_ES_FMT_PROP      = 0x03,
  ESMS_DIRECTORY_ID_ES_DATA_BLK_DEF  = 0x04
} ESMSDirectoryId;

const char * ESMSDirectoryIdStr(
  ESMSDirectoryId id
);

/** \~english
 * \brief ESMS script file directory fetcher return values.
 *
 * Refer to #getDirectoryOffset() function.
 * It is possible to test value using val < 0 test to ensure success return.
 */
typedef enum {
  ESMS_DF_OK          =  0,  /**< Directory successfully fetched.            */
  ESMS_DF_READ_ERROR  = -1,  /**< ESMS script file reading error happen.     */
  ESMS_DF_NOT_FOUND   = -2   /**< Unable to found looked directory.          */
} ESMSDirectoryFetcherErrorCodes;

/** \~english
 * \brief Fetch a specified ESMS script file directory by ID.
 *
 * \param essFilename ESMS script filename.
 * \param id Looked directory ID.
 * \param offset On success, directory offset return pointer
 * (can be NULL).
 * \return ESMSDirectoryFetcherErrorCodes On success, a zero value is returned.
 * Otherwise, a negative value is returned. Meaning of this value is defined
 * by #ESMSDirectoryFetcherErrorCodes enumeration.
 */
ESMSDirectoryFetcherErrorCodes getDirectoryOffset(
  const lbc * essFilename,
  ESMSDirectoryId id,
  uint64_t * offset
);

int isPresentDirectory(
  const lbc * essFilename,
  ESMSDirectoryId id
);

int seekDirectoryOffset(
  BitstreamReaderPtr essHandle,
  const lbc * essFilename,
  ESMSDirectoryId id
);

int checkDirectoryMagic(
  BitstreamReaderPtr script,
  const char * magic,
  size_t magicSize
);

/** \~english
 * \brief ESMS script file #isAValidESMSFile() validation function return
 * codes.
 */
typedef enum {
  ESMS_FV_NO_SCRIPT            = -8,  /**< Unable to find script.            */
  ESMS_FV_READ_ERROR           = -7,  /**< Reading error happen.             */
  ESMS_FV_HEADER_ERROR         = -6,  /**< Script header is invalid.         */
  ESMS_FV_VERSION_ERROR        = -5,  /**< Script version is unsupported.    */
  ESMS_FV_INCOMPLETE_FILE      = -4,  /**< Script is incomplete.             */
  ESMS_FV_INCOMPATIBLE_FLAGS   = -3,  /**< Script is incompatible with
    current muxing settings.                                                 */
  ESMS_FV_INVALID_SOURCE_FILE  = -2,  /**< Script referenced source file is
    invalid.                                                                 */
  ESMS_FV_MEMORY_ERROR         = -1,  /**< Memory management error.          */

  ESMS_FV_OK                   =  0   /**< Script is valid.                  */
} ESMSFileValidatorRet;

const char * ESMSValidatorErrorStr(
  const ESMSFileValidatorRet code
);

/** \~english
 * \brief Print on stdout a human-readable description of
 * #ESMSFileValidatorRet values.
 *
 * \param code Return code to print.
 */
void printESMSValidatorError(
  ESMSFileValidatorRet code
);

/** \~english
 * \brief Test validity of supplied ESMS script file and compatibility with
 * muxing parameters.
 *
 * \param essFileName Tested ESMS script filepath.
 * \param flags Muxing parameters flags.
 * \param version On success, script version returning pointer (can be NULL).
 * \return ESMSFileValidatorRet Returned code.
 */
ESMSFileValidatorRet isAValidESMSFile(
  const lbc * essFileName,
  const uint64_t flags,
  unsigned * version
);

#endif