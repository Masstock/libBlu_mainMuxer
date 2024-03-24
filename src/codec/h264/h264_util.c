#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "h264_util.h"

void updateH264LevelLimits(
  H264ParametersHandlerPtr handle,
  uint8_t level_idc
)
{
  assert(NULL != handle);

  handle->constraints.MaxMBPS      = getH264MaxMBPS(level_idc);
  handle->constraints.MaxFS        = getH264MaxFS(level_idc);
  handle->constraints.MaxDpbMbs    = getH264MaxDpbMbs(level_idc);
  handle->constraints.MaxBR        = getH264MaxBR(level_idc);
  handle->constraints.MaxCPB       = getH264MaxCPB(level_idc);
  handle->constraints.MaxVmvR      = getH264MaxVmvR(level_idc);
  handle->constraints.MinCR        = getH264MinCR(level_idc);
  handle->constraints.MaxMvsPer2Mb = getH264MaxMvsPer2Mb(level_idc);

  handle->constraints.SliceRate                 = getH264SliceRate(level_idc);
  handle->constraints.MinLumaBiPredSize         = getH264MinLumaBiPredSize(level_idc);
  handle->constraints.direct_8x8_inference_flag = getH264direct_8x8_inference_flag(level_idc);
  handle->constraints.frame_mbs_only_flag       = getH264frame_mbs_only_flag(level_idc);
  handle->constraints.MaxSubMbRectSize          = getH264MaxSubMbRectSize(level_idc);
}

