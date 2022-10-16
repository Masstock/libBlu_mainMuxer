#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "h264_parser.h"

int initNal(
  H264ParametersHandlerPtr param
)
{
  H264AUNalUnitPtr auNalUnitCell;

  int64_t nalStartOffset;
  uint32_t value;

  assert(NULL != param);

  if (param->file.packetInitialized)
    LIBBLU_H264_ERROR_RETURN(
      "Double initialisation of a NAL unit at 0x%" PRIx64 " offset.\n",
      tellPos(param->file.inputFile)
    );

  nalStartOffset = tellPos(param->file.inputFile);

  while (
    !isEof(param->file.inputFile)
    && nextUint24(param->file.inputFile) != 0x000001
    && nextUint32(param->file.inputFile) != 0x00000001
  ) {
    /* [v8 leading_zero_8bits] // 0x00 */
    if (readBits(param->file.inputFile, &value, 8) < 0)
      return -1;

    if (value != 0x00)
      LIBBLU_H264_ERROR_RETURN(
        "Broken NAL header, "
        "expected leading_zero_8bits field at 0x%" PRIX64 ".\n",
        nalStartOffset
      );
  }

  if (
    !isEof(param->file.inputFile)
    && nextUint24(param->file.inputFile) != 0x000001
  ) {
    /* [v8 zero_byte] // 0x00 */
    if (readBits(param->file.inputFile, &value, 8) < 0)
      return -1;

    if (value != 0x00) {
      LIBBLU_H264_ERROR_RETURN(
        "Broken NAL header, "
        "expected zero_byte field before 0x%" PRIX64 ".\n",
        nalStartOffset
      );
    }
  }

  if (isEof(param->file.inputFile))
    LIBBLU_H264_ERROR_RETURN(
      "Broken NAL header, "
      "unexpected end of file.\n"
    );

  /* [v24 start_code_prefix_one_3bytes] // 0x000001 */
  if (skipBits(param->file.inputFile, 24) < 0)
    return -1;

  /* nal_unit() */
  /* [v1 forbidden_zero_bit] // 0b0 */
  if (readBits(param->file.inputFile, &value, 1) < 0)
    return -1;

  /* [u2 nal_ref_idc] */
  if (readBits(param->file.inputFile, &value, 2) < 0)
    return -1;
  param->file.refIdc = value;

  /* [u5 nal_unit_type] */
  if (readBits(param->file.inputFile, &value, 5) < 0)
    return -1;
  param->file.type = value;

  if (
    param->file.type == NAL_UNIT_TYPE_PREFIX_NAL_UNIT
    || param->file.type == NAL_UNIT_TYPE_CODED_SLICE_EXT
    || param->file.type == NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT
  ) {
    /* TODO: Add 3D MVC support. */
    LIBBLU_H264_ERROR_RETURN(
      "Unsupported NAL header, "
      "unit type match a H.264/MVC bitstream, currently unsupported.\n"
    );
  }

  param->file.packetInitialized = true;
  if (deserializeRbspCell(param) < 0)
    return -1;

  LIBBLU_H264_DEBUG_NAL(
    "0x%08" PRIX64 " === NAL Unit - %s ===\n",
    nalStartOffset,
    H264NalUnitTypeStr(param->file.type)
  );
  LIBBLU_H264_DEBUG_NAL(
    " -> nal_ref_idc: 0x%" PRIx8 ".\n",
    param->file.refIdc
  );
  LIBBLU_H264_DEBUG_NAL(
    " -> nal_unit_type: 0x%" PRIx8 ".\n",
    param->file.type
  );

  /* Initializing Access Unit NALs collector new cell: */
  if (NULL == (auNalUnitCell = createNewNalCell(param, param->file.type)))
    return -1;
  auNalUnitCell->startOffset = nalStartOffset;

  return 0;
}

int parseH264RbspTrailingBits(
  H264ParametersHandlerPtr handle
)
{
  /* rbsp_trailing_bits() */
  uint32_t value;

  assert(NULL != handle);

  /* [v1 rbsp_stop_one_bit] // 0b1 */
  if (readBitsNal(handle, &value, 1) < 0)
    return -1;

  if (value != 0x1) {
    LIBBLU_H264_ERROR_RETURN(
      "Broken H.264 bitstream, "
      "expected rbsp_stop_one_bit before 0x%" PRIx64 ".\n",
      tellPos(handle->file.inputFile)
    );
  }

  while (!isByteAlignedNal(handle)) {
    /* [v1 rbsp_alignment_zero_bit] // 0b0 */
    if (readBitsNal(handle, &value, 1) < 0)
      return -1;

    if (value != 0x0) {
      LIBBLU_H264_ERROR_RETURN(
        "Broken H.264 bitstream, "
        "expected rbsp_alignment_zero_bit before 0x%" PRIx64 ".\n",
        tellPos(handle->file.inputFile)
      );
    }
  }

  return 0;
}

static int64_t calcCurPicOrderCnt(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  int PicOrderCountMsb, TopFieldOrderCnt, BottomFieldOrderCnt, PicOrderCnt;
  int64_t streamPicOrderCnt;
  bool bottomFieldPicture;

  H264SPSDataParameters * sps;
  H264SliceHeaderParameters * header;
  H264CurrentProgressParam * cur;

  unsigned prevPicOrderCntMsb, prevPicOrderCntLsb, prevTopFieldOrderCnt;
  bool prevPresenceOfMemManCtrlOp5, prevBottomFieldPicture;

  assert(NULL != handle);
  assert(handle->sequenceParametersSetPresent);

  sps =    &handle->sequenceParametersSet.data;
  header = &handle->slice.header;
  cur =    &handle->curProgParam;

  if (
    !handle->accessUnitDelimiterValid
    || !handle->sequenceParametersSetGopValid
    || !handle->slicePresent
  ) {
    LIBBLU_H264_ERROR_RETURN(
      "Unable to compute Picture Order Counter (PicOrderCnt) without"
      "parsing of an AUD, a SPS and a slice.\n"
    );
  }

  switch (sps->picOrderCntType) {
    case 0x0:
      /* Mode 0 - 8.2.1.1 Decoding process for picture order count type 0 */
      TopFieldOrderCnt = 0, BottomFieldOrderCnt = 0;

      prevPresenceOfMemManCtrlOp5 = cur->presenceOfMemManCtrlOp5;
      prevBottomFieldPicture = cur->bottomFieldPicture;

      prevTopFieldOrderCnt = cur->TopFieldOrderCnt;

      if (header->IdrPicFlag) {
        if (cur->curNbSlices == 1 && 1 < cur->nbPics) {
          cur->cumulPicOrderCnt += cur->maxPicOrderCnt + (
            options.doubleFrameTiming ? 1 : 2
          ); /* Saving new GOP PTS (last GOP final PTS + one frame) */

          /**
           * Reset max counter to get new GOP fina PTS
           * (biggest PTS value of a GOP).
           */
          cur->maxPicOrderCnt = 0;
        }

        prevPicOrderCntMsb = 0, prevPicOrderCntLsb = 0;
      }
      else {
        /* Check presence of 'memory_management_control_operation' equal to 5 */
        if (prevPresenceOfMemManCtrlOp5) {
          prevPicOrderCntMsb = 0;
          prevPicOrderCntLsb =
            (prevBottomFieldPicture) ? 0 : prevTopFieldOrderCnt
          ;
        }
        else {
          prevPicOrderCntMsb = cur->PicOrderCntMsb;
          prevPicOrderCntLsb = cur->PicOrderCntLsb;
        }
      }

      if (
        (header->picOrderCntLsb < prevPicOrderCntLsb)
        && (
          (prevPicOrderCntLsb - header->picOrderCntLsb)
          >= (sps->MaxPicOrderCntLsb >> 1)
        )
      )
        PicOrderCountMsb = prevPicOrderCntMsb + sps->MaxPicOrderCntLsb;
      else if (
        (header->picOrderCntLsb > prevPicOrderCntLsb)
        && (
          (header->picOrderCntLsb - prevPicOrderCntLsb)
          > (sps->MaxPicOrderCntLsb >> 1)
        )
      )
        PicOrderCountMsb = prevPicOrderCntMsb - sps->MaxPicOrderCntLsb;
      else
        PicOrderCountMsb = prevPicOrderCntMsb;

      bottomFieldPicture = header->fieldPic && header->bottomField;

      if (!header->fieldPic) {
        TopFieldOrderCnt =
          PicOrderCountMsb
          + header->picOrderCntLsb
        ;
        BottomFieldOrderCnt =
          TopFieldOrderCnt
          + header->deltaPicOrderCntBottom
        ;
        PicOrderCnt = MIN(
          BottomFieldOrderCnt,
          TopFieldOrderCnt
        );
      }
      else {
        PicOrderCnt = PicOrderCountMsb + header->picOrderCntLsb;

        if (bottomFieldPicture)
          BottomFieldOrderCnt = PicOrderCnt;
        else
          TopFieldOrderCnt = PicOrderCnt;
      }

      if (cur->maxPicOrderCnt < PicOrderCnt)
        cur->maxPicOrderCnt = PicOrderCnt;

      /* Saving frame informations: */
      if (header->refPic) {
        cur->PicOrderCntMsb = PicOrderCountMsb;
        cur->PicOrderCntLsb = header->picOrderCntLsb;

        cur->presenceOfMemManCtrlOp5 =
          !header->IdrPicFlag
          && header->decRefPicMarking.adaptativeRefPicMarkingMode
          && header->decRefPicMarking.presenceOfMemManCtrlOp5
        ;
        cur->bottomFieldPicture = bottomFieldPicture;
        cur->TopFieldOrderCnt = TopFieldOrderCnt;
      }

      streamPicOrderCnt = PicOrderCnt + cur->cumulPicOrderCnt;
      break;

    case 0x1:
      /* TODO */
      LIBBLU_H264_ERROR_RETURN(
        "Unsupported 'pic_order_cnt_type' mode 0x1, unable to compute "
        "PicOrderCnt.\n"
      );
      break;

    default:
      /* TODO */
      LIBBLU_H264_ERROR_RETURN(
        "Unsupported 'pic_order_cnt_type' mode %" PRIu8 ", unable to compute "
        "PicOrderCnt.\n",
        sps->picOrderCntType
      );
  }

  return streamPicOrderCnt;
}

static int computeAuPicOrderCnt(
  H264ParametersHandlerPtr handle,
  LibbluESParsingSettings * settings
)
{
  int64_t streamPicOrderCnt;

  assert(NULL != handle);

  if ((streamPicOrderCnt = calcCurPicOrderCnt(handle, settings->options)) < 0)
    return -1;

  if (handle->curProgParam.initPicOrderCntAU) {
    handle->curProgParam.picOrderCntAU = MIN(
      handle->curProgParam.picOrderCntAU, streamPicOrderCnt
    );

    /* lbc_printf(
      "picOrderCntAU: %" PRId64 "\n",
      handle->curProgParam.picOrderCntAU
    ); */
  }
  else {
    handle->curProgParam.picOrderCntAU = streamPicOrderCnt;
    handle->curProgParam.initPicOrderCntAU = true;
  }

  if (
    handle->curProgParam.picOrderCntAU & 0x1
    && !handle->curProgParam.cumulPicOrderCnt
    && streamPicOrderCnt & 0x1
    && H264_COMPLETE_PICTURE(handle->curProgParam)
    && !settings->options.doubleFrameTiming
  ) {
    /* Odd picOrderCnt found, need doubling timing values. */
    LIBBLU_WARNING(
      "Unexpected Odd picOrderCnt values found, "
      "timing values need to be doubled ('picOrderCntAU' == %" PRId64 ").\n",
      handle->curProgParam.picOrderCntAU
    );

    settings->options.doubleFrameTiming = true;
    settings->askForRestart = true;
  }

  return 0;
}

int setBufferingInformationsAccessUnit(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options,
  int64_t pts,
  int64_t dts
)
{
  EsmsPesPacketExtData param;
  uint64_t cpbRemovalTime, dpbOutputTime;

#if 0
  if (pts < 0)
    LIBBLU_H264_ERROR_RETURN("Unexpected negative PTS value.\n");
  if (dts < 0)
    LIBBLU_H264_ERROR_RETURN("Unexpected negative DTS value.\n");
#else
  pts = MAX(pts, 0);
  dts = MAX(dts, 0);
#endif

  /** Collecting CPB removal time and DPB output time of AU.
   * According to Rec. ITU-T H.222, if the AVC video stream provides
   * insufficient information to determine the CPB removal time and the
   * DPB output time of Access Units, these values shall be taken from,
   * respectively, DTS and PTS timestamps values of the looked AU.
   */
  if (!options.disableHrdVerifier) {
    cpbRemovalTime = handle->curProgParam.auCpbRemovalTime;
    dpbOutputTime = handle->curProgParam.auDpbOutputTime;
  }
  else
    cpbRemovalTime = (uint64_t) dts, dpbOutputTime = (uint64_t) pts;

  param = (EsmsPesPacketExtData) {
    .h264.cpbRemovalTime = cpbRemovalTime,
    .h264.dpbOutputTime  = dpbOutputTime
  };

  return setEsmsPesPacketExtensionData(
    handle->esms, param
  );
}

bool isStartOfANewH264AU(
  H264CurrentProgressParam * curState,
  uint8_t naluType
)
{
  bool startAUNaluType;

  if (!curState->reachVclNaluOfaPrimCodedPic) {
    /* No VCL NALU of primary coded picture as been reached. */
    return false;
  }

  /* Check the NALU type */
  startAUNaluType = false;
  switch (naluType) {
    case NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER:
    case NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET:
    case NAL_UNIT_TYPE_PIC_PARAMETERS_SET:
    case NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION:
    case NAL_UNIT_TYPE_PREFIX_NAL_UNIT:
      startAUNaluType = true;
      break;

    default:
      /* Check for special cases. */
      if (14 <= naluType && naluType <= 18)
        startAUNaluType = true;
  }

  if (startAUNaluType)
    curState->reachVclNaluOfaPrimCodedPic = false;

  return startAUNaluType;
}

