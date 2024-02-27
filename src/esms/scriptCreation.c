#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "scriptCreation.h"

#define FWRITE(fd, v, s, e)                                                   \
  do {                                                                        \
    uint8_t _buf[s];                                                          \
    uint64_t _v = (v);                                                        \
    size_t _s = (s);                                                          \
    for (size_t _i = 0; _i < _s; _i++)                                        \
      _buf[_i] = _v >> ((_s - _i - 1) << 3ull);                               \
    if (!fwrite(&_buf, _s, 1, fd))                                            \
      e;                                                                      \
  } while (0)

#define WRITE(br, v, s, e)                                                    \
  do {                                                                        \
    uint64_t _v = (v);                                                        \
    uint64_t _s = (s);                                                        \
    for (unsigned _i = 0; _i < _s; _i++) {                                    \
      if (writeByte((br), _v >> ((_s - _i - 1ull) << 3)) < 0)                 \
        e;                                                                    \
    }                                                                         \
  } while (0)

#define WRITE_ARRAY(br, a, s, e)                                              \
  do {                                                                        \
    if (writeBytes(br, (a), (s)) < 0)                                         \
      e;                                                                      \
  } while (0)

/* ### Pre-parsing : ####################################################### */

#define ESMS_DIRECTORY_IDX_LENGTH  9 // [u8 dir_ID] [u64 dir_offset]

int writeEsmsHeader(
  BitstreamWriterPtr esms_bs
)
{

  LIBBLU_SCRIPTWO_DEBUG("Writing ESMS file header with place holders.\n");
  LIBBLU_SCRIPTW_DEBUG(
    "Elementary Stream Manipulation Script (ESMS) header:\n"
  );

  /* [u32 ESMS_magic] */
  WRITE(esms_bs, ESMS_FILE_HEADER_MAGIC, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " ESMS File header magic: " STR(ESMS_FILE_HEADER_MAGIC) ".\n"
  );

  /* [u16 ESMS_version] */
  WRITE(esms_bs, ESMS_FORMAT_VER, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Format version: " STR(ESMS_FORMAT_VER) ".\n"
  );

  /** Reserved paddding
   * [u8 flags_byte] [u8 nb_directory] [v<7*ESMS_MAX_ALLOWED_DIR> reserved]
   */
  size_t pad_size = ESMS_DIRECTORY_IDX_LENGTH * ESMS_MAX_ALLOWED_DIR + 2;
  for (size_t i = 0; i < pad_size; i++)
    WRITE(esms_bs, 0x00, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " *Place holders* (%zu bytes).\n",
    pad_size
  );

  return 0;
}

/* ### ESMS Handler creation/destruction : ################################# */

EsmsHandlerPtr createEsmsHandler(
  LibbluESType type,
  LibbluESSettingsOptions options,
  LibbluESFmtSpecPropType fmt_prop_type
)
{
  LIBBLU_SCRIPTWO_DEBUG("Initialization of ESMS writting handler.\n");

  EsmsHandlerPtr esms_hdl = calloc(1, sizeof(EsmsHandler));
  if (NULL == esms_hdl)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  esms_hdl->type = type;
  esms_hdl->flags = computeFlagsLibbluESSettingsOptions(options);

  LIBBLU_SCRIPTWO_DEBUG(
    " Type: %s, Flags: 0x%016" PRIX64 ".\n",
    libbluESTypeStr(esms_hdl->type),
    esms_hdl->flags
  );

  /* Get the allocation size required by format specific properties */
  size_t fmt_prop_alloc_size = sizeofLibbluESFmtSpecPropType(
    fmt_prop_type
  );

  LIBBLU_SCRIPTWO_DEBUG(
    " Format specific properties size: %zu byte(s).\n",
    fmt_prop_alloc_size
  );

  /* If (FMT_SPEC_INFOS_NONE != fmt_prop_type), allocate the required space. */
  if (0 < fmt_prop_alloc_size) {
    void * fmt_prop_ptr = malloc(fmt_prop_alloc_size);
    if (NULL == fmt_prop_ptr)
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
    esms_hdl->fmt_prop.shared_ptr = fmt_prop_ptr;
  }

  return esms_hdl;

free_return:
  destroyEsmsHandler(esms_hdl);
  return NULL;
}

/** \~english
 * \brief Add given directory parameters to ESMS esms_hdl indexer.
 *
 * \param esms_hdl Destination ESMS esms_hdl handle.
 * \param dir_ID Id of the directory.
 * \param dir_offset Offset of the directory in ESMS esms_hdl output file.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _appendDirEsms(
  EsmsHandlerPtr esms_hdl,
  uint8_t dir_ID,
  int64_t dir_offset
)
{
  EsmsFileDirectories * dirs = &esms_hdl->directories;

  LIBBLU_SCRIPTWO_DEBUG(
    "Appending directory '%s' (dir_ID = %u) at dir_offset = 0x%" PRIX64 ".\n",
    ESMSDirectoryIdStr(dir_ID),
    dir_ID,
    dir_offset
  );

  if (dir_offset < 0)
    LIBBLU_ERROR_RETURN("Out of range ESMS directory offset.\n");
  if (ESMS_MAX_ALLOWED_DIR <= dirs->nb_used_entries)
    LIBBLU_ERROR_RETURN("Too many directories saved in ESMS file.\n");

  for (unsigned idx = 0; idx < dirs->nb_used_entries; idx++) {
    if (dirs->entries[idx].dir_ID == dir_ID)
      LIBBLU_ERROR_RETURN("ESMS directory ID %u duplicated.\n", dir_ID);
  }

  /* Adding values : */
  dirs->entries[dirs->nb_used_entries++] = (EsmsFileDirectory) {
    .dir_ID = dir_ID,
    .dir_offset = dir_offset
  };

  return 0;
}

#define APPEND_DIR(hdl, bs, id, e)                                            \
  do {                                                                        \
    if (_appendDirEsms(hdl, id, tellWritingPos(bs)) < 0)                      \
      e;                                                                      \
  } while (0)

int appendSourceFileWithCrcEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const lbc * filename,
  uint16_t crc_checked_bytes,
  uint32_t crc,
  uint8_t * src_file_idx
)
{
  assert(NULL != esms_hdl);
  assert(NULL != filename);

  LIBBLU_SCRIPTWO_DEBUG(
    "Appending source file '%" PRI_LBCS "' "
    "with checksum 0x%08" PRIX32 " (%" PRIu16 " byte(s) covered).\n",
    filename,
    crc,
    crc_checked_bytes
  );

  if (isPresentEsmsESSourceFiles(esms_hdl->source_files, filename))
    LIBBLU_ERROR_RETURN("Duplicated source filename in ESMS file.\n");

  EsmsESSourceFileProperties prop = (EsmsESSourceFileProperties) {
    .crc_checked_bytes = crc_checked_bytes,
    .crc = crc
  };

  if (appendEsmsESSourceFiles(&esms_hdl->source_files, filename, prop) < 0)
    return -1;

  LIBBLU_SCRIPTWO_DEBUG(
    "Appended source file index (src_file_idx): 0x%02X.\n",
    esms_hdl->source_files.nb_source_files - 1
  );

  if (NULL != src_file_idx)
    *src_file_idx = esms_hdl->source_files.nb_source_files - 1;

  return 0;
}

int appendSourceFileEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const lbc * filepath,
  uint8_t * src_file_idx
)
{
  BitstreamReaderPtr input;

  if (NULL == (input = createBitstreamReader(filepath, READ_BUFFER_LEN)))
    return -1;

  int64_t input_size = remainingSize(input);
  uint16_t crc_size = MIN(input_size, CRC32_USED_BYTES);
  uint8_t crc_buf[CRC32_USED_BYTES];

  if (readBytes(input, crc_buf, crc_size) < 0)
    goto free_return;
  uint32_t crc_value = lb_compute_crc32(crc_buf, 0, crc_size);

  int ret = appendSourceFileWithCrcEsmsHandler(
    esms_hdl,
    filepath,
    crc_size,
    crc_value,
    src_file_idx
  );

  closeBitstreamReader(input);
  return ret;

free_return:
  closeBitstreamReader(input);
  return -1;
}

int appendDataBlockEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const uint8_t * data_block,
  uint32_t data_block_size,
  unsigned * data_block_id
)
{
  assert(NULL != esms_hdl);
  assert((0 == data_block_size) ^ (NULL != data_block));

  uint8_t * data_block_copy = NULL;
  if (0 < data_block_size) {
    if (NULL == (data_block_copy = (uint8_t *) malloc(data_block_size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    memcpy(data_block_copy, data_block, data_block_size);
  }

  EsmsDataBlockEntry entry = (EsmsDataBlockEntry) {
    .data_block = data_block_copy,
    .data_block_size = data_block_size
  };

  if (appendEsmsDataBlocks(&esms_hdl->data_blocks, entry, data_block_id) < 0)
    goto free_return;
  return 0;

free_return:
  cleanEsmsDataBlockEntry(entry);
  return -1;
}

int updateDataBlockEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const uint8_t * data_block,
  uint32_t data_block_size,
  unsigned data_block_idx
)
{
  assert(NULL != esms_hdl);
  assert((0 == data_block_size) ^ (NULL != data_block));

  uint8_t * data_block_copy = NULL;
  if (0 < data_block_size) {
    if (NULL == (data_block_copy = (uint8_t *) malloc(data_block_size)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    memcpy(data_block_copy, data_block, data_block_size);
  }

  EsmsDataBlockEntry entry = (EsmsDataBlockEntry) {
    .data_block = data_block_copy,
    .data_block_size = data_block_size
  };

  if (updateEsmsDataBlocks(&esms_hdl->data_blocks, entry, data_block_idx) < 0)
    goto free_return;
  return 0;

free_return:
  cleanEsmsDataBlockEntry(entry);
  return -1;
}

/* ### PES cutting script : ################################################ */

/* ###### Building : ####################################################### */


static int _initEsmsPesPacket(
  EsmsHandlerPtr esms_hdl,
  EsmsPesPacket prop
)
{
  assert(NULL != esms_hdl);

  EsmsCommandsPipeline * pipeline = &esms_hdl->commands_pipeline;

  if (pipeline->initialized_frame)
    LIBBLU_ERROR_RETURN("Attempt to double initialize ESMS PES frame.\n");

  pipeline->cur_frame = prop;
  pipeline->initialized_frame = true;

  return 0;
}


int initVideoPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint8_t picture_type,
  bool dts_present,
  uint64_t pts,
  uint64_t dts
)
{
  if (esms_hdl->type != ES_VIDEO)
    LIBBLU_ERROR_RETURN("Can't create video PES frame in non-video stream.\n");

  return _initEsmsPesPacket(
    esms_hdl,
    (EsmsPesPacket) {
      .prop = {
        .type = esms_hdl->type,
        .prefix.video.picture_type = picture_type,
        .dts_present = dts_present
      },
      .pts = pts,
      .dts = dts
    }
  );
}


int initAudioPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  bool extension_frame,
  bool dts_present,
  uint64_t pts,
  uint64_t dts
)
{
  if (esms_hdl->type != ES_AUDIO)
    LIBBLU_ERROR_RETURN("Can't create audio PES frame in non-audio stream.\n");

  return _initEsmsPesPacket(
    esms_hdl,
    (EsmsPesPacket) {
      .prop = {
        .type = esms_hdl->type,
        .prefix.audio.extension_frame = extension_frame,
        .dts_present = dts_present
      },
      .pts = pts,
      .dts = dts
    }
  );
}


int initHDMVPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  bool dts_present,
  uint64_t pts,
  uint64_t dts
)
{
  if (esms_hdl->type != ES_HDMV)
    LIBBLU_ERROR_RETURN("Can't create HDMV PES frame in non-HDMV stream.\n");

  assert(0 == (pts >> 33));
  assert(0 == (dts >> 33) || !dts_present);

  return _initEsmsPesPacket(
    esms_hdl,
    (EsmsPesPacket) {
      .prop.dts_present = dts_present,
      .pts = pts,
      .dts = dts
    }
  );
}


bool _isEsmsPesPacketExtensionDataSupported(
  EsmsHandlerPtr esms_hdl
)
{
  switch (esms_hdl->prop.coding_type) {
  case STREAM_CODING_TYPE_AVC:
    return true;
  default:
    return false;
  }
}


int setExtensionDataPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  EsmsPesPacketExtData data
)
{
  EsmsCommandsPipeline * pipeline = &esms_hdl->commands_pipeline;

  if (!_isEsmsPesPacketExtensionDataSupported(esms_hdl))
    LIBBLU_ERROR_RETURN(
      "No codec specific ESMS PES frame extension data available.\n"
    );

  if (!pipeline->initialized_frame)
    LIBBLU_ERROR_RETURN(
      "Missing a pending destination initialized PES frame "
      "for the extension data.\n"
    );

  if (pipeline->cur_frame.prop.ext_data_present)
    LIBBLU_ERROR_RETURN(
      "Extension data as already been defined for the pending PES frame.\n"
    );

  pipeline->cur_frame.ext_data = data;
  pipeline->cur_frame.prop.ext_data_present = true;
  return 0;
}


static EsmsCommand * _newCommand(
  EsmsHandlerPtr esms_hdl
)
{
  EsmsPesPacket * cur_frame = &esms_hdl->commands_pipeline.cur_frame;

  if (ESMS_MAX_SUPPORTED_COMMANDS_NB <= cur_frame->nb_commands)
    LIBBLU_ERROR_NRETURN("Too many scripting commands in ESMS PES frame.\n");

  return &cur_frame->commands[cur_frame->nb_commands++];
}


int appendAddDataCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  const uint8_t * data,
  uint16_t data_size
)
{
  if (INT16_MAX - 5 < data_size)
    LIBBLU_ERROR_RETURN("Add data command size limit exceeded.\n");

  uint8_t * new_data;
  if (NULL == (new_data = (uint8_t *) malloc(data_size)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  memcpy(new_data, data, data_size);

  EsmsCommand * com;
  if (NULL == (com = _newCommand(esms_hdl))) {
    free(new_data);
    return -1;
  }

  *com = (EsmsCommand) {
    .type = ESMS_ADD_DATA,
    .data.add_data = {
      .insert_offset = insert_offset,
      .insert_mode = insert_mode,
      .data = new_data,
      .data_size = data_size
    }
  };

  return 0;
}


int appendChangeByteOrderCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint8_t unit_size,
  uint32_t section_offset,
  uint32_t section_size
)
{

  if (section_size % unit_size) {
    /* Data field is not composed of complete blocks of unit_size bytes,
    swapping cannot be processed. */
    LIBBLU_ERROR_RETURN(
      "Byte order change commmand domain length is invalid and shall be a "
      "modulo of unit_size %" PRIu8 ".\n", unit_size
    );
  }

  EsmsCommand * com;
  if (NULL == (com = _newCommand(esms_hdl)))
    return -1;

  *com = (EsmsCommand) {
    .type = ESMS_CHANGE_BYTEORDER,
    .data.change_byte_order = {
      .unit_size = unit_size,
      .section_offset = section_offset,
      .section_size = section_size
    }
  };

  return 0;
}


int appendAddPesPayloadCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint8_t src_file_id,
  uint32_t insert_offset,
  uint64_t src_offset,
  uint32_t size
)
{
  assert(NULL != esms_hdl);
  assert(0 < size);

  if (esms_hdl->source_files.nb_source_files <= src_file_id)
    LIBBLU_ERROR_RETURN("Unknown source file index %u.\n", src_file_id);

  EsmsCommand * com;
  if (NULL == (com = _newCommand(esms_hdl)))
    return -1;

  *com = (EsmsCommand) {
    .type = ESMS_ADD_PAYLOAD_DATA,
    .data.add_pes_payload = {
      .src_file_id = src_file_id,
      .insert_offset = insert_offset,
      .src_offset = src_offset,
      .size = size
    }
  };

  return 0;
}


int appendPaddingDataCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  uint32_t size,
  uint8_t filling_byte
)
{
  assert(NULL != esms_hdl);
  assert(0 < size);

  EsmsCommand * com;
  if (NULL == (com = _newCommand(esms_hdl)))
    return -1;

  *com = (EsmsCommand) {
    .type = ESMS_ADD_PADDING_DATA,
    .data.add_padding = {
      .insert_offset = insert_offset,
      .insert_mode = insert_mode,
      .size = size,
      .filling_byte = filling_byte
    }
  };

  return 0;
}


int appendAddDataBlockCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  uint8_t data_block_id
)
{
  assert(NULL != esms_hdl);

  if (esms_hdl->data_blocks.nb_data_blocks <= data_block_id)
    LIBBLU_ERROR_RETURN("Unknown data block id %u.\n", data_block_id);

  EsmsCommand * com;
  if (NULL == (com = _newCommand(esms_hdl)))
    return -1;

  *com = (EsmsCommand) {
    .type = ESMS_ADD_DATA_BLOCK,
    .data.add_data_block = {
      .insert_offset = insert_offset,
      .insert_mode = insert_mode,
      .data_block_id = data_block_id
    }
  };

  return 0;
}


/* ###### Writing : ######################################################## */


static int _writeEsmsPesCuttingHeader(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  /* Write on output file ESMS PES Cutting section header. */
  APPEND_DIR(esms_hdl, esms_bs, ESMS_DIRECTORY_ID_ES_PES_CUTTING, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "PES frames Cutting script section:\n"
  );

  /* [v32 PESC_magic] */
  WRITE(esms_bs, PES_CUTTING_HEADER_MAGIC, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " PES frames Cutting script section magic (PESC_magic): "
    STR(PES_CUTTING_HEADER_MAGIC) ".\n"
  );

  return 0;
}


static int _writeEsmsPesPacketH264ExtData(
  BitstreamWriterPtr esms_bs,
  const EsmsPesPacketH264ExtData * p
)
{
  bool large_tf = (p->cpb_removal_time >> 32) || (p->dpb_output_time >> 32);

  LIBBLU_SCRIPTW_DEBUG(
    "    Type: H.264 Extension data (inferred).\n"
  );

  /* [u16 ext_data_size] */
  uint16_t ext_data_size = large_tf ? 17 : 9;
  WRITE(esms_bs, ext_data_size, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "    Extension data size (ext_data_size): %" PRIu16 " (0x%04" PRIX16 ").\n"
  );

  /* [b1 large_time_fields] [v7 reserved] */
  WRITE(esms_bs, large_tf << 7, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "    Large time fields (large_time_fields): %s (0x%02X).\n",
    BOOL_STR(large_tf),
    large_tf << 7
  );

  /* [u<32 << large_time_fields> cpb_removal_time] */
  WRITE(esms_bs, p->cpb_removal_time, 4 << large_tf, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "    Coded Picture Buffer removal time (cpb_removal_time): "
    "%" PRIu64 " (0x%0*" PRIX64 ").\n",
    p->cpb_removal_time,
    8 << large_tf,
    p->cpb_removal_time
  );

  /* [u<32 << large_time_fields> dpb_output_time] */
  WRITE(esms_bs, p->dpb_output_time, 4 << large_tf, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "    Decoded Picture Buffer output time (dpb_output_time): "
    "%" PRIu64 " (0x%0*" PRIX64 ").\n",
    p->dpb_output_time,
    8 << large_tf,
    p->dpb_output_time
  );

  return 0;
}


static int _writeEsmsPesPacketExtensionData(
  BitstreamWriterPtr esms_bs,
  EsmsPesPacketExtData ext_param,
  LibbluStreamCodingType coding_type
)
{
  switch (coding_type) {
  case STREAM_CODING_TYPE_AVC:
    return _writeEsmsPesPacketH264ExtData(esms_bs, &ext_param.h264);
  default:
    break;
  }

  /* [v16 ext_data_size] // 0: No extension */
  WRITE(esms_bs, 0, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "    Extension data size (ext_data_size): 0 (no extension).\n"
  );

  return 0;
}


static int _writeEsmsAddDataCommand(
  BitstreamWriterPtr esms_bs,
  const EsmsAddDataCommand * com
)
{
  /* [u16 command_size] */
  uint16_t command_size = ADD_DATA_COM_LEN + com->data_size;
  WRITE(esms_bs, command_size, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Command size (command_size): %" PRIu16 " (0x%04" PRIX16 ").\n",
    command_size,
    command_size
  );

  /* [u32 insert_offset] */
  WRITE(esms_bs, com->insert_offset, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion offset (insert_offset): %" PRIu32 " (0x%08" PRIX32 ").\n",
    com->insert_offset,
    com->insert_offset
  );

  /* [u8 insert_mode] */
  WRITE(esms_bs, com->insert_mode, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion mode (insert_mode): %s (0x%02" PRIX8 ").\n",
    EsmsDataInsertionModeStr(com->insert_mode),
    com->insert_mode
  );

  /* [v(n) data] */
  WRITE_ARRAY(esms_bs, com->data, com->data_size, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Inserted data (data): %" PRIu16 " byte(s).\n",
    com->data_size
  );

  return 0;
}


static int _writeEsmsChangeByteOrderCommand(
  BitstreamWriterPtr esms_bs,
  const EsmsChangeByteOrderCommand * com
)
{
  /* [u16 command_size] */
  uint16_t command_size = CHANGE_BYTEORDER_COM_LEN;
  WRITE(esms_bs, command_size, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Command size (command_size): %" PRIu16 " (0x%04" PRIX16 ").\n",
    command_size,
    command_size
  );

  /* [u8 unit_size] */
  WRITE(esms_bs, com->unit_size, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Unit size (unit_size): %" PRIu8 " bytes (0x%02" PRIX8 ").\n",
    com->unit_size,
    com->unit_size
  );

  /* [u32 section_offset] */
  WRITE(esms_bs, com->section_offset, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Section offset (section_offset): %" PRIu32 " (0x%08" PRIX32 ").\n",
    com->section_offset,
    com->section_offset
  );

  /* [u32 section_size] */
  WRITE(esms_bs, com->section_size, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Section offset (section_size): %" PRIu32 " (0x%08" PRIX32 ").\n",
    com->section_size,
    com->section_size
  );

  return 0;
}


