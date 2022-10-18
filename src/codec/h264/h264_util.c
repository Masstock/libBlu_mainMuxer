#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "h264_util.h"

/* ### H.264 Context : ##################################################### */

static int initInputFileH264ParametersHandle(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings * settings
)
{
  return initH264NalDeserializerContext(&handle->file, settings->esFilepath);
}

static int initOutputFileH264ParametersHandle(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings * settings
)
{
  assert(NULL != handle);

  handle->esms = createEsmsFileHandler(
    ES_VIDEO,
    settings->options,
    FMT_SPEC_INFOS_H264
  );
  if (NULL == handle->esms)
    return -1;

  handle->esmsFile = createBitstreamWriter(
    settings->scriptFilepath,
    WRITE_BUFFER_LEN
  );
  if (NULL == handle->esmsFile)
    return -1;

  if (appendSourceFileEsms(handle->esms, settings->esFilepath, &handle->esmsSrcFileIdx) < 0)
    return -1;

  if (writeEsmsHeader(handle->esmsFile) < 0)
    return -1;

  return 0;
}

H264ParametersHandlerPtr initH264ParametersHandler(
  const LibbluESParsingSettings * settings
)
{
  H264ParametersHandlerPtr handle;

  assert(NULL != settings);

  LIBBLU_H264_DEBUG(
    "Allocate the %u bytes context.\n",
    sizeof(H264ParametersHandle)
  );

  handle = (H264ParametersHandlerPtr) calloc(1, sizeof(H264ParametersHandle));
  if (NULL == handle)
    goto free_return;

  handle->curProgParam = (H264CurrentProgressParam) {
    .curFrameNalUnits = NULL,
    .lastPicStruct = -1,

    .useVuiUpdate =
      0x00 != settings->options.fpsChange
      || isUsedLibbluAspectRatioMod(settings->options.arChange)
      || 0x00 != settings->options.levelChange
    ,
  };

  handle->warningFlags = INIT_H264_WARNING_FLAGS();

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

  handle->accessUnitDelimiterValid = false;
  handle->sequenceParametersSetValid = false;
  handle->sei.bufferingPeriodValid = false;
  handle->sei.picTimingValid = false;
  handle->curProgParam.curNbSlices = 0;
  handle->curProgParam.bottomFieldPresent = false;
  handle->curProgParam.topFieldPresent = false;
  handle->curProgParam.idrPresent = false;
  handle->curProgParam.initPicOrderCntAU = false;
  handle->curProgParam.lastNalUnitType = NAL_UNIT_TYPE_UNSPECIFIED;
}

int completeH264ParametersHandler(
  H264ParametersHandlerPtr handle,
  const LibbluESParsingSettings * settings
)
{
  if (addEsmsFileEnd(handle->esmsFile, handle->esms) < 0)
    return -1;

  closeBitstreamWriter(handle->esmsFile);
  handle->esmsFile = NULL;

  if (updateEsmsHeader(settings->scriptFilepath, handle->esms) < 0)
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

  destroyEsmsFileHandler(handle->esms);
  closeBitstreamWriter(handle->esmsFile);
  cleanH264NalDeserializerContext(handle->file);

  for (i = 0; i < H264_MAX_PPS; i++)
    free(handle->picParametersSet[i]);

  if (
    handle->slicePresent
    && handle->slice.header.decRefPicMarkingPresent
  ) {
    /* Free memory_management_control_operation storage structures : */
    if (
      !handle->slice.header.decRefPicMarking.IdrPicFlag
      && handle->slice.header.decRefPicMarking.adaptativeRefPicMarkingMode
    )
      closeH264MemoryManagementControlOperations(
        handle->slice.header.decRefPicMarking.MemMngmntCtrlOp
      );
  }

  free(handle->curProgParam.curFrameNalUnits);
  for (i = 0; i < handle->modNalLst.nbSequenceParametersSet; i++)
    free(handle->modNalLst.sequenceParametersSets[i].linkedParam);
  free(handle->modNalLst.sequenceParametersSets);
  destroyH264HrdVerifierContext(handle->hrdVerifier);
  free(handle);
}