void updateH264ProfileLimits(
  H264ParametersHandlerPtr handle,
  H264ProfileIdcValue profile_idc,
  H264ContraintFlags constraints_flags
)
{
  H264ConstraintsParam *constraintsParam;

  assert(NULL != handle);

  constraintsParam = &handle->constraints;

  constraintsParam->cpbBrVclFactor = getH264cpbBrVclFactor(profile_idc);
  constraintsParam->cpbBrNalFactor = getH264cpbBrNalFactor(profile_idc);
  constraintsParam->restrEntropyCodingMode = H264_ENTROPY_CODING_MODE_UNRESTRICTED;

  switch (profile_idc) {
  case H264_PROFILE_BASELINE:
    /* A.2.1 Baseline profile constraints */
    constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IP;
    constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
    constraintsParam->restrictedFrameMbsOnlyFlag = true;
    constraintsParam->forbiddenWeightedPredModesUse = true;
    constraintsParam->restrEntropyCodingMode =
      H264_ENTROPY_CODING_MODE_CAVLC_ONLY;
    constraintsParam->maxAllowedNumSliceGroups = 8;
    constraintsParam->forbiddenPPSExtensionParameters = true;
    constraintsParam->maxAllowedLevelPrefix = 15;
    constraintsParam->restrictedPcmSampleValues = true;

    if (constraints_flags.set1) {
      /* A.2.1.1 Constrained Baseline profile constraints */
      constraintsParam->forbiddenArbitrarySliceOrder = true;
      constraintsParam->maxAllowedNumSliceGroups = 1;
      constraintsParam->forbiddenRedundantPictures = true;
    }
    break;

  case H264_PROFILE_MAIN:
    /* A.2.2 Main profile constraints */
    constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IPB;
    constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
    constraintsParam->forbiddenArbitrarySliceOrder = true;
    constraintsParam->maxAllowedNumSliceGroups = 1;
    constraintsParam->forbiddenPPSExtensionParameters = true;
    constraintsParam->forbiddenRedundantPictures = true;
    constraintsParam->maxAllowedLevelPrefix = 15;
    constraintsParam->restrictedPcmSampleValues = true;
    break;

  case H264_PROFILE_EXTENDED:
    /* A.2.3 Extended profile constraints */
    constraintsParam->forcedDirect8x8InferenceFlag = true;
    constraintsParam->restrEntropyCodingMode =
      H264_ENTROPY_CODING_MODE_CAVLC_ONLY;
    constraintsParam->maxAllowedNumSliceGroups = 8;
    constraintsParam->forbiddenPPSExtensionParameters = true;
    constraintsParam->maxAllowedLevelPrefix = 15;
    constraintsParam->restrictedPcmSampleValues = true;
    break;

  case H264_PROFILE_HIGH:
    /* A.2.4 High profile constraints */
    constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IPB;
    constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
    constraintsParam->forbiddenArbitrarySliceOrder = true;
    constraintsParam->maxAllowedNumSliceGroups = 1;
    constraintsParam->forbiddenRedundantPictures = true;
    constraintsParam->restrictedChromaFormatIdc = H264_MONO_420_CHROMA_FORMAT_IDC;
    constraintsParam->maxAllowedBitDepthMinus8 = 0;
    constraintsParam->forbiddenQpprimeYZeroTransformBypass = true;

    if (constraints_flags.set4) {
      /* A.2.4.1 Progressive High profile constraints */
      constraintsParam->restrictedFrameMbsOnlyFlag = true;

      if (constraints_flags.set5) {
        /* A.2.4.2 Constrained High profile constraints */
        constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IP;
      }
    }
    break;

  case H264_PROFILE_HIGH_10:
    /* A.2.5 High 10 profile constraints */
    constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IPB;
    constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
    constraintsParam->forbiddenArbitrarySliceOrder = true;
    constraintsParam->maxAllowedNumSliceGroups = 1;
    constraintsParam->forbiddenRedundantPictures = true;
    constraintsParam->restrictedChromaFormatIdc = H264_MONO_420_CHROMA_FORMAT_IDC;
    constraintsParam->maxAllowedBitDepthMinus8 = 0;
    constraintsParam->forbiddenQpprimeYZeroTransformBypass = true;

    if (constraints_flags.set4) {
      /* A.2.5.1 Progressive High 10 profile constraints */
      constraintsParam->restrictedFrameMbsOnlyFlag = true;
    }
    if (constraints_flags.set3) {
      /* A.2.8 High 10 Intra profile constraints */
      constraintsParam->idrPicturesOnly = true;
    }
    break;

  case H264_PROFILE_HIGH_422:
    /* A.2.6 High 4:2:2 profile constraints */
    constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IPB;
    constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
    constraintsParam->forbiddenArbitrarySliceOrder = true;
    constraintsParam->maxAllowedNumSliceGroups = 1;
    constraintsParam->forbiddenRedundantPictures = true;
    constraintsParam->restrictedChromaFormatIdc =
      H264_MONO_TO_422_CHROMA_FORMAT_IDC;
    constraintsParam->maxAllowedBitDepthMinus8 = 2;
    constraintsParam->forbiddenQpprimeYZeroTransformBypass = true;

    if (constraints_flags.set3) {
      /* A.2.9 High 10 Intra profile constraints */
      constraintsParam->idrPicturesOnly = true;
    }
    break;

  case H264_PROFILE_HIGH_444_PREDICTIVE:
  case H264_PROFILE_CAVLC_444_INTRA:
    /* A.2.7 High 4:4:4 Predictive profile constraints */
    constraintsParam->allowed_slice_types = H264_PRIM_PIC_TYPE_IPB;
    constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
    constraintsParam->forbiddenArbitrarySliceOrder = true;
    constraintsParam->maxAllowedNumSliceGroups = 1;
    constraintsParam->forbiddenRedundantPictures = true;
    constraintsParam->maxAllowedBitDepthMinus8 = 6;

    if (
      constraints_flags.set3
      || profile_idc == H264_PROFILE_CAVLC_444_INTRA
    ) {
      /* A.2.10 High 4:4:4 Intra profile constraints */
      constraintsParam->idrPicturesOnly = true;

      if (profile_idc == H264_PROFILE_CAVLC_444_INTRA) {
        /* A.2.11 CAVLC 4:4:4 Intra profile constraints */
        constraintsParam->restrEntropyCodingMode =
          H264_ENTROPY_CODING_MODE_CAVLC_ONLY;
      }
    }
    break;

  default:
    LIBBLU_WARNING(
      "Profile-specific constraints for the bitstream's "
      "profile_idc are not yet implemented.\n"
      " -> Some parameters may not be supported and/or bitstream "
      "may be wrongly indicated as Rec. ITU-T H.264 compliant.\n"
    );
  }
}