static int _writeEsmsAddPesPayloadCommand(
  BitstreamWriterPtr esms_bs,
  const EsmsAddPesPayloadCommand * com
)
{
  bool src_offset_ext_present = com->src_offset >> 32;
  bool size_ext_present       = com->size >> 16;

  /* [u16 command_size] */
  uint16_t command_size = sizeEsmsAddPesPayloadCommand(
    src_offset_ext_present,
    size_ext_present
  );
  WRITE(esms_bs, command_size, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Command size (command_size): %" PRIu16 " (0x%" PRIX16 ").\n",
    command_size,
    command_size
  );

  /* [b1 src_offset_ext_present] */
  /* [b1 size_ext_present] */
  /* [v6 reserved] */
  uint8_t flags_byte = (src_offset_ext_present << 7) | (size_ext_present << 6);
  WRITE(esms_bs, flags_byte, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Source offset extension present (src_offset_ext_present): "
    "%s (0x%02X).\n",
    BOOL_STR(src_offset_ext_present),
    src_offset_ext_present << 7
  );
  LIBBLU_SCRIPTW_DEBUG(
    "     Size extension present (size_ext_present): "
    "%s (0x%02X).\n",
    BOOL_STR(size_ext_present),
    size_ext_present << 6
  );

  /* [u8 src_file_id] */
  WRITE(esms_bs, com->src_file_id, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Source file id (src_file_id): 0x%02" PRIX8 ".\n",
    com->src_file_id
  );

  /* [u32 insert_offset] */
  WRITE(esms_bs, com->insert_offset, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion offset (insert_offset): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    com->insert_offset,
    com->insert_offset
  );

  /* [u32 src_offset] */
  WRITE(esms_bs, com->src_offset, 4, return -1);

  if (src_offset_ext_present) {
    /* [u32 src_offset_ext] */
    WRITE(esms_bs, com->src_offset >> 32, 4, return -1);
  }

  LIBBLU_SCRIPTW_DEBUG(
    "     Source offset (src_offset/src_offset_ext): "
    "%" PRId64 " (0x%0*" PRIX64 ").\n",
    com->src_offset,
    8 << src_offset_ext_present,
    com->src_offset
  );

  /* [u16 size] */
  WRITE(esms_bs, com->size, 2, return -1);

  if (size_ext_present) {
    /* [u16 size_ext] */
    WRITE(esms_bs, com->size >> 16, 2, return -1);
  }

  LIBBLU_SCRIPTW_DEBUG(
    "     Size (size/size_ext): %" PRIu32 " (0x%0*" PRIX32 ").\n",
    com->size,
    4 << size_ext_present,
    com->size
  );

  return 0;
}


static int _writeEsmsAddPaddingDataCommand(
  BitstreamWriterPtr esms_bs,
  const EsmsAddPaddingCommand * com
)
{
  /* [u16 command_size] */
  uint16_t command_size = ADD_PADD_COM_LEN;
  WRITE(esms_bs, command_size, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Command size (command_size): %" PRIu16 " (0x%" PRIX16 ").\n",
    command_size,
    command_size
  );

  /* [u32 insert_offset] */
  WRITE(esms_bs, com->insert_offset, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion offset (insert_offset): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    com->insert_offset,
    com->insert_offset
  );

  /* [u8 insert_mode] */
  WRITE(esms_bs, com->insert_mode, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion mode (insert_mode): %s (0x%02" PRIX8 ").\n",
    EsmsDataInsertionModeStr(com->insert_mode),
    com->insert_mode
  );

  /* [u32 size] */
  WRITE(esms_bs, com->size, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Size (size): %" PRIu32 " (0x%08" PRIX32 ").\n",
    com->size,
    com->size
  );

  /* [u8 filling_byte] */
  WRITE(esms_bs, com->filling_byte, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Filling byte (filling_byte): 0x%02" PRIX8 ".\n",
    com->filling_byte
  );

  return 0;
}


static int _writeEsmsAddDataBlockCommand(
  BitstreamWriterPtr esms_bs,
  const EsmsAddDataBlockCommand * com
)
{
  /* [u16 command_size] */
  uint16_t command_size = ADD_DATA_SECTION_COM_LEN;
  WRITE(esms_bs, command_size, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Command size (command_size): %" PRIu16 " (0x%" PRIX16 ").\n",
    command_size,
    command_size
  );

  /* [u32 insert_offset] */
  WRITE(esms_bs, com->insert_offset, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion offset (insert_offset): "
    "%" PRIu32 " (0x%08" PRIX32 ").\n",
    com->insert_offset,
    com->insert_offset
  );

  /* [u8 insert_mode] */
  WRITE(esms_bs, com->insert_mode, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Insertion mode (insert_mode): %s (0x%02" PRIX8 ").\n",
    EsmsDataInsertionModeStr(com->insert_mode),
    com->insert_mode
  );

  /* [u8 data_block_id] */
  WRITE(esms_bs, com->data_block_id, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Data block ID (data_block_id): 0x%02" PRIX8 ".\n",
    com->data_block_id
  );

  return 0;
}


static int _writeEsmsCommand(
  BitstreamWriterPtr esms_bs,
  const EsmsCommand * com
)
{

  /* [u8 command_type] */
  WRITE(esms_bs, com->type, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "     Command type: %s (0x%" PRIX8 ").\n",
    EsmsCommandTypeStr(com->type),
    com->type
  );

  /* [u16 command_size] [vn command()] */
  const EsmsCommandData * cd = &com->data;
  switch (com->type) {
  case ESMS_ADD_DATA:
    return _writeEsmsAddDataCommand(esms_bs, &cd->add_data);
  case ESMS_CHANGE_BYTEORDER:
    return _writeEsmsChangeByteOrderCommand(esms_bs, &cd->change_byte_order);
  case ESMS_ADD_PAYLOAD_DATA:
    return _writeEsmsAddPesPayloadCommand(esms_bs, &cd->add_pes_payload);
  case ESMS_ADD_PADDING_DATA:
    return _writeEsmsAddPaddingDataCommand(esms_bs, &cd->add_padding);
  case ESMS_ADD_DATA_BLOCK:
    return _writeEsmsAddDataBlockCommand(esms_bs, &cd->add_data_block);
  }

  LIBBLU_ERROR_RETURN("Unknown com type %d.\n", com->type);
}

static uint32_t _computeInsertSize(
  uint64_t cur_size,
  uint32_t insert_off,
  uint32_t insert_size,
  EsmsDataInsertionMode insert_mode
)
{
  switch (insert_mode) {
  case INSERTION_MODE_OVERWRITE:
    return MAX(cur_size, insert_off + insert_size);
  case INSERTION_MODE_INSERT:
    return cur_size + insert_size;
  }

  LIBBLU_ERROR_RETURN("Unknown insert mode %u.\n", insert_mode);
}

/** \~english
 * \brief Compute the final PES frame size in bytes after use of frame
 * associated esms_hdl commands.
 *
 * \return uint32_t Size of the PES frame in bytes.
 */