int processAccessUnit(
  H264ParametersHandlerPtr handle,
  LibbluESParsingSettings * settings
)
{
  int ret;

  bool enoughData;
  size_t hrdAuLength;

  /* Check HRD Verifier */
  if (
    IN_USE_H264_CPB_HRD_VERIFIER(handle)
    && !handle->enoughDataToUseHrdVerifier
  ) {
    enoughData = true;

    if (!handle->sequenceParametersSetGopValid)
      enoughData = false;

    if (!handle->slicePresent)
      enoughData = false;

    if (!handle->sei.picTimingValid)
      enoughData = false;

    if (!handle->sei.bufferingPeriodGopValid)
      enoughData = false;

    if (!enoughData) {
      LIBBLU_H264_INFO(
        "Not enough data to process HRD verifier, cancellation of its use.\n"
      );
      destroyH264HrdVerifierContext(handle->hrdVerifier);
      handle->hrdVerifier = NULL;
      settings->options.disableHrdVerifier = true;
    }
  }

  if (IN_USE_H264_CPB_HRD_VERIFIER(handle)) {
    hrdAuLength =
      handle->curProgParam.curFrameLength
      - handle->hrdVerifier->nbProcessedBytes
    ;

    ret = processAUH264HrdContext(
      handle->hrdVerifier,
      &handle->sequenceParametersSet.data,
      &handle->slice.header,
      &handle->sei.picTiming,
      &handle->sei.bufferingPeriod,
      &handle->curProgParam,
      &handle->constraints,
      handle->sei.bufferingPeriodValid,
      hrdAuLength * 8,
      settings->options.doubleFrameTiming
    );
    if (ret < 0)
      return -1;

    handle->hrdVerifier->nbProcessedBytes += hrdAuLength;
  }

  /* Processing PES frame cutting */
  if (H264_COMPLETE_PICTURE(handle->curProgParam)) {
    if (processCompleteAccessUnit(handle, settings->options) < 0)
      return -1;

    if (IN_USE_H264_CPB_HRD_VERIFIER(handle))
      handle->hrdVerifier->nbProcessedBytes = 0;
  }
  else {
    handle->sei.bufferingPeriodValid = false;
    handle->sei.picTimingValid = false;
  }

  return 0;
}


int processCompleteAccessUnit(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  int ret;

  size_t curInsertingOffset, writtenBytes;

  H264SPSDataParameters * sps;
  H264HrdParameters * nalHrd;

  H264AUNalUnitPtr auNalUnitCell;
  unsigned auNalUnitCellIdx;

  int64_t pts, dts;

  LibbluESH264SpecProperties * param;

  assert(NULL != handle);

  /* Check Access Unit compliance : */
  if (
    !handle->accessUnitDelimiterValid
    || !handle->sequenceParametersSetGopValid
    || !handle->slicePresent
  ) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Uncompliant access unit, shall contain at least a AUD, "
      "a SPS and one slice.\n "
    );
  }

  sps = &handle->sequenceParametersSet.data;

  if (!sps->vuiParametersPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Missing VUI from AU SPS, unable to complete access unit.\n"
    );

  if (handle->curProgParam.curNbSlices < handle->constraints.sliceNb) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Pending access unit contains %u slices (at least %u expected).\n",
      handle->curProgParam.curNbSlices,
      handle->constraints.sliceNb
    );
  }

  if (H264_SLICE_IS_TYPE_B(handle->slice.header.sliceType)) {
    if (
      !handle->slice.header.fieldPic
      || handle->slice.header.bottomField
    )
      handle->curProgParam.curNbConsecutiveBPics++;

    if (
      handle->constraints.consecutiveBPicNb
      < handle->curProgParam.curNbConsecutiveBPics
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Too many consecutive B pictures "
        "(found %u pictures, expected no more than %u).\n",
        handle->curProgParam.curNbConsecutiveBPics,
        handle->constraints.consecutiveBPicNb
      );
    }
  }
  else
    handle->curProgParam.curNbConsecutiveBPics = 0;

  if (!handle->curProgParam.initializedParam) {
    /* Setting H.264 Output parameters : */
    handle->esms->prop.codingType = STREAM_CODING_TYPE_AVC;

    /* Convert to BDAV syntax. */
    handle->esms->prop.videoFormat = getHdmvVideoFormat(
      sps->FrameWidth,
      sps->FrameHeight,
      !sps->frameMbsOnly
    );
    if (0x00 == handle->esms->prop.videoFormat)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Resolution %ux%u%c is unsupported.\n",
        sps->FrameWidth,
        sps->FrameHeight,
        !sps->frameMbsOnly
      );

    handle->esms->prop.frameRate = getHdmvFrameRateCode(
      handle->curProgParam.frameRate
    );
    if (!handle->esms->prop.frameRate)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Frame-rate %.3f is unsupported.\n",
        handle->curProgParam.frameRate
      );

    handle->esms->prop.profileIDC = sps->profileIdc;
    handle->esms->prop.levelIDC = sps->levelIdc;

    param = handle->esms->fmtSpecProp.h264;
    param->constraintFlags = valueH264ContraintFlags(sps->constraintFlags);
    if (
      sps->vuiParametersPresent
      && sps->vuiParameters.nalHrdParamPresent
    ) {
      nalHrd = &sps->vuiParameters.nalHrdParam;
      param->cpbSize = nalHrd->schedSel[nalHrd->cpbCntMinus1].cpbSize;
      param->bitrate = nalHrd->schedSel[0].bitRate;
      handle->esms->bitrate = nalHrd->schedSel[0].bitRate;
    }
    else {
      /**
       * According to Rec. ITU-T H.222, if NAL hrd_parameters() are not present
       * in the AVC video stream, then cpb_size shall be the size
       * 1200 * MAXCPB[level_idc] for the level_idc of the video stream and
       * bit_rate inferred to be equal to default values for
       * BitRate[SchedSelIdx] based on profile and level defined in Rec. ITU-T
       * H.264 Annex E.
       */
      param->cpbSize = 1200 * handle->constraints.MaxCPB;
      handle->esms->bitrate = handle->constraints.brNal;
    }

    handle->curProgParam.frameDuration =
      MAIN_CLOCK_27MHZ / handle->curProgParam.frameRate
    ;
    handle->curProgParam.fieldDuration =
      handle->curProgParam.frameDuration / 2
    ;

    handle->curProgParam.lastDts = 0;
    handle->curProgParam.dtsIncrement = 0;

    handle->esms->ptsRef = (uint64_t) handle->curProgParam.frameDuration; /* BUG: Fix for still frames */
    handle->curProgParam.initializedParam = true;
  }

  if (handle->curProgParam.picOrderCntAU < 0) {
    LIBBLU_H264_ERROR_RETURN(
      "Broken picture order count, "
      "negative result (%" PRId64 ") on picture %ld.\n",
      handle->curProgParam.picOrderCntAU,
      handle->curProgParam.nbPics
    );
  }

  /* Based on tsMuxer */
  handle->curProgParam.picOrderCntAU++;

  dts =
    handle->curProgParam.lastDts
    + handle->curProgParam.dtsIncrement
  ;
  pts =
    dts
    + (
      (
        handle->curProgParam.picOrderCntAU
        / ((options.doubleFrameTiming) ? 1 : 2)
      )
      - handle->curProgParam.nbPics + 1
    )
    * handle->curProgParam.frameDuration
  ;

  LIBBLU_H264_DEBUG_AU_PROCESSING(" -> PTS: %" PRId64 "\n", pts);
  LIBBLU_H264_DEBUG_AU_PROCESSING(" -> DTS: %" PRId64 "\n", dts);

  switch (handle->curProgParam.lastPicStruct) {
    case H264_PIC_STRUCT_TOP_FLWD_BOT_TOP:
    case H264_PIC_STRUCT_BOT_FLWD_TOP_BOT:
      handle->curProgParam.dtsIncrement =
        handle->curProgParam.frameDuration
        + handle->curProgParam.fieldDuration;
      break;

    case H264_PIC_STRUCT_DOUBLED_FRAME:
      handle->curProgParam.dtsIncrement =
        2 * handle->curProgParam.frameDuration;
      break;

    case H264_PIC_STRUCT_TRIPLED_FRAME:
      handle->curProgParam.dtsIncrement =
        3 * handle->curProgParam.frameDuration;
      break;

    default:
      /* normal (or if pic_struct absent) */
      handle->curProgParam.dtsIncrement =
        handle->curProgParam.frameDuration;
  }

  handle->curProgParam.lastDts = dts;

  ret = initEsmsVideoPesFrame(
    handle->esms,
    handle->slice.header.sliceType,
    H264_SLICE_IS_TYPE_I(handle->slice.header.sliceType)
    || H264_SLICE_IS_TYPE_P(handle->slice.header.sliceType),
    pts, dts
  );
  if (ret < 0)
    return -1;

  if (setBufferingInformationsAccessUnit(handle, options, pts, dts) < 0)
    return -1;

  curInsertingOffset = 0;

  if (!handle->curProgParam.curFrameNbNalUnits)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected empty access unit, no NAL unit received.\n"
    );

  for (
    auNalUnitCellIdx = 0;
    auNalUnitCellIdx < handle->curProgParam.curFrameNbNalUnits;
    auNalUnitCellIdx++
  ) {
    auNalUnitCell =
      handle->curProgParam.curFrameNalUnits + auNalUnitCellIdx
    ;

    if (auNalUnitCell->replace) {
      assert(
        NULL != handle->curProgParam.curFrameNalUnits[
          auNalUnitCellIdx
        ].replacementParam
      );

      switch (auNalUnitCell->nalUnitType) {
#if 0
        case NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION:
          if (
            isH264SeiBufferingPeriodPatchMessage(
              (H264SeiRbspParameters *) auNalUnitCell->replacementParam
            )
          ) {
            writtenBytes = appendH264SeiBufferingPeriodPlaceHolder(
              handle,
              curInsertingOffset,
              (H264SeiRbspParameters *) auNalUnitCell->replacementParam
            );
            break;
          }

          writtenBytes = appendH264Sei(
            handle,
             curInsertingOffset,
            (H264SeiRbspParameters *) auNalUnitCell->replacementParam
          );
          break;
#endif

        case NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET:
          /* Update SPS */
          writtenBytes = appendH264SequenceParametersSet(
            handle,
            curInsertingOffset,
            (H264SPSDataParameters *) auNalUnitCell->replacementParam
          );
          break;

        default:
          LIBBLU_H264_ERROR_RETURN(
            "Unknown replacement NAL unit type code: 0x%" PRIx8 ".\n",
            auNalUnitCell->nalUnitType
          );
      }

      if (!writtenBytes)
        return -1; /* Zero byte written = Error. */

      /* TODO */
      curInsertingOffset += writtenBytes /* written bytes */;
      free(
        handle->curProgParam.curFrameNalUnits[
          auNalUnitCellIdx
        ].replacementParam
      );
    }
    else {
      assert(curInsertingOffset <= 0xFFFFFFFF);
      ret = appendAddPesPayloadCommand(
        handle->esms,
        handle->esmsSrcFileIdx,
        (uint32_t) curInsertingOffset,
        auNalUnitCell->startOffset,
        auNalUnitCell->length
      );
      if (ret < 0)
        return -1;

      curInsertingOffset += auNalUnitCell->length;
    }
  }

  /* Saving biggest picture AU size (for CPB computations): */
  if (handle->curProgParam.largestFrameSize < curInsertingOffset) {
    /* Biggest I picture AU. */
    if (H264_SLICE_IS_TYPE_I(handle->slice.header.sliceType))
      handle->curProgParam.largestIFrameSize = MAX(
        handle->curProgParam.largestIFrameSize,
        curInsertingOffset
      );

    /* Biggest picture AU. */
    handle->curProgParam.largestFrameSize = curInsertingOffset;
  }

  if (writeEsmsPesPacket(handle->esmsFile, handle->esms) < 0)
    return -1;

  /* Clean H264AUNalUnit : */
  if (handle->curProgParam.inProcessNalUnitCell) {
    /* Replacing current cell at list top. */
    handle->curProgParam.curFrameNalUnits[0] =
      handle->curProgParam.curFrameNalUnits[
        handle->curProgParam.curFrameNbNalUnits
      ]
    ;
  }
  handle->curProgParam.curFrameNbNalUnits = 0;
  handle->curProgParam.curFrameLength = 0;

  /* Reset settings for the next Access Unit : */
  resetH264ParametersHandler(handle);

  handle->esms->endPts = pts;

  return 0;
}

