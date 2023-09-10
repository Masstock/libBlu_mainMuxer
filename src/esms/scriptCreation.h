/** \~english
 * \file scriptCreation.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Elementary Stream Modification Script creation module.
 */

#ifndef __LIBBLU_MUXER__ESMS__SCRIPT_CREATION_H__
#define __LIBBLU_MUXER__ESMS__SCRIPT_CREATION_H__

#include "../elementaryStreamOptions.h"
#include "../elementaryStreamProperties.h"
#include "../streamCodingType.h"
#include "../util.h"
#include "scriptData.h"
#include "scriptDebug.h"

/* ### Pre-parsing : ####################################################### */

/** \~english
 * \brief Write on output file ESMS file header with place holders.
 *
 * \param esms_bs Output bitstream.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int writeEsmsHeader(
  BitstreamWriterPtr esms_bs
);

/* ### ESMS Handler : ###################################################### */

/** \~english
 * \brief Default number of bytes used from source file to compute ESMS
 * script CRC.
 *
 * This value is used by #appendSourceFileEsmsHandler() function.
 */
#define CRC32_USED_BYTES  512

/** \~english
 * \brief ESMS PES frames building pipeline structure.
 *
 * Stores current in-build PES frame during ESMS script creation.
 */
typedef struct {
  EsmsPesPacket cur_frame;  /**< Current builded frame parameters.      */

  bool initialized_frame;        /**< Is current builded frame has already
    been initialized.                                                        */
  uint64_t nb_completed_frames;  /**< Number of PES frames composing the ES. */
} EsmsCommandsPipeline;

/** \~english
 * \typedef EsmsHandlerPtr
 * \brief #EsmsHandler pointer type.
 */
/** \~english
 * \brief ESMS script file handling structure.
 *
 * A Elementary Stream Manipulation Script handle used to create and parse
 * scripts. Script files are composed of an header followed by sections
 * (called 'directories') describing generated ES properties,
 * ES source files and instructions to build from PES frames.
 */
typedef struct {
  LibbluESType type;         /**< Generated stream type.                     */
  LibbluESProperties prop;   /**< ES properties.                             */
  LibbluESFmtProp fmt_prop;  /**< ES format specific properties.             */

  uint64_t PTS_reference;   /**< Referential 'zero' time PTS value offset.   */
  uint32_t bitrate;         /**< ES nominal bitrate.                         */
  uint64_t PTS_final;       /**< Last PTS value, only used to feedback
    progression.                                                             */

  uint64_t flags;           /**< Script generation flags field.              */

  EsmsFileDirectories directories;                  /**< ESMS script
    directories indexer.                                                     */
  EsmsESSourceFiles source_files;      /**< ESMS script linked
    ES source files indexer.                                                 */
  EsmsCommandsPipeline commands_pipeline;  /**< ESMS script PES
    frames generation commands pipeline.                                     */
  EsmsDataBlocks data_blocks;                /**< ESMS pre-defined
    data blocks indexer.                                                     */
} EsmsHandler, *EsmsHandlerPtr;

/* ### ESMS Handler creation/destruction : ################################# */

/** \~english
 * \brief Create a ESMS script #EsmsHandler and intialize it according
 * to supplied parameters.
 *
 * \param type Type of the script referenced elementary stream.
 * \param options ES parsing options.
 * \param fmt_prop_type Type of the format specific informations section
 * to be reserved (may be set to #FMT_SPEC_INFOS_NONE to disable).
 * \return EsmsHandlerPtr On success, created object is returned. Otherwise
 * a NULL pointer is returned.
 *
 * Created object must be passed to #destroyEsmsHandler() function
 * after-use.
 */
EsmsHandlerPtr createEsmsHandler(
  LibbluESType type,
  LibbluESSettingsOptions options,
  LibbluESFmtSpecPropType fmt_prop_type
);

/** \~english
 * \brief Destroy memory allocation used by a #EsmsHandler object.
 *
 * \param handler Object to free.
 *
 * Supplied object must has been created using #createEsmsHandler()
 * function.
 */