static uint32_t _computePesPacketSize(
  const EsmsPesPacket * pes,
  const EsmsDataBlocks * data_blks
)
{
  uint64_t size = 0;

  for (unsigned i = 0; i < pes->nb_commands; i++) {
    const EsmsCommand * com = &pes->commands[i];

  switch (com->type) {
    case ESMS_ADD_DATA:
      const EsmsAddDataCommand * ad_c = &com->data.add_data;
      size = _computeInsertSize(
        size, ad_c->insert_offset, ad_c->data_size, ad_c->insert_mode
      );
      break;

    case ESMS_ADD_PAYLOAD_DATA:
      const EsmsAddPesPayloadCommand * app_c = &com->data.add_pes_payload;
      size = MAX(size, app_c->insert_offset + app_c->size);
      break;

    case ESMS_ADD_PADDING_DATA:
      const EsmsAddPaddingCommand * ap_c = &com->data.add_padding;
      size = MAX(size, ap_c->insert_offset + ap_c->size);
      break;

    case ESMS_CHANGE_BYTEORDER:
      break;

    case ESMS_ADD_DATA_BLOCK:
      const EsmsAddDataBlockCommand * adb_c = &com->data.add_data_block;
      const EsmsDataBlockEntry * dbe = &data_blks->entries[adb_c->data_block_id];
      size = _computeInsertSize(
        size, adb_c->insert_offset, dbe->data_block_size, adb_c->insert_mode
      );
    }
  }

  if (UINT32_MAX < size)
    LIBBLU_ERROR_ZRETURN("PES frame size overflow.\n");
  return (uint32_t) size;
}

static int _setPesPacketProperties(
  const EsmsHandlerPtr esms_hdl,
  EsmsPesPacket * pes
)
{
  /* Compute and set size : */
  uint32_t size;
  if (0 == (size = _computePesPacketSize(pes, &esms_hdl->data_blocks)))
    LIBBLU_ERROR_RETURN("Unable to compute ESMS PES packet size.\n");
  pes->size = size;

  /* Update properties : */
  pes->prop.type            = esms_hdl->type;
  pes->prop.pts_33bit       = (pes->pts  >> 32) & 0x1;
  pes->prop.dts_33bit       = (pes->dts  >> 32) & 0x1;
  pes->prop.size_long_field = (pes->size >> 16);

  return 0;
}

static uint16_t _getPesPacketPropWord(
  const EsmsPesPacket * pes_packet
)
{
  const EsmsPesPacketProp * pes_packet_prop = &pes_packet->prop;
  uint16_t prefix = 0x00;

  switch (pes_packet_prop->type) {
  case ES_VIDEO:
    const EsmsPesPacketPropVideo * vprop = &pes_packet_prop->prefix.video;

    /* [u2 picture_type] [v5 reserved] [v1 '0' marker_bit] */
    prefix = vprop->picture_type << 6;

    LIBBLU_SCRIPTW_DEBUG(
      "   Picture type (picture_type): 0x%X.\n",
      vprop->picture_type
    );
    break;

  case ES_AUDIO:
    const EsmsPesPacketPropAudio * aprop = &pes_packet_prop->prefix.audio;

    /* [b1 extension_frame] [v6 reserved] [v1 '0' marker_bit] */
    prefix = aprop->extension_frame << 7;
    LIBBLU_SCRIPTW_DEBUG(
      "   Audio extension frame (extension_frame): %s (0x%X).\n",
      BOOL_STR(aprop->extension_frame),
      aprop->extension_frame
    );
    break;

  case ES_HDMV:
    /* [v7 reserved] [v1 '0' marker_bit] */
    break;

  default:
    /* Error, unexpected type */
    LIBBLU_ERROR_RETURN(
      "Unexpected ESMS stream type %u at PES frame writing.\n",
      pes_packet_prop->type
    );
  }

  assert(0x0 == (prefix & 0x1)); // Ensure presence of '0' marker_bit.

  /** [v8 frame_prop_flags]
   * -> b1: pts_33bit
   * -> b1: dts_present
   * -> b1: dts_33bit
   * -> b1: size_long_field
   * -> b1: ext_data_present
   * -> v3: reserved
   */
  uint8_t frame_prop_flags = (
    (pes_packet_prop->pts_33bit          << 7)
    | (pes_packet_prop->dts_present      << 6)
    | (pes_packet_prop->dts_33bit        << 5)
    | (pes_packet_prop->size_long_field  << 4)
    | (pes_packet_prop->ext_data_present << 3)
  );

  LIBBLU_SCRIPTW_DEBUG(
    "   Flags properties flags (frame_prop_flags): 0x%02" PRIX16 ".\n",
    frame_prop_flags
  );
#define _P(s, v)  LIBBLU_SCRIPTW_DEBUG("    " s ": %s;\n", BOOL_STR(v))
  _P("PTS 33rd bit value", pes_packet_prop->pts_33bit);
  _P("DTS field presence", pes_packet_prop->dts_present);
  _P("DTS 33rd bit value", pes_packet_prop->dts_33bit);
  _P("Long size field", pes_packet_prop->size_long_field);
  _P("Extension data presence", pes_packet_prop->ext_data_present);
#undef _P

  return (prefix << 8) | frame_prop_flags;
}

int writePesPacketEsmsHandler(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  assert(NULL != esms_bs);
  assert(NULL != esms_hdl);

  EsmsCommandsPipeline * pipeline = &esms_hdl->commands_pipeline;
  if (!pipeline->initialized_frame)
    LIBBLU_ERROR_RETURN("Attempt to write uninitialized ESMS PES frame.\n");

  if (pipeline->nb_completed_frames == 0) {
    if (_writeEsmsPesCuttingHeader(esms_bs, esms_hdl) < 0)
      return -1;
  }

  EsmsPesPacket * pes_packet = &pipeline->cur_frame;
  if (_setPesPacketProperties(esms_hdl, pes_packet) < 0)
    return -1;
  EsmsPesPacketProp * pes_packet_prop = &pes_packet->prop;

  LIBBLU_SCRIPTW_DEBUG(
    "  Script frame (0x%016" PRIX64 "):\n",
    tellWritingPos(esms_bs)
  );

  /* [v16 prop_word] */
  uint16_t prop_word = _getPesPacketPropWord(pes_packet);
  WRITE(esms_bs, prop_word, 2, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "   => Properties word: 0x%04" PRIX16 ".\n",
    prop_word
  );

  /* [u32 pts] */
  WRITE(esms_bs, pes_packet->pts, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "   PTS lowest 32-bits (pts): %" PRIu64 " (0x%08" PRIX64 ").\n",
    pes_packet->pts & 0xFFFFFFFF,
    pes_packet->pts & 0xFFFFFFFF
  );

  if (pes_packet_prop->dts_present) {
    /* [u32 dts] */
    WRITE(esms_bs, pes_packet->dts, 4, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "   DTS lowest 32-bits (dts): %" PRIu64 " (0x%08" PRIX64 ").\n",
      pes_packet->dts & 0xFFFFFFFF,
      pes_packet->dts & 0xFFFFFFFF
    );
  }

  if (pes_packet_prop->ext_data_present) {
    LIBBLU_SCRIPTW_DEBUG("   Extension data:\n");

    int ret = _writeEsmsPesPacketExtensionData(
      esms_bs,
      pes_packet->ext_data,
      esms_hdl->prop.coding_type
    );
    if (ret < 0)
      return -1;
  }

  /* [u<16 << size_long_field> size] */
  WRITE(esms_bs, pes_packet->size, 2 << pes_packet_prop->size_long_field, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "   PES packet size (size): %" PRIu32 " (0x%0*" PRIX32 ").\n",
    pes_packet->size,
    4 << pes_packet_prop->size_long_field,
    pes_packet->size
  );

  /* [u8 nb_commands] */
  WRITE(esms_bs, pes_packet->nb_commands, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "   Number of commands (nb_commands): %u (0x%02" PRIX8 ").\n",
    pes_packet->nb_commands,
    pes_packet->nb_commands
  );

  /* Writing modification esms_hdl commands : */
  for (unsigned i = 0; i < pes_packet->nb_commands; i++) {
    EsmsCommand * com = &pes_packet->commands[i];
    LIBBLU_SCRIPTW_DEBUG("    Command %u:\n", i);

    if (_writeEsmsCommand(esms_bs, com) < 0)
      return -1;

    cleanEsmsCommand(com);
  }

  pipeline->nb_completed_frames++;
  pipeline->initialized_frame = false;
  return 0;
}