int decodeH264AccessUnitDelimiter(
  H264ParametersHandlerPtr handle
)
{
  /* access_unit_delimiter_rbsp() */
  int ret;
  bool isConstant;

  uint32_t value;

  H264AccessUnitDelimiterParameters accessUnitDelimiterParam;

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Access Unit Delimiter NAL unit type (receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  /* [u3 primary_pic_type] */
  if (readBitsNal(handle, &value, 3) < 0)
    return -1;
  accessUnitDelimiterParam.primaryPictType = value;

  /* rbsp_trailing_bits() */
  if (parseH264RbspTrailingBits(handle) < 0)
    return -1;

  if (
    !handle->accessUnitDelimiterPresent
    || !handle->accessUnitDelimiterValid
  ) {
    ret = checkH264AccessUnitDelimiterCompliance(
      accessUnitDelimiterParam
    );
    if (ret < 0)
      return -1;
  }
  else {
    if (5 < handle->curProgParam.lastNalUnitType) {
      isConstant = constantH264AccessUnitDelimiterCheck(
        handle->accessUnitDelimiter, accessUnitDelimiterParam
      );

      if (isConstant)
        handle->curProgParam.presenceOfUselessAccessUnitDelimiter = true;
      else {
        LIBBLU_H264_ERROR_RETURN(
          "Presence of an unexpected access unit delimiter NALU "
          "(Rec. ITU-T H.264 7.4.1.2.3 violation).\n"
        );
      }
    }
  }

  handle->accessUnitDelimiter = accessUnitDelimiterParam; /* Update */
  handle->accessUnitDelimiterPresent = true;
  handle->accessUnitDelimiterValid = true;

  return 0;
}

int buildScalingList(
  H264ParametersHandlerPtr handle,
  uint8_t * scalingList,
  const unsigned sizeOfList,
  bool * useDefaultScalingMatrix
)
{
  uint32_t value;
  unsigned i;
  int lastScale, deltaScale, nextScale;

  lastScale = 8, nextScale = 8;

  for (i = 0; i < sizeOfList; i++) {
    if (nextScale != 0) {
      /* [se delta_scale] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        return -1;
      deltaScale = (int) value;

      nextScale = (lastScale + deltaScale + 256) & 0xFF;
      *useDefaultScalingMatrix = (i == 0 && nextScale == 0);
    }

    scalingList[i] = (nextScale == 0) ? lastScale : nextScale;
    lastScale = scalingList[i];
  }

  return 0;
}

int parseH264HrdParameters(
  H264ParametersHandlerPtr handle,
  H264HrdParameters * param
)
{
  /* hrd_parameters() */
  /* Rec.ITU-T H.264 (06/2019) - Annex E */
  uint32_t value;
  unsigned ShedSelIdx;

  /* [ue cpb_cnt_minus1] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpbCntMinus1 = value;

  if (31 < param->cpbCntMinus1)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected cpb_cnt_minus1 value (%u, expected at least 31).\n",
      param->cpbCntMinus1
    );

  /* [u4 bit_rate_scale] */
  if (readBitsNal(handle, &value, 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->bitRateScale = value;

  /* [u4 cpb_size_scale] */
  if (readBitsNal(handle, &value, 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpbSizeScale = value;

  memset(
    param->schedSel,
    0x00,
    sizeof(H264SchedSel) * (param->cpbCntMinus1 + 1)
  );

  for (ShedSelIdx = 0; ShedSelIdx <= param->cpbCntMinus1; ShedSelIdx++) {
    /* [ue bit_rate_value_minus1[ShedSelIdx]] */
    if (readExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->schedSel[ShedSelIdx].bitRateValueMinus1 = value;

    param->schedSel[ShedSelIdx].bitRate =
      (uint64_t) (param->schedSel[ShedSelIdx].bitRateValueMinus1 + 1)
      << (6 + param->bitRateScale)
    ;

    /* [ue cpb_size_value_minus1[ShedSelIdx]] */
    if (readExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->schedSel[ShedSelIdx].cpbSizeValueMinus1 = value;

    param->schedSel[ShedSelIdx].cpbSize =
      ((uint64_t) (param->schedSel[ShedSelIdx].cpbSizeValueMinus1 + 1))
      << (4 + param->cpbSizeScale)
    ;

    /* [b1 cbr_flag[ShedSelIdx]] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->schedSel[ShedSelIdx].cbrFlag = value;
  }

  /* [u5 initial_cpb_removal_delay_length_minus1] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->initialCpbRemovalDelayLengthMinus1 = value;

  /* [u5 cpb_removal_delay_length_minus1] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpbRemovalDelayLengthMinus1 = value;

  /* [u5 dpb_output_delay_length_minus1] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->dpbOutputDelayLengthMinus1 = value;

  /* [u5 time_offset_length] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->timeOffsetLength = value;

  return 0;
}

int parseH264VuiParameters(
  H264ParametersHandlerPtr handle,
  H264VuiParameters * param
)
{
  /* vui_parameters() */
  /* Rec.ITU-T H.264 (06/2019) - Annex E */
  uint32_t value;

  /* [b1 aspect_ratio_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->aspectRatioInfoPresent = value;

  if (param->aspectRatioInfoPresent) {
    /* [u8 aspect_ratio_idc] */
    if (readBitsNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->aspectRatioIdc = value;

    if (param->aspectRatioIdc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
      /* [u16 sar_width] */
      if (readBitsNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->extendedSAR.width = value;

      /* [u16 sar_height] */
      if (readBitsNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->extendedSAR.height = value;
    }
    else
      param->extendedSAR.width = 0, param->extendedSAR.height = 0;
  }
  else {
    param->aspectRatioIdc = H264_ASPECT_RATIO_IDC_UNSPECIFIED;
    param->extendedSAR.width = 0, param->extendedSAR.height = 0;
  }

  /* [b1 overscan_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->overscanInfoPresent = value;

  if (param->overscanInfoPresent) {
    /* [b1 overscan_appropriate_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->overscanAppropriate = value;
  }
  else
    param->overscanAppropriate = false;

  /* [b1 video_signal_type_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->videoSignalTypePresent = value;

  if (param->videoSignalTypePresent) {
    /* [u3 video_format] */
    if (readBitsNal(handle, &value, 3) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->videoSignalType.videoFormat = value;

    /* [b1 video_full_range_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->videoSignalType.videoFullRange = value;

    /* [b1 colour_description_present_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->videoSignalType.colourDescPresent = value;

    if (param->videoSignalType.colourDescPresent) {
      /* [u8 colour_primaries] */
      if (readBitsNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->videoSignalType.colourDescription.colourPrimaries = value;

      /* [u8 transfer_characteristics] */
      if (readBitsNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->videoSignalType.colourDescription.transferCharact = value;

      /* [u8 matrix_coefficients] */
      if (readBitsNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->videoSignalType.colourDescription.matrixCoeff = value;
    }
    else {
      param->videoSignalType.colourDescription.colourPrimaries =
        H264_COLOR_PRIM_UNSPECIFIED;
      param->videoSignalType.colourDescription.transferCharact =
        H264_TRANS_CHAR_UNSPECIFIED;
      param->videoSignalType.colourDescription.matrixCoeff =
        H264_MATRX_COEF_UNSPECIFIED;
    }
  }
  else {
    param->videoSignalType.videoFormat =
      H264_VIDEO_FORMAT_UNSPECIFIED;
    param->videoSignalType.videoFullRange = false;
    param->videoSignalType.colourDescPresent = false;
    param->videoSignalType.colourDescription.colourPrimaries =
      H264_COLOR_PRIM_UNSPECIFIED;
    param->videoSignalType.colourDescription.transferCharact =
      H264_TRANS_CHAR_UNSPECIFIED;
    param->videoSignalType.colourDescription.matrixCoeff =
      H264_MATRX_COEF_UNSPECIFIED;
  }

  /* [b1 chroma_loc_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->chromaLocInfoPresent = value;

  if (param->chromaLocInfoPresent) {
    /* [ue chroma_sample_loc_type_top_field] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->ChromaLocInfo.sampleLocTypeTopField = value;

    /* [ue chroma_sample_loc_type_bottom_field] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->ChromaLocInfo.sampleLocTypeBottomField = value;
  }
  else
    param->ChromaLocInfo.sampleLocTypeTopField = 0,
    param->ChromaLocInfo.sampleLocTypeBottomField = 0
  ;

  /* [b1 timing_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->timingInfoPresent = value;

  if (param->timingInfoPresent) {
    /* [u32 num_units_in_tick] */
    if (readBitsNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->timingInfo.numUnitsInTick = value;

    /* [u32 time_scale] */
    if (readBitsNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->timingInfo.timeScale = value;

    /* [b1 fixed_frame_rate_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->timingInfo.fixedFrameRateFlag = value;

    param->timingInfo.frameRate =
      (double) param->timingInfo.timeScale / (
        param->timingInfo.numUnitsInTick * 2
      )
    ;

    param->timingInfo.frameRateCode = getHdmvFrameRateCode(
      (float) param->timingInfo.timeScale / (
        param->timingInfo.numUnitsInTick * 2
      )
    );

    param->timingInfo.maxFPS = DIV_ROUND_UP(
      param->timingInfo.timeScale,
      2 * param->timingInfo.numUnitsInTick
    );
  }

  /* [b1 nal_hrd_parameters_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->nalHrdParamPresent = value;

  if (param->nalHrdParamPresent) {
    if (parseH264HrdParameters(handle, &param->nalHrdParam) < 0)
      return -1;
  }

  /* [b1 vcl_hrd_parameters_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->vclHrdParamPresent = value;

  if (param->vclHrdParamPresent) {
    if (parseH264HrdParameters(handle, &param->vclHrdParam) < 0)
      return -1;
  }

  param->cpbDpbDelaysPresent =
    param->nalHrdParamPresent
    || param->vclHrdParamPresent
  ;

  if (param->cpbDpbDelaysPresent) {
    /* [b1 low_delay_hrd_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->lowDelayHrd = value;
  }

  /* [b1 pic_struct_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->picStructPresent = value;

  /* [b1 bitstream_restriction_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->bitstreamRestrictionsPresent = value;

  if (param->bitstreamRestrictionsPresent) {
    /* [b1 motion_vectors_over_pic_boundaries_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.motionVectorsOverPicBoundaries = value;

    /* [ue max_bytes_per_pic_denom] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.maxBytesPerPicDenom = value;

    /* [ue max_bits_per_mb_denom] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.maxBitsPerPicDenom = value;

    /* [ue log2_max_mv_length_horizontal] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.log2MaxMvLengthHorizontal = value;

    /* [ue log2_max_mv_length_vertical] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.log2MaxMvLengthVertical = value;

    /* [ue max_num_reorder_frames] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.maxNumReorderFrames = value;

    /* [ue max_dec_frame_buffering] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistreamRestrictions.maxDecFrameBuffering = value;
  }
  else {
    /* Default values : */
    param->bistreamRestrictions.motionVectorsOverPicBoundaries = true;
    param->bistreamRestrictions.maxBytesPerPicDenom = 2;
    param->bistreamRestrictions.maxBitsPerPicDenom = 1;
    param->bistreamRestrictions.log2MaxMvLengthHorizontal = 15;
    param->bistreamRestrictions.log2MaxMvLengthVertical = 15;
  }

  return 0;
}

int parseH264SequenceParametersSetData(
  H264ParametersHandlerPtr handle,
  H264SPSDataParameters * param
)
{
  /* seq_parameter_set_data() */
  int ret;

  uint32_t value;
  unsigned i;

  assert(NULL != handle);

  /* [u8 profile_idc] */
  if (readBitsNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->profileIdc = value;

  /* Constraints flags : */
  /* [b1 constraint_set0_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraintFlags.set0 = value;

  /* [b1 constraint_set1_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraintFlags.set1 = value;

  /* [b1 constraint_set2_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraintFlags.set2 = value;

  /* [b1 constraint_set3_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraintFlags.set3 = value;

  /* [b1 constraint_set4_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraintFlags.set4 = value;

  /* [b1 constraint_set5_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraintFlags.set5 = value;

  /* [v2 reserved_zero_2bits] // 0b00 */
  if (readBitsNal(handle, &value, 2) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  /* Extra constraints flags : */
  param->constraintFlags.reservedFlags = value;

  /* [u8 level_idc] */
  if (readBitsNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->levelIdc = value;

  /* [ue seq_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->seqParametersSetId = value;

  if (H264_PROFILE_IS_HIGH(param->profileIdc)) {
    /* High profiles linked-properties */

    /* [ue chroma_format_idc] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->chromaFormat = value;

    if (param->chromaFormat == H264_CHROMA_FORMAT_444) {
      /* [b1 separate_colour_plane_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->separateColourPlaneFlag444 = value;
    }
    else
      param->separateColourPlaneFlag444 = false;

    /* [ue bit_depth_luma_minus8] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bitDepthLumaMinus8 = value;

    /* [ue bit_depth_chroma_minus8] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bitDepthChromaMinus8 = value;

    /* [b1 qpprime_y_zero_transform_bypass_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->qpprimeYZeroTransformBypass = value;

    /* [b1 seq_scaling_matrix_present_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->seqScalingMatrixPresent = value;

    if (param->seqScalingMatrixPresent) {
      /* Initialization for comparison checks purposes : */
      memset(&param->seqScalingMatrix, 0x0, sizeof(H264SeqScalingMatrix));

      for (i = 0; i < ((param->chromaFormat != H264_CHROMA_FORMAT_444) ? 8 : 12); i++) {
        /* [b1 seq_scaling_list_present_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->seqScalingMatrix.seqScalingListPresent[i] = value;

        if (param->seqScalingMatrix.seqScalingListPresent[i]) {
          if (i < 6) {
            ret = buildScalingList(
              handle,
              param->seqScalingMatrix.scalingList4x4[i],
              16,
              &(param->seqScalingMatrix.useDefaultScalingMatrix4x4[i])
            );
          }
          else {
            ret = buildScalingList(
              handle,
              param->seqScalingMatrix.scalingList8x8[i - 6],
              64,
              &(param->seqScalingMatrix.useDefaultScalingMatrix8x8[i - 6])
            );
          }
          if (ret < 0)
            return -1;
        }
      }
    }
  }
  else {
    /* Default values (according to specs): */
    param->chromaFormat = H264_CHROMA_FORMAT_420;
    param->separateColourPlaneFlag444 = false;
    param->bitDepthLumaMinus8 = 0;
    param->bitDepthChromaMinus8 = 0;
    param->qpprimeYZeroTransformBypass = 0;
    param->seqScalingMatrixPresent = false;
  }

  /* [ue log2_max_frame_num_minus4] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->log2MaxFrameNumMinus4 = value;
  param->MaxFrameNum = 1 << (param->log2MaxFrameNumMinus4 + 4);

  /* [ue pic_order_cnt_type] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->picOrderCntType = value;

  if (2 < param->picOrderCntType)
    LIBBLU_H264_ERROR_RETURN(
      "Unknown/Reserved pic_order_cnt_type = %u.\n",
      param->picOrderCntType
    );

  /* Set default values (for test purposes) : */
  param->log2MaxPicOrderCntLsbMinus4 = 0;
  param->deltaPicOrderAlwaysZero = 0;
  param->offsetForNonRefPic = 0;
  param->offsetForTopToBottomField = 0;
  param->numRefFramesInPicOrderCntCycle = 0;

  if (param->picOrderCntType == 0) {
    /* [ue log2_max_pic_order_cnt_lsb_minus4] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->log2MaxPicOrderCntLsbMinus4 = value;
  }
  else if (param->picOrderCntType == 2) {
    /* [b1 delta_pic_order_always_zero_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->deltaPicOrderAlwaysZero = value;

    /* [se offset_for_non_ref_pic] */
    if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->offsetForNonRefPic = (int) value;

    /* [se offset_for_top_to_bottom_field] */
    if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->offsetForTopToBottomField = (int) value;

    /* [se num_ref_frames_in_pic_order_cnt_cycle] */
    if (readSignedExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->numRefFramesInPicOrderCntCycle = value;

    if (255 < param->numRefFramesInPicOrderCntCycle)
      LIBBLU_H264_ERROR_RETURN(
        "'num_ref_frames_in_pic_order_cnt_cycle' = %u causes overflow "
        "(shall not exceed 255).\n",
        value
      );

    for (i = 0; i < param->numRefFramesInPicOrderCntCycle; i++) {
      /* [se offset_for_ref_frame] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->offsetForRefFrame[i] = (int) value;
    }
  }

  /* [ue max_num_ref_frames] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->maxNumRefFrames = value;

  /* [b1 gaps_in_frame_num_value_allowed_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->gapsInFrameNumValueAllowed = value;

  /* [ue pic_width_in_mbs_minus1] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->picWidthInMbsMinus1 = value;

  /* [ue pic_height_in_map_units_minus1] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->picHeightInMapUnitsMinus1 = value;

  /* [b1 frame_mbs_only_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->frameMbsOnly = value;

  if (!param->frameMbsOnly) {
    /* [b1 mb_adaptive_frame_field_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->mbAdaptativeFrameField = value;
  }
  else
    param->mbAdaptativeFrameField = false;

  /* [b1 direct_8x8_inference_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->direct8x8Interference = value;

  /* [b1 frame_cropping_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->frameCropping = value;

  if (param->frameCropping) {
    /* [ue frame_crop_left_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frameCropOffset.left = value;

    /* [ue frame_crop_right_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frameCropOffset.right = value;

    /* [ue frame_crop_top_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frameCropOffset.top = value;

    /* [ue frame_crop_bottom_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frameCropOffset.bottom = value;
  }
  else {
    param->frameCropOffset.left = 0;
    param->frameCropOffset.right = 0;
    param->frameCropOffset.top = 0;
    param->frameCropOffset.bottom = 0;
  }

  /* [b1 vui_parameters_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->vuiParametersPresent = value;

  if (param->vuiParametersPresent) {
    if (parseH264VuiParameters(handle, &param->vuiParameters) < 0)
      return -1;
  }

  if (
    param->vuiParametersPresent
    && param->vuiParameters.timingInfoPresent
  ) {
    /* Frame rate information present */
    handle->curProgParam.frameRate =
      param->vuiParameters.timingInfo.frameRate;
  }
  else
    handle->curProgParam.frameRate = -1;

  /* Derivate values computation : */
  switch (param->chromaFormat) {
    case H264_CHROMA_FORMAT_400:
    case H264_CHROMA_FORMAT_444:
      param->SubWidthC = 1;
      param->SubHeightC = 1;
      break;

    case H264_CHROMA_FORMAT_422:
      param->SubWidthC = 2;
      param->SubHeightC = 1;
      break;

    case H264_CHROMA_FORMAT_420:
      param->SubWidthC = 2;
      param->SubHeightC = 2;
  }

  /* Computed values : */
  param->BitDepthLuma = param->bitDepthLumaMinus8 + 8;
  param->QpBdOffsetLuma = param->bitDepthLumaMinus8 * 6;

  param->BitDepthChroma = param->bitDepthChromaMinus8 + 8;
  param->QpBdOffsetChroma = param->bitDepthChromaMinus8 * 6;

  param->ChromaArrayType =
    (param->separateColourPlaneFlag444) ? 0 : param->chromaFormat
  ;

  param->MbWidthC = 16 / param->SubWidthC;
  param->MbHeightC = 16 / param->SubHeightC;

  param->RawMbBits =
    (uint64_t) 256 * param->BitDepthLuma
    + (uint64_t) 2 * param->MbWidthC * param->MbHeightC * param->BitDepthChroma
  ;

  if (param->picOrderCntType == 0) {
    param->MaxPicOrderCntLsb = 1 << (param->log2MaxPicOrderCntLsbMinus4 + 4);
  }
  else {
    param->ExpectedDeltaPerPicOrderCntCycle = 0;
    for (i = 0; i < param->numRefFramesInPicOrderCntCycle; i++)
      param->ExpectedDeltaPerPicOrderCntCycle += param->offsetForRefFrame[i];
  }

  param->PicWidthInMbs = param->picWidthInMbsMinus1 + 1;
  param->PicWidthInSamplesL = param->PicWidthInMbs * 16;
  param->PicWidthInSamplesC = param->PicWidthInMbs * param->MbWidthC;

  param->PicHeightInMapUnits = param->picHeightInMapUnitsMinus1 + 1;

  param->FrameHeightInMbs =
    (2 - param->frameMbsOnly) * param->PicHeightInMapUnits
  ;

  if (param->ChromaArrayType == 0) {
    param->CropUnitX = 1;
    param->CropUnitY = 2 - param->frameMbsOnly;
  }
  else {
    param->CropUnitX = param->SubWidthC;
    param->CropUnitY = param->SubHeightC * (2 - param->frameMbsOnly);
  }

  param->FrameWidth =
    param->PicWidthInMbs * 16
    - param->CropUnitX * (
      param->frameCropOffset.left + param->frameCropOffset.right
    )
  ;
  param->FrameHeight =
    param->FrameHeightInMbs * 16
    - param->CropUnitY * (
      param->frameCropOffset.top + param->frameCropOffset.bottom
    )
  ;

  return 0;
}

int decodeH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  int ret;

  bool constantSps;

  /* seq_parameter_set_rbsp() */
  H264SPSDataParameters seqParamSetDataParam;

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Sequence Parameters Set NAL unit type (receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  /* seq_parameter_set_data() */
  if (parseH264SequenceParametersSetData(handle, &seqParamSetDataParam) < 0)
    return -1;

  /* rbsp_trailing_bits() */
  if (parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* Check */
  if (!handle->accessUnitDelimiterValid)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Missing mandatory AUD before SPS.\n"
    );

  ret = 0;
  if (!handle->sequenceParametersSetPresent) {
    ret = checkH264SequenceParametersSetCompliance(
      seqParamSetDataParam,
      handle
    );
    if (ret < 0)
      return -1;

    if (checkH264SpsBitstreamCompliance(seqParamSetDataParam, options) < 0)
      return -1;
  }
  else {
    constantSps = constantH264SequenceParametersSetCheck(
      handle->sequenceParametersSet.data,
      seqParamSetDataParam
    );

    if (!constantSps) {
      ret = checkH264SequenceParametersSetChangeCompliance(
        handle->sequenceParametersSet.data,
        seqParamSetDataParam
      );
      if (ret < 0)
        LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
          "Fluctuating SPS parameters.\n"
        );
    }
    else if (handle->sequenceParametersSetValid)
      handle->curProgParam.presenceOfUselessSequenceParameterSet = true;
  }

  handle->sequenceParametersSetPresent = true;
  handle->sequenceParametersSetGopValid = true;
  handle->sequenceParametersSetValid = true; /* All checks succeed. */
  handle->sequenceParametersSet.data = seqParamSetDataParam; /* Update */

  return 0;
}

int decodeH264PicParametersSet(H264ParametersHandlerPtr handle)
{
  /* pic_parameter_set_rbsp() */
  int ret;

  uint32_t value;
  unsigned i, iGroup, ppsId;

  bool constantPps;
  bool updatePps;

  H264PicParametersSetSliceGroupsParameters * ppsSG;
  H264SequenceParametersSetDataParameters * spsData;

  H264PicParametersSetParameters picParamSetDataParam; /* Output structure */

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_PIC_PARAMETERS_SET)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Picture Parameters Set NAL unit type (receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  if (!handle->accessUnitDelimiterPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory AUD NALU before PPS NALU.\n"
    );

  if (!handle->sequenceParametersSetPresent) {
    /* Required or 'chroma_format_idc' */
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory SPS NALU before PPS NALU.\n"
    );
  }

  /* [ue pic_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.picParametersSetId = value;

  if (H264_MAX_PPS <= picParamSetDataParam.picParametersSetId)
    LIBBLU_H264_ERROR_RETURN(
      "PPS pic_parameter_set_id value overflow.\n"
    );

  /* [ue seq_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.seqParametersSetId = value;

  /* [b1 entropy_coding_mode_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.entropyCodingMode = value;

  /* [b1 bottom_field_pic_order_in_frame_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.bottomFieldPicOrderInFramePresent = value;

  /* [ue num_slice_groups_minus1] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.nbSliceGroups = value + 1;

  if (H264_MAX_ALLOWED_SLICE_GROUPS < picParamSetDataParam.nbSliceGroups)
    LIBBLU_H264_ERROR_RETURN(
      "Number of slice groups exceed maximum value supported (%u < %u).\n",
      H264_MAX_ALLOWED_SLICE_GROUPS,
      picParamSetDataParam.nbSliceGroups
    );

  if (1 < picParamSetDataParam.nbSliceGroups) {
    /* Slices split in separate groups, need definitions. */
    ppsSG = &picParamSetDataParam.sliceGroups;

    /* [ue slice_group_map_type] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    ppsSG->mapType = value;

    switch (ppsSG->mapType) {
      case H264_SLICE_GROUP_TYPE_INTERLEAVED:
        for (
          iGroup = 0;
          iGroup < picParamSetDataParam.nbSliceGroups;
          iGroup++
        ) {
          /* [ue run_length_minus1[iGroup]] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          ppsSG->data.runLengthMinus1[iGroup] = value;
        }
        break;

      case H264_SLICE_GROUP_TYPE_DISPERSED:
        break;

      case H264_SLICE_GROUP_TYPE_FOREGROUND_LEFTOVER:
        for (
          iGroup = 0;
          iGroup < picParamSetDataParam.nbSliceGroups;
          iGroup++
        ) {
          /* [ue top_left[iGroup]] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          ppsSG->data.topLeft[iGroup] = value;

          /* [ue top_left[iGroup]] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          ppsSG->data.bottomRight[iGroup] = value;
        }
        break;

      case H264_SLICE_GROUP_TYPE_CHANGING_1:
      case H264_SLICE_GROUP_TYPE_CHANGING_2:
      case H264_SLICE_GROUP_TYPE_CHANGING_3:
        /* [b1 slice_group_change_direction_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        ppsSG->data.changeDirection = value;

        /* [ue slice_group_change_rate_minus1] */
        if (readExpGolombCodeNal(handle, &value, 32) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        ppsSG->data.changeRateMinus1 = value;
        break;

      case H264_SLICE_GROUP_TYPE_EXPLICIT:
        /* [ue pic_size_in_map_units_minus1] */
        if (readExpGolombCodeNal(handle, &value, 32) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        ppsSG->data.picSizeInMapUnitsMinus1 = value;

        for (i = 0; i <= ppsSG->data.picSizeInMapUnitsMinus1; i++) {
          /* [u<ceil(log2(pic_size_in_map_units_minus1 + 1))> slice_group_id] */
          /* TODO: Optimize computation of the field size (using look-up tables ?) */
          if (skipBitsNal(handle, ceil(log2(ppsSG->data.picSizeInMapUnitsMinus1 + 1))) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
        }
        break;

      default:
        LIBBLU_H264_ERROR_RETURN(
          "Unknown slice groups 'slice_group_map_type' == %d value in PPS.\n",
          ppsSG->mapType
        );
    }
  }

  /* [ue num_ref_idx_l0_default_active_minus1] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.numRefIdxl0DefaultActiveMinus1 = value;

  /* [ue num_ref_idx_l1_default_active_minus1] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.numRefIdxl1DefaultActiveMinus1 = value;

  /* [b1 weighted_pred_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.weightedPred = value;

  /* [u2 weighted_bipred_idc] */
  if (readBitsNal(handle, &value, 2) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.weightedBipredIdc = value;

  if (
    picParamSetDataParam.weightedBipredIdc
    == H264_WEIGHTED_PRED_B_SLICES_RESERVED
  )
    LIBBLU_H264_ERROR_RETURN(
      "Reserved 'weighted_bipred_idc' == 3 value in PPS.\n"
    );

  /* [se pic_init_qp_minus26] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.picInitQpMinus26 = (int) value;

  /* [se pic_init_qs_minus26] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.picInitQsMinus26 = (int) value;

  /* [se chroma_qp_index_offset] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.chromaQpIndexOff = (int) value;

  /* [b1 deblocking_filter_control_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.deblockingFilterControlPresent = value;

  /* [b1 constrained_intra_pred_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.contraintIntraPred = value;

  /* [b1 redundant_pic_cnt_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  picParamSetDataParam.redundantPicCntPresent = value;

  picParamSetDataParam.picParamExtensionPresent = moreRbspDataNal(handle);

  if (picParamSetDataParam.picParamExtensionPresent) {
    /* [b1 transform_8x8_mode_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    picParamSetDataParam.transform8x8Mode = value;

    /* [b1 pic_scaling_matrix_present_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    picParamSetDataParam.picScalingMatrixPresent = value;

    if (picParamSetDataParam.picScalingMatrixPresent) {
      spsData = &handle->sequenceParametersSet.data;
      picParamSetDataParam.nbScalingMatrix =
        6
        + ((spsData->chromaFormat != H264_CHROMA_FORMAT_444) ? 2 : 6)
        * picParamSetDataParam.transform8x8Mode
      ;

      for (i = 0; i < 12; i++) {
        /* Initialization for comparison checks purposes */
        picParamSetDataParam.picScalingListPresent[i] = false;
      }

      for (i = 0; i < 6; i++) {
        picParamSetDataParam.useDefaultScalingMatrix4x4[i] = true;
        picParamSetDataParam.useDefaultScalingMatrix8x8[i] = true;
      }

      for (i = 0; i < picParamSetDataParam.nbScalingMatrix; i++) {
        /* [b1 pic_scaling_list_present_flag[i]] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        picParamSetDataParam.picScalingListPresent[i] = value;

        if (picParamSetDataParam.picScalingListPresent[i]) {
          if (i < 6) {
            ret = buildScalingList(
              handle,
              picParamSetDataParam.scalingList4x4[i],
              16,
              picParamSetDataParam.useDefaultScalingMatrix4x4 + i
            );
          }
          else {
            ret = buildScalingList(
              handle,
              picParamSetDataParam.scalingList8x8[i - 6],
              64,
              picParamSetDataParam.useDefaultScalingMatrix4x4 + (i - 6)
            );
          }
          if (ret < 0)
            return -1;
        }
      }
    }
    else
      picParamSetDataParam.nbScalingMatrix = 0;

    /* [se second_chroma_qp_index_offset] */
    if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    picParamSetDataParam.secondChromaQpIndexOff = (int) value;
  }
  else {
    /* Default values if end of Rbsp. */
    picParamSetDataParam.transform8x8Mode = false;
    picParamSetDataParam.picScalingMatrixPresent = false;
    picParamSetDataParam.secondChromaQpIndexOff =
      picParamSetDataParam.chromaQpIndexOff
    ;
  }

  /* rbsp_trailing_bits() */
  if (parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* Checks : */
  ppsId = picParamSetDataParam.picParametersSetId; /* For readability */
  updatePps = false;

  if (H264_MAX_PPS <= ppsId)
    LIBBLU_ERROR_RETURN("Internal error, unexpected PPS id %u.\n", ppsId);

  if (!handle->picParametersSetIdsPresent[ppsId]) {
    /* Newer PPS */
    ret = checkH264PicParametersSetCompliance(
      picParamSetDataParam,
      handle
    );
    if (ret < 0)
      return -1;

    handle->picParametersSetIdsPresentNb++;
    handle->picParametersSetIdsPresent[ppsId] = true;
    updatePps = true;
  }
  else {
    /* Already present PPS */
    constantPps = constantH264PicParametersSetCheck(
      *(handle->picParametersSet[ppsId]),
      picParamSetDataParam
    );

    if (!constantPps) {
      /* PPS if unconstant, check if changes are allowed */
      ret = checkH264PicParametersSetChangeCompliance(
        *(handle->picParametersSet[ppsId]),
        picParamSetDataParam,
        handle
      );
      if (ret < 0)
        LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
          "Forbidden PPS parameters change.\n"
        );

      updatePps = true;
    }
  }

  if (updatePps) {
    if (updatePPSH264Parameters(handle, &picParamSetDataParam, ppsId) < 0)
      return -1;
  }

  handle->picParametersSetIdsValid[ppsId] = true;

  return 0;
}

int parseH264SeiBufferingPeriod(
  H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod * param
)
{
  /* buffering_period(payloadSize) - Annex D.1.2 */
  uint32_t value;
  unsigned SchedSelIdx;

  H264SPSDataParameters * spsData;
  H264HrdParameters * hrd;

  assert(NULL != handle);
  assert(NULL != param);

  if (
    !handle->sequenceParametersSetPresent
    || !handle->sequenceParametersSet.data.vuiParametersPresent
  )
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory VUI parameters in SPS, "
      "Buffering Period SEI message shouldn't be present.\n"
    );

  spsData = &handle->sequenceParametersSet.data;

#if 0 /* Verification in checks */
  if (
    !spsData->vuiParameters.nalHrdParamPresent
    && !spsData->vuiParameters.vclHrdParamPresent
  )
    LIBBLU_H264_ERROR_RETURN(
      "NalHrdBpPresentFlag and VclHrdBpPresentFlag are equal to 0b0, "
      "so Buffering Period SEI message shouldn't be present.\n"
    );
#endif

  /* Pre-init values for check purpose : */
  memset(
    param->nalHrdParam, 0,
    sizeof(uint32_t) * H264_MAX_CPB_CONFIGURATIONS * 2
  );
  memset(
    param->vclHrdParam, 0,
    sizeof(uint32_t) * H264_MAX_CPB_CONFIGURATIONS * 2
  );

  /* [ue seq_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->seqParametersSetId = value;

  if (spsData->vuiParameters.nalHrdParamPresent) {
    hrd = &spsData->vuiParameters.nalHrdParam;

    for (SchedSelIdx = 0; SchedSelIdx <= hrd->cpbCntMinus1; SchedSelIdx++) {
      /* [u<initial_cpb_removal_delay_length_minus1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (
        readBitsNal(
          handle, &value, hrd->initialCpbRemovalDelayLengthMinus1 + 1
        ) < 0
      )
        LIBBLU_H264_READING_FAIL_RETURN();
      param->nalHrdParam[SchedSelIdx].initialCpbRemovalDelay = value;

      /* [u<initial_cpb_removal_delay_length_minus1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (
        readBitsNal(
          handle, &value, hrd->initialCpbRemovalDelayLengthMinus1 + 1
        ) < 0
      )
        LIBBLU_H264_READING_FAIL_RETURN();
      param->nalHrdParam[SchedSelIdx].initialCpbRemovalDelayOff = value;
    }
  }

  if (spsData->vuiParameters.vclHrdParamPresent) {
    hrd = &spsData->vuiParameters.vclHrdParam;

    for (SchedSelIdx = 0; SchedSelIdx <= hrd->cpbCntMinus1; SchedSelIdx++) {
      /* [u<initial_cpb_removal_delay_length_minus1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (
        readBitsNal(
          handle, &value, hrd->initialCpbRemovalDelayLengthMinus1 + 1
        ) < 0
      )
        LIBBLU_H264_READING_FAIL_RETURN();
      param->vclHrdParam[SchedSelIdx].initialCpbRemovalDelay = value;

      /* [u<initial_cpb_removal_delay_length_minus1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (
        readBitsNal(
          handle, &value, hrd->initialCpbRemovalDelayLengthMinus1 + 1
        ) < 0
      )
        LIBBLU_H264_READING_FAIL_RETURN();
      param->vclHrdParam[SchedSelIdx].initialCpbRemovalDelayOff = value;
    }
  }

  return 0;
}

int parseH264SeiPicTiming(
  H264ParametersHandlerPtr handle,
  H264SeiPicTiming * param
)
{
  /* pic_timing(payloadSize) - Annex D.1.3 */
  uint32_t value;
  unsigned picIdx;

  size_t cpbRemovalDelayFieldLength;
  size_t dpbOutputDelayFieldLength;
  size_t timeOffsetFieldLength;

  H264SPSDataParameters * spsData;

  assert(NULL != handle);
  assert(NULL != param);

  if (
    !handle->sequenceParametersSetPresent
    || !handle->sequenceParametersSet.data.vuiParametersPresent
  )
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory VUI parameters, "
      "Pic Timing SEI message shouldn't be present.\n"
    );

  spsData = &handle->sequenceParametersSet.data;

  if (
    !spsData->vuiParameters.cpbDpbDelaysPresent
    && !spsData->vuiParameters.picStructPresent
  )
    LIBBLU_H264_ERROR_RETURN(
      "CpbDpbDelaysPresentFlag and pic_struct_present_flag are equal to 0b0, "
      "so Pic Timing SEI message shouldn't be present."
    );

  /* Recovering cpb_removal_delay_length_minus1 and dpb_output_delay_length_minus1 values. */
  if (spsData->vuiParameters.nalHrdParamPresent) {
    cpbRemovalDelayFieldLength =
      spsData->vuiParameters.nalHrdParam.cpbRemovalDelayLengthMinus1 + 1;
    dpbOutputDelayFieldLength =
      spsData->vuiParameters.nalHrdParam.dpbOutputDelayLengthMinus1 + 1;
    timeOffsetFieldLength =
      spsData->vuiParameters.nalHrdParam.timeOffsetLength;
  }
  else if (spsData->vuiParameters.vclHrdParamPresent) {
    cpbRemovalDelayFieldLength =
      spsData->vuiParameters.vclHrdParam.cpbRemovalDelayLengthMinus1 + 1;
    dpbOutputDelayFieldLength =
      spsData->vuiParameters.vclHrdParam.dpbOutputDelayLengthMinus1 + 1;
    timeOffsetFieldLength =
      spsData->vuiParameters.vclHrdParam.timeOffsetLength;
  }
  else { /* Default values, shall not happen */
    cpbRemovalDelayFieldLength = 24;
    dpbOutputDelayFieldLength = 24;
    timeOffsetFieldLength = 24;
  }

  if (spsData->vuiParameters.cpbDpbDelaysPresent) {
    /* [u<initial_cpb_removal_delay_length_minus1> cpb_removal_delay] */
    if (readBitsNal(handle, &value, cpbRemovalDelayFieldLength) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->cpbRemovalDelay = value;

    /* [u<initial_cpb_removal_delay_length_minus1> dpb_output_delay] */
    if (readBitsNal(handle, &value, dpbOutputDelayFieldLength) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->dpbOutputDelay = value;
  }

  if (spsData->vuiParameters.picStructPresent) {
    /* [u4 pic_struct] */
    if (readBitsNal(handle, &value, 4) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->picStruct = value;

    handle->curProgParam.lastPicStruct = value;

    switch (param->picStruct) {
      case H264_PIC_STRUCT_FRAME:
      case H264_PIC_STRUCT_TOP_FIELD:
      case H264_PIC_STRUCT_BOTTOM_FIELD:
        param->numClockTS = 1;
        break;

      case H264_PIC_STRUCT_TOP_FLWD_BOTTOM:
      case H264_PIC_STRUCT_BOTTOM_FLWD_TOP:
      case H264_PIC_STRUCT_DOUBLED_FRAME:
        param->numClockTS = 2;
        break;

      case H264_PIC_STRUCT_TOP_FLWD_BOT_TOP:
      case H264_PIC_STRUCT_BOT_FLWD_TOP_BOT:
      case H264_PIC_STRUCT_TRIPLED_FRAME:
        param->numClockTS = 3;
        break;

      default:
        LIBBLU_ERROR_RETURN(
          "Reserved 'pic_struct' == %d value in Picture Timing SEI message.\n",
          param->picStruct
        );
    }

    memset(
      param->clockTimestampParam, 0x00,
      sizeof(H264ClockTimestampParam) * param->numClockTS
    );

    for (picIdx = 0; picIdx < param->numClockTS; picIdx++) {
      /* [b1 clock_timestamp_flag[picIdx]] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->clockTimestampPresent[picIdx] = value;

      if (param->clockTimestampPresent[picIdx]) {
        /* [u2 ct_type] */
        if (readBitsNal(handle, &value, 2) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->clockTimestampParam[picIdx].ctType = value;

        /* [b1 nuit_field_based_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->clockTimestampParam[picIdx].nuitFieldBased = value;

        /* [u5 counting_type] */
        if (readBitsNal(handle, &value, 5) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->clockTimestampParam[picIdx].countingType = value;

        /* [b1 full_timestamp_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[picIdx].fullTimestamp = value;

        /* [b1 discontinuity_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[picIdx].discontinuity = value;

        /* [b1 cnt_dropped_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[picIdx].cntDropped = value;

        /* [u8 n_frames] */
        if (readBitsNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[picIdx].nFrames = value;

        param->ClockTimestamp[picIdx].secondsValuePresent = false;
        param->ClockTimestamp[picIdx].minutesValuePresent = false;
        param->ClockTimestamp[picIdx].hoursValuePresent = false;

        if (param->ClockTimestamp[picIdx].fullTimestamp) {
          param->ClockTimestamp[picIdx].secondsValuePresent = true;
          param->ClockTimestamp[picIdx].minutesValuePresent = true;
          param->ClockTimestamp[picIdx].hoursValuePresent = true;

          /* [u6 seconds_value] */
          if (readBitsNal(handle, &value, 6) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[picIdx].secondsValue = value;

          /* [u6 minutes_value] */
          if (readBitsNal(handle, &value, 6) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[picIdx].minutesValue = value;

          /* [u5 hours_value] */
          if (readBitsNal(handle, &value, 5) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[picIdx].hoursValue = value;
        }
        else {
          param->ClockTimestamp[picIdx].secondsValue = 0;
          param->ClockTimestamp[picIdx].minutesValue = 0;
          param->ClockTimestamp[picIdx].hoursValue = 0;

          /* [b1 seconds_flag] */
          if (readBitsNal(handle, &value, 1) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[picIdx].secondsValuePresent = value;

          if (param->ClockTimestamp[picIdx].secondsValuePresent) {
            /* [u6 seconds_value] */
            if (readBitsNal(handle, &value, 6) < 0)
              LIBBLU_H264_READING_FAIL_RETURN();
            param->ClockTimestamp[picIdx].secondsValue = value;

            /* [b1 minutes_flag] */
            if (readBitsNal(handle, &value, 1) < 0)
              LIBBLU_H264_READING_FAIL_RETURN();
            param->ClockTimestamp[picIdx].minutesValuePresent = value;

            if (param->ClockTimestamp[picIdx].minutesValuePresent) {
              /* [u6 minutes_value] */
              if (readBitsNal(handle, &value, 6) < 0)
                LIBBLU_H264_READING_FAIL_RETURN();
              param->ClockTimestamp[picIdx].minutesValue = value;

              /* [b1 hours_flag] */
              if (readBitsNal(handle, &value, 1) < 0)
                LIBBLU_H264_READING_FAIL_RETURN();
              param->ClockTimestamp[picIdx].hoursValuePresent = value;

              if (param->ClockTimestamp[picIdx].hoursValuePresent) {
                /* [u5 minutes_value] */
                if (readBitsNal(handle, &value, 5) < 0)
                  LIBBLU_H264_READING_FAIL_RETURN();
                param->ClockTimestamp[picIdx].hoursValue = value;
              }
            }
          }
        }

        if (0 < timeOffsetFieldLength) {
          /* [u<time_offset_length> time_offset] */
          if (readBitsNal(handle, &value, timeOffsetFieldLength) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[picIdx].timeOffset = value;
        }
        else
          param->ClockTimestamp[picIdx].timeOffset = 0;
      }
    }
  }

  return 0;
}

int parseH264SeiUserDataUnregistered(
  H264ParametersHandlerPtr handle,
  H264SeiUserDataUnregistered * param,
  const size_t payloadSize
)
{
  /* user_data_unregistered(payloadSize) - Annex D.1.7 */

  assert(NULL != handle);
  assert(NULL != param);

  /* [u128 uuid_iso_iec_11578] */
  if (readBytesNal(handle, param->uuidIsoIec11578, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* [v<8 * (payloadSize - 16)> uuid_iso_iec_11578] */
  param->userDataPayloadOverflow = (
    H264_MAX_USER_DATA_PAYLOAD_SIZE < payloadSize - 16
  );
  param->userDataPayloadLength = MIN(
    payloadSize - 16,
    H264_MAX_USER_DATA_PAYLOAD_SIZE
  );

  return readBytesNal(
    handle, param->userDataPayload, param->userDataPayloadLength
  );
}

int parseH264SeiRecoveryPoint(
  H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint * param
)
{
  /* recovery_point(payloadSize) - Annex D.1.8 */
  uint32_t value;

  assert(NULL != handle);
  assert(NULL != param);

  /* [ue recovery_frame_cnt] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->recoveryFrameCnt = value; /* Max: MaxFrameNum - 1 = 2 ** (12 + 4) - 1 */

  /* [b1 exact_match_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->exactMatch = value;

  /* [b1 broken_link_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->brokenLink = value;

  /* [u2 changing_slice_group_idc] */
  if (readBitsNal(handle, &value, 2) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->changingSliceGroupIdc = value;

  return 0;
}

int parseH264SeiReservedSeiMessage(
  H264ParametersHandlerPtr handle,
  const size_t payloadSize
)
{
  /* [v<8 * payloadSize> reserved_sei_message_payload_byte] */
  return skipBitsNal(handle, 8 * payloadSize);
}

int parseH264SupplementalEnhancementInformationMessage(
  H264ParametersHandlerPtr handle, H264SeiMessageParameters * param
)
{
  /* sei_message() */
  int ret;

  uint32_t value;

  assert(NULL != handle);
  assert(NULL != param);

  param->payloadType = 0;
  do {
    /* [v8 ff_byte] / [u8 last_payload_type_byte] */
    if (readBitsNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->payloadType += value;

    /**
     * If the byte read is 0xFF, the field is considered as 'ff_byte' elsewhere,
     * it is the final 'last_payload_type_byte'.
     */
  } while (value == 0xFF);

  param->payloadSize = 0;

  if (!moreRbspDataNal(handle)) {
    /* x264 related fix (user_data_unregistered wrong length). */
    param->tooEarlyEnd = true;
    return 0;
  }
  param->tooEarlyEnd = false;

  do {
    /* [v8 ff_byte] / [u8 last_payload_size_byte] */
    if (readBitsNal(handle, &value, 8) < 0) {
      LIBBLU_H264_READING_FAIL_RETURN();
    }

    param->payloadSize += value;

    /**
     * If the byte read is 0xFF, the field is considered as 'ff_byte' elsewhere,
     * it is the final 'last_payload_size_byte'.
     */
  } while (value == 0xFF);

  /* sei_payload(payloadType, payloadSize) */
  /* From Annex D */
  switch (param->payloadType) {
    case H264_SEI_TYPE_BUFFERING_PERIOD:
      ret = parseH264SeiBufferingPeriod(
        handle,
        &param->bufferingPeriod
      );
      break;

    case H264_SEI_TYPE_PIC_TIMING:
      ret = parseH264SeiPicTiming(
        handle,
        &param->picTiming
      );
      break;

    case H264_SEI_TYPE_USER_DATA_UNREGISTERED:
      ret = parseH264SeiUserDataUnregistered(
        handle,
        &param->userDataUnregistered,
        param->payloadSize
      );
      break;

    case H264_SEI_TYPE_RECOVERY_POINT:
      ret = parseH264SeiRecoveryPoint(
        handle,
        &param->recoveryPoint
      );
      break;

    default:
      LIBBLU_H264_DEBUG_SEI(
        "Presence of reserved SEI message "
        "(Unsupported message, not checked).\n"
      );
      ret = parseH264SeiReservedSeiMessage(
        handle,
        param->payloadSize
      );
  }
  if (ret < 0)
    return -1;

  if (!isByteAlignedNal(handle)) {
    /* [v1 bit_equal_to_one] // 0b1 */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();

    if (!value)
      LIBBLU_H264_ERROR_RETURN(
        "Incorrect 'bit_equal_to_one' == 0b0 value at SEI message end.\n"
      );

    while (!isByteAlignedNal(handle)) {
      /* [v1 bit_equal_to_zero] // 0b0 */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();

      if (value)
        LIBBLU_H264_ERROR_RETURN(
          "Incorrect 'bit_equal_to_zero' == 0b1 value at SEI message end.\n"
        );
    }
  }

  return 0;
}

int decodeH264SupplementalEnhancementInformation(
  H264ParametersHandlerPtr handle
)
{
  /* sei_rbsp() */
  int ret;
  bool isConstant;

  H264SeiMessageParameters seiMsg;

  assert(NULL != handle);

  if (
    getNalUnitType(handle)
    != NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION
  )
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Supplemental Enhancement Information NAL unit type "
      "(receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  do {
    /* Parsing SEI_message() : */
    ret = parseH264SupplementalEnhancementInformationMessage(
      handle,
      &seiMsg
    );
    if (ret < 0)
      return -1;

    if (seiMsg.tooEarlyEnd) {
      /* x264 fix */
      LIBBLU_H264_DEBUG(
        "Warning: Too early end of NAL unit "
        "(x264 user_data_unregistered wrong length fix, skipped).\n"
      );
      return reachNextNal(handle);
    }

    LIBBLU_H264_DEBUG_SEI(
      " SEI Message payload - %s.\n",
      H264SEIMessagePayloadTypeStr(seiMsg.payloadType)
    );
    LIBBLU_H264_DEBUG_SEI(
      "  => Type (payloadType): %zu.\n",
      seiMsg.payloadType
    );
    LIBBLU_H264_DEBUG_SEI(
      "  => Size (payloadSize): %zu byte(s).\n",
      seiMsg.payloadSize
    );
    LIBBLU_H264_DEBUG_SEI(
      "  Content:\n"
    );

    /* Compliance checks and parameters saving : */
    ret = 0;
    switch (seiMsg.payloadType) {
      case H264_SEI_TYPE_BUFFERING_PERIOD:
        if (!handle->sei.bufferingPeriodPresent) {
          ret = checkH264SeiBufferingPeriodCompliance(
            seiMsg.bufferingPeriod,
            handle
          );
        }
        else {
          isConstant = constantH264SeiBufferingPeriodCheck(
            handle,
            handle->sei.bufferingPeriod,
            seiMsg.bufferingPeriod
          );

          if (!isConstant) {
            ret = checkH264SeiBufferingPeriodChangeCompliance(
              handle,
              handle->sei.bufferingPeriod,
              seiMsg.bufferingPeriod
            );
          }
        }
        if (ret < 0)
          return -1;

        handle->sei.bufferingPeriod = seiMsg.bufferingPeriod; /* Update */
        handle->sei.bufferingPeriodPresent = true;
        handle->sei.bufferingPeriodValid = true;
        break;

      case H264_SEI_TYPE_PIC_TIMING:
        /* Always update */
        ret = checkH264SeiPicTimingCompliance(
          seiMsg.picTiming,
          handle
        );
        if (ret < 0)
          return -1;

        if (handle->sei.picTimingPresent) {
          /* Check changes */
#if 0
          ret = checkH264SeiPicTimingChangeCompliance(
            handle,
            &handle->sei.picTiming,
            &seiMsg.picTiming
          );
#endif
        }
        if (ret < 0)
          return -1;

        handle->sei.picTiming = seiMsg.picTiming; /* Always update */
        handle->sei.picTimingPresent = true;
        handle->sei.picTimingValid = true;
        break;

      case H264_SEI_TYPE_RECOVERY_POINT:

        if (!handle->sei.recoveryPointPresent) {
          ret = checkH264SeiRecoveryPointCompliance(
            seiMsg.recoveryPoint,
            handle
          );
        }
        else {
          isConstant = constantH264SeiRecoveryPointCheck(
            handle->sei.recoveryPoint,
            seiMsg.recoveryPoint
          );
          if (!isConstant) {
            /* Check changes */
            ret = checkH264SeiRecoveryPointChangeCompliance(
              handle,
              seiMsg.recoveryPoint
            );
          }
        }
        if (ret < 0)
          return -1;

        handle->sei.recoveryPoint = seiMsg.recoveryPoint; /* Update */
        handle->sei.recoveryPointPresent = true;
        handle->sei.recoveryPointValid = true;
        break;

      default:
        LIBBLU_H264_DEBUG_SEI("  NOTE: Unchecked SEI message type.\n");
    }

  } while (moreRbspDataNal(handle));

  /* rbsp_trailing_bits() */
  if (parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  return 0;
}

int parseH264RefPicListModification(
  H264ParametersHandlerPtr handle,
  H264RefPicListModification * param,
  H264SliceTypeValue sliceType
)
{
  uint32_t value;

  H264RefPicListModificationPictureIndex * index;
  unsigned listLen;

  assert(NULL != handle);
  assert(NULL != param);

  if (!H264_SLICE_IS_TYPE_I(sliceType) && !H264_SLICE_IS_TYPE_SI(sliceType)) {
    /* [b1 ref_pic_list_modification_flag_l0] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->refPicListModificationFlagl0 = value;

    if (param->refPicListModificationFlagl0) {
      listLen = 0;

      while (1) {
        index = param->refPicListModificationl0 + listLen;

        /* [ue modification_of_pic_nums_idc] */
        if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        index->modOfPicNumsIdc = value;

        if (3 < index->modOfPicNumsIdc)
          LIBBLU_H264_ERROR_RETURN(
            "Unexpected 'modification_of_pic_nums_idc' == %d value.\n",
            index->modOfPicNumsIdc
          );

        if (
          index->modOfPicNumsIdc == H264_MOD_OF_PIC_IDC_SUBSTRACT_ABS_DIFF
          || index->modOfPicNumsIdc == H264_MOD_OF_PIC_IDC_ADD_ABS_DIFF
        ) {
          /* [ue abs_diff_pic_num_minus1] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->absDiffPicNumMinus1 = value;
        }
        else if (index->modOfPicNumsIdc == H264_MOD_OF_PIC_IDC_LONG_TERM) {
          /* [ue long_term_pic_num] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->longTermPicNum = value;
        }
        else
          break; /* End of list */

        if (H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC <= listLen)
          LIBBLU_H264_ERROR_RETURN(
            "Reference Pictures List modification overflow, "
            "number of defined pictures exceed %d.\n",
            H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC
          );
        listLen++;
      }
      param->nbRefPicListModificationl0 = listLen;
    }
  }

  if (H264_SLICE_IS_TYPE_B(sliceType)) {
    /* [b1 ref_pic_list_modification_flag_l1] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->refPicListModificationFlagl1 = value;

    if (param->refPicListModificationFlagl1) {
      listLen = 0;

      while (1) {
        index = param->refPicListModificationl1 + listLen;

        /* [ue modification_of_pic_nums_idc] */
        if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        index->modOfPicNumsIdc = value;

        if (3 < index->modOfPicNumsIdc)
          LIBBLU_H264_ERROR_RETURN(
            "Invalid 'modification_of_pic_nums_idc' == %u value in "
            "ref_pic_list_modification() section of slice header.\n"
          );

        if (
          index->modOfPicNumsIdc == H264_MOD_OF_PIC_IDC_SUBSTRACT_ABS_DIFF
          || index->modOfPicNumsIdc == H264_MOD_OF_PIC_IDC_ADD_ABS_DIFF
        ) {
          /* [ue abs_diff_pic_num_minus1] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->absDiffPicNumMinus1 = value;
        }
        else if (index->modOfPicNumsIdc == H264_MOD_OF_PIC_IDC_LONG_TERM) {
          /* [ue long_term_pic_num] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->longTermPicNum = value;
        }
        else
          break; /* End of list */

        if (H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC <= listLen)
          LIBBLU_H264_ERROR_RETURN(
            "Reference Pictures List modification overflow, "
            "number of defined pictures exceed %d.\n",
            H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC
          );
        listLen++;
      }
      param->nbRefPicListModificationl1 = listLen;
    }
  }

  return 0;
}

int parseH264PredWeightTable(
  H264ParametersHandlerPtr handle,
  H264PredWeightTable * param,
  H264SliceHeaderParameters * sliceHeaderParam
)
{
  /* pred_weight_table() - 7.3.3.2 Prediction weight table syntax */
  uint32_t value;
  unsigned i, j;

  H264SPSDataParameters * spsData;

  assert(NULL != handle);
  assert(NULL != param);
  assert(NULL != sliceHeaderParam);

  spsData = &handle->sequenceParametersSet.data;

  /* [ue luma_log2_weight_denom] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->lumaLog2WeightDenom = value;

  if (spsData->ChromaArrayType != 0) {
    /* [ue chroma_log2_weight_denom] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->chromaLog2WeightDenom = value;
  }

  for (i = 0; i <= sliceHeaderParam->numRefIdxl0ActiveMinus1; i++) {
    /* [b1 luma_weight_l0_flag[i]] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->weightL0[i].lumaWeightFlag = value;

    if (param->weightL0[i].lumaWeightFlag) {
      /* [se luma_weight_l0[i]] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL0[i].lumaWeight = (int) value;

      /* [se luma_offset_l0[i]] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL0[i].lumaOffset = (int) value;
    }

    if (spsData->ChromaArrayType != 0) {
      /* [b1 chroma_weight_l0_flag[i]] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL0[i].lumaWeightFlag = value;

      if (param->weightL0[i].lumaWeightFlag) {
        for (j = 0; j < H264_MAX_CHROMA_CHANNELS_NB; j++) {
          /* [se chroma_weight_l0[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL0[i].chromaWeight[j] = (int) value;

          /* [se chroma_offset_l0[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL0[i].chromaOffset[j] = (int) value;
        }
      }
    }
  }

  if (H264_SLICE_IS_TYPE_B(sliceHeaderParam->sliceType)) {
    for (i = 0; i <= sliceHeaderParam->numRefIdxl1ActiveMinus1; i++) {
      /* [b1 luma_weight_l1_flag[i]] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL1[i].lumaWeightFlag = value;

      if (param->weightL1[i].lumaWeightFlag) {
        /* [se luma_weight_l1[i]] */
        if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->weightL1[i].lumaWeight = (int) value;

        /* [se luma_offset_l1[i]] */
        if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->weightL1[i].lumaOffset = (int) value;
      }

      if (spsData->ChromaArrayType != 0) {
        /* [b1 chroma_weight_l1_flag[i]] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->weightL1[i].lumaWeightFlag = value;

        for (j = 0; j < H264_MAX_CHROMA_CHANNELS_NB; j++) {
          /* [se chroma_weight_l1[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL1[i].chromaWeight[j] = (int) value;

          /* [se chroma_offset_l1[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL1[i].chromaOffset[j] = (int) value;
        }
      }
    }
  }

  return 0;
}

int parseH264DecRefPicMarking(
  H264ParametersHandlerPtr handle,
  H264DecRefPicMarking * param,
  bool IdrPicFlag
)
{
  /* dec_ref_pic_marking() */
  /* 7.3.3.3 Decoded reference picture marking syntax */
  uint32_t value;

  H264MemMngmntCtrlOpBlkPtr lastOp, newOp;

  assert(NULL != handle);
  assert(NULL != param);

  param->IdrPicFlag = IdrPicFlag;

  param->adaptativeRefPicMarkingMode = false;

  param->presenceOfMemManCtrlOp4 = false;
  param->presenceOfMemManCtrlOp5 = false;
  param->presenceOfMemManCtrlOp6 = false;

  if (param->IdrPicFlag) {
    /* [b1 no_output_of_prior_pics_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->noOutputOfPriorPics = value;

    /* [b1 long_term_reference_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->longTermReference = value;
  }
  else {
    /* [b1 adaptive_ref_pic_marking_mode_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->adaptativeRefPicMarkingMode = value;

    if (param->adaptativeRefPicMarkingMode) {
      param->MemMngmntCtrlOp = NULL;
      lastOp = NULL;

      do {
        if (NULL == (newOp = createH264MemoryManagementControlOperations()))
          return -1;

        /* [ue memory_management_control_operation] */
        if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        newOp->operation = value;

        switch (newOp->operation) {
          case H264_MEM_MGMNT_CTRL_OP_END:
          case H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED:
          case H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED:
          case H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM:
            break;

          case H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM:
            if (param->presenceOfMemManCtrlOp4)
              LIBBLU_H264_ERROR_RETURN(
                "Presence of multiple 'memory_management_control_operation' "
                "equal to 4 in a slice header.\n"
              );
            param->presenceOfMemManCtrlOp4 = true;
            break;

          case H264_MEM_MGMNT_CTRL_OP_ALL_UNUSED:
            if (param->presenceOfMemManCtrlOp5)
              LIBBLU_H264_ERROR_RETURN(
                "Presence of multiple 'memory_management_control_operation' "
                "equal to 5 in a slice header.\n"
              );
            param->presenceOfMemManCtrlOp5 = true;
            break;

          case H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM:
            if (param->presenceOfMemManCtrlOp6)
              LIBBLU_H264_ERROR_RETURN(
                "Presence of multiple 'memory_management_control_operation' "
                "equal to 6 in a slice header.\n"
              );
            param->presenceOfMemManCtrlOp6 = true;
            break;

          default:
            LIBBLU_H264_ERROR_RETURN(
              "Unknown 'memory_management_control_operation' == %u value in "
              "slice header.\n",
              newOp->operation
            );
        }

        if (
          newOp->operation == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED
          || newOp->operation == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM
        ) {
          /* [ue difference_of_pic_nums_minus1] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          newOp->diffOfPicNumsMinus1 = value;
        }

        if (newOp->operation == H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED) {
          /* [ue long_term_pic_num] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          newOp->longTermPicNum = value;
        }

        if (
          newOp->operation == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM
          || newOp->operation == H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM
        ) {
          /* [ue long_term_frame_idx] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          newOp->longTermFrameIdx = value;
        }

        if (newOp->operation == H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM) {
          /* [ue max_long_term_frame_idx_plus1] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          newOp->maxLongTermFrameIdxPlus1 = value;
        }

        /* Add op to loop : */
        if (NULL == param->MemMngmntCtrlOp)
          param->MemMngmntCtrlOp = newOp;
        else
          lastOp->nextOperation = newOp;
        lastOp = newOp;
      } while (newOp->operation != H264_MEM_MGMNT_CTRL_OP_END);
    }
  }

  return 0;
}

int parseH264SliceHeader(
  H264ParametersHandlerPtr handle,
  H264SliceHeaderParameters * param
)
{
  /* slice_header() - 7.3.3 Slice header syntax */
  uint32_t value;
  unsigned linkedPPSId;

  H264SequenceParametersSetDataParameters * spsData;
  H264PicParametersSetParameters * ppsData;

  H264SliceGroupMapTypeValue sliceGroupMapType;

  /* bool frameNumEqualPrevRefFrameNum; */  /* TODO */

  assert(NULL != handle);
  assert(NULL != param);

  if (!handle->sequenceParametersSetPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory SPS parameters, Unable to decode slice_header().\n"
    );

  spsData = &handle->sequenceParametersSet.data;

  if (0 == handle->picParametersSetIdsPresentNb)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory PPS parameters, Unable to decode slice_header().\n"
    );

  /* ppsData will be set when know active ID */

  /* Get informations from context (current AU) */
  param->IdrPicFlag =
    getNalUnitType(handle) == NAL_UNIT_TYPE_IDR_SLICE
  ;
  param->isSliceExt =
    getNalUnitType(handle) == NAL_UNIT_TYPE_CODED_SLICE_EXT
    || getNalUnitType(handle) == NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT
  ;
  param->picOrderCntType = spsData->picOrderCntType;

  /* [ue first_mb_in_slice] */
  if (readExpGolombCodeNal(handle, &value, 32) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->firstMbInSlice = value;

  /* [ue slice_type] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->sliceType = value;

  if (H264_MAX_SLICE_TYPE_VALUE < param->sliceType)
    LIBBLU_H264_ERROR_RETURN(
      "Unknown 'slice_type' == %u in slice header.\n",
      param->sliceType
    );

  /* [ue pic_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->picParametersSetId = linkedPPSId = value;

  /* Checking PPS id */
  if (H264_MAX_PPS <= linkedPPSId)
    LIBBLU_H264_ERROR_RETURN(
      "Slice header pic_parameter_set_id value overflow.\n"
    );
  if (!handle->picParametersSetIdsPresent[linkedPPSId])
    LIBBLU_H264_ERROR_RETURN(
      "Slice header referring unknown PPS id %u.\n",
      linkedPPSId
    );

  ppsData = handle->picParametersSet[linkedPPSId];

  if (spsData->separateColourPlaneFlag444) {
    /* [u2 colour_plane_id] */
    if (readBitsNal(handle, &value, 2) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->colourPlaneId = value;
  }
  else
    param->colourPlaneId = H264_COLOUR_PLANE_ID_Y; /* Default, not used, only for checking purposes */

  /* [u<log2_max_frame_num_minus4 + 4> frame_num] */
  if (
    readBitsNal(
      handle, &value,
      spsData->log2MaxFrameNumMinus4 + 4
    ) < 0
  )
    LIBBLU_H264_READING_FAIL_RETURN();
  param->frameNum = value;

#if 0
  if (param->IdrPicFlag) {
    if (0 != param->frameNum) {
      LIBBLU_H264_ERROR_RETURN(
        "Broken picture identifier, unexpected value 'frame_num' == %u for "
        "an IDR picture, shall be equal to 0.\n"
      );
    }

    handle->curProgParam.PrevRefFrameNum = 0;
  }
  else {
    /* Non-IDR pict */
    frameNumEqualPrevRefFrameNum =
      (param->frameNum == handle->curProgParam.PrevRefFrameNum)
    ;

    if (frameNumEqualPrevRefFrameNum) {

    }

    if (!frameNumEqualPrevRefFrameNum) {
      handle->curProgParam.gapsInFrameNum = (
        param->frameNum
        != (
          (handle->curProgParam.PrevRefFrameNum + 1)
          % spsData->MaxFrameNum
        )
      );

      if (handle->curProgParam.gapsInFrameNum) {
        /**
         * Invoke 8.2.5.2 Decoding process for gaps in frame_num
         * (regardless to gaps_in_frame_num_value_allowed_flag)
         */

        LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
          "Forbidden gaps in 'frame_num' value "
          "(last: %" PRIu16 ", cur: %" PRIu16 ").\n",
          handle->curProgParam.PrevRefFrameNum,
          param->frameNum
        );
      }
    }

#if 0
    else if (param->frameNum != handle->curProgParam.PrevRefFrameNum) {
      /* Update if required PrevRefFrameNum */
      /* Next picture to the last referential one is marked as the new one. */
      handle->curProgParam.PrevRefFrameNum =
        (handle->curProgParam.PrevRefFrameNum + 1) %
        spsData->MaxFrameNum
      ;
    }
#endif
  }
#endif

  if (!spsData->frameMbsOnly) {
    /* [b1 field_pic_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->fieldPic = value;

    if (param->fieldPic) {
      /* [b1 bottom_field_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->bottomField = value;
    }
    else
      param->bottomField = false;
  }
  else {
    param->fieldPic = false;
    param->bottomField = false;
  }

  /* Compute parameters: */
  param->refPic = getNalRefIdc(handle) != 0;
  param->mbaffFrameFlag =
    spsData->mbAdaptativeFrameField
    && !param->fieldPic
  ;
  param->picHeightInMbs =
    spsData->FrameHeightInMbs
    / (1 + param->fieldPic)
  ;
  param->picHeightInSamplesL =
    param->picHeightInMbs * 16
  ;
  param->picHeightInSamplesC =
    param->picHeightInMbs
    * spsData->MbHeightC
  ;
  param->picSizeInMbs =
    param->picHeightInMbs
    * spsData->PicWidthInMbs
  ;

  if (param->IdrPicFlag) {
    /* [ue idr_pic_id] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->idrPicId = value;
  }
  else
    param->idrPicId = 0; /* For test purpose */

  if (spsData->picOrderCntType == 0) {
    /* [u<log2_max_pic_order_cnt_lsb_minus4 + 4> pic_order_cnt_lsb] */
    if (
      readBitsNal(
        handle, &value,
        spsData->log2MaxPicOrderCntLsbMinus4 + 4
      ) < 0
    )
      LIBBLU_H264_READING_FAIL_RETURN();
    param->picOrderCntLsb = value;

    if (
      ppsData->bottomFieldPicOrderInFramePresent
      && !param->fieldPic
    ) {
      /* [se delta_pic_order_cnt_bottom] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->deltaPicOrderCntBottom = (int) value;
    }
    else
      param->deltaPicOrderCntBottom = 0;
  }
  else {
    param->picOrderCntLsb = 0; /* For test purpose */
    param->deltaPicOrderCntBottom = 0;
  }

  if (
    spsData->picOrderCntType == 1
    && !spsData->deltaPicOrderAlwaysZero
  ) {
    /* [se delta_pic_order_cnt[0]] */
    if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->deltaPicOrderCnt[0] = (long) value;

    if (
      ppsData->bottomFieldPicOrderInFramePresent
      && !param->fieldPic
    ) {
      /* [se delta_pic_order_cnt[1]] */
      if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->deltaPicOrderCnt[1] = (long) value;
    }
    else
      param->deltaPicOrderCnt[1] = 0;
  }
  else
    param->deltaPicOrderCnt[0] = 0, param->deltaPicOrderCnt[1] = 0;

  if (ppsData->redundantPicCntPresent) {
    /* [ue redundant_pic_cnt] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->redundantPicCnt = value;
  }
  else
    param->redundantPicCnt = 0;

  if (H264_SLICE_IS_TYPE_B(param->sliceType)) {
    /* [b1 direct_spatial_mv_pred_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->directSpatialMvPred = value;
  }

  if (
    H264_SLICE_IS_TYPE_P(param->sliceType)
    || H264_SLICE_IS_TYPE_SP(param->sliceType)
    || H264_SLICE_IS_TYPE_B(param->sliceType)
  ) {
    unsigned maxRefIdx;

    /* [b1 num_ref_idx_active_override_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->numRefIdxActiveOverride = value;

    if (param->numRefIdxActiveOverride) {
      /* [ue num_ref_idx_l0_active_minus1] */
      if (readExpGolombCodeNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->numRefIdxl0ActiveMinus1 = value;

      maxRefIdx =
        (param->fieldPic) ?
          H264_REF_PICT_LIST_MAX_NB_FIELD
        :
          H264_REF_PICT_LIST_MAX_NB
      ;

      if (maxRefIdx <= param->numRefIdxl0ActiveMinus1)
        LIBBLU_H264_ERROR_RETURN(
          "Maximum reference index for reference picture list 0 exceed %u in "
          "slice header.\n",
          maxRefIdx
        );

      if (H264_SLICE_IS_TYPE_B(param->sliceType)) {
        /* [ue num_ref_idx_l1_active_minus1] */
          if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->numRefIdxl1ActiveMinus1 = value;

        if (maxRefIdx <= param->numRefIdxl0ActiveMinus1)
          LIBBLU_H264_ERROR_RETURN(
            "Maximum reference index for reference picture list 0 exceed %u in "
            "slice header.\n",
            maxRefIdx
          );
      }
    }
    else {
      param->numRefIdxl0ActiveMinus1 =
        ppsData->numRefIdxl0DefaultActiveMinus1
      ;
      param->numRefIdxl1ActiveMinus1 =
        ppsData->numRefIdxl1DefaultActiveMinus1
      ;
    }
  }

  if (param->isSliceExt) {
    /* ref_pic_list_mvc_modification() */
    /* Annex H */

    /* TODO: Add 3D MVC support. */
    LIBBLU_H264_ERROR_RETURN(
      "Unsupported 3D MVC bitstream.\n"
    );
  }
  else {
    /* ref_pic_list_modification() */
    if (
      parseH264RefPicListModification(
        handle, &param->refPicListMod, param->sliceType
      ) < 0
    )
      return -1;
  }

  if (
    (
      ppsData->weightedPred
      && (
        H264_SLICE_IS_TYPE_P(param->sliceType)
        || H264_SLICE_IS_TYPE_SP(param->sliceType)
      )
    )
    || (
      (
        ppsData->weightedBipredIdc
        == H264_WEIGHTED_PRED_B_SLICES_EXPLICIT
      )
      && H264_SLICE_IS_TYPE_B(param->sliceType)
    )
  ) {
    /* pred_weight_table() */
    if (parseH264PredWeightTable(handle, &param->predWeightTable, param) < 0)
      return -1;
  }

  param->decRefPicMarkingPresent = getNalRefIdc(handle) != 0;
  if (param->decRefPicMarkingPresent) {
    /* dec_ref_pic_marking() */
    if (
      parseH264DecRefPicMarking(
        handle, &param->decRefPicMarking, param->IdrPicFlag
      ) < 0
    )
      return -1;
  }
  else {
    param->decRefPicMarking.adaptativeRefPicMarkingMode = false;
    param->decRefPicMarking.presenceOfMemManCtrlOp4 = false;
    param->decRefPicMarking.presenceOfMemManCtrlOp5 = false;
    param->decRefPicMarking.presenceOfMemManCtrlOp6 = false;
    param->decRefPicMarking.IdrPicFlag = param->IdrPicFlag;
  }

  if (
    ppsData->entropyCodingMode
    && !H264_SLICE_IS_TYPE_I(param->sliceType)
    && !H264_SLICE_IS_TYPE_SI(param->sliceType)
  ) {
    /* [ue cabac_init_idc] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->cabacInitIdc = value;
  }
  else
    param->cabacInitIdc = 0; /* For check purpose */

  /* [se slice_qp_delta] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->sliceQpDelta = (int) value;

  if (
    H264_SLICE_IS_TYPE_SP(param->sliceType)
    || H264_SLICE_IS_TYPE_SI(param->sliceType)
  ) {
    if (H264_SLICE_IS_TYPE_SP(param->sliceType)) {
      /* [b1 sp_for_switch_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->spForSwitch = value;
    }

    /* [se slice_qs_delta] */
    if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->sliceQsDelta = (int) value;
  }

  if (
    ppsData->deblockingFilterControlPresent
  ) {
    /* [ue disable_deblocking_filter_idc] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->disableDeblockingFilterIdc = value;

    if (2 < param->disableDeblockingFilterIdc)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved value 'disable_deblocking_filter_idc' == %u "
        "in slice header.\n",
        param->disableDeblockingFilterIdc
      );

    if (param->disableDeblockingFilterIdc != 1) {
      /* [se slice_alpha_c0_offset_div2] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->sliceAlphaC0OffsetDiv2 = (int) value;

      /* [se slice_beta_offset_div2] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->sliceBetaOffsetDiv2 = (int) value;
    }
    else
      param->sliceAlphaC0OffsetDiv2 = 0, param->sliceBetaOffsetDiv2 = 0;
  }
  else {
    param->disableDeblockingFilterIdc = 0;
    param->sliceAlphaC0OffsetDiv2 = 0;
    param->sliceBetaOffsetDiv2 = 0;
  }

  if (1 < ppsData->nbSliceGroups) {
    sliceGroupMapType = ppsData->sliceGroups.mapType;

    if (
      sliceGroupMapType == H264_SLICE_GROUP_TYPE_CHANGING_1
      || sliceGroupMapType == H264_SLICE_GROUP_TYPE_CHANGING_2
      || sliceGroupMapType == H264_SLICE_GROUP_TYPE_CHANGING_3
    ) {
      /* [ue slice_group_change_cycle] */
      if (readExpGolombCodeNal(handle, &value, 32) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->sliceGroupChangeCycle = value;
    }
  }

  return 0;
}

H264FirstVclNaluPCPRet detectFirstVclNalUnitOfPrimCodedPicture(
  H264SliceHeaderParameters last,
  H264SliceHeaderParameters cur
)
{
  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'frame_num' differs in value ? : %u => %u.\n",
    last.frameNum, cur.frameNum
  );

  if (last.frameNum != cur.frameNum)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'pic_parameter_set_id' differs in value ? : %u => %u.\n",
    last.picParametersSetId, cur.picParametersSetId
  );

  if (last.picParametersSetId != cur.picParametersSetId)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'field_pic_flag' differs in value ? : %u => %u.\n",
    last.fieldPic, cur.fieldPic
  );

  if (last.fieldPic != cur.fieldPic)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'bottom_field_flag' differs in value ? : %u => %u.\n",
    last.bottomField, cur.bottomField
  );

  if (last.fieldPic && last.bottomField != cur.bottomField)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'nal_ref_idc' == 0 (IDR-pic) differs in value ? : %s => %s.\n",
    BOOL_STR(last.refPic), BOOL_STR(cur.refPic)
  );

  if (last.refPic ^ cur.refPic)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'pic_order_cnt_type' == 0 for both ? : %s => %s.\n",
    BOOL_STR(last.picOrderCntType == 0), BOOL_STR(cur.picOrderCntType == 0)
  );

  if ((last.picOrderCntType == 0) && (cur.picOrderCntType == 0)) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'pic_order_cnt_lsb' differs in value ? : %u => %u.\n",
      last.picOrderCntLsb, cur.picOrderCntLsb
    );

    if (last.picOrderCntLsb != cur.picOrderCntLsb)
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;

    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'delta_pic_order_cnt_bottom' differs in value ? : "
      "%d => %d.\n",
      last.deltaPicOrderCntBottom, cur.deltaPicOrderCntBottom
    );

    if (last.deltaPicOrderCntBottom != cur.deltaPicOrderCntBottom)
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;
  }

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'pic_order_cnt_type' == 1 for both ? : %s => %s.\n",
    BOOL_STR(last.picOrderCntType == 1), BOOL_STR(cur.picOrderCntType == 1)
  );

  if ((last.picOrderCntType == 1) && (cur.picOrderCntType == 1)) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'delta_pic_order_cnt[0]' differs in value ? : %ld => %ld.\n",
      last.deltaPicOrderCnt[0], cur.deltaPicOrderCnt[0]
    );

    if (last.deltaPicOrderCnt[0] != cur.deltaPicOrderCnt[0])
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;

    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'delta_pic_order_cnt[1]' differs in value ? : %ld => %ld.\n",
      last.deltaPicOrderCnt[1], cur.deltaPicOrderCnt[1]
    );

    if (last.deltaPicOrderCnt[1] != cur.deltaPicOrderCnt[1])
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;
  }

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'IdrPicFlag' differs in value ? : %u => %u.\n",
    last.IdrPicFlag, cur.IdrPicFlag
  );

  if (last.IdrPicFlag != cur.IdrPicFlag)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  if (cur.IdrPicFlag) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'idr_pic_id' differs in value ? : %u => %u.\n",
      last.idrPicId, cur.idrPicId
    );

    if (last.idrPicId != cur.idrPicId)
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;
  }

  return H264_FRST_VCL_NALU_PCP_RET_FALSE;
}

int decodeH264SliceLayerWithoutPartitioning(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  int ret;

  H264SliceLayerWithoutPartitioningParameters sliceParam;

  /* slice_header() */
  if (parseH264SliceHeader(handle, &sliceParam.header) < 0)
    return -1;

  /* slice_data() */
  /* rbsp_trailing_bits() */
  /* *Ignored* */
  if (reachNextNal(handle) < 0)
    return -1;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    "Checking if Coded Slice VCL NALU is the first unit of a new "
    "primary coded picture.\n"
  );
  ret = H264_FRST_VCL_NALU_PCP_RET_TRUE;
  /**
   * NOTE: If no Coded Slice VCL NALU is present before, this NALU is necessary
   * the start of a new primary coded picture.
   */

  if (handle->slicePresent) {
    ret = detectFirstVclNalUnitOfPrimCodedPicture(
      sliceParam.header,
      handle->slice.header
    );
  }

  switch (ret) {
    case H264_FRST_VCL_NALU_PCP_RET_FALSE:
      break;

    case H264_FRST_VCL_NALU_PCP_RET_TRUE:
      LIBBLU_H264_DEBUG_AU_PROCESSING(
        " => This VCL NALU mark a new primary picture.\n"
      );
      if (handle->curProgParam.reachVclNaluOfaPrimCodedPic) {
        LIBBLU_H264_DEBUG_AU_PROCESSING(
          "Another primary picture is already present, marking the start "
          "of a new Access Unit.\n"
        );
      }

      handle->curProgParam.reachVclNaluOfaPrimCodedPic = true;
      break;

    default:
      return -1;
  }

  /* Always do checks : */
  LIBBLU_H264_DEBUG_SLICE("  Slice header:\n");
  if (!handle->slicePresent) {
    ret = checkH264SliceLayerWithoutPartitioningCompliance(
      sliceParam, handle, options
    );
  }
  else {
    ret = checkH264SliceLayerWithoutPartitioningChangeCompliance(
      handle, handle->slice, sliceParam, options
    );
  }
  if (ret < 0)
    return -1;

  /* Update current slice : */
  if (
    handle->slicePresent
    && handle->slice.header.decRefPicMarkingPresent
  ) {
    /* Free previous memory_management_control_operation storage structures : */
    if (
      !handle->slice.header.decRefPicMarking.IdrPicFlag
      && handle->slice.header.decRefPicMarking.adaptativeRefPicMarkingMode
    )
      closeH264MemoryManagementControlOperations(
        handle->slice.header.decRefPicMarking.MemMngmntCtrlOp
      );
  }

  handle->slice.header = sliceParam.header;
  handle->slicePresent = true;

  return 0;
}

int decodeH264FillerData(H264ParametersHandlerPtr handle)
{
  size_t length;
  uint32_t value;

  assert(NULL != handle);

  if (!handle->slicePresent)
    LIBBLU_H264_ERROR_RETURN(
      "Presence of Filler data NAL before first slice NAL unit.\n"
    );

  length = 0;
  while (moreRbspDataNal(handle)) {
    /* [v8 ff_byte] */
    if (readBitsNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();

    if (value != 0xFF) {
      LIBBLU_H264_ERROR_RETURN(
        "Wrong filling byte 'ff_byte' == 0x%02" PRIX32 " in "
        "filler data NAL.\n",
        value
      );
    }
    length++;
  }

  LIBBLU_H264_DEBUG_AUD(
    " Filler data length: %zu byte(s) (0x%zx).\n",
    length, length
  );

  /* rbsp_trailing_bits() */
  if (parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  if (CHECK_H264_WARNING_FLAG(handle, presenceOfFillerData)) {
    LIBBLU_WARNING(
      "Presence of filling data bytes, reducing compression efficiency.\n"
    );
    MARK_H264_WARNING_FLAG(handle, presenceOfFillerData);
  }

  return 0;
}

int decodeH264EndOfSequence(H264ParametersHandlerPtr handle)
{
  assert(NULL != handle);

  if (isInitNal(handle))
    LIBBLU_H264_ERROR_RETURN("Unknown data in end of sequence NAL Unit.\n");

  return 0;
}

int analyzeH264(
  LibbluESParsingSettings * settings
)
{
  H264ParametersHandlerPtr handle;

  unsigned long duration;
  bool seiSection; /* Used to know when inserting SEI custom NALU. */

  uint8_t nalUnitType;

  assert(NULL != settings);

#if 0
  /* Pre-gen CRC-32 (file closed after operation) */
  h264InputFile = createBitstreamReader(settings->esFilepath, READ_BUFFER_LEN);
  if (NULL == h264InputFile)
    goto free_return;
  if (readBytes(h264InputFile, crcBuf, CRC32_USED_BYTES) < 0)
    goto free_return;

  closeBitstreamReader(h264InputFile);

  /* Re-open input file : */
  h264InputFile = createBitstreamReader(settings->esFilepath, READ_BUFFER_LEN);
  if (NULL == h264InputFile)
    goto free_return;

  if (NULL == (handle = initH264ParametersHandler(h264InputFile, &settings->options)))
    goto free_return;

  handle->esmsFile = createBitstreamWriter(
    settings->scriptFilepath, WRITE_BUFFER_LEN
  );
  if (NULL == handle->esmsFile)
    goto free_return;

  if (writeEsmsHeader(handle->esmsFile) < 0)
    goto free_return;

  if (
    appendSourceFileEsmsWithCrc(
      h264Infos, settings->esFilepath,
      CRC32_USED_BYTES, lb_compute_crc32(crcBuf, 0, CRC32_USED_BYTES),
      &handle->esmsSrcFileIdx
    ) < 0
  )
    goto free_return;
#endif

  if (NULL == (handle = initH264ParametersHandler(settings)))
    goto free_return;

  seiSection = false;

  while (!noMoreNal(handle) && !settings->askForRestart) {
    /* Main NAL units parsing loop. */

    /* Progress bar : */
    printFileParsingProgressionBar(handle->file.inputFile);

    if (initNal(handle) < 0)
      goto free_return;

    nalUnitType = getNalUnitType(handle);

    if (isStartOfANewH264AU(&handle->curProgParam, nalUnitType)) {
      LIBBLU_H264_DEBUG_AU_PROCESSING(
        "Detect the start of a new Access Unit.\n"
      );
      if (processAccessUnit(handle, settings) < 0)
        goto free_return;
    }

    switch (nalUnitType) {
      case NAL_UNIT_TYPE_NON_IDR_SLICE: /* 0x01 / 1 */
      case NAL_UNIT_TYPE_IDR_SLICE:     /* 0x05 / 5 */
        if (decodeH264SliceLayerWithoutPartitioning(handle, settings->options) < 0)
          goto free_return;

        /* Compute PicOrderCnt: */
        if (computeAuPicOrderCnt(handle, settings) < 0)
          goto free_return;
        break;

      case NAL_UNIT_TYPE_SLICE_PART_A_LAYER: /* 0x02 / 2 */
      case NAL_UNIT_TYPE_SLICE_PART_B_LAYER: /* 0x03 / 3 */
      case NAL_UNIT_TYPE_SLICE_PART_C_LAYER: /* 0x04 / 4 */
        LIBBLU_ERROR("Unsupported partitioned slice data NALUs.\n");
        /* handle->constraints.forbiddenSliceDataPartitionLayersNal */
        LIBBLU_H264_COMPLIANCE_ERROR_FRETURN(
          "Unsupported partitioned slice data NALUs.\n"
        );

      case NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION: /* 6 - SEI */
        if (settings->options.discardSei) {
          /* Flag force SEI messages suppression, skip decoding. */
          LIBBLU_H264_DEBUG_SEI(" Discard SEI messages.\n");

          if (discardCurNalCell(handle) < 0)
            goto free_return;

          if (reachNextNal(handle) < 0)
            goto free_return;
          continue;
        }
        else {
          /* Normal SEI message decoding. */
          if (decodeH264SupplementalEnhancementInformation(handle) < 0)
            goto free_return;

          if (
            settings->options.forceRebuildSei
            && (
              handle->sei.bufferingPeriodValid
              || handle->sei.picTimingValid
            )
          ) {
            LIBBLU_H264_DEBUG_SEI(
              " ### SEI NAL unit contains buffering_period or "
              "pic_timing messages, discarded for rebuilding.\n"
            );
            /**
             * TODO: Patch to only suppress buffering_period and
             * pic_timing messages, not potential other messages
             * contained in NAL unit.
             */

            if (discardCurNalCell(handle) < 0)
              goto free_return;

            handle->sei.bufferingPeriodValid = false;
            handle->sei.picTimingValid = false;

            if (reachNextNal(handle) < 0)
              goto free_return;
            continue;
          }
        }
        break;

      case NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET: /* 7 - SPS */
        if (decodeH264SequenceParametersSet(handle, settings->options) < 0)
          goto free_return;

        if (handle->curProgParam.presenceOfUselessSequenceParameterSet) {
          if (!handle->curProgParam.discardUselessSequenceParameterSet) {
            LIBBLU_WARNING(
              "Presence of duplicated sequence parameter set NALU(s) "
              "in Access Unit(s).\n"
            );

            handle->curProgParam.discardUselessSequenceParameterSet = true;
          }

          handle->curProgParam.presenceOfUselessSequenceParameterSet = false;

          if (!settings->options.disableFixes) {
            if (discardCurNalCell(handle) < 0)
              goto free_return;
            continue;
          }
        }

        /* Apply SPS VUI patch in case of missing/broken VUI : */
        if (
          handle->curProgParam.missingVSTVuiParameters
          || handle->curProgParam.wrongVuiParameters
        ) {
          handle->curProgParam.useVuiRebuilding |=
            !settings->options.disableFixes
          ;
        }

        if (patchH264SequenceParametersSet(handle, settings->options) < 0)
          goto free_return;

        if (
          !settings->options.disableHrdVerifier
          && !IN_USE_H264_CPB_HRD_VERIFIER(handle)
        ) {
          if (initIfRequiredH264CpbHrdVerifier(handle, &settings->options) < 0)
            goto free_return;
        }
        break;

      case NAL_UNIT_TYPE_PIC_PARAMETERS_SET: /* 8 - PPS */
        if (decodeH264PicParametersSet(handle) < 0)
          goto free_return;

        seiSection = true; /* Made SEI insertion available. */
        break;

      case NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER: /* 9 - AUD */
        if (decodeH264AccessUnitDelimiter(handle) < 0)
          goto free_return;

        if (handle->curProgParam.presenceOfUselessAccessUnitDelimiter) {
          if (!handle->curProgParam.discardUselessAccessUnitDelimiter) {
            LIBBLU_WARNING(
              "Presence of duplicated access unit delimiter NALU(s) "
              "in Access Unit(s) (Rec. ITU-T H.264 7.4.1.2.3 violation).\n"
            );

            handle->curProgParam.discardUselessAccessUnitDelimiter = true;
          }

          handle->curProgParam.presenceOfUselessAccessUnitDelimiter = false;

          if (!settings->options.disableFixes) {
            if (discardCurNalCell(handle) < 0)
              goto free_return;
            continue;
          }
        }
        break;

      case NAL_UNIT_TYPE_END_OF_SEQUENCE:   /* 10 - EOS */
      case NAL_UNIT_TYPE_END_OF_STREAM:
        if (decodeH264EndOfSequence(handle) < 0)
          goto free_return;
        break;

      case NAL_UNIT_TYPE_FILLER_DATA:     /* 12 */
        if (decodeH264FillerData(handle) < 0)
          goto free_return;
        break;

      default:
        LIBBLU_H264_ERROR_RETURN(
          "Unknown NAL Unit type 'nal_unit_type' == %u.\n",
          nalUnitType
        );
    }

    if (addNalCellToAccessUnit(handle) < 0)
      goto free_return;

    /**
     * Apply SEI messages patch in case of missing/broken
     * SEI buffering_period/pic_timing messages :
     * TODO: Move into patch function ?
     * TODO: Find rebuilding algorithm.
     */
    if (seiSection) {
      if (settings->options.forceRebuildSei) {
#if 0
        if (insertH264SeiBufferingPeriodPlaceHolder(handle) < 0)
          goto free_return;

        handle->sei.bufferingPeriodValid = true;
#else
        LIBBLU_H264_ERROR_FRETURN(
          "SEI rebuilding option is currently broken.\n"
        );
#endif
      }

      seiSection = false;
    }
  }

  if (settings->askForRestart) {
    LIBBLU_INFO(
      "Parsing guessing error found, "
      "restart parsing with corrected parameters.\n"
    );

    /* Quick exit. */
    destroyH264ParametersHandler(handle);
    return 0;
  }

  if (0 < handle->curProgParam.curNbSlices) {
    /* Process remaining pending slices. */
    if (processCompleteAccessUnit(handle, settings->options) < 0)
      goto free_return;
  }

  if (!handle->curProgParam.nbPics)
    goto free_return; /* Number of complete pictures read equals zero. */

  if (handle->curProgParam.nbPics == 1) {
    /* Only one picture = Still picture. */
    LIBBLU_INFO("Stream is detected as still picture.\n");
    setStillPicture(handle->esms);
  }

  /* Complete Buffering Period SEI computation if needed. */
#if 0
  if (completeH264SeiBufferingPeriodComputation(handle, h264Infos) < 0)
    return -1;
#endif

  lbc_printf(" === Parsing finished with success. ===              \n");

  /* [u8 endMarker] */
  if (writeByte(handle->esmsFile, ESMS_SCRIPT_END_MARKER) < 0)
    goto free_return;

  /* Display infos : */
  duration = (handle->esms->endPts - handle->esms->ptsRef) / MAIN_CLOCK_27MHZ;

  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: Video/H.264-AVC, %ux%u%c, ",
    handle->sequenceParametersSet.data.FrameWidth,
    handle->sequenceParametersSet.data.FrameHeight,
    (handle->sequenceParametersSet.data.frameMbsOnly ? 'p' : 'i')
  );
  lbc_printf("%.3f Im/s.\n", handle->curProgParam.frameRate);
  lbc_printf("Stream Duration: %02lu:%02lu:%02lu\n",
    (duration / 60) / 60, (duration / 60) % 60, duration % 60
  );
  lbc_printf("=======================================================================================\n");

  if (completeH264ParametersHandler(handle, settings) < 0)
    goto free_return;
  destroyH264ParametersHandler(handle);

  return 0;

free_return:
  destroyH264ParametersHandler(handle);

  return -1;
}

