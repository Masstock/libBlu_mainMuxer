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
#include "../elementaryStreamProperties.h"

#define ESMS_FILE_HEADER  "ESMS"

/** \~english
 * \brief Elementary Stream Manipulation Script file header magic number.
 */
#define ESMS_FILE_HEADER_MAGIC  0x45534D53

/** \~english
 * \brief Currently defined ESMS format version code.
 *
 * Parser syntax version.
 */
#define ESMS_FORMAT_VER  0x0001

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
 * \brief ESMS "ES properties" section header magic number.
 */
#define ES_PROPERTIES_HEADER_MAGIC  0x45535052

/** \~english
 * \brief ESMS "PES cutting" section header magic number.
 */
#define PES_CUTTING_HEADER_MAGIC  0x50455343

/** \~english
 * \brief ESMS audio "Codec specific parameters" section header magic number.
 */
#define ES_CODEC_SPEC_PARAM_HEADER_AUDIO_MAGIC  0x4353504D4155444F

/** \~english
 * \brief ESMS video "Codec specific parameters" section header magic number.
 */
#define ES_CODEC_SPEC_PARAM_HEADER_VIDEO_MAGIC  0x4353504D5649444F

/** \~english
 * \brief ESMS "Data blocks definition" section header magic number.
 */
#define DATA_BLOCKS_DEF_HEADER_MAGIC  0x4454424B

/* Commands : */

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
  FLAG_FORCE_REBUILD_SEI           = 1 << FLAG_SHFT_FORCE_REBUILD_SEI,
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
  uint16_t crc_checked_bytes;
  uint32_t crc;
} EsmsESSourceFileProperties;

typedef struct {
  lbc * filepath;
  BitstreamReaderPtr handle;
  EsmsESSourceFileProperties properties;
} EsmsESSourceFilesEntry;

static inline void cleanEsmsESSourceFilesEntry(
  EsmsESSourceFilesEntry entry
)
{
  free(entry.filepath);
  closeBitstreamReader(entry.handle);
}

#define ESMS_MAX_SUPPORTED_NB_ES_SOURCE_FILES  8

typedef struct {
  EsmsESSourceFilesEntry entries[ESMS_MAX_SUPPORTED_NB_ES_SOURCE_FILES];
  uint8_t nb_source_files;
  bool opened;
} EsmsESSourceFiles;

static inline void cleanEsmsESSourceFiles(
  EsmsESSourceFiles src_files
)
{
  for (unsigned i = 0; i < src_files.nb_source_files; i++)
    cleanEsmsESSourceFilesEntry(src_files.entries[i]);
}

static inline bool isPresentEsmsESSourceFiles(
  EsmsESSourceFiles src_files,
  const lbc * filepath
)
{
  for (unsigned i = 0; i < src_files.nb_source_files; i++) {
    if (lbc_equal(src_files.entries[i].filepath, filepath))
      return true;
  }
  return false;
}

int appendEsmsESSourceFiles(
  EsmsESSourceFiles * dst,
  const lbc * filepath,
  EsmsESSourceFileProperties properties
);

int openAllEsmsESSourceFiles(
  EsmsESSourceFiles * dst
);

/* ### ESMS Directories : ################################################## */

typedef enum {
  ESMS_DIRECTORY_ID_ES_PROP          = 0x01,
  ESMS_DIRECTORY_ID_ES_PES_CUTTING   = 0x02,
  ESMS_DIRECTORY_ID_ES_FMT_PROP      = 0x03,
  ESMS_DIRECTORY_ID_ES_DATA_BLK_DEF  = 0x04
} ESMSDirectoryId;

static inline const char * ESMSDirectoryIdStr(
  ESMSDirectoryId dir_ID
)
{
  static const char * strings[] = {
    "unknown",
    "ES Properties",
    "ES PES Cutting",
    "ES Format Properties",
    "ES Data Blocks Definition"
  };

  if (dir_ID < ARRAY_SIZE(strings))
    return strings[dir_ID];
  return "unknown";
}