/* ###### Complete script : ################################################ */

static int _setStreamType(
  EsmsHandlerPtr esms_hdl
)
{
  LibbluESType type;
  if (determineLibbluESType(esms_hdl->prop.coding_type, &type) < 0)
    LIBBLU_ERROR_RETURN("Corrupted ESMS, invalid stream type.\n");

  esms_hdl->type = type;
  return 0;
}


// static size_t _encodeUtf8(
//   char ** enc_string_ret,
//   const lbc * string,
//   size_t max_size
// )
// {
//   char * encoded_string;
//   if (NULL == (encoded_string = lbc_convfrom(string)))
//     return 0;

//   size_t enc_string_size;
//   if (max_size < (enc_string_size = strlen(encoded_string)))
//     LIBBLU_ERROR_FRETURN("String UTF-8 conversion size overflow.\n");

//   *enc_string_ret = encoded_string;
//   return enc_string_size;

// free_return:
//   free(encoded_string);
//   return 0;
// }


static int _writeEsmsEsPropertiesSection(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  LIBBLU_SCRIPTW_DEBUG(
    "ES Properties section:\n"
  );

  /* [v32 ESPR_magic] */
  WRITE(esms_bs, ES_PROPERTIES_HEADER_MAGIC, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " ES Properties section magic (ESPR_magic): "
    STR(ES_PROPERTIES_HEADER_MAGIC) ".\n"
  );

  if (_setStreamType(esms_hdl) < 0)
    return -1;

  /* [u8 type] */
  WRITE(esms_bs, esms_hdl->type, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Type (type): %s (0x%02" PRIX8 ").\n",
    libbluESTypeStr(esms_hdl->type),
    esms_hdl->type
  );

  /* [u8 coding_type] */
  WRITE(esms_bs, esms_hdl->prop.coding_type, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Stream coding type (coding_type): %s (0x%02" PRIX8 ").\n",
    LibbluStreamCodingTypeStr(esms_hdl->prop.coding_type),
    esms_hdl->prop.coding_type
  );

  /* [b1 PTS_reference_33bit] [b1 PTS_final_33bit] [v6 reserved] */
  uint8_t byte = (
    (((esms_hdl->PTS_reference >> 32) & 0x1) << 7)
    | (((esms_hdl->PTS_final   >> 32) & 0x1) << 6)
  );
  WRITE(esms_bs, byte, 1, return -1);

  /* [u32 PTS_reference] */
  WRITE(esms_bs, esms_hdl->PTS_reference, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Presentation Time Stamp reference lowest 32-bits (PTS_reference): "
    "%" PRIu64 " (0x%08" PRIX64 ").\n",
    esms_hdl->PTS_reference & 0xFFFFFFFF,
    esms_hdl->PTS_reference & 0xFFFFFFFF
  );

  /* [u32 PTS_final] */
  WRITE(esms_bs, esms_hdl->PTS_final, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Last Presentation Time Stamp lowest 32-bits (PTS_final): "
    "%" PRIu64 " (0x%08" PRIX64 ").\n",
    esms_hdl->PTS_final & 0xFFFFFFFF,
    esms_hdl->PTS_final & 0xFFFFFFFF
  );

  /* [u32 bitrate] */
  WRITE(esms_bs, esms_hdl->bitrate, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Nominal Bit-Rate (bitrate): %" PRIu32 " (0x%08" PRIX32 ").\n",
    esms_hdl->bitrate,
    esms_hdl->bitrate
  );

  /* [u64 scripting_flags] */
  WRITE(esms_bs, esms_hdl->flags, 8, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Script building flags (scripting_flags): 0x%016" PRIX64 ".\n",
    esms_hdl->flags
  );

  /* [u8 nb_source_files] */
  WRITE(esms_bs, esms_hdl->source_files.nb_source_files, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Number of source files (nb_source_files): %" PRIu8 " (0x%02" PRIX8 ").\n",
    esms_hdl->source_files.nb_source_files,
    esms_hdl->source_files.nb_source_files
  );

  for (unsigned i = 0; i < esms_hdl->source_files.nb_source_files; i++) {
    const EsmsESSourceFilesEntry * entry = &esms_hdl->source_files.entries[i];
    LIBBLU_SCRIPTW_DEBUG(" Source file %u:\n", i);

    size_t fp_enc_size = lbc_strlen(entry->filepath);

    /* [u16 src_filepath_size] */
    WRITE(esms_bs, fp_enc_size, 2, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "  Filepath size (src_filepath_size): %" PRIu16 " (0x%04" PRIX16 ").\n",
      fp_enc_size,
      fp_enc_size
    );

    /* [u<8*src_filepath_size> src_filepath] */
    WRITE_ARRAY(esms_bs, (uint8_t *) entry->filepath, fp_enc_size, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "  Filepath (src_filepath): '%" PRI_LBCS "'.\n",
      entry->filepath
    );

    /* [u16 crc_checked_bytes] */
    WRITE(esms_bs, entry->properties.crc_checked_bytes, 2, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "  Checksum checked bytes (crc_checked_bytes): "
      "%" PRIu16 " (0x%04" PRIX16 ").\n",
      entry->properties.crc_checked_bytes,
      entry->properties.crc_checked_bytes
    );

    /* [u32 crc] */
    WRITE(esms_bs, entry->properties.crc, 4, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "  Checksum (crc): 0x%08" PRIX32 ".\n",
      entry->properties.crc,
      entry->properties.crc
    );
  }

  return 0;
}


static int _writeH264FmtSpecificInfos(
  BitstreamWriterPtr esms_bs,
  const LibbluESH264SpecProperties * prop
)
{

  LIBBLU_SCRIPTW_DEBUG(" H.264 Format specific informations:\n");

  /* [v8 constraint_flags] */
  WRITE(esms_bs, prop->constraint_flags, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  Constraint flags (constraint_flags): 0x%02" PRIX8 ".\n",
    prop->constraint_flags
  );

  /* [u32 CpbSize] */
  WRITE(esms_bs, prop->CpbSize, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  Coded Picture Buffer size (CpbSize): %" PRIu32 " (0x%08" PRIX32 ").\n",
    prop->CpbSize,
    prop->CpbSize
  );

  /* [u32 BitRate] */
  WRITE(esms_bs, prop->BitRate, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  Bit-Rate (BitRate): %" PRIu32 " (0x%08" PRIX32 ").\n",
    prop->BitRate,
    prop->BitRate
  );

  return 0;
}