void updateH264BDConstraints(
  H264ParametersHandlerPtr handle,
  uint8_t level_idc,
  const H264VuiParameters *vui_parameters
)
{
  assert(NULL != handle);

  handle->bd_constraints.min_nb_slices                 = getH264BDMinNbSlices(level_idc);
  handle->bd_constraints.max_GOP_length                = getH264BDMaxGOPLength(vui_parameters);
  handle->bd_constraints.max_nb_consecutive_B_pictures = H264_BD_MAX_CONSECUTIVE_B_PICTURES;

  handle->bd_constraints_initialized = true;
}

/* ### H.264 Context : ##################################################### */

static int initInputFileH264ParametersHandle(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings *settings
)
{
  return initH264NalDeserializerContext(&handle->file, settings->esFilepath);
}

static int initOutputFileH264ParametersHandle(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings *settings
)
{
  assert(NULL != handle);

  if (NULL == (handle->script = createEsmsHandler(ES_VIDEO, settings->options, FMT_SPEC_INFOS_H264)))
    return -1;

  if (NULL == (handle->script_file = createBitstreamWriterDefBuf(settings->scriptFilepath)))
    return -1;

  if (appendSourceFileEsmsHandler(handle->script, settings->esFilepath, &handle->script_src_file_idx) < 0)
    return -1;

  if (writeEsmsHeader(handle->script_file) < 0)
    return -1;

  return 0;
}

H264ParametersHandlerPtr initH264ParametersHandler(
  const LibbluESParsingSettings *settings
)
{
  H264ParametersHandlerPtr handle;

  assert(NULL != settings);

  LIBBLU_H264_DEBUG(
    "Allocate the %u bytes context.\n",
    sizeof(H264ParametersHandler)
  );

  handle = (H264ParametersHandlerPtr) calloc(1, sizeof(H264ParametersHandler));
  if (NULL == handle)
    goto free_return;

  handle->cur_prog_param = (H264CurrentProgressParam) {
    .useVuiUpdate =
      0x00 != settings->options.fps_mod
      || isUsedLibbluAspectRatioMod(settings->options.ar_mod)
      || 0x00 != settings->options.level_mod
  };

  if (initInputFileH264ParametersHandle(handle, settings) < 0)
    goto free_return;
  if (initOutputFileH264ParametersHandle(handle, settings) < 0)
    goto free_return;

  return handle;

free_return:
  destroyH264ParametersHandler(handle);
  return NULL;
}

void resetH264ParametersHandler(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  handle->access_unit_delimiter_valid = false;
  handle->sequence_parameter_set_valid = false;
  handle->sei.bufferingPeriodValid = false;
  handle->sei.picTimingValid = false;
  handle->cur_prog_param.nb_slices_in_pic = 0;
  handle->cur_prog_param.auContent.bottomFieldPresent = false;
  handle->cur_prog_param.auContent.topFieldPresent = false;
  handle->cur_prog_param.auContent.framePresent = false;
  handle->cur_prog_param.lstNaluType = NAL_UNIT_TYPE_UNSPECIFIED;
}

int completeH264ParametersHandler(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings *settings
)
{
  if (completePesCuttingScriptEsmsHandler(handle->script_file, handle->script) < 0)
    return -1;

  closeBitstreamWriter(handle->script_file);
  handle->script_file = NULL;

  if (updateEsmsFile(settings->scriptFilepath, handle->script) < 0)
    return -1;

  return 0;
}

void destroyH264ParametersHandler(
  H264ParametersHandlerPtr handle
)
{
  unsigned i;

  if (NULL == handle)
    return;

  destroyEsmsHandler(handle->script);
  closeBitstreamWriter(handle->script_file);
  cleanH264NalDeserializerContext(handle->file);

  for (i = 0; i < H264_MAX_PPS; i++)
    free(handle->picture_parameter_set[i]);
  free(handle->cur_prog_param.cur_access_unit.NALUs);
  free(handle->modified_NALU_list.sequenceParametersSets);
  free(handle);
}

