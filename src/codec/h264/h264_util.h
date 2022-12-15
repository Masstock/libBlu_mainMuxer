/** \~english
 * \file h264_util.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams specific utilities module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__UTIL_H__
#define __LIBBLU_MUXER__CODECS__H264__UTIL_H__

#include "../../util.h"
#include "../../elementaryStreamOptions.h"
#include "../../esms/scriptCreation.h"
#include "h264_hrdVerifier.h"
#include "h264_error.h"
#include "h264_data.h"

/* ### H.264 Context : ##################################################### */

typedef struct {
  bool presenceOfFillerData;

  bool vuiInvalidOverscan;
  bool vuiUnexpectedVideoFormat;
  bool vuiUnexpectedVideoFullRange;
  bool vuiUnexpectedColourPrimaries;
  bool vuiUnexpectedTranferCharact;
  bool vuiUnexpectedMatrixCoeff;
  bool vuiMissingVideoSignalType;
  bool vuiMissingColourDesc;

  bool unexpectedPresenceOfBufPeriodSei;

  bool useOfLowEfficientCavlc;
  bool useOfLowEfficientConstaintIntraPred;

  bool tooMuchMemoryManagementCtrlOp;  /**< The number of present
    'memory_management_control_operation' exceeds the maximum supported.     */
} H264WarningFlags;

typedef struct {
  EsmsFileHeaderPtr esms;
  BitstreamWriterPtr esmsFile;

  H264NalDeserializerContext file;
  unsigned esmsSrcFileIdx;

  H264AccessUnitDelimiterParameters accessUnitDelimiter;
  bool accessUnitDelimiterPresent;
  bool accessUnitDelimiterValid;

  H264SequenceParametersSetParameters sequenceParametersSet;
  bool sequenceParametersSetPresent;
  bool sequenceParametersSetGopValid;
  bool sequenceParametersSetValid;

  H264PicParametersSetParameters * picParametersSet[H264_MAX_PPS];
  bool picParametersSetIdsPresent[H264_MAX_PPS];
  bool picParametersSetIdsValid[H264_MAX_PPS];
  unsigned picParametersSetIdsPresentNb;

  H264SupplementalEnhancementInformationParameters sei;

  H264SliceLayerWithoutPartitioningParameters slice;
  bool slicePresent;

  H264ConstraintsParam constraints;

  H264CurrentProgressParam curProgParam;

  H264ModifiedNalUnitsList modNalLst;

  H264HrdVerifierContextPtr hrdVerifier;
  bool enoughDataToUseHrdVerifier;

  /* H264CabacParsingContext cabac; */
  H264WarningFlags warningFlags;
} H264ParametersHandler, *H264ParametersHandlerPtr;

#define CHECK_H264_WARNING_FLAG(H264ParametersHandlerPtr, warningName)        \
  (!(H264ParametersHandlerPtr)->warningFlags.warningName)

#define MARK_H264_WARNING_FLAG(H264ParametersHandlerPtr, warningName)         \
  ((H264ParametersHandlerPtr)->warningFlags.warningName = true)

static inline bool inUseHrdVerifierH264ParametersHandler(
  const H264ParametersHandlerPtr handle
)
{
  return (NULL != handle->hrdVerifier);
}

void updateH264LevelLimits(
  H264ParametersHandlerPtr handle,
  uint8_t level_idc
);

void updateH264ProfileLimits(
  H264ParametersHandlerPtr handle,
  H264ProfileIdcValue profile_idc,
  H264ContraintFlags constraintsFlags,
  bool applyBdavConstraints
);

unsigned getH264BrNal(
  H264ConstraintsParam constraints,
  H264ProfileIdcValue profile_idc
);

/* Handling functions : */
H264ParametersHandlerPtr initH264ParametersHandler(
  const LibbluESParsingSettings * settings
);

void resetH264ParametersHandler(
  H264ParametersHandlerPtr handle
);

int completeH264ParametersHandler(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings * settings
);

void destroyH264ParametersHandler(
  H264ParametersHandlerPtr handle
);

int updatePPSH264Parameters(
  H264ParametersHandlerPtr handle,
  const H264PicParametersSetParameters * param,
  unsigned id
);

int initIfRequiredH264CpbHrdVerifier(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions * options
);

int deserializeRbspCell(
  H264ParametersHandlerPtr handle
);

typedef struct {
  uint8_t * array;
  size_t arraySize;

  uint8_t * writingPointer;
  uint8_t * endPointer;

  uint8_t curByte;
  bool rbspZone;

  unsigned curNbBits;
  unsigned nbZeroBytes;
} H264NalByteArrayHandler, *H264NalByteArrayHandlerPtr;