static int _writeEsmsVideoSpecParam(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  const LibbluESProperties * prop = &esms_hdl->prop;

  /* [v64 CSPMVIDO_magic] */
  WRITE(esms_bs, ES_CODEC_SPEC_PARAM_HEADER_VIDEO_MAGIC, 8, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Video format specific properties magic (CSPMVIDO_magic): "
    STR(ES_CODEC_SPEC_PARAM_HEADER_VIDEO_MAGIC) ".\n"
  );

  /* [u4 video_format] [u4 frame_rate] */
  WRITE(esms_bs, (prop->video_format << 4) | prop->frame_rate, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Video format (video_format): %s (0x%" PRIX8 ").\n",
    HdmvVideoFormatStr(prop->video_format),
    prop->video_format
  );
  LIBBLU_SCRIPTW_DEBUG(
    " Frame-rate (frame_rate): %s (0x%" PRIX8 ").\n",
    HdmvFrameRateCodeStr(prop->frame_rate),
    prop->frame_rate
  );

  /* [u8 profile_IDC] */
  WRITE(esms_bs, prop->profile_IDC, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Profile IDC (profile_IDC): %" PRId8 " (0x%02" PRIX8 ").\n",
    prop->profile_IDC,
    prop->profile_IDC
  );

  /* [u8 level_IDC] */
  WRITE(esms_bs, prop->level_IDC, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Level IDC (level_IDC): %" PRId8 " (0x%02" PRIX8 ").\n",
    prop->level_IDC,
    prop->level_IDC
  );

  /* [b1 still_picture] [v7 reserved] */
  WRITE(esms_bs, prop->still_picture << 7, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Still picture (still_picture): %s (0b%x).\n",
    prop->still_picture,
    prop->still_picture
  );

  if (esms_hdl->prop.coding_type == STREAM_CODING_TYPE_AVC) {
    /* [v72 h264_fmt_spec_infos()] */
    if (_writeH264FmtSpecificInfos(esms_bs, esms_hdl->fmt_prop.h264) < 0)
      return -1;
  }

  return 0;
}


static int _writeAc3FmtSpecificInfos(
  BitstreamWriterPtr esms_bs,
  const LibbluESAc3SpecProperties * param
)
{

  LIBBLU_SCRIPTW_DEBUG(" AC-3 Format specific informations:\n");

  /* [u3 sample_rate_code] [u5 bsid] */
  WRITE(esms_bs, (param->sample_rate_code << 5) | param->bsid, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  Audio sample rate code (sample_rate_code): 0x%" PRIX8 ".\n",
    param->sample_rate_code
  );

  LIBBLU_SCRIPTW_DEBUG(
    "  Bit Stream Identification (bsid): 0x%" PRIX8 ".\n",
    param->bsid
  );

  /* [u6 bit_rate_code] [u2 surround_mode] */
  WRITE(esms_bs, (param->bit_rate_code << 2) | param->surround_mode, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  Bit-rate code (bit_rate_code): 0x%" PRIX8 ".\n",
    param->bit_rate_code
  );

  LIBBLU_SCRIPTW_DEBUG(
    "  Surround mode (surround_mode): 0x%" PRIX8 ".\n",
    param->surround_mode
  );

  /* [u3 bsmod] [u4 num_channels] [b1 full_svc] */
  WRITE(esms_bs, (param->bsmod << 5) | (param->num_channels << 1) | param->full_svc, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  Bit Stream mode (bsmod): 0x%" PRIX8 ".\n",
    param->bsmod
  );

  LIBBLU_SCRIPTW_DEBUG(
    "  Number of audio channels (num_channels): 0x%" PRIX8 ".\n",
    param->num_channels
  );

  LIBBLU_SCRIPTW_DEBUG(
    "  Full service (full_svc): 0b%x\n",
    param->full_svc
  );

  return 0;
}


static int _writeEsmsAudioSpecParam(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  const LibbluESProperties * prop = &esms_hdl->prop;

  /* [v64 CSPMAUDO_magic] */
  WRITE(esms_bs, ES_CODEC_SPEC_PARAM_HEADER_AUDIO_MAGIC, 8, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Audio format specific properties magic (CSPMAUDO_magic): "
    STR(ES_CODEC_SPEC_PARAM_HEADER_AUDIO_MAGIC) ".\n"
  );

  /* [u4 audio_format] [u4 sample_rate] */
  WRITE(esms_bs, (prop->audio_format << 4) | prop->sample_rate, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Audio format (audio_format): %s (0x%" PRIX8 ").\n",
    AudioFormatCodeStr(prop->audio_format),
    prop->audio_format
  );
  LIBBLU_SCRIPTW_DEBUG(
    " Sample-rate (sample_rate): %u Hz (0x%" PRIX8 ").\n",
    valueSampleRateCode(prop->sample_rate),
    prop->sample_rate
  );

  /* [u8 bit_depth] */
  WRITE(esms_bs, prop->bit_depth, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Bit-depth (bit_depth): %u bits (0x%" PRIX8 ").\n",
    valueBitDepthCode(prop->bit_depth),
    prop->bit_depth
  );

  /* [v8 reserved] // 0x00 */
  WRITE(esms_bs, 0x00, 1, return -1);

  if (
    esms_hdl->prop.coding_type == STREAM_CODING_TYPE_AC3
    || esms_hdl->prop.coding_type == STREAM_CODING_TYPE_TRUEHD
    || esms_hdl->prop.coding_type == STREAM_CODING_TYPE_EAC3
    || esms_hdl->prop.coding_type == STREAM_CODING_TYPE_EAC3_SEC
  ) {
    /* [v24 ac3_fmt_spec_infos()] */
    if (_writeAc3FmtSpecificInfos(esms_bs, esms_hdl->fmt_prop.ac3) < 0)
      return -1;
  }

  return 0;
}

