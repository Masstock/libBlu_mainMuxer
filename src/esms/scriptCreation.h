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

/** \~english
 * \brief Default number of bytes used from source file to compute ESMS
 * script CRC.
 *
 * This value is used by #appendSourceFileEsms() function.
 */
#define CRC32_USED_BYTES  512

/** \~english
 * \brief ESMS PES frame header structure.
 */
typedef struct {
  uint8_t pictureType;  /**< Type of the picture.                            */
  bool extensionFrame;  /**< Is an extension frame.                          */
  bool dtsPresent;      /**< DTS timing value present.                       */
  bool extParamPresent; /**< Codec specific extension parameters present.    */

  uint64_t pts;         /**< Presentation Time Stamp in 27MHz clock ticks.   */
  uint64_t dts;         /**< Decoding Time Stamp in 27MHz clock ticks.       */
  uint32_t length;      /**< Frame length in bytes.                          */

  EsmsCommand commands[MAX_SUPPORTED_COMMANDS];  /**< PES frame
    building scripting commands.                                             */
  unsigned nbCommands;  /**< Number of used commands.                        */

  EsmsPesPacketExtData extParam;
} EsmsPesPacketHeader;

/** \~english
 * \brief ESMS PES frames building pipeline structure.
 *
 * Stores current in-build PES frame during ESMS script creation.
 */
typedef struct {
  size_t nbFrames;  /**< Number of PES frames composing the ES.              */

  bool initFrame;   /**< Is current builded frame has already been
    initialized.                                                             */
  EsmsPesPacketHeader curFrame;  /**< Current builded frame parameters.       */
} EsmsFileScriptCommandsPipeline;

/** \~english
 * \typedef EsmsFileHeaderPtr
 * \brief #EsmsFileHeader pointer type.
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
  LibbluESType streamType;          /**< Generated stream
    type.                                                                    */
  LibbluESProperties prop;          /**< ES properties.        */
  LibbluESFmtSpecProp fmtSpecProp;  /**<
    ES format specific properties.                                           */

  uint64_t ptsRef;          /**< Referential 'zero' time PTS value offset.   */
  uint32_t bitrate;         /**< ES nominal bitrate.                         */
  uint64_t endPts;          /**< Last PTS value, only used to feedback
    progression.                                                             */

  uint64_t flags;           /**< Script generation flags field.              */

  EsmsFileDirectories directories;                  /**< ESMS script
    directories indexer.                                                     */
  EsmsESSourceFiles sourceFiles;      /**< ESMS script linked
    ES source files indexer.                                                 */
  EsmsFileScriptCommandsPipeline commandsPipeline;  /**< ESMS script PES
    frames generation commands pipeline.                                     */
  EsmsDataBlocks dataBlocks;                /**< ESMS pre-defined
    data blocks indexer.                                                     */
} EsmsFileHeader, *EsmsFileHeaderPtr;

/** \~english
 * \brief Create a ESMS script #EsmsFileHeader and intialize it according
 * to supplied parameters.
 *
 * \param type Type of the script referenced elementary stream.
 * \param flags ESMS flags field.
 * \param fmtSpecPropType Type of the format specific informations section
 * to be reserved (may be set to #FMT_SPEC_INFOS_NONE to disable).
 * \return EsmsFileHeaderPtr On success, created object is returned. Otherwise
 * a NULL pointer is returned.
 *
 * Created object must be passed to #destroyEsmsFileHandler() function
 * after-use.
 */
EsmsFileHeaderPtr createEsmsFileHandler(
  LibbluESType type,
  LibbluESSettingsOptions options,
  LibbluESFmtSpecPropType fmtSpecPropType
);

/** \~english
 * \brief Destroy memory allocation used by a #EsmsFileHeader object.
 *
 * \param handler Object to free.
 *
 * Supplied object must has been created using #createEsmsFileHandler()
 * function.
 */
void destroyEsmsFileHandler(
  EsmsFileHeaderPtr handler
);

/* Stream parameters configuration : */

/** \~english
 * \brief Define given ESMS script referenced video stream as composed of a
 * still picture.
 *
 * \param handler ESMS script handle.
 */
static inline void setStillPicture(
  EsmsFileHeaderPtr handler
)
{
  assert(NULL != handler);
  handler->prop.stillPicture = true;
}

/** \~english
 * \brief Update and mark as complete given ESMS script file.
 *
 * \param essFileName ESMS script filename.
 * \param script ESMS script parameters.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int updateEsmsHeader(
  const lbc * essFileName,
  const EsmsFileHeaderPtr script
);

/** \~english
 * \brief Add given directory parameters to ESMS script indexer.
 *
 * \param script Destination ESMS script handle.
 * \param dirId Id of the directory.
 * \param dirOffset Offset of the directory in ESMS script output file.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendDirEsms(
  EsmsFileHeaderPtr script,
  const uint8_t dirId,
  const uint64_t dirOffset
);

/** \~english
 * \brief Add given source file to ESMS script with pre-computed CRC value.
 *
 * \param script Destination ESMS script handle.
 * \param filename Source filename.
 * \param crcCheckedBytes Number of bytes used from file start in CRC
 * computation.
 * \param crc32 CRC value.
 * \param newFileIdx On success, created file index return pointer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * This function shall only be used if source file is already opened and
 * cannot be processed by #appendSourceFileEsms() function. No tests are
 * done on supplied values, potentialy leading to broken scripts.
 */