int updatePPSH264Parameters(
  H264ParametersHandlerPtr handle,
  const H264PicParametersSetParameters * param,
  unsigned id
)
{
  H264PicParametersSetParameters * newPps;

  assert(NULL != handle);
  assert(id < H264_MAX_PPS);

  if (NULL == handle->picParametersSet[id]) {
    /* Need allocation */
    newPps = (H264PicParametersSetParameters *) malloc(
      sizeof(H264PicParametersSetParameters)
    );
    if (NULL == newPps)
      LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");

    handle->picParametersSet[id] = newPps;
  }

  /* Copy PPS */
  memcpy(
    handle->picParametersSet[id],
    param,
    sizeof(H264PicParametersSetParameters)
  );

  return 0;
}

int initIfRequiredH264CpbHrdVerifier(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions * options
)
{
  assert(NULL == handle->hrdVerifier);
  assert(NULL != options);

  if (checkH264CpbHrdVerifierAvailablity(&handle->curProgParam, *options)) {
    /* If HRD verifier can be initialized, try to do so. */
    handle->hrdVerifier = createH264HrdVerifierContext(
      options,
      &handle->sequenceParametersSet.data,
      &handle->constraints
    );
    if (NULL == handle->hrdVerifier)
      return -1;
  }
  else
    options->disableHrdVerifier = true; /* Otherwise disable it. */

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

  baHandler = (H264NalByteArrayHandlerPtr) malloc(
    sizeof(H264NalByteArrayHandler)
  );
  if (NULL == baHandler)
    LIBBLU_H264_ERROR_NRETURN("Memory allocation error.\n");

  baHandler->allocatedArrayLength = 0;
  baHandler->array = NULL;

  baHandler->writingPointer = NULL;
  baHandler->endPointer = NULL;

  baHandler->curByte = 0x00;
  baHandler->rbspZone = false;

  baHandler->curNbBits = 0;
  baHandler->nbZeroBytes = 0;

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
  uint8_t nalUnitType
)
{
  H264AUNalUnit * newList;
  H264AUNalUnitPtr cell;

  unsigned usedNalNb;
  unsigned allocatedNalNb;

  assert(NULL != handle);

  usedNalNb = handle->curProgParam.curFrameNbNalUnits;
  allocatedNalNb = handle->curProgParam.allocatedNalUnitsCells;

  if (allocatedNalNb < (usedNalNb + 1)) {
    if (0 < (usedNalNb - allocatedNalNb))
      LIBBLU_H264_ERROR_NRETURN(
        "Presence of ghost cell(s) in current Access Unit NALs list.\n"
      );

    /* Need reallocation to add new cell(s) to current Access Unit NALs list */
    newList = (H264AUNalUnit *) realloc(
      handle->curProgParam.curFrameNalUnits,
      (usedNalNb + 1) * sizeof(H264AUNalUnit)
    );
    if (NULL == newList) {
      /* Error happen during realloc, free list and return error */
      LIBBLU_H264_ERROR_NRETURN("Memory allocation error.\n");
    }

    handle->curProgParam.allocatedNalUnitsCells = allocatedNalNb + 1;
    handle->curProgParam.curFrameNalUnits = newList; /* Update new list */
  }

  cell = handle->curProgParam.curFrameNalUnits + usedNalNb;

  cell->nalUnitType = nalUnitType;
  cell->startOffset = 0;

  cell->length = 0;
  cell->replace = false;
  cell->replacementParam = NULL;

  handle->curProgParam.inProcessNalUnitCell = true;

  return cell;
}

H264AUNalUnitPtr recoverCurNalCell(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  if (!handle->curProgParam.inProcessNalUnitCell)
    LIBBLU_H264_ERROR_NRETURN(
      "No NAL unit cell in process for current Access Unit.\n"
    );

  return
    handle->curProgParam.curFrameNalUnits
    + handle->curProgParam.curFrameNbNalUnits
  ;
}

int replaceCurNalCell(
  H264ParametersHandlerPtr handle,
  void * newParam,
  size_t newParamSize
)
{
  H264AUNalUnit * nalUnit;
  void * data;

  assert(NULL != handle);
  assert(NULL != newParam);

  if (!handle->curProgParam.inProcessNalUnitCell)
    LIBBLU_H264_ERROR_RETURN(
      "No NAL unit cell in process for current Access Unit.\n"
    );

  if (NULL == (data = malloc(newParamSize)))
    LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");
  memcpy(data, newParam, newParamSize);

  nalUnit =
    handle->curProgParam.curFrameNalUnits
    + handle->curProgParam.curFrameNbNalUnits
  ;
  nalUnit->replace = true;
  nalUnit->replacementParam = data;

  return 0;
}