typedef struct {
  uint8_t dir_ID;
  int64_t dir_offset;
} EsmsFileDirectory;

/** \~english
 * \brief ESMS max allowed number of directories.
 *
 * Corresponding therefore the maximum number of allowed indexed sections
 * in a ESMS file.
 */
#define ESMS_MAX_ALLOWED_DIR  10

typedef struct {
  EsmsFileDirectory entries[ESMS_MAX_ALLOWED_DIR];
  uint8_t nb_used_entries;
} EsmsFileDirectories;

/* ### ESMS Data Blocks : ################################################## */

typedef struct {
  uint8_t * data_block;
  uint32_t data_block_size;
} EsmsDataBlockEntry;

static inline void cleanEsmsDataBlockEntry(
  EsmsDataBlockEntry entry
)
{
  free(entry.data_block);
}

#define ESMS_DEFAULT_NB_DATA_BLOCKS_ENTRIES 2
#define ESMS_MAX_SUPPORTED_DATA_BLOCKS_ENTRIES 255

typedef struct {
  EsmsDataBlockEntry * entries;
  unsigned nb_alloc_data_blocks;
  unsigned nb_data_blocks;
} EsmsDataBlocks;

static inline void cleanEsmsDataBlocks(
  EsmsDataBlocks dataBlocks
)
{
  unsigned i;

  for (i = 0; i < dataBlocks.nb_data_blocks; i++)
    cleanEsmsDataBlockEntry(dataBlocks.entries[i]);
  free(dataBlocks.entries);
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

/* ### ESMS Script commands : ############################################## */

/** \~english
 * \brief #ScriptCommand type.
 */
typedef enum {
  ESMS_ADD_DATA,          /**< Add data bytes.                               */
  ESMS_CHANGE_BYTEORDER,  /**< Change byte-order.                            */
  ESMS_ADD_PAYLOAD_DATA,  /**< Add data from a script linked source file.    */
  ESMS_ADD_PADDING_DATA,  /**< Add padding bytes.                            */
  ESMS_ADD_DATA_BLOCK     /**< Add bytes from a script data section.         */
} EsmsCommandType;

static inline const char * EsmsCommandTypeStr(
  EsmsCommandType type
)
{
  static const char * strings[] = {
    "Add data",
    "Change byte order",
    "Add payload data",
    "Add padding data",
    "Add data section"
  };

  return strings[type];
}

static inline bool isValidEsmsCommandType(
  EsmsCommandType type
)
{
  return
    type == ESMS_ADD_DATA
    || type == ESMS_CHANGE_BYTEORDER
    || type == ESMS_ADD_PAYLOAD_DATA
    || type == ESMS_ADD_PADDING_DATA
    || type == ESMS_ADD_DATA_BLOCK
  ;
}

typedef enum {
  INSERTION_MODE_OVERWRITE  = 0x0,
  INSERTION_MODE_INSERT     = 0x1
} EsmsDataInsertionMode;

static inline const char * EsmsDataInsertionModeStr(
  EsmsDataInsertionMode mode
)
{
  static const char * strings[] = {
    "Overwrite",
    "Insert"
  };

  return strings[mode];
}

static inline bool isValidEsmsDataInsertionMode(
  uint8_t value
)
{
  return
    value == INSERTION_MODE_OVERWRITE
    || value == INSERTION_MODE_INSERT
  ;
}

/* ###### ESMS Add Data command : ########################################## */

typedef struct {
  uint32_t insert_offset;
  EsmsDataInsertionMode insert_mode;

  uint8_t * data;
  uint16_t data_size;
} EsmsAddDataCommand;

/** \~english
 * \brief ESMS PES cutting script "Add data" command minimal length in bytes.
 */
#define ADD_DATA_COM_LEN  5 // + data_size

static inline void cleanEsmsAddDataCommand(
  EsmsAddDataCommand command
)
{
  free(command.data);
}

int applyEsmsAddDataCommand(
  EsmsAddDataCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size
);

/* ###### ESMS Change Byte Order command : ################################# */

typedef struct {
  uint8_t unit_size;        /**< Integer unit size in bytes.                 */
  uint32_t section_offset;  /**< Swapped region start offset in bytes.       */
  uint32_t section_size;    /**< Swapped region size in bytes.               */
} EsmsChangeByteOrderCommand;

/** \~english
 * \brief ESMS PES cutting script "Change byte order" command length in bytes.
 */
#define CHANGE_BYTEORDER_COM_LEN  9

static inline int setEsmsChangeByteOrderCommand(
  EsmsChangeByteOrderCommand * dst,
  uint8_t unitSize,
  uint32_t offset,
  uint32_t length
)
{
  *dst = (EsmsChangeByteOrderCommand) {
    .unit_size = unitSize,
    .section_offset = offset,
    .section_size = length
  };

  return 0;
}

int applyEsmsChangeByteOrderCommand(
  EsmsChangeByteOrderCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size
);

/* ###### ESMS Add PES Payload command : ################################### */

typedef struct {
  uint8_t src_file_id;     /**< Source file identifier.                      */
  uint32_t insert_offset;  /**< Destination offset in PES packet.            */
  int64_t src_offset;      /**< Source offset in file.                       */
  uint32_t size;           /**< Size of the copied field.                    */
} EsmsAddPesPayloadCommand;

static inline uint16_t sizeEsmsAddPesPayloadCommand(
  bool src_offset_ext_present,
  bool size_ext_present
)
{
  uint16_t size = 6;
  size += 4 << src_offset_ext_present;
  size += 2 << size_ext_present;
  return size;
}

/** \~english
 * \brief ESMS PES cutting script "Add payload data" command minimal length
 * in bytes.
 */
#define ADD_PAYLOAD_DATA_COM_MIN_LEN  12

static inline int setEsmsAddPesPayloadCommand(
  EsmsAddPesPayloadCommand * dst,
  unsigned fileIdx,
  uint32_t dstOffset,
  int64_t srcOffset,
  uint32_t size
)
{
  *dst = (EsmsAddPesPayloadCommand) {
    .src_file_id = fileIdx,
    .insert_offset = dstOffset,
    .src_offset = srcOffset,
    .size = size
  };

  return 0;
}

int applyEsmsAddPesPayloadCommand(
  EsmsAddPesPayloadCommand command,
  uint8_t * packetData,
  uint32_t packetSize,
  const EsmsESSourceFiles * src_files
);

/* ###### ESMS Add Padding command : ####################################### */

typedef struct {
  uint32_t insert_offset;
  EsmsDataInsertionMode insert_mode;
  uint32_t size;
  uint8_t filling_byte;
} EsmsAddPaddingCommand;

#define ADD_PADDING_COM_LEN  10

static inline int setEsmsAddPaddingCommand(
  EsmsAddPaddingCommand * dst,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  uint32_t size,
  uint8_t filling_byte
)
{
  *dst = (EsmsAddPaddingCommand) {
    .insert_offset = insert_offset,
    .insert_mode = insert_mode,
    .size = size,
    .filling_byte = filling_byte
  };

  return 0;
}

int applyEsmsAddPaddingCommand(
  EsmsAddPaddingCommand command,
  uint8_t * payload_dst,
  uint32_t payload_size
);

/* ###### ESMS Add Data Block command : #################################### */

typedef struct {
  uint32_t insert_offset;
  EsmsDataInsertionMode insert_mode;
  uint8_t data_block_id;
} EsmsAddDataBlockCommand;

#define ADD_DATA_BLOCK_COM_LEN  6

static inline int setEsmsAddDataBlockCommand(
  EsmsAddDataBlockCommand * dst,
  uint32_t offset,
  EsmsDataInsertionMode mode,
  uint8_t blockIdx
)
{
  *dst = (EsmsAddDataBlockCommand) {
    .insert_offset = offset,
    .insert_mode = mode,
    .data_block_id = blockIdx
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
  uint8_t * payload_dst,
  uint32_t payload_size,
  EsmsDataBlocks data_blocks
);

/* ###### ESMS script commands common : #################################### */

typedef union {
  EsmsAddDataCommand add_data;
  EsmsChangeByteOrderCommand change_byte_order;
  EsmsAddPesPayloadCommand add_pes_payload;
  EsmsAddPaddingCommand add_padding;
  EsmsAddDataBlockCommand add_data_block;
} EsmsCommandData;

typedef struct {
  EsmsCommandType type;
  EsmsCommandData data;
} EsmsCommand;

static inline void cleanEsmsCommand(
  EsmsCommand * com
)
{
  switch (com->type) {
    case ESMS_ADD_DATA:
      return cleanEsmsAddDataCommand(com->data.add_data);
    case ESMS_CHANGE_BYTEORDER:
    case ESMS_ADD_PAYLOAD_DATA:
    case ESMS_ADD_PADDING_DATA:
    case ESMS_ADD_DATA_BLOCK:
      break;
  }
}

/* ### ESMS script PES packet : ############################################ */

typedef struct {
  uint64_t cpb_removal_time;
  uint64_t dpb_output_time;
} EsmsPesPacketH264ExtData;

typedef union {
  EsmsPesPacketH264ExtData h264;
} EsmsPesPacketExtData;

typedef struct {
  uint8_t picture_type;
} EsmsPesPacketPropVideo;

typedef struct {
  bool extension_frame;
} EsmsPesPacketPropAudio;

typedef struct {
  LibbluESType type;

  union {
    EsmsPesPacketPropVideo video;
    EsmsPesPacketPropAudio audio;
  } prefix;

  bool pts_33bit;
  bool dts_present;
  bool dts_33bit;
  bool size_long_field;
  bool ext_data_present;
} EsmsPesPacketProp;

/** \~english
 * \brief ESMS PES frame script max allowed number of commands.
 */
#define ESMS_MAX_SUPPORTED_COMMANDS_NB  32

/** \~english
 * \brief ESMS PES frame header structure.
 */
typedef struct {
  EsmsPesPacketProp prop;  /**< Packet contained frame properties.           */

  uint64_t pts;   /**< Presentation Time Stamp in 90kHz clock ticks.         */
  uint64_t dts;   /**< Decoding Time Stamp in 90kHz clock ticks.             */
  uint32_t size;  /**< Packet size in bytes.                                 */

  EsmsPesPacketExtData ext_data;  /**< Extension data, present if
    #prop.ext_data_present is true.                                          */

  EsmsCommand commands[ESMS_MAX_SUPPORTED_COMMANDS_NB];  /**< PES frame
    building scripting commands.                                             */
  unsigned nb_commands;  /**< Number of used commands.                       */
} EsmsPesPacket;

static inline void cleanEsmsPesPacketPtr(
  EsmsPesPacket * pes_packet
)
{
  for (unsigned i = 0; i < pes_packet->nb_commands; i++)
    cleanEsmsCommand(&pes_packet->commands[i]);
}

/* ### ESMS files utilities : ############################################## */

/** \~english
 * \brief ESMS script file directory fetcher return values.
 *
 * Refer to #getDirectoryOffset() function.
 * It is possible to test value using val < 0 test to ensure success return.
 */
typedef enum {
  ESMS_DF_OK          =  0,  /**< Directory successfully fetched.            */
  ESMS_DF_READ_ERROR  = -1,  /**< ESMS script file reading error happen.     */
  ESMS_DF_INVALID     = -2,  /**< Invalid directories index.                 */
  ESMS_DF_NOT_FOUND   = -3   /**< Unable to found looked directory.          */
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
  int64_t * offset
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
  BitstreamReaderPtr esms_bs,
  uint64_t expected,
  unsigned size
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
 * \param ESMS_version_ret On success, script version returning pointer
 * (can be NULL).
 * \return ESMSFileValidatorRet Returned code.
 */
ESMSFileValidatorRet isAValidESMSFile(
  const lbc * esms_fp,
  uint64_t expected_flags,
  uint16_t * ESMS_version_ret
);

#endif