static inline void destroyEsmsHandler(
  EsmsHandlerPtr handler
)
{
  if (NULL == handler)
    return;
  cleanEsmsESSourceFiles(handler->source_files);
  cleanEsmsDataBlocks(handler->data_blocks);
  free(handler->fmt_prop.shared_ptr);
  free(handler);
}

/* ### Stream parameters configuration : ################################### */

/** \~english
 * \brief Define given ESMS script referenced video stream as composed of a
 * still picture.
 *
 * \param handler ESMS script handle.
 */
static inline void setStillPictureEsmsHandler(
  EsmsHandlerPtr handler
)
{
  handler->prop.still_picture = true;
}

/** \~english
 * \brief Add given source file to ESMS script with pre-computed CRC value.
 *
 * \param esms_hdl Destination ESMS script handle.
 * \param filepath Source filename.
 * \param crc_checked_bytes Number of bytes used from file start in CRC
 * computation.
 * \param crc CRC value.
 * \param newFileIdx On success, created file index return pointer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * This function shall only be used if source file is already opened and
 * cannot be processed by #appendSourceFileEsmsHandler() function. No tests are
 * done on supplied values, potentialy leading to broken scripts.
 */
int appendSourceFileWithCrcEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const lbc * filepath,
  uint16_t crc_checked_bytes,
  uint32_t crc,
  uint8_t * idx
);

/** \~english
 * \brief Add given source file to ESMS script.
 *
 * \param esms_hdl Destination ESMS script handle.
 * \param filepath Source filename.
 * \param src_file_idx On success, created file index return pointer
 * (can be NULL).
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendSourceFileEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const lbc * filepath,
  uint8_t * src_file_idx
);

/** \~english
 * \brief Return true if maximum allowed number of data blocks in ESMS data
 * section as been reached.
 *
 * \param script Used ESMS script handle.
 * \return true The limit has been reached.
 * \return false Free data blocks remaining.
 */
static inline bool isDataBlocksNbLimitReachedEsmsHandler(
  const EsmsHandlerPtr esms_hdl
)
{
  return
    ESMS_MAX_SUPPORTED_DATA_BLOCKS_ENTRIES
    <= esms_hdl->data_blocks.nb_data_blocks
  ;
}

/** \~english
 * \brief Add a data block to ESMS data section.
 *
 * \param esms_hdl Destination ESMS script handle.
 * \param data_block Data block bytes array.
 * \param data_block_size Length of data block array.
 * \param data_block_id On success, created data block index return pointer
 * (can be NULL).
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * This function shall only be called after ensuring
 * #isDataBlocksNbLimitReachedEsmsHandler() function return true.
 */
int appendDataBlockEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const uint8_t * data_block,
  uint32_t data_block_size,
  unsigned * data_block_id
);

/** \~english
 * \brief Update data block entry content from ESMS data section.
 *
 * \param esms_hdl Destination ESMS script handle.
 * \param data_block Newer data block bytes array.
 * \param data_block_size Newer length of data block array.
 * \param data_block_idx Index of the data block to edit.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int updateDataBlockEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  const uint8_t * data_block,
  uint32_t data_block_size,
  unsigned data_block_idx
);

/* ### PES cutting script : ################################################ */

/** \~english
 * \brief Initialize a new video ESMS PES frame on the pipeline.
 *
 * \param esms_hdl Used ESMS script handle.
 * \param picture_type Type of the picture (if stream is of video type).
 * \param dts_present Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if
 * dts_present value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initVideoPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint8_t picture_type,
  bool dts_present,
  uint64_t pts,
  uint64_t dts
);

/** \~english
 * \brief Initialize a new audio ESMS PES frame on the pipeline.
 *
 * \param esms_hdl Used ESMS script handle.
 * \param extension_frame Is the frame a extension audio frame.
 * \param dts_present Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if
 * dts_present value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initAudioPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  bool extension_frame,
  bool dts_present,
  uint64_t pts,
  uint64_t dts
);

/** \~english
 * \brief Initialize a new HDMV ESMS PES frame on the pipeline.
 *
 * \param esms_hdl Used ESMS script handle.
 * \param dts_present Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if
 * dts_present value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initHDMVPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  bool dts_present,
  uint64_t pts,
  uint64_t dts
);

/** \~english
 * \brief Add codec specific extension data to the pending ESMS PES frame in
 * the pipeline.
 *
 * \param esms_hdl Used ESMS script handle.
 * \param data Attached extension data.
 */
int setExtensionDataPesPacketEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  EsmsPesPacketExtData data
);

/** \~english
 * \brief Adds a "Add data" ESMS PES script command.
 *
 * \param esms_hdl Destination ESMS script handle.
 * \param insert_offset Data block insertion offset in PES frame.
 * \param insert_mode Insertion mode of data block.
 * \param data Data block array (copied).
 * \param data_size Data block array length.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendAddDataCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  const uint8_t * data,
  uint16_t data_size
);

/** \~english
 * \brief Adds a "Change byte order" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param unit_size Length in bytes of a value field to be swapped.
 * \param section_offset Swapping area starting offset.
 * \param section_size Length in bytes of the swapping area. This
 * value shall be a multiple of unit_size.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * \todo Change section_size units from bytes to unit_size units ?
 */
int appendChangeByteOrderCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint8_t unit_size,
  uint32_t section_offset,
  uint32_t section_size
);

/** \~english
 * \brief Adds a "Add PES payload" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param src_file_id Data source file index.
 * \param insert_offset Data block insertion offset in PES frame.
 * \param src_offset Data block offset in source file.
 * \param size Length of data block in bytes.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Data block (of size bytes) is copied from source file (refered to by
 * src_file_id, at src_offset offset) to PES frame (at insert_offset).
 */
int appendAddPesPayloadCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint8_t src_file_id,
  uint32_t insert_offset,
  uint64_t src_offset,
  uint32_t size
);

/** \~english
 * \brief Adds a "Add padding data" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param insert_offset Padding insertion offset in PES frame.
 * \param insert_mode Insertion mode of padding.
 * \param size Length of padding in bytes.
 * \param filling_byte Padding used byte (commonly 0x00 or 0xFF).
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendPaddingDataCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  uint32_t size,
  uint8_t filling_byte
);

/** \~english
 * \brief Adds a "Add data section" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param insert_offset Data block insertion offset in PES frame.
 * \param insert_mode Insertion mode of data block.
 * \param data_block_id Index of the data block to insert from data blocks
 * section.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendAddDataBlockCommandEsmsHandler(
  EsmsHandlerPtr esms_hdl,
  uint32_t insert_offset,
  EsmsDataInsertionMode insert_mode,
  uint8_t data_block_id
);

/** \~english
 * \brief Write on output file a ESMS PES Cutting section frame from current
 * builded PES frame in pipeline.
 *
 * \param esms_bs Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Prior to this function call, a PES frame must be builded using:
 *  - A initialization function, such as #initVideoPesPacketEsmsHandler() accoding
 *    to script destination stream type;
 *  - Eventually, modification script commands (such as
 *    #appendAddDataCommandEsmsHandler()).
 */
int writePesPacketEsmsHandler(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr script
);

/** \~english
 * \brief Complete end of ESMS script file.
 *
 * \param esms_bs Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Adds ES properties, Data block definition and codec specific parameters
 * sections. Sections are added on ESMS Directories indexer.
 */
int completePesCuttingScriptEsmsHandler(
  BitstreamWriterPtr esms_bs,
  EsmsHandlerPtr script
);

/* ### Post-parsing : ###################################################### */

/** \~english
 * \brief Update and mark as complete given ESMS script file.
 *
 * \param esms_filepath ESMS script filename.
 * \param esms_hdl ESMS script parameters.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int updateEsmsFile(
  const lbc * esms_filepath,
  const EsmsHandlerPtr esms_hdl
);

#endif