int updatePPSH264Parameters(
  H264ParametersHandlerPtr handle,
  const H264PicParametersSetParameters *param,
  unsigned id
)
{
  H264PicParametersSetParameters *newPps;

  assert(NULL != handle);
  assert(id < H264_MAX_PPS);

  if (NULL == handle->picture_parameter_set[id]) {
    /* Need allocation */
    newPps = (H264PicParametersSetParameters *) malloc(
      sizeof(H264PicParametersSetParameters)
    );
    if (NULL == newPps)
      LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");

    handle->picture_parameter_set[id] = newPps;
  }

  /* Copy PPS */
  memcpy(
    handle->picture_parameter_set[id],
    param,
    sizeof(H264PicParametersSetParameters)
  );

  return 0;
}

int deserializeRbspCell(
  H264ParametersHandlerPtr handle
)
{
  uint32_t value;

  assert(NULL != handle);

  if (!handle->file.packetInitialized)
    LIBBLU_H264_ERROR_RETURN(
      "NAL unit not initialized, unable to deserialize.\n"
    );

  handle->file.remainingRbspCellBits = 0;

  if (
    !isEof(handle->file.inputFile)
    && nextUint24(handle->file.inputFile) == 0x000003
  ) {
    /* [v8 rbsp_byte[0]] [v8 rbsp_byte[1]] */
    if (readValueBigEndian(handle->file.inputFile, 2, &value) < 0)
      return -1;

    handle->file.currentRbspCellBits = value;
    handle->file.remainingRbspCellBits = 16;

    /* [v8 emulation_prevention_three_byte] */
    if (skipBytes(handle->file.inputFile, 1) < 0)
      return -1;
  }
  else if (
    !isEof(handle->file.inputFile)
    && nextUint24(handle->file.inputFile) != 0x000001
    && nextUint32(handle->file.inputFile) != 0x00000001
  ) {
    /* [v8 rbsp_byte[0]] */
    if (readValueBigEndian(handle->file.inputFile, 1, &value) < 0)
      return -1;

    handle->file.currentRbspCellBits = value;
    handle->file.remainingRbspCellBits = 8;
  }

  while (
    !isEof(handle->file.inputFile)
    && nextUint32(handle->file.inputFile) == 0x00000000
  ) {
    /* [v8 trailing_zero_8bits] // 0x00 */
    if (skipBytes(handle->file.inputFile, 1) < 0)
      return -1;
  }

  if (
    isEof(handle->file.inputFile)
    || nextUint24(handle->file.inputFile) == 0x000001
    || nextUint32(handle->file.inputFile) == 0x00000001
  ) {
    handle->file.packetInitialized = false;
  }

  return 0;
}

H264NalByteArrayHandlerPtr createH264NalByteArrayHandler(
  H264NalHeaderParameters headerParam
)
{
  H264NalByteArrayHandlerPtr baHandler;

  baHandler = (H264NalByteArrayHandlerPtr) malloc(sizeof(H264NalByteArrayHandler));
  if (NULL == baHandler)
    LIBBLU_H264_ERROR_NRETURN("Memory allocation error.\n");

  *baHandler = (H264NalByteArrayHandler) {0};

  /* Write NAL header : */
  if (writeH264NalHeader(baHandler, headerParam) < 0)
    return NULL;
  return baHandler;
}

void destroyH264NalByteArrayHandler(
  H264NalByteArrayHandlerPtr baHandler
)
{
  if (NULL == baHandler)
    return;

  free(baHandler->array);
  free(baHandler);
}