int discardCurNalCell(
  H264ParametersHandlerPtr handle
)
{
  H264AUNalUnit * nalUnit;

  assert(NULL != handle);

  if (!handle->curProgParam.inProcessNalUnitCell)
    LIBBLU_H264_ERROR_RETURN(
      "No NAL unit cell in process for current Access Unit.\n"
    );

  nalUnit =
    handle->curProgParam.curFrameNalUnits
    + handle->curProgParam.curFrameNbNalUnits
  ;
  free(nalUnit->replacementParam);

  handle->curProgParam.inProcessNalUnitCell = false;
  return 0;
}

int addNalCellToAccessUnit(
  H264ParametersHandlerPtr handle
)
{
  H264AUNalUnit * nalUnit;

  assert(NULL != handle);

  if (!handle->curProgParam.inProcessNalUnitCell)
    LIBBLU_H264_ERROR_RETURN(
      "No NAL unit cell in process for current Access Unit.\n"
    );

  handle->curProgParam.lastNalUnitType = getNalUnitType(handle);

  nalUnit =
    handle->curProgParam.curFrameNalUnits
    + handle->curProgParam.curFrameNbNalUnits
  ;

  nalUnit->length = tellPos(handle->file.inputFile) - nalUnit->startOffset;

  handle->curProgParam.curFrameLength += nalUnit->length;
  handle->curProgParam.curFrameNbNalUnits++;
  handle->curProgParam.inProcessNalUnitCell = false;

  return 0;
}

int writeH264NalByteArrayBit(
  H264NalByteArrayHandlerPtr baHandler,
  bool bit
)
{
  size_t newLength;
  uint8_t * newArray;

  assert(NULL != baHandler);

  baHandler->curByte = (baHandler->curByte << 1) | bit;
  baHandler->curNbBits++;

  assert(baHandler->curNbBits <= 8);
  if (8 == baHandler->curNbBits) {
    /* Write complete byte. */

    if (baHandler->endPointer <= baHandler->writingPointer + 1) {
      /* Need realloc */

      newLength =
        baHandler->allocatedArrayLength
        + H264_NAL_BYTE_ARRAY_SIZE_MULTIPLIER
      ;

      newArray = (uint8_t *) realloc(
        baHandler->array,
        newLength * sizeof(uint8_t)
      );
      if (NULL == newArray)
        LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");

      /* Update pointers : */
      if (NULL == baHandler->writingPointer) /* Init pointers. */
        baHandler->writingPointer = baHandler->endPointer = newArray;

      baHandler->array = newArray;
      baHandler->endPointer += newLength;
      baHandler->allocatedArrayLength = newLength;
    }

    if (baHandler->nbZeroBytes == 2 && baHandler->curByte <= 0x02) {
      /* More than two consecutive 0x00 rbsp_byte written,  */
      /* 0x000000, 0x000001 or 0x000002 pattern may happen. */
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

unsigned calcExpGolombCodeNbLeadingZeroBits(
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

  leadingZeroBits = calcExpGolombCodeNbLeadingZeroBits(++value);

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
    handle.nalUnitType == NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET
    || handle.nalUnitType == NAL_UNIT_TYPE_PIC_PARAMETERS_SET
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
  if (writeH264NalByteArrayBits(baHandler, handle.nalRefIdc, 2) < 0)
    return -1;
  /* [u5 nal_unit_type] */
  if (writeH264NalByteArrayBits(baHandler, handle.nalUnitType, 5) < 0)
    return -1;

  if (
    handle.nalUnitType == NAL_UNIT_TYPE_PREFIX_NAL_UNIT
    || handle.nalUnitType == NAL_UNIT_TYPE_CODED_SLICE_EXT
    || handle.nalUnitType == NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT
  ) {
    LIBBLU_H264_ERROR_RETURN(
      "Unsupported NALU type '%s', missing functionnality.\n",
      H264NalUnitTypeStr(handle.nalUnitType)
    );

    /* TODO */
    if (handle.nalUnitType != NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT) {
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

  return calcExpGolombCodeNbLeadingZeroBits(++value) * 2 + 1;
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
