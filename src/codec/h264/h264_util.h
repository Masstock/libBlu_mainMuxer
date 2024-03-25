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

#include "../../elementaryStreamOptions.h"
#include "../../esms/scriptCreation.h"
#include "../../util.h"
#include "../common/esParsingSettings.h"
#include "h264_error.h"
#include "h264_data.h"

/* ### H.264 Context : ##################################################### */

typedef struct {
  unsigned max_amount;

  unsigned char AU_unexpected_buffering_period_SEI_msg;
  unsigned char AU_filler_data;

  unsigned char AUD_wrong_primary_pic_type;
  unsigned char AUD_duplicated;

  unsigned char SPS_value_change;
  unsigned char SPS_duplicated;
  unsigned char SPS_patch_lower_level;

  unsigned char VUI_wrong_SAR;
  unsigned char VUI_wrong_video_format;
  unsigned char VUI_wrong_color_primaries;
  unsigned char VUI_wrong_transfer_characteristics;
  unsigned char VUI_wrong_matrix_coefficients;
  unsigned char VUI_wrong_overscan_appropriate_flag;
  unsigned char VUI_wrong_video_full_range_flag;
  unsigned char VUI_missing_video_signal_type;
  unsigned char VUI_missing_colour_description;

  unsigned char PPS_entropy_coding_mode_flag_change;

  unsigned char SEI_buf_period_initial_cpb_removal_delay_is_zero;
  unsigned char SEI_buf_period_initial_cpb_removal_delay_exceeds;

  unsigned char slice_wrong_slice_type;
  unsigned char slice_too_many_memory_management_control_operation; /**<
    The number of present 'memory_management_control_operation' exceeds the
    maximum supported.                                                       */
} H264WarningFlags;

typedef struct {
  EsmsHandlerPtr script;
  BitstreamWriterPtr script_file;

  H264NalDeserializerContext file;
  uint8_t script_src_file_idx;

  H264AccessUnitDelimiterParameters access_unit_delimiter;
  bool access_unit_delimiter_present;
  bool access_unit_delimiter_valid;

  H264SequenceParametersSetParameters sequence_parameter_set;
  bool sequence_parameter_set_present;
  bool sequence_parameter_set_GOP_valid;
  bool sequence_parameter_set_valid;

  H264PicParametersSetParameters *picture_parameter_set[H264_MAX_PPS];
  bool picture_parameter_set_present[H264_MAX_PPS];
  bool picture_parameter_set_GOP_valid[H264_MAX_PPS];
  bool picture_parameter_set_valid[H264_MAX_PPS];
  unsigned char nb_picture_parameter_set_present;
  unsigned char nb_picture_parameter_set_valid;

  H264SupplementalEnhancementInformationParameters sei;

  H264SliceLayerWithoutPartitioningParameters slice;
  bool slice_valid;

  H264ConstraintsParam constraints;
  H264BDConstraintsParam bd_constraints;
  bool bd_constraints_initialized;

  H264CurrentProgressParam cur_prog_param;

  H264ModifiedNalUnitsList modified_NALU_list;

  H264WarningFlags warning_flags;
} H264ParametersHandler, *H264ParametersHandlerPtr;

void updateH264LevelLimits(
  H264ParametersHandlerPtr handle,
  uint8_t level_idc
);

void updateH264ProfileLimits(
  H264ParametersHandlerPtr handle,
  H264ProfileIdcValue profile_idc,
  H264ContraintFlags constraints_flags
);

void updateH264BDConstraints(
  H264ParametersHandlerPtr handle,
  uint8_t level_idc,
  const H264VuiParameters *vui_parameters,
  bool max_BR_15_mbps
);

/* Handling functions : */
H264ParametersHandlerPtr initH264ParametersHandler(
  const LibbluESParsingSettings *settings
);

void resetH264ParametersHandler(
  H264ParametersHandlerPtr handle,
  bool is_IDR_access_unit
);

int completeH264ParametersHandler(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings *settings
);

void destroyH264ParametersHandler(
  H264ParametersHandlerPtr handle
);

int updatePPSH264Parameters(
  H264ParametersHandlerPtr handle,
  const H264PicParametersSetParameters *param,
  unsigned id
);

int deserializeRbspCell(
  H264ParametersHandlerPtr handle
);