H264AUNalUnitPtr createNewNalCell(
  H264ParametersHandlerPtr handle,
  H264NalUnitTypeValue nal_unit_type
)
{
  assert(NULL != handle);
  assert(!handle->cur_prog_param.cur_access_unit.is_in_process_NALU);

  unsigned nb_allocated_NALUs = handle->cur_prog_param.cur_access_unit.nb_allocated_NALUs;
  unsigned nb_used_NALUs = handle->cur_prog_param.cur_access_unit.nb_used_NALUs;

  if (nb_allocated_NALUs <= nb_used_NALUs) {
    /* Need reallocation to add new cell(s) to current Access Unit NALs list */
    size_t newSize = GROW_ALLOCATION(nb_allocated_NALUs, H264_AU_DEFAULT_NB_NALUS);

    if (lb_mul_overflow_size_t(newSize, sizeof(H264AUNalUnit)))
      LIBBLU_H264_ERROR_NRETURN("Nb AU allocated NALUs overflow.\n");

    H264AUNalUnit *newList = (H264AUNalUnit *) realloc(
      handle->cur_prog_param.cur_access_unit.NALUs,
      newSize *sizeof(H264AUNalUnit)
    );
    if (NULL == newList)
      LIBBLU_H264_ERROR_NRETURN("Memory allocation error.\n");

    handle->cur_prog_param.cur_access_unit.NALUs = newList;
    handle->cur_prog_param.cur_access_unit.nb_allocated_NALUs = newSize;
  }

  H264AUNalUnitPtr cell = &handle->cur_prog_param.cur_access_unit.NALUs[nb_used_NALUs];
  cell->nal_unit_type = nal_unit_type;
  cell->startOffset = 0;
  cell->length = 0;
  cell->replace = false;

  handle->cur_prog_param.cur_access_unit.is_in_process_NALU = true;

  return cell;
}

static H264AUNalUnitPtr _retrieveCurrentNalCell(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  if (!handle->cur_prog_param.cur_access_unit.is_in_process_NALU)
    LIBBLU_H264_ERROR_NRETURN(
      "No NAL unit cell in process for current Access Unit.\n"
    );

  return &handle->cur_prog_param.cur_access_unit.NALUs[
    handle->cur_prog_param.cur_access_unit.nb_used_NALUs
  ];
}

int replaceCurNalCell(
  H264ParametersHandlerPtr handle,
  H264AUNalUnitReplacementData newParam
)
{
  H264AUNalUnit *nalUnit;

  assert(NULL != handle);

  nalUnit = _retrieveCurrentNalCell(handle);
  nalUnit->replace = true;
  nalUnit->replacementParameters = newParam;

  return 0;
}

int discardCurNalCell(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  if (!handle->cur_prog_param.cur_access_unit.is_in_process_NALU)
    LIBBLU_H264_ERROR_RETURN(
      "No NAL unit cell in process for current Access Unit.\n"
    );

  handle->cur_prog_param.cur_access_unit.is_in_process_NALU = false;
  return 0;
}

int addNalCellToAccessUnit(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  H264AUNalUnit *nalUnit = _retrieveCurrentNalCell(handle);
  nalUnit->length = tellPos(handle->file.inputFile) - nalUnit->startOffset;

  handle->cur_prog_param.lstNaluType = getNalUnitType(handle);

  handle->cur_prog_param.cur_access_unit.nb_used_NALUs++;
  handle->cur_prog_param.cur_access_unit.is_in_process_NALU = false;

  if (
    (getNalUnitType(handle) == NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET
    || getNalUnitType(handle) == NAL_UNIT_TYPE_PIC_PARAMETER_SET)
    && 0 == handle->cur_prog_param.nbPics
  ) {
    /* StreamEye fix */
    handle->cur_prog_param.cur_access_unit.size += nalUnit->length;
  }
  handle->cur_prog_param.cur_access_unit.size += nalUnit->length;

  return 0;
}

int reachNextNal(
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

#if 0
  if (0 < nextUint8(handle->file.inputFile))
    ret = skipBytes(handle->file.inputFile, 1);
#else
  while (0 < nextUint8(handle->file.inputFile)) {
    if ((ret = skipBytes(handle->file.inputFile, 1)) < 0)
      break;
  }
#endif
  return ret;
#endif
}