#define H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(h264NalByteArrayHandlerPtr)         \
  (                                                                           \
    (size_t) (                                                                \
      (h264NalByteArrayHandlerPtr)->writingPointer                            \
      - (h264NalByteArrayHandlerPtr)->array                                   \
    )                                                                         \
  )

H264NalByteArrayHandlerPtr createH264NalByteArrayHandler(
  H264NalHeaderParameters headerParam
);
void destroyH264NalByteArrayHandler(
  H264NalByteArrayHandlerPtr baHandler
);

H264AUNalUnitPtr createNewNalCell(
  H264ParametersHandlerPtr handle,
  H264NalUnitTypeValue nal_unit_type
);

H264AUNalUnitPtr recoverCurNalCell(
  H264ParametersHandlerPtr handle
);

int replaceCurNalCell(
  H264ParametersHandlerPtr handle,
  void * newParam,
  size_t newParamSize
);

int discardCurNalCell(
  H264ParametersHandlerPtr handle
);

int addNalCellToAccessUnit(
  H264ParametersHandlerPtr handle
);

/** \~english
 * \brief Return the nal_ref_idc field value of the currently parsed NALU.
 *
 * \param handle H.264 parsing handle.
 * \return H264NalRefIdcValue nal_ref_idc value.
 */
static inline H264NalRefIdcValue getNalRefIdc(
  const H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  return handle->file.nal_ref_idc;
}

/** \~english
 * \brief Return the nal_unit_type field value of the currently parsed NALU.
 *
 * \param handle H.264 parsing handle.
 * \return H264NalUnitTypeValue nal_unit_type value.
 */
static inline H264NalUnitTypeValue getNalUnitType(
  const H264ParametersHandlerPtr handle
)
{
  /* Returns current nal type (or last parsed nal type, 0x0 if none) */
  assert(NULL != handle);

  return handle->file.nal_unit_type;
}

/* Reading functions : */
static inline int readBitNal(
  H264ParametersHandlerPtr handle,
  bool * bit
)
{
  assert(NULL != handle);

  if (!handle->file.remainingRbspCellBits) {
    if (deserializeRbspCell(handle) < 0)
      return -1;
  }

  if (NULL == bit)
    handle->file.remainingRbspCellBits--;
  else
    *bit = (
      handle->file.currentRbspCellBits
       >> (--handle->file.remainingRbspCellBits)
    ) & 0x1;

  return 0;
}

static inline int readBitsNal(
  H264ParametersHandlerPtr handle,
  uint32_t * value,
  size_t length
)
{
  bool bit;

  assert(NULL != handle);
  assert(NULL != value);
  assert(length <= 32);

  if (NULL != value)
    *value = 0;

  while (length--) {
    if (readBitNal(handle, &bit) < 0)
      return -1;

    if (NULL != value)
      *value = (*value << 1) | bit;
  }

  return 0;
}

static inline int readBytesNal(
  H264ParametersHandlerPtr handle,
  uint8_t * bytes,
  size_t length
)
{
  uint32_t byte;

  assert(NULL != handle);
  assert(NULL != bytes);
  assert(0 < length);

  if (NULL != bytes)
    *bytes = 0;

  while (length--) {
    if (readBitsNal(handle, &byte, 8) < 0)
      return -1;

    if (NULL != bytes)
      *(bytes++) = byte;
  }

  return 0;
}

/** \~english
 * \brief
 *
 * \param handle
 * \param value
 * \param maxLength Max size of decoded value field in bits. If equal to -1,
 * size is unrestricted (32 bits max, size of an uint32_t).
 * \return int
 *
 * From Rec.ITU-T H.264 (06/2019) - 9.1 Parsing process for Exp-Golomb codes.
 */
static inline int readExpGolombCodeNal(
  H264ParametersHandlerPtr handle,
  uint32_t * value,
  int maxLength
)
{
  /*  */
  /* maxLength can be -1 (unrestricted) */
  uint32_t endValue, maxValue;
  int nbLeadingZeroBits;
  bool bit;

  assert(NULL != handle);
  assert(NULL != value);

  nbLeadingZeroBits = -1;
  for (bit = false; !bit; nbLeadingZeroBits++) {
    if (readBitNal(handle, &bit) < 0)
      return -1;
  }

  if (nbLeadingZeroBits == -1 || 32 < nbLeadingZeroBits)
    LIBBLU_H264_ERROR_RETURN(
      "Corrupted/invalid Exp-Golomb code.\n"
    );

  if (nbLeadingZeroBits == 0) {
    *value = 0;
    return 0;
  }

  *value = (1 << nbLeadingZeroBits) - 1;

  if (readBitsNal(handle, &endValue, nbLeadingZeroBits) < 0)
    return -1;
  *value += endValue;

  if (0 < maxLength) {
    maxLength = MIN(32, maxLength);
    maxValue = (maxLength == 32) ? 0xFFFFFFFF : (unsigned) (1 << maxLength) - 1;

    if (maxValue < *value)
      LIBBLU_H264_ERROR_RETURN(
        "Invalid Exp-Golomb code, overflow occurs.\n"
      );
  }

  return 0;
}