typedef struct {
  uint8_t *array;           /**< Builded NALU destination byte array.       */
  uint8_t *writingPointer;  /**< Byte array writting offset pointer.        */
  uint8_t *endPointer;      /**< Pointer to the byte following the end of
    the allocated byte array.                                                */
  uint16_t allocatedSize;    /**< Byte array allocated size.                 */

  bool rbspZone;             /**< Set to true to append following bits as
    'rbsp_byte' fields, requiring insertion of 'emulation_prevention_
    three_byte' to prevent forbidden byte sequences.                         */

  uint8_t curByte;           /**< Pending byte, used as a buffer for bit-
    level writtings.                                                         */
  unsigned curNbBits;        /**< Current amount of bits in curByte buffer.  */

  unsigned nbZeroBytes;      /**< Current number of consecutive zero bytes.  */
} H264NalByteArrayHandler, *H264NalByteArrayHandlerPtr;

static inline uint16_t nbBytesH264NalByteArrayHandler(
  const H264NalByteArrayHandlerPtr baHandler
)
{
  return (baHandler->writingPointer - baHandler->array);
}

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

int replaceCurNalCell(
  H264ParametersHandlerPtr handle,
  H264AUNalUnitReplacementData newParam
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
  assert(NULL != handle);
  return handle->file.nal_unit_type;
}

static inline H264NalRefIdcValue getNalUnitRefIdc(
  const H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);
  return handle->file.nal_ref_idc;
}

/* Reading functions : */
static inline int readBitNal(
  H264ParametersHandlerPtr handle,
  bool *bit_ret
)
{
  assert(NULL != handle);

  if (!handle->file.remainingRbspCellBits) {
    if (deserializeRbspCell(handle) < 0)
      return -1;
  }

  if (NULL == bit_ret)
    handle->file.remainingRbspCellBits--;
  else
    *bit_ret = (
      handle->file.currentRbspCellBits
       >> (--handle->file.remainingRbspCellBits)
    ) & 0x1;

  return 0;
}

static inline int readBitsNal(
  H264ParametersHandlerPtr handle,
  uint32_t *value_ret,
  size_t length
)
{
  assert(NULL != handle);
  assert(length <= 32);

  uint32_t value = 0;
  while (length--) {
    bool bit;
    if (readBitNal(handle, &bit) < 0)
      return -1;
    value = (value << 1) | bit;
  }

  if (NULL != value_ret)
    *value_ret = value;

  return 0;
}

static inline int readBytesNal(
  H264ParametersHandlerPtr handle,
  uint8_t *bytes,
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
 * \param handle H.264 parsing context handle.
 * \param value_ret Parsed value return pointer.
 * \param max_bit_size Max size of decoded value field in bits. If equal to -1,
 * size is unrestricted and might overflow maximum range.
 * \return int
 *
 * From Rec.ITU-T H.264 (06/2019) - 9.1 Parsing process for Exp-Golomb codes.
 */
static inline int readExpGolombCodeNal(
  H264ParametersHandlerPtr handle,
  uint32_t *value_ret,
  int max_bit_size
)
{
  assert(NULL != handle);
  assert(NULL != value_ret);
  assert(-1 == max_bit_size || (0 < max_bit_size && max_bit_size <= 32));

  // Parse prefix
  int nb_leading_zeros = -1;
  for (bool bit = false; !bit; nb_leading_zeros++) {
    if (readBitNal(handle, &bit) < 0)
      return -1;
  }

  // Check prefix
  if (nb_leading_zeros < 0 || 32 < nb_leading_zeros)
    LIBBLU_H264_ERROR_RETURN(
      "Corrupted/invalid Exp-Golomb code, invalid prefix length (%d).\n",
      nb_leading_zeros
    );

  // Interpret prefix
  uint64_t value = (1ull << nb_leading_zeros) - 1ull;
  if (!nb_leading_zeros)
    goto success;

  // Parse suffix
  uint32_t suffix;
  if (readBitsNal(handle, &suffix, (size_t) nb_leading_zeros) < 0)
    return -1;
  value += suffix;

  // Check parsed value range
  if (0 < max_bit_size) {
    uint64_t max_value = (1ull << max_bit_size) - 1ull;

    if (max_value < value)
      LIBBLU_H264_ERROR_RETURN(
        "Invalid Exp-Golomb code, overflow occurs (%" PRIu32 " < %" PRIu32 ").\n",
        max_value,
        value
      );
  }

success:
  if (NULL != value_ret)
    *value_ret = (uint32_t) value;

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
  uint32_t *value_ret,
  int max_bit_size
)
{
  uint32_t u_value;
  if (readExpGolombCodeNal(handle, &u_value, max_bit_size) < 0)
    return -1;

  if (NULL != value_ret) {
    uint32_t e_value = (u_value >> 1) + (u_value & 0x1); // Extend
    *value_ret = (u_value & 0x1) ? e_value : -e_value;
  }

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

int reachNextNal(
  H264ParametersHandlerPtr handle
);

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