int writeH264NalByteArrayBit(
  H264NalByteArrayHandlerPtr baHandler,
  bool bit
)
{
  assert(NULL != baHandler);

  baHandler->curByte = (baHandler->curByte << 1) | bit;
  baHandler->curNbBits++;

  assert(baHandler->curNbBits <= 8);
  if (8 == baHandler->curNbBits) {
    /* Write complete byte. */

    if (baHandler->endPointer <= baHandler->writingPointer + 1) {
      /* Need realloc */
      uint8_t *newArray;
      size_t newSize;

      newSize = GROW_ALLOCATION(baHandler->allocatedSize, 1024);
      if (newSize < baHandler->allocatedSize)
        LIBBLU_H264_ERROR_RETURN("NALU byte array writer size overflow.\n");

      if (NULL == (newArray = (uint8_t *) realloc(baHandler->array, newSize)))
        LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");

      /* Update pointers : */
      if (NULL == baHandler->writingPointer) /* Init pointers. */
        baHandler->writingPointer = baHandler->endPointer = newArray;

      baHandler->array          = newArray;
      baHandler->allocatedSize  = newSize;
      baHandler->endPointer    += newSize;
    }

    if (baHandler->nbZeroBytes == 2 && baHandler->curByte <= 0x03) {
      /**
       * More than two consecutive 0x00 rbsp_byte written, 0x000000, 0x000001
       * or 0x000002 3-bytes forbidden pattern may happen at a byte-aligned
       * position if current byte is 0x00, 0x01 or 0x02. Also if current byte
       * is 0x03, might take 0x000003 pattern as an 'emulation_prevention_three
       * _byte' preceded by two 0x00 bytes. Use rather 0x00000303 4-bytes
       * pattern.
       *  => Write 'emulation_prevention_three_byte'.
       */

      /* More than two consecutive 0x00 rbsp_byte written,  */
      /* 0x000000, 0x000001, 0x000002 or 0x000003 pattern may happen. */
      /* => Write emulation_prevention_three_byte           */

      /* [v8 emulation_prevention_three_byte] // 0x03 */
      *(baHandler->writingPointer++) = 0x03;

      baHandler->nbZeroBytes = 0;
    }

    *(baHandler->writingPointer++) = baHandler->curByte;

    if (baHandler->rbspZone && baHandler->curByte == 0x00)
      baHandler->nbZeroBytes++;
    else
      baHandler->nbZeroBytes = 0;

    /* Clear for new byte : */
    baHandler->curNbBits = 0;
    baHandler->curByte = 0x00;
  }

  return 0;
}

int writeH264NalByteArrayBits(
  H264NalByteArrayHandlerPtr baHandler,
  uint64_t value,
  size_t nbBits
)
{
  unsigned i;

  assert(nbBits <= 64);

  for (i = 0; i < nbBits; i++) {
    if (writeH264NalByteArrayBit(baHandler, (value >> (nbBits - i - 1)) & 0x1) < 0)
      return -1;
  }

  return 0;
}

int writeH264NalByteArrayByte(
  H264NalByteArrayHandlerPtr baHandler,
  uint8_t byte
)
{
  return writeH264NalByteArrayBits(baHandler, byte, 8);
}

static unsigned _calcExpGolombCodeNbLeadingZeroBits(
  unsigned value
)
{
  if (value <= 1)
    return 0;
  if (value <= 3)
    return 1;
  if (value <= 7)
    return 2;
  if (value <= 15)
    return 3;
  if (value <= 31)
    return 4;
  if (value <= 63)
    return 5;
  if (value <= 127)
    return 6;
  if (value <= 255)
    return 7;
  return floor(log2(value));
}

int writeH264NalByteArrayExpGolombCode(
  H264NalByteArrayHandlerPtr baHandler,
  unsigned value
)
{
  unsigned leadingZeroBits;

  leadingZeroBits = _calcExpGolombCodeNbLeadingZeroBits(++value);

  /* prefix bits */
  if (writeH264NalByteArrayBits(baHandler, 0x0, leadingZeroBits) < 0)
    return -1;

  /* middle bit */
  if (writeH264NalByteArrayBits(baHandler, 0x1, 1) < 0)
    return -1;

  /* suffix bits */
  if (writeH264NalByteArrayBits(baHandler, value, leadingZeroBits) < 0)
    return -1;

  return 0;
}