int appendSourceFileEsmsWithCrc(
  EsmsFileHeaderPtr script,
  const lbc * filename,
  const uint16_t crcCheckedBytes,
  const uint32_t crc32,
  unsigned * newFileIdx
);

/** \~english
 * \brief Add given source file to ESMS script.
 *
 * \param script Destination ESMS script handle.
 * \param filename Source filename.
 * \param newFileIdx On success, created file index return pointer
 * (can be NULL).
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendSourceFileEsms(
  EsmsFileHeaderPtr script,
  const lbc * filename,
  unsigned * newFileIdx
);

/** \~english
 * \brief Return true if maximum allowed number of data blocks in ESMS data
 * section as been reached.
 *
 * \param script Used ESMS script handle.
 * \return true The limit has been reached.
 * \return false Free data blocks remaining.
 */
static inline bool isDataBlocksNbLimitReachedEsms(
  const EsmsFileHeaderPtr script
)
{
  return nbUsedEntriesLimitReachedEsmsDataBlocks(script->dataBlocks);
}

/** \~english
 * \brief Add a data block to ESMS data section.
 *
 * \param script Destination ESMS script handle.
 * \param data Data block bytes array.
 * \param size Length of data block array.
 * \param idx On success, created data block index return pointer
 * (can be NULL).
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * This function shall only be called after ensuring
 * #isDataBlocksNbLimitReachedEsms() function return true.
 */
int appendDataBlockEsms(
  EsmsFileHeaderPtr script,
  const uint8_t * data,
  uint32_t size,
  unsigned * idx
);

/** \~english
 * \brief Update data block entry content from ESMS data section.
 *
 * \param script Destination ESMS script handle.
 * \param data Newer data block bytes array.
 * \param size Newer length of data block array.
 * \param idx Index of the data block to edit.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int updateDataBlockEsms(
  EsmsFileHeaderPtr script,
  const uint8_t * data,
  uint32_t size,
  unsigned idx
);

/** \~english
 * \brief Write on output file ESMS file header.
 *
 * \param esmsFile Output bitstream.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int writeEsmsHeader(
  BitstreamWriterPtr esmsFile
);

/** \~english
 * \brief Write on output file ESMS ES Properties section.
 *
 * \param esmsFile Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int writeEsmsEsPropertiesSection(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
);

/** \~english
 * \brief Write on output file ESMS PES Cutting section header.
 *
 * \param esmsFile Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Section is added on ESMS Directories indexer.
 */
int writeEsmsPesCuttingHeader(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
);

int writeEsmsPesPacketExtensionData(
  BitstreamWriterPtr esmsFile,
  EsmsPesPacketExtData extParam,
  LibbluStreamCodingType codec
);

/** \~english
 * \brief Write on output file a ESMS PES Cutting section frame from current
 * builded PES frame in pipeline.
 *
 * \param esmsFile Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Prior to this function call, a PES frame must be builded using:
 *  - A initialization function, such as #initEsmsVideoPesFrame() accoding
 *    to script destination stream type;
 *  - Eventually, modification script commands (such as
 *    #appendAddDataCommand()).
 */
int writeEsmsPesPacket(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
);

int writeH264FmtSpecificInfos(
  BitstreamWriterPtr esmsFile,
  LibbluESH264SpecProperties * param
);

int writeAc3FmtSpecificInfos(
  BitstreamWriterPtr esmsFile,
  LibbluESAc3SpecProperties * param
);

/** \~english
 * \brief Write on output file ESMS ES Codec specific parameters section.
 *
 * \param esmsFile Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int writeEsmsEsCodecSpecParametersSection(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
);

/** \~english
 * \brief Write on output file ESMS Data blocks definition section.
 *
 * \param esmsFile Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int writeEsmsDataBlocksDefSection(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
);

/** \~english
 * \brief Complete end of ESMS script file.
 *
 * \param esmsFile Output bitstream.
 * \param script Source ESMS script handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Adds ES properties, Data block definition and codec specific parameters
 * sections. Sections are added on ESMS Directories indexer.
 */
int addEsmsFileEnd(
  BitstreamWriterPtr esmsFile,
  EsmsFileHeaderPtr script
);