/** \~english
 * \brief
 *
 * \param handle
 * \param value
 * \param maxLength Max size of decoded value field in bits. If equal to -1,
 * size is unrestricted (32 bits max, size of an uint32_t).
 * \return int
 *
 * From Rec.ITU-T H.264 (06/2019) - 9.1.1 Mapping process for signed
 * Exp-Golomb codes.
 */
static inline int readSignedExpGolombCodeNal(
  H264ParametersHandlerPtr handle,
  uint32_t * value,
  int maxLength
)
{
  uint32_t unsignedValue, convValue;

  if (readExpGolombCodeNal(handle, &unsignedValue, maxLength) < 0)
    return -1; /* Error during normal parsing. */

  /* Conversion : */
  convValue = (unsignedValue >> 1) + (unsignedValue & 0x1);
  if (unsignedValue & 0x1)
    *value = convValue;
  else
    *value = - convValue;

  return 0;
}

static inline int skipBitsNal(
  H264ParametersHandlerPtr handle,
  size_t length
)
{
  assert(NULL != handle);

  while (length--) {
    if (readBitNal(handle, NULL) < 0)
      return -1;
  }

  return 0;
}

static inline int reachNextNal(
  H264ParametersHandlerPtr handle
)
{

#if 0
  uint32_t value;
  size_t fieldLength;

  while (
    !isEof(handle->file.inputFile)
    && (value = nextUint32(handle->file.inputFile)) != 0x00000001
    && (value >> 8) != 0x000001
  ) {
    fieldLength = ((value >> 8) & 0x000003) ? 3 : 1;

    if (skipBytes(handle->file.inputFile, fieldLength) < 0) {
      if (isEof(handle->file.inputFile))
        break;
      return -1;
    }
  }

  handle->file.packetInitialized = false;
  return 0;
#else
  register uint32_t value;
  int ret;

  ret = 0;
  while (
    ret <= 0
    && 0 < (value = nextUint32(handle->file.inputFile))
  ) {
    if (0x01 == (value & 0xFFFFFF))
      break;
    if (0x00 == (value & 0xFF))
      ret = skipBytes(handle->file.inputFile, 1);
    else
      ret = skipBytes(handle->file.inputFile, 3);
  }

  handle->file.packetInitialized = false;

  if (0 < nextUint8(handle->file.inputFile))
    ret = skipBytes(handle->file.inputFile, 1);
  return ret;
#endif
}

/* Writing functions : */
int writeH264NalByteArrayBit(
  H264NalByteArrayHandlerPtr baHandler,
  bool bit
);
int writeH264NalByteArrayBits(
  H264NalByteArrayHandlerPtr baHandler,
  uint64_t value,
  size_t nbBits
);
int writeH264NalByteArrayByte(
  H264NalByteArrayHandlerPtr baHandler,
  uint8_t byte
);
int writeH264NalByteArrayExpGolombCode(
  H264NalByteArrayHandlerPtr baHandler,
  unsigned value
);
int writeH264NalByteArraySignedExpGolombCode(
  H264NalByteArrayHandlerPtr baHandler,
  int value
);
int writeH264NalHeader(
  H264NalByteArrayHandlerPtr baHandler,
  H264NalHeaderParameters handle
);

size_t calcH264ExpGolombCodeLength(
  unsigned value,
  bool isSigned
);

/* Status functions : */
bool isByteAlignedNal(
  H264ParametersHandlerPtr handle
);
bool moreRbspDataNal(
  H264ParametersHandlerPtr handle
);
bool moreRbspTrailingDataNal(
  H264ParametersHandlerPtr handle
);
bool noMoreNal(
  H264ParametersHandlerPtr handle
);
bool isInitNal(
  H264ParametersHandlerPtr handle
);

bool isByteAlignedH264NalByteArray(
  H264NalByteArrayHandlerPtr baHandler
);

#endif