/** \~english
 * \brief Write on output file ESMS ES Codec specific parameters section.
 *
 * \param esmsFile Output bitstream.
 * \param esms_hdl Source ESMS esms_hdl handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _writeEsmsEsCodecSpecParametersSection(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  const LibbluESProperties * prop = &esms_hdl->prop;

  assert(NULL != esms_bs);
  assert(NULL != esms_hdl);

  LIBBLU_SCRIPTW_DEBUG(
    "Format specific Properties section:\n"
  );

  switch (prop->coding_type) {
  case STREAM_CODING_TYPE_H262:
  case STREAM_CODING_TYPE_AVC:
    return _writeEsmsVideoSpecParam(esms_bs, esms_hdl);

  case STREAM_CODING_TYPE_LPCM:
  case STREAM_CODING_TYPE_AC3:
  case STREAM_CODING_TYPE_DTS:
  case STREAM_CODING_TYPE_TRUEHD:
  case STREAM_CODING_TYPE_EAC3:
  case STREAM_CODING_TYPE_HDHR:
  case STREAM_CODING_TYPE_HDMA:
  case STREAM_CODING_TYPE_EAC3_SEC:
  case STREAM_CODING_TYPE_DTSE_SEC:
    return _writeEsmsAudioSpecParam(esms_bs, esms_hdl);

  case STREAM_CODING_TYPE_IG:
  case STREAM_CODING_TYPE_PG:
    break; // None

  default:
    LIBBLU_ERROR_RETURN(
      "Unknown codec specific informations for stream coding type 0x%x.\n",
      prop->coding_type
    );
  }

  return 0;
}


/** \~english
 * \brief Write on output file ESMS Data blocks definition section.
 *
 * \param esmsFile Output bitstream.
 * \param esms_hdl Source ESMS esms_hdl handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _writeEsmsDataBlocksDefSection(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{
  const EsmsDataBlocks * db = &esms_hdl->data_blocks;

  LIBBLU_SCRIPTW_DEBUG(
    "Data Blocks Definition section:\n"
  );

  /* [v32 DTBK_magic] */
  WRITE(esms_bs, DATA_BLOCKS_DEF_HEADER_MAGIC, 4, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Data Blocks Definition section magic (DTBK_magic): "
    STR(DATA_BLOCKS_DEF_HEADER_MAGIC) ".\n"
  );

  /* [u8 nb_data_blocks] */
  if (writeByte(esms_bs, db->nb_data_blocks) < 0)
    return -1;

  LIBBLU_SCRIPTW_DEBUG(
    " Number of data blocks (nb_data_blocks): %" PRIu8 " (0x%02" PRIX8 ").\n",
    db->nb_data_blocks,
    db->nb_data_blocks
  );

  for (unsigned i = 0; i < db->nb_data_blocks; i++) {
    const EsmsDataBlockEntry * entry = &db->entries[i];
    LIBBLU_SCRIPTW_DEBUG(" Data block definition %u:\n", i);

    /* [u32 data_block_size[i]] */
    WRITE(esms_bs, entry->data_block_size, 4, return -1);
    LIBBLU_SCRIPTW_DEBUG(
      "  Size (data_block_size): %" PRIu32 " (0x%08" PRIX32 ").\n",
      entry->data_block_size,
      entry->data_block_size
    );

    /* [v<8*data_block_size[i]> data_block[i]] */
    WRITE_ARRAY(esms_bs, entry->data_block, entry->data_block_size, return -1);
  }

  return 0;
}

int completePesCuttingScriptEsmsHandler(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr esms_hdl
)
{

  /* Complete ESMS PES Cutting section. */
  if (0 == esms_hdl->commands_pipeline.nb_completed_frames) {
    /* Init section if never initialized (empty script) */
    if (_writeEsmsPesCuttingHeader(esms_bs, esms_hdl) < 0)
      return -1;
  }

  /* [v8 end_marker] // ESMS PES Cutting section loop end marker. */
  WRITE(esms_bs, ESMS_SCRIPT_END_MARKER, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    "  End of script (end_marker): " STR(ESMS_SCRIPT_END_MARKER) ".\n"
  );

  /* ES Properties */
  APPEND_DIR(esms_hdl, esms_bs, ESMS_DIRECTORY_ID_ES_PROP, return -1);

  if (_writeEsmsEsPropertiesSection(esms_bs, esms_hdl) < 0)
    return -1;

  if (0 < esms_hdl->data_blocks.nb_data_blocks) {
    /* Data blocks definition */
    APPEND_DIR(esms_hdl, esms_bs, ESMS_DIRECTORY_ID_ES_DATA_BLK_DEF, return -1);

    if (_writeEsmsDataBlocksDefSection(esms_bs, esms_hdl) < 0)
      return -1;
  }

  /* Codec specific parameters */
  APPEND_DIR(esms_hdl, esms_bs, ESMS_DIRECTORY_ID_ES_FMT_PROP, return -1);

  if (_writeEsmsEsCodecSpecParametersSection(esms_bs, esms_hdl) < 0)
    return -1;

  return 0;
}


/* ### Post-parsing : ###################################################### */


static int _writeESMSDirectories(
  FILE * esms_fd,
  const EsmsFileDirectories * dirs
)
{

  /* [u8 nb_directory] */
  FWRITE(esms_fd, dirs->nb_used_entries, 1, return -1);

  LIBBLU_SCRIPTW_DEBUG(
    " Number of directories/sections in file (nb_directory): "
    "%" PRIu8 " (0x%02" PRIX8 ").\n",
    dirs->nb_used_entries,
    dirs->nb_used_entries
  );

  for (uint8_t dir_idx = 0; dir_idx < dirs->nb_used_entries; dir_idx++) {
    const EsmsFileDirectory * dir = &dirs->entries[dir_idx];
    LIBBLU_SCRIPTW_DEBUG(" Directory %u:\n", dir_idx);

    /* [u8 dir_ID] */
    FWRITE(esms_fd, dir->dir_ID, 1, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "  Section ID (dir_ID): %s (0x%02" PRIX8 ").\n",
      ESMSDirectoryIdStr(dir->dir_ID),
      dir->dir_ID
    );

    /* [u64 dir_offset] */
    FWRITE(esms_fd, dir->dir_offset, 8, return -1);

    LIBBLU_SCRIPTW_DEBUG(
      "  Section offset (dir_offset): 0x%016" PRIX64 ".\n",
      dir->dir_offset
    );
  }

  return 0;
}


int updateEsmsFile(
  const lbc * esms_filepath,
  const EsmsHandlerPtr esms_hdl
)
{

  LIBBLU_SCRIPTWO_DEBUG("Updating ESMS file with final parameters.\n");

  /* Open ESMS file in update mode (overwrite previous content) */
  LIBBLU_SCRIPTWO_DEBUG(" Opening ESMS file in update mode.\n");
  FILE * esms_fd = lbc_fopen(esms_filepath, "rb+");
  if (NULL == esms_fd)
    LIBBLU_ERROR_RETURN(
      "Unable to update ESMS Header: File opening error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  LIBBLU_SCRIPTWO_DEBUG(" Patching the ESMS header.\n");
  LIBBLU_SCRIPTW_DEBUG(
    "Elementary Stream Manipulation Script (ESMS) header:\n"
  );

  /* [v32 ESMS_magic] */
  /* [u16 ESMS_version] */
  if (fseek(esms_fd, 0x6, SEEK_SET) < 0) {
    LIBBLU_ERROR_RETURN(
      "Unable to update ESMS Header: File seeking error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  }

  LIBBLU_SCRIPTW_DEBUG(
    " ESMS File header magic: *skipped*.\n"
  );
  LIBBLU_SCRIPTW_DEBUG(
    " Format version: *skipped*.\n"
  );

  LIBBLU_SCRIPTW_DEBUG(
    " Flags byte (flags_byte): 0x%" PRIX8 ".\n",
    0x01
  );

  /* [v8 flags_byte] // 0x01: Completed file */
  FWRITE(esms_fd, 0x01, 1, goto write_error);

  LIBBLU_SCRIPTW_DEBUG(
    " Flags byte (flags_byte): 0x%" PRIX8 ".\n",
    0x01
  );

  if (_writeESMSDirectories(esms_fd, &esms_hdl->directories) < 0)
    goto write_error;

  fclose(esms_fd);
  return 0;

write_error:
  fclose(esms_fd);
  LIBBLU_ERROR_RETURN(
    "Unable to update ESMS: Error on write, %s (errno: %d).\n",
    strerror(errno),
    errno
  );
}