int writeH264NalByteArraySignedExpGolombCode(
  H264NalByteArrayHandlerPtr baHandler,
  int value
)
{
  value *= 2;

  if (0 < value)
    value--;
  else
    value = -value;

  return writeH264NalByteArrayExpGolombCode(baHandler, (unsigned) value);
}

int writeH264NalHeader(
  H264NalByteArrayHandlerPtr baHandler,
  H264NalHeaderParameters handle
)
{
  assert(NULL != baHandler);

  if (
    handle.nal_unit_type == NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET
    || handle.nal_unit_type == NAL_UNIT_TYPE_PIC_PARAMETER_SET
    /* || handle.firstNalInAU */
  ) {
    /* [v8 zero_byte] */
    if (writeH264NalByteArrayBits(baHandler, 0x00, 8) < 0)
      return -1;
  }

  /* [v24 start_code_prefix_one_3bytes] */
  if (writeH264NalByteArrayBits(baHandler, 0x000001, 24) < 0)
    return -1;

  /* nal_unit() */
  /* [v1 forbidden_zero_bit] */
  if (writeH264NalByteArrayBits(baHandler, 0x0, 1) < 0)
    return -1;

  /* [u2 nal_ref_idc] */
  if (writeH264NalByteArrayBits(baHandler, handle.nal_ref_idc, 2) < 0)
    return -1;
  /* [u5 nal_unit_type] */
  if (writeH264NalByteArrayBits(baHandler, handle.nal_unit_type, 5) < 0)
    return -1;

  if (
    handle.nal_unit_type == NAL_UNIT_TYPE_PREFIX_NAL_UNIT
    || handle.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_EXT
    || handle.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT
  ) {
    LIBBLU_H264_ERROR_RETURN(
      "Unsupported NALU type '%s', missing functionnality.\n",
      H264NalUnitTypeStr(handle.nal_unit_type)
    );

    /* TODO */
    if (handle.nal_unit_type != NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT) {
      /* [b1 svc_extension_flag] */
    }
    else {
      /* [b1 avc_3d_extension_flag] */
    }
  }

  baHandler->rbspZone = true;

  return 0; /* OK */
}

size_t calcH264ExpGolombCodeLength(
  unsigned value,
  bool isSigned
)
{
  if (isSigned)
    value = ((int) value <= 0) ? (unsigned) -((int) value * 2) : value * 2 - 1;

  return _calcExpGolombCodeNbLeadingZeroBits(++value) * 2 + 1;
}

bool isByteAlignedNal(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  return (0 == (handle->file.remainingRbspCellBits & 0x7));
}

bool moreRbspDataNal(
  H264ParametersHandlerPtr handle
)
{
  /* 7.2 Specification of syntax functions, categories, and descriptors - more_rbsp_data() */
  size_t pos;

  assert(NULL != handle);

  if (!handle->file.remainingRbspCellBits) {
    if (!handle->file.packetInitialized)
      return false;

    if (deserializeRbspCell(handle) < 0)
      return -1;
  }

  if (handle->file.packetInitialized)
    return true;

  for (pos = 0; 0x0 == ((handle->file.currentRbspCellBits >> pos) & 1); pos++)
    ;

  return pos != handle->file.remainingRbspCellBits - 1;
}

bool moreRbspTrailingDataNal(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  return handle->file.remainingRbspCellBits || handle->file.packetInitialized;
}

bool noMoreNal(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  return isEof(handle->file.inputFile);
}

bool isInitNal(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  return handle->file.packetInitialized;
}

bool isByteAlignedH264NalByteArray(
  H264NalByteArrayHandlerPtr baHandler
)
{
  assert(NULL != baHandler);

  return baHandler->curNbBits == 0;
}