#if 0
/** \~english
 * \brief Initialize a new ESMS PES frame on the pipeline.
 *
 * \param script Used ESMS script handle.
 * \param pictType Type of the picture (if stream is of video type).
 * \param extFrame Is the frame is a audio extension frame.
 * \param dtsPres Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if dtsPres
 * value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initEsmsPesPacket(
  EsmsFileHeaderPtr script,
  uint8_t pictType,
  bool extFrame,
  bool dtsPres,
  uint64_t pts,
  uint64_t dts
);
#endif

/** \~english
 * \brief Initialize a new video ESMS PES frame on the pipeline.
 *
 * \param script Used ESMS script handle.
 * \param pictType Type of the picture (if stream is of video type).
 * \param dtsPres Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if dtsPres
 * value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initEsmsVideoPesFrame(
  EsmsFileHeaderPtr script,
  const uint8_t pictType,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
);

/** \~english
 * \brief Initialize a new audio ESMS PES frame on the pipeline.
 *
 * \param script Used ESMS script handle.
 * \param extFrame Is the frame a extension audio frame.
 * \param dtsPres Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if dtsPres
 * value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initEsmsAudioPesFrame(
  EsmsFileHeaderPtr script,
  bool extFrame,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
);

/** \~english
 * \brief Initialize a new HDMV ESMS PES frame on the pipeline.
 *
 * \param script Used ESMS script handle.
 * \param dtsPres Is the DTS field is used.
 * \param pts Presentation Time Stamp (in 27MHz clock ticks).
 * \param dts Decoding Time Stamp (in 27MHz clock ticks), only used if dtsPres
 * value is true.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initEsmsHdmvPesFrame(
  EsmsFileHeaderPtr script,
  bool dtsPres,
  const uint64_t pts,
  const uint64_t dts
);

/** \~english
 * \brief Return true is ESMS PES frame extension data is allowed for the
 * used codec.
 *
 * Extension data is stored in #EsmsPesPacketExtData structure.
 *
 * \param script Used ESMS script handle.
 * \return true Codec can handle extension data.
 * \return false No extension data available for used codec.
 */
bool isEsmsPesPacketExtensionDataSupported(
  EsmsFileHeaderPtr script
);

/** \~english
 * \brief Add codec specific extension data to the pending ESMS PES frame in
 * the pipeline.
 *
 * \param script Used ESMS script handle.
 * \param data Attached extension data.
 */
int setEsmsPesPacketExtensionData(
  EsmsFileHeaderPtr script,
  EsmsPesPacketExtData data
);

/** \~english
 * \brief Adds a "Add data" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param offset Data block insertion offset in PES frame.
 * \param mode Insertion mode of data block.
 * \param dataLength Data block array length.
 * \param data Data block array.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendAddDataCommand(
  EsmsFileHeaderPtr script,
  const uint32_t offset,
  const EsmsDataInsertionMode mode,
  const uint16_t dataLength,
  const uint8_t * data
);

/** \~english
 * \brief Adds a "Change byte order" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param valueLength Length in bytes of a value field to be swapped.
 * \param swappingDomainStartOffset Swapping area starting offset.
 * \param swappingDomainLength Length in bytes of the swapping area. This
 * value shall be a multiple of valueLength.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * \todo Change swappingDomainLength units from bytes to valueLength units ?
 */
int appendChangeByteOrderCommand(
  EsmsFileHeaderPtr script,
  const uint8_t valueLength,
  const uint32_t swappingDomainStartOffset,
  const uint32_t swappingDomainLength
);

/** \~english
 * \brief Adds a "Add PES payload" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param fileIdx Data source file index.
 * \param offset Data block insertion offset in PES frame.
 * \param fileOffset Data block offset in source file.
 * \param payloadLength Length of data block in bytes.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Data block (of length payloadLength bytes) is copied from source file
 * (refered by fileIdx, from fileOffset) to PES frame (at offset).
 */
int appendAddPesPayloadCommand(
  EsmsFileHeaderPtr script,
  unsigned fileIdx,
  const uint32_t offset,
  const uint64_t fileOffset,
  const uint32_t payloadLength
);

/** \~english
 * \brief Adds a "Add padding data" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param offset Padding insertion offset in PES frame.
 * \param mode Insertion mode of padding.
 * \param length Length of padding in bytes.
 * \param paddingByte Padding used byte (commonly 0x00 or 0xFF).
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendPaddingDataCommand(
  EsmsFileHeaderPtr script,
  const uint32_t offset,
  const EsmsDataInsertionMode mode,
  const uint16_t length,
  const uint8_t paddingByte
);

/** \~english
 * \brief Adds a "Add data section" ESMS PES script command.
 *
 * \param script Destination ESMS script handle.
 * \param offset Data block insertion offset in PES frame.
 * \param mode Insertion mode of data block.
 * \param dataSectBlockIdx Index of the data block to insert from data blocks
 * section.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int appendAddDataBlockCommand(
  EsmsFileHeaderPtr script,
  const uint32_t offset,
  const EsmsDataInsertionMode mode,
  const uint8_t dataSectBlockIdx
);

/** \~english
 * \brief Compute the final PES frame length in bytes after use of frame
 * associated script commands.
 *
 * \param commands PES frame modification script commands.
 * \param nbCommands Number of commands.
 * \param dataSects ESMS data blocks section.
 * \return uint32_t Length of the PES frame in bytes.
 */
uint32_t computePesFrameLength(
  const EsmsCommand * commands,
  int nbCommands,
  EsmsDataBlocks dataBlocks
);

#endif