#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "h264_parser.h"

/** \~english
 * \brief Parse NALU header and initialize the NALU deserializer with it.
 *
 * \param param H.264 parsing handle.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int _initNALU(
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
    && 0x000001 != nextUint24(param->file.inputFile)
    && 0x00000001 != nextUint32(param->file.inputFile)
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
    && 0x000001 != nextUint24(param->file.inputFile)
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
  param->file.nal_ref_idc = value;

  /* [u5 nal_unit_type] */
  if (readBits(param->file.inputFile, &value, 5) < 0)
    return -1;
  param->file.nal_unit_type = value;

  LIBBLU_H264_DEBUG_NAL(
    "0x%08" PRIX64 " === NAL Unit - %s ===\n",
    nalStartOffset,
    H264NalUnitTypeStr(getNalUnitType(param))
  );

  LIBBLU_H264_DEBUG_NAL(
    " -> nal_ref_idc: %u (%s).\n",
    getNalRefIdc(param),
    H264NalRefIdcValueStr(getNalRefIdc(param))
  );

  LIBBLU_H264_DEBUG_NAL(
    " -> nal_unit_type: %u.\n",
    getNalUnitType(param),
    H264NalUnitTypeStr(getNalUnitType(param))
  );

  if (
    NAL_UNIT_TYPE_PREFIX_NAL_UNIT == getNalUnitType(param)
    || NAL_UNIT_TYPE_CODED_SLICE_EXT == getNalUnitType(param)
    || NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT == getNalUnitType(param)
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

  /* Initializing Access Unit NALs collector new cell: */
  if (NULL == (auNalUnitCell = createNewNalCell(param, param->file.nal_unit_type)))
    return -1;
  auNalUnitCell->startOffset = nalStartOffset;

  return 0;
}

static int _parseH264RbspTrailingBits(
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

static inline bool _completePicturePresent(
  const H264ParametersHandlerPtr handle
)
{
  const H264CurrentProgressParam *param = &handle->curProgParam;

  return
    (param->auContent.bottomFieldPresent && param->auContent.topFieldPresent)
    || param->auContent.framePresent
  ;
}

static inline bool _complementaryFieldPairPresent(
  const H264ParametersHandlerPtr handle
)
{
  const H264CurrentProgressParam *param = &handle->curProgParam;

  return
    param->auContent.bottomFieldPresent
    && param->auContent.topFieldPresent
  ;
}

static int32_t _computePicOrderCntType0(
  H264ParametersHandlerPtr handle
)
{
  /* Mode 0 - 8.2.1.1 Decoding process for picture order count type 0 */
  /* Input: PicOrderCntMsb of previous picture */
  int32_t MaxPicOrderCntLsb;
  int32_t prevPicOrderCntMsb, prevPicOrderCntLsb;
  int32_t PicOrderCntMsb;
  int32_t TopFieldOrderCnt, BottomFieldOrderCnt;
  int32_t PicOrderCnt;

  const H264SPSDataParameters *sps       = &handle->sequenceParametersSet.data;
  const H264SliceHeaderParameters *slice = &handle->slice.header;
  H264LastPictureProperties *lstPic      = &handle->curProgParam.lstPic;

  MaxPicOrderCntLsb = (int32_t) sps->MaxPicOrderCntLsb;

  /* Derivation of variables prevPicOrderCntMsb and prevPicOrderCntLsb : */
  if (slice->isIdrPic) {
    prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
  }
  else {
    if (lstPic->presenceOfMemManCtrlOp5) {
      if (!lstPic->isBottomField) {
        prevPicOrderCntMsb = 0;
        prevPicOrderCntLsb = lstPic->TopFieldOrderCnt;
      }
      else {
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
      }
    }
    else {
      prevPicOrderCntMsb = lstPic->PicOrderCntMsb;
      prevPicOrderCntLsb = lstPic->pic_order_cnt_lsb;
    }
  }

  /* Derivation of PicOrderCntMsb of the current picture ([1] 8-3) : */
  if (
    (slice->pic_order_cnt_lsb < prevPicOrderCntLsb)
    && (
      (prevPicOrderCntLsb - slice->pic_order_cnt_lsb)
      >= (MaxPicOrderCntLsb / 2)
    )
  ) {
    PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
  }
  else if (
    (slice->pic_order_cnt_lsb > prevPicOrderCntLsb)
    && (
      (slice->pic_order_cnt_lsb - prevPicOrderCntLsb)
      > (MaxPicOrderCntLsb / 2)
    )
  ) {
    PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
  }
  else {
    PicOrderCntMsb = prevPicOrderCntMsb;
  }

  /* Derivation of TopFieldOrderCnt/BottomFieldOrderCnt ([1] 8-4, 8-5): */
  TopFieldOrderCnt = lstPic->TopFieldOrderCnt;
  BottomFieldOrderCnt = 0;

  if (!slice->bottom_field_flag)
    TopFieldOrderCnt = PicOrderCntMsb + slice->pic_order_cnt_lsb;

  if (!slice->field_pic_flag)
    BottomFieldOrderCnt = TopFieldOrderCnt + slice->delta_pic_order_cnt_bottom;
  else
    BottomFieldOrderCnt = PicOrderCntMsb + slice->pic_order_cnt_lsb;

  /* Derivation of PicOrderCnt for the current picture ([1] 8.2.1, 8-1) */
  /* TODO: Check in case of complementary field pictures */
  if (!slice->field_pic_flag || _complementaryFieldPairPresent(handle))
    PicOrderCnt = MIN(TopFieldOrderCnt, BottomFieldOrderCnt);
  else if (!slice->bottom_field_flag)
    PicOrderCnt = TopFieldOrderCnt;
  else
    PicOrderCnt = BottomFieldOrderCnt;

  /* Update last picture properties */
  lstPic->TopFieldOrderCnt  = TopFieldOrderCnt;
  lstPic->PicOrderCntMsb    = PicOrderCntMsb;
  lstPic->pic_order_cnt_lsb = slice->pic_order_cnt_lsb;

  /* fprintf(
    stderr, "isIdrPic: %d, PicOrderCnt: %d, MaxPicOrderCntLsb: %d, BottomFieldOrderCnt: %d, TopFieldOrderCnt: %d, PicOrderCntMsb: %d, pic_order_cnt_lsb: %d\n",
    slice->isIdrPic,
    PicOrderCnt,
    MaxPicOrderCntLsb,
    BottomFieldOrderCnt,
    TopFieldOrderCnt,
    PicOrderCntMsb,
    slice->pic_order_cnt_lsb
  ); */

  return PicOrderCnt;
}

static int32_t calcCurPicOrderCnt(
  H264ParametersHandlerPtr handle
)
{
  int32_t PicOrderCnt;

  const H264SPSDataParameters *sps = &handle->sequenceParametersSet.data;

  assert(handle->sequenceParametersSetPresent);

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

  switch (sps->pic_order_cnt_type) {
  case 0x0:
    /* Mode 0 - 8.2.1.1 Decoding process for picture order count type 0 */
    PicOrderCnt = _computePicOrderCntType0(handle);
    break;

  case 0x1:
    /* TODO */
    LIBBLU_TODO_MSG(
      "Unsupported 'pic_order_cnt_type' mode 0x1, unable to compute "
      "PicOrderCnt.\n"
    );

  default:
    /* TODO */
    LIBBLU_TODO_MSG(
      "Unsupported 'pic_order_cnt_type' mode %" PRIu8 ", unable to compute "
      "PicOrderCnt.\n",
      sps->pic_order_cnt_type
    );
  }

  if (PicOrderCnt < 0) {
    LIBBLU_H264_ERROR_RETURN(
      "Broken Picture Order Count on picture %" PRId64 ", "
      "computation result is negative (PicOrderCnt = %" PRId32 ").\n",
      handle->curProgParam.nbPics,
      PicOrderCnt
    );
  }

  return PicOrderCnt;
}

static int _computeAuPicOrderCnt(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
)
{
  int64_t PicOrderCnt;

  H264SliceHeaderParameters slice_header;
  H264LastPictureProperties *lastPicProp;

  assert(NULL != handle);

  if ((PicOrderCnt = calcCurPicOrderCnt(handle)) < 0)
    return -1;

  if (
    handle->curProgParam.half_PicOrderCnt
    && (PicOrderCnt & 0x1)
    && _completePicturePresent(handle)
  ) {
    /* Unexpected Odd PicOrderCnt, value shall not be divided. */
    if (options.second_pass)
      LIBBLU_H264_ERROR_RETURN(
        "Unexpected odd picture order count value, "
        "values are broken.\n"
      );

    handle->curProgParam.half_PicOrderCnt = false;
    handle->curProgParam.restartRequired = true;
    LIBBLU_H264_DEBUG_AU_TIMINGS("DISABLE HALF\n");
  }

  slice_header = handle->slice.header;

  int64_t DivPicOrderCnt = PicOrderCnt >> handle->curProgParam.half_PicOrderCnt;

  LIBBLU_H264_DEBUG_AU_TIMINGS(
    "PicOrderCnt=%" PRId64 " DivPicOrderCnt=%" PRId64 "\n",
    PicOrderCnt,
    DivPicOrderCnt
  );

  lastPicProp = &handle->curProgParam.lstPic;
  lastPicProp->presenceOfMemManCtrlOp5 = slice_header.presenceOfMemManCtrlOp5;
  lastPicProp->isBottomField = slice_header.bottom_field_flag;
  lastPicProp->PicOrderCnt = PicOrderCnt;

  handle->curProgParam.PicOrderCnt = DivPicOrderCnt;

  return 0;
}

static int _setBufferingInformationsAccessUnit(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options,
  uint64_t pts,
  uint64_t dts
)
{
  /** Collecting CPB removal time and DPB output time of AU.
   * According to Rec. ITU-T H.222, if the AVC video stream provides
   * insufficient information to determine the CPB removal time and the
   * DPB output time of Access Units, these values shall be taken from,
   * respectively, DTS and PTS timestamps values of the looked AU.
   */
  uint64_t cpb_removal_time, dpb_output_time;
  if (!options.disable_HRD_verifier) {
    cpb_removal_time = handle->curProgParam.auCpbRemovalTime;
    dpb_output_time = handle->curProgParam.auDpbOutputTime;
  }
  else {
    cpb_removal_time = dts * 300;
    dpb_output_time  = pts * 300;
  }

  EsmsPesPacketExtData param = (EsmsPesPacketExtData) {
    .h264.cpb_removal_time = cpb_removal_time,
    .h264.dpb_output_time  = dpb_output_time
  };

  return setExtensionDataPesPacketEsmsHandler(
    handle->esms, param
  );
}

/** \~english
 * \brief Return true if the current NALU corresponds to the start of a new
 * Access Unit.
 *
 * \param curState Current parsing state.
 * \param nal_unit_type Current NALU nal_unit_type field value.
 * \return true The current NALU is the start of a new Access Unit.
 * \return false The current NALU is not the start of a new Access Unit.
 *
 * Conditions are defined in 7.4.1.2.3, checks for the presence of specific
 * nal_unit_type. If one AU starting nal_unit_type as already been reached,
 * waits for parsing of at least one VCL NALU of a primary coded picture as
 * defined in 7.4.1.2.4.
 */
bool _isStartOfANewAU(
  H264CurrentProgressParam *curState,
  H264NalUnitTypeValue nal_unit_type
)
{
  bool startAUNaluType;

  if (!curState->reachVclNaluOfaPrimCodedPic) {
    /* No VCL NALU of primary coded picture as been reached. */
    return false;
  }

  switch (nal_unit_type) {
  case NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION:
  case NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET:
  case NAL_UNIT_TYPE_PIC_PARAMETER_SET:
  case NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER:
  case NAL_UNIT_TYPE_PREFIX_NAL_UNIT:
  case NAL_UNIT_TYPE_SUBSET_SEQUENCE_PARAMETER_SET:
  case NAL_UNIT_TYPE_DEPTH_PARAMETER_SET:
  case NAL_UNIT_TYPE_RESERVED_17:
  case NAL_UNIT_TYPE_RESERVED_18:
    startAUNaluType = true;
    break;

  default:
    startAUNaluType = false;
  }

  if (startAUNaluType)
    curState->reachVclNaluOfaPrimCodedPic = false;

  return startAUNaluType;
}

/** \~english
 * \brief Compute the duration of the pictures to determine the DTS value
 * incrementation.
 *
 * \param handle
 * \return int64_t
 *
 * Based on tsMuxer.
 */
static int64_t _computeDtsIncrement(
  const H264ParametersHandlerPtr handle
)
{
  int64_t frameDuration = handle->curProgParam.frameDuration;
  int64_t fieldDuration = handle->curProgParam.fieldDuration;

  if (handle->sei.picTimingValid) {
    /* Presence of picture timing SEI message */

    switch (handle->sei.picTiming.pic_struct) {
    case H264_PIC_STRUCT_TOP_FLWD_BOT_TOP:
    case H264_PIC_STRUCT_BOT_FLWD_TOP_BOT:
      return frameDuration + fieldDuration;
    case H264_PIC_STRUCT_DOUBLED_FRAME:
      return 2 * frameDuration;
    case H264_PIC_STRUCT_TRIPLED_FRAME:
      return 3 * frameDuration;
    default:
      break;
    }
  }

  /* Otherwise, every picture is considered as during one frame. */
  return frameDuration;
}

static void setEsmsPtsRef(
  EsmsHandlerPtr script,
  const H264CurrentProgressParam *curProgParam
)
{
  uint64_t reference = (uint64_t) (
    curProgParam->initDecPicNbCntShift
    * curProgParam->frameDuration
  );

  script->PTS_reference = DIV_ROUND_UP(reference, 300ull);

  LIBBLU_H264_DEBUG_AU_TIMINGS(
    "Set PTS_reference: %" PRIu64 " ticks@90kHz %" PRIu64 " ticks@27MHz "
    "(initDecPicNbCntShift=%" PRIu64 ", frame duration=%" PRIu64 ").\n",
    script->PTS_reference,
    reference,
    curProgParam->initDecPicNbCntShift,
    curProgParam->frameDuration
  );
}

static int64_t _computeStreamPicOrderCnt(
  H264ParametersHandlerPtr handle
)
{
  H264CurrentProgressParam *cp = &handle->curProgParam;

  if (/* handle->slice.header.isIdrPic && */ 0 == cp->PicOrderCnt && 0 < cp->LastMaxStreamPicOrderCnt) {
    cp->MaxStreamPicOrderCnt = cp->LastMaxStreamPicOrderCnt + 1;
    cp->LastMaxStreamPicOrderCnt = 0;
  }

  LIBBLU_H264_DEBUG_AU_TIMINGS(
    "PicOrderCnt=%" PRId64 " MaxStreamPicOrderCnt=%" PRId64 "\n",
    cp->PicOrderCnt,
    cp->MaxStreamPicOrderCnt
  );

  int64_t StreamPicOrderCnt = cp->PicOrderCnt + cp->MaxStreamPicOrderCnt;

  cp->LastMaxStreamPicOrderCnt = MAX(
    cp->LastMaxStreamPicOrderCnt,
    StreamPicOrderCnt
  );

  return StreamPicOrderCnt;
}

static int _initProperties(
  EsmsHandlerPtr esms,
  H264CurrentProgressParam *curProgParam,
  const H264SPSDataParameters *sps,
  H264ConstraintsParam constraints
)
{
  /* Convert to BDAV syntax. */
  HdmvVideoFormat videoFormat = getHdmvVideoFormat(
    sps->FrameWidth,
    sps->FrameHeight,
    !sps->frame_mbs_only_flag
  );
  if (!videoFormat)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Resolution %ux%u%c is unsupported.\n",
      sps->FrameWidth,
      sps->FrameHeight,
      (sps->frame_mbs_only_flag) ? "p" : "i"
    );

  if (curProgParam->frameRate < 0)
    LIBBLU_H264_ERROR_RETURN("Missing Frame-rate information (not present in bitstream).\n");

  HdmvFrameRateCode frameRate = getHdmvFrameRateCode(curProgParam->frameRate);
  if (!frameRate)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Frame-rate %.3f is unsupported.\n",
      curProgParam->frameRate
    );

  esms->prop = (LibbluESProperties) {
    .coding_type   = STREAM_CODING_TYPE_AVC,
    .video_format  = videoFormat,
    .frame_rate    = frameRate,
    .profile_IDC   = sps->profile_idc,
    .level_IDC     = sps->level_idc
  };

  LibbluESH264SpecProperties *param = esms->fmt_prop.h264;
  param->constraint_flags = valueH264ContraintFlags(sps->constraint_set_flags);
  if (
    sps->vui_parameters_present_flag
    && sps->vui_parameters.nal_hrd_parameters_present_flag
  ) {
    const H264HrdParameters *hrd = &sps->vui_parameters.nal_hrd_parameters;
    param->CpbSize = hrd->schedSel[hrd->cpb_cnt_minus1].CpbSize;
    param->BitRate = hrd->schedSel[0].BitRate;
    esms->bitrate = hrd->schedSel[0].BitRate;
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
    param->CpbSize = 1200 * constraints.MaxCPB;
    param->BitRate = constraints.brNal;
    esms->bitrate = constraints.brNal;
  }

  curProgParam->frameDuration = MAIN_CLOCK_27MHZ / curProgParam->frameRate;
  curProgParam->fieldDuration = curProgParam->frameDuration / 2;

  setEsmsPtsRef(esms, curProgParam);
  return 0;
}

static int _registerAccessUnitNALUs(
  H264ParametersHandlerPtr handle
)
{
  H264CurrentProgressParam *curProgParam = &handle->curProgParam;

  if (!curProgParam->curAccessUnit.nbUsedNalus)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected empty access unit, no NALU received.\n"
    );

  size_t curInsertingOffset = 0;
  for (unsigned idx = 0; idx < curProgParam->curAccessUnit.nbUsedNalus; idx++) {
    const H264AUNalUnitPtr auNalUnitCell = &curProgParam->curAccessUnit.nalus[idx];

    if (auNalUnitCell->replace) {
      size_t writtenBytes = 0;

      switch (auNalUnitCell->nal_unit_type) {
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

      case NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET:
        /* Update SPS */
        writtenBytes = appendH264SequenceParametersSet(
          handle,
          curInsertingOffset,
          &auNalUnitCell->replacementParameters.sps
        );
        break;

      default:
        LIBBLU_H264_ERROR_RETURN(
          "Unknown replacementParameters NAL unit type code: 0x%" PRIx8 ".\n",
          auNalUnitCell->nal_unit_type
        );
      }

      if (0 == writtenBytes)
        return -1; /* Zero byte written = Error. */

      /* TODO */
      curInsertingOffset += writtenBytes /* written bytes */;
    }
    else {
      assert(curInsertingOffset <= UINT32_MAX);

      int ret = appendAddPesPayloadCommandEsmsHandler(
        handle->esms,
        handle->esmsSrcFileIdx,
        curInsertingOffset,
        auNalUnitCell->startOffset,
        auNalUnitCell->length
      );
      if (ret < 0)
        return -1;

      curInsertingOffset += auNalUnitCell->length;
    }
  }

  /* Saving biggest picture AU size (for CPB computations): */
  if (curProgParam->largestAUSize < curInsertingOffset) {
    /* Biggest I picture AU. */
    if (is_I_H264SliceTypeValue(handle->slice.header.slice_type))
      curProgParam->largestIPicAUSize = MAX(
        curProgParam->largestIPicAUSize,
        curInsertingOffset
      );

    /* Biggest picture AU. */
    curProgParam->largestAUSize = curInsertingOffset;
  }

  return 0;
}

static int _processPESFrame(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
)
{
  assert(NULL != handle);

  /* Check Access Unit compliance : */
  if (
    !handle->accessUnitDelimiterValid
    || !handle->sequenceParametersSetGopValid
    || !handle->slicePresent
  ) {
    LIBBLU_H264_CK_FAIL_RETURN(
      "Uncompliant access unit, shall contain at least a AUD, "
      "a SPS and one slice.\n "
    );
  }

  const H264SPSDataParameters *sps = &handle->sequenceParametersSet.data;
  if (!sps->vui_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Missing VUI from AU SPS, unable to complete access unit.\n"
    );

  H264CurrentProgressParam *cur = &handle->curProgParam;

  if (cur->nbSlicesInPic < handle->constraints.sliceNb) {
    LIBBLU_H264_CK_FAIL_RETURN(
      "Pending access unit contains %u slices (at least %u expected).\n",
      cur->nbSlicesInPic,
      handle->constraints.sliceNb
    );
  }

  /* Check B pictures amount */
  if (is_B_H264SliceTypeValue(handle->slice.header.slice_type)) {
    if (handle->slice.header.bottom_field_flag)
      cur->nbConsecutiveBPics++;

    if (
      handle->constraints.consecutiveBPicNb
      < cur->nbConsecutiveBPics
    ) {
      LIBBLU_H264_CK_FAIL_RETURN(
        "Too many consecutive B pictures "
        "(found %u pictures, expected no more than %u).\n",
        cur->nbConsecutiveBPics,
        handle->constraints.consecutiveBPicNb
      );
    }
  }
  else
    cur->nbConsecutiveBPics = 0;

  if (!cur->initializedParam) {
    /* Set pictures counting mode (apply correction if 2nd pass) */
    if (!options.second_pass) {
      cur->half_PicOrderCnt = true;
        // By default, expect to divide by two the PicOrderCnt.
    }
    else {
      cur->half_PicOrderCnt = options.half_PicOrderCnt;
      cur->initDecPicNbCntShift = options.initDecPicNbCntShift;
    }

    if (_initProperties(handle->esms, &handle->curProgParam, sps, handle->constraints) < 0)
      return -1;

    cur->initializedParam = true;
  }

  int64_t StreamPicOrderCnt = _computeStreamPicOrderCnt(handle);
  int64_t pts_diff_shift = (
    StreamPicOrderCnt - cur->decPicNbCnt
    + cur->initDecPicNbCntShift
  );

  if (pts_diff_shift < 0) {
    LIBBLU_H264_DEBUG_AU_TIMINGS(
      "StreamPicOrderCnt=%" PRId64 " ptsDiffShift=%" PRId64 "\n",
      StreamPicOrderCnt,
      pts_diff_shift
    );

    if (options.second_pass)
      LIBBLU_H264_ERROR_RETURN(
        "Broken picture order count, negative decoding delay.\n"
      );

    cur->initDecPicNbCntShift += - pts_diff_shift;
    cur->restartRequired = true;

    LIBBLU_H264_DEBUG_AU_TIMINGS(
      "ADJUST initDecPicNbCntShift=%" PRId64 "\n",
      cur->initDecPicNbCntShift
    );
  }

  int64_t dts = cur->lstPic.dts + cur->lstPic.duration;
  int64_t pts = dts + pts_diff_shift *cur->frameDuration;
  uint64_t dts_90kHz = (cur->lstPic.dts / 300) + DIV_ROUND_UP(cur->lstPic.duration, 300);
  uint64_t pts_90kHz = dts_90kHz + DIV_ROUND_UP(pts_diff_shift *cur->frameDuration, 300);

  LIBBLU_H264_DEBUG_AU_TIMINGS(" -> PTS: %" PRIu64 "@90kHz %" PRIu64 "@27MHz\n", pts_90kHz, pts);
  LIBBLU_H264_DEBUG_AU_TIMINGS(" -> DTS: %" PRIu64 "@90kHz %" PRIu64 "@27MHz\n", dts_90kHz, dts);
  LIBBLU_H264_DEBUG_AU_TIMINGS(" -> StreamPicOrderCnt: %" PRId32 "\n", StreamPicOrderCnt);

  cur->lstPic.dts += cur->lstPic.duration;
  cur->lstPic.duration = _computeDtsIncrement(handle);
  cur->decPicNbCnt++;

  int ret = initVideoPesPacketEsmsHandler(
    handle->esms,
    handle->slice.header.slice_type,
    pts_90kHz != dts_90kHz,
    pts_90kHz,
    dts_90kHz
  );
  if (ret < 0)
    return -1;

  if (_setBufferingInformationsAccessUnit(handle, options, pts_90kHz, dts_90kHz) < 0)
    return -1;

  if (_registerAccessUnitNALUs(handle) < 0)
    return -1;

  if (writePesPacketEsmsHandler(handle->esmsFile, handle->esms) < 0)
    return -1;

  /* Clean H264AUNalUnit : */
  if (cur->curAccessUnit.inProcessNalu) {
    /* Replacing current cell at list top. */
    cur->curAccessUnit.nalus[0] =
      cur->curAccessUnit.nalus[
        cur->curAccessUnit.nbUsedNalus
      ]
    ;
  }

  cur->curAccessUnit.size = 0;
  cur->curAccessUnit.nbUsedNalus = 0;

  /* Reset settings for the next Access Unit : */
  resetH264ParametersHandler(handle);

  handle->esms->PTS_final = pts_90kHz;

  return 0;
}

/* static int _initHr */

static bool _areEnoughDataToInitHrdVerifier(
  const H264ParametersHandlerPtr handle
)
{

  if (!handle->sequenceParametersSetGopValid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing valid SPS.\n");

  if (!handle->slicePresent)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing slice header.\n");

  if (!handle->sei.picTimingValid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing Picture Timing SEI message.\n");

  if (!handle->sei.bufferingPeriodGopValid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing Buffering Period SEI message.\n");

  return true;
}

static bool _areSupportedParametersToInitHrdVerifier(
  const H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions *options
)
{
  return checkH264CpbHrdVerifierAvailablity(
    &handle->curProgParam,
    options,
    &handle->sequenceParametersSet.data
  );
}

static int _checkInitializationHrdVerifier(
  H264HrdVerifierContextPtr *dst,
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions *options,
  bool *disable
)
{
  /* Check if enough data is present to process HRD Verifier */
  if (!_areEnoughDataToInitHrdVerifier(handle))
    goto disable_hrd_verifier;

  /* Check context parameter support */
  if (!_areSupportedParametersToInitHrdVerifier(handle, options))
    goto disable_hrd_verifier;

  /* Initialize HRD Verifier context */
  H264HrdVerifierContextPtr ctx = createH264HrdVerifierContext(
    options,
    &handle->sequenceParametersSet.data,
    &handle->constraints
  );
  if (NULL == ctx)
    return -1;

  *dst = ctx;
  return 0;

disable_hrd_verifier:
  *disable = true;
  return 0;
}

static int _processHrdVerifierAccessUnit(
  H264ParametersHandlerPtr handle,
  H264HrdVerifierContextPtr hrdVerifierCtx
)
{
  assert(hrdVerifierCtx->pesNbAlreadyProcessedBytes <= handle->curProgParam.curAccessUnit.size * 8);

  int64_t frameSize = handle->curProgParam.curAccessUnit.size * 8
    - hrdVerifierCtx->pesNbAlreadyProcessedBytes
  ;

  // return processAUH264HrdContext(
  //   hrdVerifierCtx,
  //   &handle->curProgParam,
  //   &handle->sequenceParametersSet.data,
  //   &handle->slice.header,
  //   &handle->sei.picTiming,
  //   &handle->sei.bufferingPeriod,
  //   &handle->constraints,
  //   handle->sei.bufferingPeriodValid,
  //   frameSize
  // );
  return processAUH264HrdContext(
    hrdVerifierCtx,
    handle,
    frameSize
  );
}

static int _checkAndProcessHrdVerifierAccessUnit(
  H264ParametersHandlerPtr handle,
  H264HrdVerifierContextPtr *hrdVerCtxPtr,
  LibbluESParsingSettings *settings
)
{
  if (NULL == *hrdVerCtxPtr) {
    /* Check availability and initialize HRD Verifier */
    bool disable = false;

    if (_checkInitializationHrdVerifier(hrdVerCtxPtr, handle, &settings->options, &disable) < 0)
      return -1;

    if (disable) { // Unable to initialize, disabled HRD Verifier.
      settings->options.disable_HRD_verifier = true;
      return 0;
    }
  }

  return _processHrdVerifierAccessUnit(handle, *hrdVerCtxPtr);
}

static int _processAccessUnit(
  H264ParametersHandlerPtr handle,
  H264HrdVerifierContextPtr *hrdVerifierCtxPtr,
  LibbluESParsingSettings *settings
)
{
  assert(NULL != hrdVerifierCtxPtr);

  /* Check HRD Verifier */
  if (!settings->options.disable_HRD_verifier) {
    if (_checkAndProcessHrdVerifierAccessUnit(handle, hrdVerifierCtxPtr, settings) < 0)
      return -1;
  }

  /* Processing PES frame cutting */
  if (_completePicturePresent(handle)) {
    if (_processPESFrame(handle, settings->options) < 0)
      return -1;

    if (NULL != *hrdVerifierCtxPtr)
      (*hrdVerifierCtxPtr)->pesNbAlreadyProcessedBytes = 0;

    if (
      handle->curProgParam.restartRequired
      && H264_PICTURES_BEFORE_RESTART <= handle->curProgParam.nbPics
    ) {
      settings->askForRestart = true;
    }
  }
  else {
    handle->sei.bufferingPeriodValid = false;
    handle->sei.picTimingValid = false;
  }

  return 0;
}

int decodeH264AccessUnitDelimiter(
  H264ParametersHandlerPtr handle
)
{
  /* access_unit_delimiter_rbsp() */
  uint32_t value;

  H264AccessUnitDelimiterParameters AUD_param;

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Access Unit Delimiter NAL unit type (receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  /* [u3 primary_pic_type] */
  if (readBitsNal(handle, &value, 3) < 0)
    return -1;
  AUD_param.primary_pic_type = value;

  /* rbsp_trailing_bits() */
  if (_parseH264RbspTrailingBits(handle) < 0)
    return -1;

  if (
    !handle->accessUnitDelimiterPresent
    || !handle->accessUnitDelimiterValid
  ) {
    if (checkH264AccessUnitDelimiterCompliance(&handle->warningFlags, AUD_param) < 0)
      return -1;
  }
  else {
    if (5 < handle->curProgParam.lstNaluType) {
      bool is_constant = constantH264AccessUnitDelimiterCheck(
        handle->accessUnitDelimiter, AUD_param
      );

      if (is_constant)
        handle->curProgParam.presenceOfUselessAccessUnitDelimiter = true;
      else {
        LIBBLU_H264_ERROR_RETURN(
          "Presence of an unexpected access unit delimiter NALU "
          "(Rec. ITU-T H.264 7.4.1.2.3 violation).\n"
        );
      }
    }
  }

  handle->accessUnitDelimiter = AUD_param; /* Update */
  handle->accessUnitDelimiterPresent = true;
  handle->accessUnitDelimiterValid = true;

  return 0;
}

static int _buildScalingList(
  H264ParametersHandlerPtr handle,
  uint8_t *scalingList,
  const unsigned sizeOfList,
  bool *useDefaultScalingMatrix
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

static int _parseH264HrdParameters(
  H264ParametersHandlerPtr handle,
  H264HrdParameters *param
)
{
  /* hrd_parameters() */
  /* Rec.ITU-T H.264 (06/2019) - Annex E */
  uint32_t value;
  unsigned ShedSelIdx;

  /* [ue cpb_cnt_minus1] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpb_cnt_minus1 = value;

  if (31 < param->cpb_cnt_minus1)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected cpb_cnt_minus1 value (%u, expected at least 31).\n",
      param->cpb_cnt_minus1
    );

  /* [u4 bit_rate_scale] */
  if (readBitsNal(handle, &value, 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->bit_rate_scale = value;

  /* [u4 cpb_size_scale] */
  if (readBitsNal(handle, &value, 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpb_size_scale = value;

  memset(param->schedSel, 0x00, (param->cpb_cnt_minus1 + 1) * sizeof(H264SchedSel));

  for (ShedSelIdx = 0; ShedSelIdx <= param->cpb_cnt_minus1; ShedSelIdx++) {
    /* [ue bit_rate_value_minus1[ShedSelIdx]] */
    if (readExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->schedSel[ShedSelIdx].bit_rate_value_minus1 = value;

    param->schedSel[ShedSelIdx].BitRate =
      ((int64_t) param->schedSel[ShedSelIdx].bit_rate_value_minus1 + 1)
      << (6 + param->bit_rate_scale)
    ;

    /* [ue cpb_size_value_minus1[ShedSelIdx]] */
    if (readExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->schedSel[ShedSelIdx].cpb_size_value_minus1 = value;

    param->schedSel[ShedSelIdx].CpbSize =
      ((int64_t) param->schedSel[ShedSelIdx].cpb_size_value_minus1 + 1)
      << (4 + param->cpb_size_scale)
    ;

    /* [b1 cbr_flag[ShedSelIdx]] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->schedSel[ShedSelIdx].cbr_flag = value;
  }

  /* [u5 initial_cpb_removal_delay_length_minus1] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->initial_cpb_removal_delay_length_minus1 = value;

  /* [u5 cpb_removal_delay_length_minus1] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpb_removal_delay_length_minus1 = value;

  /* [u5 dpb_output_delay_length_minus1] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->dpb_output_delay_length_minus1 = value;

  /* [u5 time_offset_length] */
  if (readBitsNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->time_offset_length = value;

  return 0;
}

/** \~english
 * \brief max_num_reorder_frames / max_dec_frame_buffering fields default
 * value.
 *
 * \param handle
 * \param sps
 * \return unsigned
 */
static unsigned _defaultMaxNumReorderFramesDecFrameBuffering(
  const H264ParametersHandlerPtr handle,
  const H264SPSDataParameters *sps
)
{
  unsigned MaxDpbMbs = handle->constraints.MaxDpbMbs;

  bool equalToZero = (
    sps->constraint_set_flags.set3
    && (
      sps->profile_idc == H264_PROFILE_CAVLC_444_INTRA
      || sps->profile_idc == H264_PROFILE_SCALABLE_HIGH
      || sps->profile_idc == H264_PROFILE_HIGH
      || sps->profile_idc == H264_PROFILE_HIGH_10
      || sps->profile_idc == H264_PROFILE_HIGH_422
      || sps->profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
    )
  );

  if (equalToZero)
    return 0;

  /* Equation A.3.1.h) MaxDpbFrames */
  return MIN(
    MaxDpbMbs / (sps->PicWidthInMbs *sps->FrameHeightInMbs),
    16
  );
}

/** \~english
 * \brief
 *
 * \param vui
 *
 * Default values according to [1] E.2.1 VUI parameters semantics.
 */
static void _setDefaultH264VuiParameters(
  H264VuiParameters *dst
)
{
  *dst = (H264VuiParameters) {
    .aspect_ratio_idc = H264_ASPECT_RATIO_IDC_UNSPECIFIED,
    .overscan_appropriate_flag = H264_OVERSCAN_APPROP_UNSPECIFIED,
    .video_format = H264_VIDEO_FORMAT_UNSPECIFIED,
    .colour_description = {
      .colour_primaries = H264_COLOR_PRIM_UNSPECIFIED,
      .transfer_characteristics = H264_TRANS_CHAR_UNSPECIFIED,
      .matrix_coefficients = H264_MATRX_COEF_UNSPECIFIED
    },
    .low_delay_hrd_flag = true, // 1 - fixed_frame_rate_flag
    .bistream_restrictions = {
      .motion_vectors_over_pic_boundaries_flag = true,
      .max_bytes_per_pic_denom = 2,
      .max_bits_per_mb_denom = 1,
      .log2_max_mv_length_horizontal = 15,
      .log2_max_mv_length_vertical = 15,
      // max_num_reorder_frames set after parsing
      // max_dec_frame_buffering set after parsing
    }
  };
}

static int _parseH264VuiParameters(
  H264ParametersHandlerPtr handle,
  H264VuiParameters *param
)
{
  /* vui_parameters() */
  /* Rec.ITU-T H.264 (06/2019) - Annex E */
  uint32_t value;

  /* [b1 aspect_ratio_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->aspect_ratio_info_present_flag = value;

  if (param->aspect_ratio_info_present_flag) {
    /* [u8 aspect_ratio_idc] */
    if (readBitsNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->aspect_ratio_idc = value;

    if (param->aspect_ratio_idc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
      /* [u16 sar_width] */
      if (readBitsNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->sar_width = value;

      /* [u16 sar_height] */
      if (readBitsNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->sar_height = value;
    }
  }

  /* [b1 overscan_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->overscan_info_present_flag = value;

  if (param->overscan_info_present_flag) {
    /* [b1 overscan_appropriate_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->overscan_appropriate_flag = value;
  }

  /* [b1 video_signal_type_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->video_signal_type_present_flag = value;

  if (param->video_signal_type_present_flag) {
    /* [u3 video_format] */
    if (readBitsNal(handle, &value, 3) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->video_format = value;

    /* [b1 video_full_range_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->video_full_range_flag = value;

    /* [b1 colour_description_present_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->colour_description_present_flag = value;

    if (param->colour_description_present_flag) {
      /* [u8 colour_primaries] */
      if (readBitsNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->colour_description.colour_primaries = value;

      /* [u8 transfer_characteristics] */
      if (readBitsNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->colour_description.transfer_characteristics = value;

      /* [u8 matrix_coefficients] */
      if (readBitsNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->colour_description.matrix_coefficients = value;
    }
  }

  /* [b1 chroma_loc_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->chroma_loc_info_present_flag = value;

  if (param->chroma_loc_info_present_flag) {
    /* [ue chroma_sample_loc_type_top_field] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->chroma_sample_loc_type_top_field = value;

    /* [ue chroma_sample_loc_type_bottom_field] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->chroma_sample_loc_type_bottom_field = value;
  }

  /* [b1 timing_info_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->timing_info_present_flag = value;

  if (param->timing_info_present_flag) {
    /* [u32 num_units_in_tick] */
    if (readBitsNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->num_units_in_tick = value;

    /* [u32 time_scale] */
    if (readBitsNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->time_scale = value;

    /* [b1 fixed_frame_rate_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->fixed_frame_rate_flag = value;

    param->FrameRate =
      (double) param->time_scale / (
        param->num_units_in_tick * 2
      )
    ;

    param->MaxFPS = DIV_ROUND_UP(
      param->time_scale,
      2 * param->num_units_in_tick
    );
  }

  /* [b1 nal_hrd_parameters_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->nal_hrd_parameters_present_flag = value;

  if (param->nal_hrd_parameters_present_flag) {
    if (_parseH264HrdParameters(handle, &param->nal_hrd_parameters) < 0)
      return -1;
  }

  /* [b1 vcl_hrd_parameters_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->vcl_hrd_parameters_present_flag = value;

  if (param->vcl_hrd_parameters_present_flag) {
    if (_parseH264HrdParameters(handle, &param->vcl_hrd_parameters) < 0)
      return -1;
  }

  param->CpbDpbDelaysPresentFlag =
    param->nal_hrd_parameters_present_flag
    || param->vcl_hrd_parameters_present_flag
  ;

  if (param->CpbDpbDelaysPresentFlag) {
    /* [b1 low_delay_hrd_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->low_delay_hrd_flag = value;
  }
  else {
    /* Default value */
    param->low_delay_hrd_flag = 1 - param->fixed_frame_rate_flag;
  }

  /* [b1 pic_struct_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->pic_struct_present_flag = value;

  /* [b1 bitstream_restriction_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->bitstream_restriction_flag = value;

  if (param->bitstream_restriction_flag) {
    /* [b1 motion_vectors_over_pic_boundaries_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.motion_vectors_over_pic_boundaries_flag = value;

    /* [ue max_bytes_per_pic_denom] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.max_bytes_per_pic_denom = value;

    /* [ue max_bits_per_mb_denom] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.max_bits_per_mb_denom = value;

    /* [ue log2_max_mv_length_horizontal] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.log2_max_mv_length_horizontal = value;

    /* [ue log2_max_mv_length_vertical] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.log2_max_mv_length_vertical = value;

    /* [ue max_num_reorder_frames] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.max_num_reorder_frames = value;

    /* [ue max_dec_frame_buffering] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bistream_restrictions.max_dec_frame_buffering = value;
  }

  return 0;
}

static void _updateH264VuiParameters(
  H264ParametersHandlerPtr handle,
  H264SPSDataParameters *sps
)
{
  H264VuiParameters *vui = &sps->vui_parameters;
  H264VuiVideoSeqBsRestrParameters *bs_restr = &vui->bistream_restrictions;

  /* Set saved frame-rate value. */
  handle->curProgParam.frameRate = -1;
  if (
    sps->vui_parameters_present_flag
    && vui->timing_info_present_flag
  ) {
    /* Frame rate information present */
    handle->curProgParam.frameRate = vui->FrameRate;
  }

  if (
    !sps->vui_parameters_present_flag
    || !vui->bitstream_restriction_flag
  ) {
    /* [1] E.2.1 VUI parameters semantics */
    unsigned nbFrames = _defaultMaxNumReorderFramesDecFrameBuffering(
      handle, sps
    );

    bs_restr->max_num_reorder_frames = nbFrames;
    bs_restr->max_dec_frame_buffering = nbFrames;
  }
}

static int _parseH264SequenceParametersSetData(
  H264ParametersHandlerPtr handle,
  H264SPSDataParameters *param
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
  param->profile_idc = value;

  /* Constraints flags : */
  /* [b1 constraint_set0_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraint_set_flags.set0 = value;

  /* [b1 constraint_set1_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraint_set_flags.set1 = value;

  /* [b1 constraint_set2_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraint_set_flags.set2 = value;

  /* [b1 constraint_set3_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraint_set_flags.set3 = value;

  /* [b1 constraint_set4_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraint_set_flags.set4 = value;

  /* [b1 constraint_set5_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->constraint_set_flags.set5 = value;

  /* [v2 reserved_zero_2bits] // 0b00 */
  if (readBitsNal(handle, &value, 2) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  /* Extra constraints flags : */
  param->constraint_set_flags.reservedFlags = value;

  /* [u8 level_idc] */
  if (readBitsNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->level_idc = value;

  /* [ue seq_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 5) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->seq_parameter_set_id = value;

  if (isHighScalableMultiviewH264ProfileIdcValue(param->profile_idc)) {
    /* High profiles linked-properties */

    /* [ue chroma_format_idc] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->chroma_format_idc = value;

    if (param->chroma_format_idc == H264_CHROMA_FORMAT_444) {
      /* [b1 separate_colour_plane_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->separate_colour_plane_flag = value;
    }
    else
      param->separate_colour_plane_flag = false;

    /* [ue bit_depth_luma_minus8] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bit_depth_luma_minus8 = value;

    /* [ue bit_depth_chroma_minus8] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->bit_depth_chroma_minus8 = value;

    /* [b1 qpprime_y_zero_transform_bypass_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->qpprime_y_zero_transform_bypass_flag = value;

    /* [b1 seq_scaling_matrix_present_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->seq_scaling_matrix_present_flag = value;

    if (param->seq_scaling_matrix_present_flag) {
      /* Initialization for comparison checks purposes : */
      memset(&param->seq_scaling_matrix, 0x0, sizeof(H264SeqScalingMatrix));

      for (i = 0; i < ((param->chroma_format_idc != H264_CHROMA_FORMAT_444) ? 8 : 12); i++) {
        /* [b1 seq_scaling_list_present_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->seq_scaling_matrix.seq_scaling_list_present_flag[i] = value;

        if (param->seq_scaling_matrix.seq_scaling_list_present_flag[i]) {
          if (i < 6) {
            ret = _buildScalingList(
              handle,
              param->seq_scaling_matrix.ScalingList4x4[i],
              16,
              &(param->seq_scaling_matrix.UseDefaultScalingMatrix4x4Flag[i])
            );
          }
          else {
            ret = _buildScalingList(
              handle,
              param->seq_scaling_matrix.ScalingList8x8[i - 6],
              64,
              &(param->seq_scaling_matrix.UseDefaultScalingMatrix8x8Flag[i - 6])
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
    param->chroma_format_idc = H264_CHROMA_FORMAT_420;
    param->separate_colour_plane_flag = false;
    param->bit_depth_luma_minus8 = 0;
    param->bit_depth_chroma_minus8 = 0;
    param->qpprime_y_zero_transform_bypass_flag = 0;
    param->seq_scaling_matrix_present_flag = false;
  }

  /* [ue log2_max_frame_num_minus4] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->log2_max_frame_num_minus4 = value;

  /* [ue pic_order_cnt_type] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->pic_order_cnt_type = value;

  if (2 < param->pic_order_cnt_type)
    LIBBLU_H264_ERROR_RETURN(
      "Unknown/Reserved pic_order_cnt_type = %u.\n",
      param->pic_order_cnt_type
    );

  /* Set default values (for test purposes) : */
  param->log2_max_pic_order_cnt_lsb_minus4 = 0;
  param->delta_pic_order_always_zero_flag = 0;
  param->offset_for_non_ref_pic = 0;
  param->offset_for_top_to_bottom_field = 0;
  param->num_ref_frames_in_pic_order_cnt_cycle = 0;

  if (param->pic_order_cnt_type == 0) {
    /* [ue log2_max_pic_order_cnt_lsb_minus4] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->log2_max_pic_order_cnt_lsb_minus4 = value;
  }
  else if (param->pic_order_cnt_type == 2) {
    /* [b1 delta_pic_order_always_zero_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->delta_pic_order_always_zero_flag = value;

    /* [se offset_for_non_ref_pic] */
    if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->offset_for_non_ref_pic = (int) value;

    /* [se offset_for_top_to_bottom_field] */
    if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->offset_for_top_to_bottom_field = (int) value;

    /* [se num_ref_frames_in_pic_order_cnt_cycle] */
    if (readSignedExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->num_ref_frames_in_pic_order_cnt_cycle = value;

    if (255 < param->num_ref_frames_in_pic_order_cnt_cycle)
      LIBBLU_H264_ERROR_RETURN(
        "'num_ref_frames_in_pic_order_cnt_cycle' = %u causes overflow "
        "(shall not exceed 255).\n",
        value
      );

    for (i = 0; i < param->num_ref_frames_in_pic_order_cnt_cycle; i++) {
      /* [se offset_for_ref_frame] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->offset_for_ref_frame[i] = (int) value;
    }
  }

  /* [ue max_num_ref_frames] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->max_num_ref_frames = value;

  /* [b1 gaps_in_frame_num_value_allowed_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->gaps_in_frame_num_value_allowed_flag = value;

  /* [ue pic_width_in_mbs_minus1] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->pic_width_in_mbs_minus1 = value;

  /* [ue pic_height_in_map_units_minus1] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->pic_height_in_map_units_minus1 = value;

  /* [b1 frame_mbs_only_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->frame_mbs_only_flag = value;

  if (!param->frame_mbs_only_flag) {
    /* [b1 mb_adaptive_frame_field_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->mb_adaptive_frame_field_flag = value;
  }
  else
    param->mb_adaptive_frame_field_flag = false;

  /* [b1 direct_8x8_inference_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->direct_8x8_inference_flag = value;

  /* [b1 frame_cropping_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->frame_cropping_flag = value;

  if (param->frame_cropping_flag) {
    /* [ue frame_crop_left_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frame_crop_offset.left = value;

    /* [ue frame_crop_right_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frame_crop_offset.right = value;

    /* [ue frame_crop_top_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frame_crop_offset.top = value;

    /* [ue frame_crop_bottom_offset] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->frame_crop_offset.bottom = value;
  }
  else {
    param->frame_crop_offset.left = 0;
    param->frame_crop_offset.right = 0;
    param->frame_crop_offset.top = 0;
    param->frame_crop_offset.bottom = 0;
  }

  /* [b1 vui_parameters_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->vui_parameters_present_flag = value;

  _setDefaultH264VuiParameters(&param->vui_parameters);

  if (param->vui_parameters_present_flag) {
    if (_parseH264VuiParameters(handle, &param->vui_parameters) < 0)
      return -1;
  }

  /* Derivate values computation : */
  switch (param->chroma_format_idc) {
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
  param->BitDepthLuma = param->bit_depth_luma_minus8 + 8;
  param->QpBdOffsetLuma = param->bit_depth_luma_minus8 * 6;

  param->BitDepthChroma = param->bit_depth_chroma_minus8 + 8;
  param->QpBdOffsetChroma = param->bit_depth_chroma_minus8 * 6;

  param->ChromaArrayType =
    (param->separate_colour_plane_flag) ? 0 : param->chroma_format_idc
  ;

  param->MaxFrameNum = 1 << (param->log2_max_frame_num_minus4 + 4);

  param->MbWidthC = 16 / param->SubWidthC;
  param->MbHeightC = 16 / param->SubHeightC;

  param->RawMbBits =
    (uint64_t) 256 * param->BitDepthLuma
    + (uint64_t) 2 * param->MbWidthC *param->MbHeightC *param->BitDepthChroma
  ;

  if (param->pic_order_cnt_type == 0) {
    param->MaxPicOrderCntLsb = 1 << (param->log2_max_pic_order_cnt_lsb_minus4 + 4);
  }
  else {
    param->ExpectedDeltaPerPicOrderCntCycle = 0;
    for (i = 0; i < param->num_ref_frames_in_pic_order_cnt_cycle; i++)
      param->ExpectedDeltaPerPicOrderCntCycle += param->offset_for_ref_frame[i];
  }

  param->PicWidthInMbs = param->pic_width_in_mbs_minus1 + 1;
  param->PicWidthInSamplesL = param->PicWidthInMbs * 16;
  param->PicWidthInSamplesC = param->PicWidthInMbs *param->MbWidthC;

  param->PicHeightInMapUnits = param->pic_height_in_map_units_minus1 + 1;
  param->PicSizeInMapUnits = param->PicWidthInMbs *param->PicHeightInMapUnits;

  param->FrameHeightInMbs =
    (2 - param->frame_mbs_only_flag) * param->PicHeightInMapUnits
  ;

  if (param->ChromaArrayType == 0) {
    param->CropUnitX = 1;
    param->CropUnitY = 2 - param->frame_mbs_only_flag;
  }
  else {
    param->CropUnitX = param->SubWidthC;
    param->CropUnitY = param->SubHeightC * (2 - param->frame_mbs_only_flag);
  }

  param->FrameWidth =
    param->PicWidthInMbs * 16
    - param->CropUnitX * (
      param->frame_crop_offset.left + param->frame_crop_offset.right
    )
  ;
  param->FrameHeight =
    param->FrameHeightInMbs * 16
    - param->CropUnitY * (
      param->frame_crop_offset.top + param->frame_crop_offset.bottom
    )
  ;

  /* Use computed values to update missing VUI fields and handle */
  _updateH264VuiParameters(handle, param);

  return 0;
}

int decodeH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
)
{
  /* seq_parameter_set_rbsp() */
  H264SPSDataParameters spsData;

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Sequence Parameters Set NAL unit type (receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  /* seq_parameter_set_data() */
  if (_parseH264SequenceParametersSetData(handle, &spsData) < 0)
    return -1;

  /* rbsp_trailing_bits() */
  if (_parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* Check */
  if (!handle->accessUnitDelimiterValid)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Missing mandatory AUD before SPS.\n"
    );

  if (!handle->sequenceParametersSetPresent) {
    if (checkH264SequenceParametersSetCompliance(handle, spsData) < 0)
      return -1;

    if (checkH264SpsBitstreamCompliance(options, spsData) < 0)
      return -1;
  }
  else {
    bool constantSps = constantH264SequenceParametersSetCheck(
      handle->sequenceParametersSet.data,
      spsData
    );

    if (!constantSps) {
      int ret = checkH264SequenceParametersSetChangeCompliance(
        handle->sequenceParametersSet.data,
        spsData
      );
      if (ret < 0)
        LIBBLU_H264_CK_FAIL_RETURN(
          "Fluctuating SPS parameters.\n"
        );
    }
    else if (handle->sequenceParametersSetValid)
      handle->curProgParam.presenceOfUselessSequenceParameterSet = true;
  }

  handle->sequenceParametersSetPresent = true;
  handle->sequenceParametersSetGopValid = true;
  handle->sequenceParametersSetValid = true; /* All checks succeed. */
  handle->sequenceParametersSet.data = spsData; /* Update */

  return 0;
}

int decodeH264PicParametersSet(
  H264ParametersHandlerPtr handle
)
{
  /* pic_parameter_set_rbsp() */
  uint32_t value;

  unsigned ppsId;
  bool updatePps;

  H264PicParametersSetParameters pps; /* Output structure */

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_PIC_PARAMETER_SET)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Picture Parameters Set NAL unit type (receive: '%s').\n",
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
  pps.pic_parameter_set_id = value;

  if (H264_MAX_PPS <= pps.pic_parameter_set_id)
    LIBBLU_H264_ERROR_RETURN(
      "PPS pic_parameter_set_id value overflow.\n"
    );

  /* [ue seq_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.seq_parameter_set_id = value;

  /* [b1 entropy_coding_mode_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.entropy_coding_mode_flag = value;

  /* [b1 bottom_field_pic_order_in_frame_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.bottom_field_pic_order_in_frame_present_flag = value;

  /* [ue num_slice_groups_minus1] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.num_slice_groups_minus1 = value;

  if (H264_MAX_ALLOWED_SLICE_GROUPS <= pps.num_slice_groups_minus1)
    LIBBLU_H264_ERROR_RETURN(
      "Number of slice groups exceed maximum value supported (%u < %u).\n",
      H264_MAX_ALLOWED_SLICE_GROUPS,
      pps.num_slice_groups_minus1 + 1
    );

  if (1 <= pps.num_slice_groups_minus1) {
    unsigned numSliceGroups = pps.num_slice_groups_minus1 + 1;

    /* Slices split in separate groups, need definitions. */
    H264PicParametersSetSliceGroupsParameters *ppsSG =
      &pps.slice_groups
    ;

    /* [ue slice_group_map_type] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    ppsSG->slice_group_map_type = value;

    switch (ppsSG->slice_group_map_type) {
    case H264_SLICE_GROUP_TYPE_INTERLEAVED:
      for (unsigned iGroup = 0; iGroup < numSliceGroups; iGroup++) {
        /* [ue run_length_minus1[iGroup]] */
        if (readExpGolombCodeNal(handle, &value, 32) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        ppsSG->data.run_length_minus1[iGroup] = value;
      }
      break;

    case H264_SLICE_GROUP_TYPE_DISPERSED:
      break;

    case H264_SLICE_GROUP_TYPE_FOREGROUND_LEFTOVER:
      for (unsigned iGroup = 0; iGroup < numSliceGroups; iGroup++) {
        /* [ue top_left[iGroup]] */
        if (readExpGolombCodeNal(handle, &value, 32) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        ppsSG->data.top_left[iGroup] = value;

        /* [ue top_left[iGroup]] */
        if (readExpGolombCodeNal(handle, &value, 32) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        ppsSG->data.bottom_right[iGroup] = value;
      }
      break;

    case H264_SLICE_GROUP_TYPE_CHANGING_1:
    case H264_SLICE_GROUP_TYPE_CHANGING_2:
    case H264_SLICE_GROUP_TYPE_CHANGING_3:
      /* [b1 slice_group_change_direction_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      ppsSG->data.slice_group_change_direction_flag = value;

      /* [ue slice_group_change_rate_minus1] */
      if (readExpGolombCodeNal(handle, &value, 32) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      ppsSG->data.slice_group_change_rate_minus1 = value;
      break;

    case H264_SLICE_GROUP_TYPE_EXPLICIT:
      /* [ue pic_size_in_map_units_minus1] */
      if (readExpGolombCodeNal(handle, &value, 32) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      ppsSG->data.pic_size_in_map_units_minus1 = value;

      /* Compute size of field(s) slice_group_id */
      unsigned fieldSize = lb_ceil_pow2_32(numSliceGroups);

      for (unsigned i = 0; i <= ppsSG->data.pic_size_in_map_units_minus1; i++) {
        /* [u<ceil(log2(pic_size_in_map_units_minus1 + 1))> slice_group_id] */
        if (skipBitsNal(handle, fieldSize) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        /* TODO: Currently skipped */
      }
      break;

    default:
      LIBBLU_H264_ERROR_RETURN(
        "Unknown slice groups 'slice_group_map_type' == %d value in PPS.\n",
        ppsSG->slice_group_map_type
      );
    }
  }

  /* [ue num_ref_idx_l0_default_active_minus1] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.num_ref_idx_l0_default_active_minus1 = value;

  /* [ue num_ref_idx_l1_default_active_minus1] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.num_ref_idx_l1_default_active_minus1 = value;

  /* [b1 weighted_pred_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.weighted_pred_flag = value;

  /* [u2 weighted_bipred_idc] */
  if (readBitsNal(handle, &value, 2) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.weighted_bipred_idc = value;

  if (H264_WEIGHTED_PRED_B_SLICES_RESERVED == pps.weighted_bipred_idc)
    LIBBLU_H264_ERROR_RETURN(
      "Reserved 'weighted_bipred_idc' == 3 value in PPS.\n"
    );

  /* [se pic_init_qp_minus26] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.pic_init_qp_minus26 = (int) value;

  /* [se pic_init_qs_minus26] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.pic_init_qs_minus26 = (int) value;

  /* [se chroma_qp_index_offset] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.chroma_qp_index_offset = (int) value;

  /* [b1 deblocking_filter_control_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.deblocking_filter_control_present_flag = value;

  /* [b1 constrained_intra_pred_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.constrained_intra_pred_flag = value;

  /* [b1 redundant_pic_cnt_present_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  pps.redundant_pic_cnt_present_flag = value;

  pps.picParamExtensionPresent = moreRbspDataNal(handle);

  if (pps.picParamExtensionPresent) {
    /* [b1 transform_8x8_mode_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    pps.transform_8x8_mode_flag = value;

    /* [b1 pic_scaling_matrix_present_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    pps.pic_scaling_matrix_present_flag = value;

    if (pps.pic_scaling_matrix_present_flag) {
      H264SequenceParametersSetDataParameters *sps =
        &handle->sequenceParametersSet.data
      ;
      pps.nbScalingMatrix =
        6
        + ((sps->chroma_format_idc != H264_CHROMA_FORMAT_444) ? 2 : 6)
        * pps.transform_8x8_mode_flag
      ;
      unsigned i;

      /* Initialization */
      memset(&pps.pic_scaling_list_present_flag, false, 12 * sizeof(bool));
      memset(&pps.UseDefaultScalingMatrix4x4Flag, true, 6 * sizeof(bool));
      memset(&pps.UseDefaultScalingMatrix8x8Flag, true, 6 * sizeof(bool));

      for (i = 0; i < pps.nbScalingMatrix; i++) {
        /* [b1 pic_scaling_list_present_flag[i]] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        pps.pic_scaling_list_present_flag[i] = value;

        if (pps.pic_scaling_list_present_flag[i]) {
          int ret;

          if (i < 6) {
            ret = _buildScalingList(
              handle,
              pps.ScalingList4x4[i], 16,
              pps.UseDefaultScalingMatrix4x4Flag + i
            );
          }
          else {
            ret = _buildScalingList(
              handle,
              pps.ScalingList8x8[i - 6], 64,
              pps.UseDefaultScalingMatrix4x4Flag + (i - 6)
            );
          }
          if (ret < 0)
            return -1;
        }
      }
    }
    else
      pps.nbScalingMatrix = 0;

    /* [se second_chroma_qp_index_offset] */
    if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    pps.second_chroma_qp_index_offset = (int) value;
  }
  else {
    /* Default values if end of Rbsp. */
    pps.transform_8x8_mode_flag = false;
    pps.pic_scaling_matrix_present_flag = false;
    pps.second_chroma_qp_index_offset =
      pps.chroma_qp_index_offset
    ;
  }

  /* rbsp_trailing_bits() */
  if (_parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* Checks : */
  ppsId = pps.pic_parameter_set_id; /* For readability */
  updatePps = false;

  if (H264_MAX_PPS <= ppsId)
    LIBBLU_ERROR_RETURN("Internal error, unexpected PPS id %u.\n", ppsId);

  if (!handle->picParametersSetIdsPresent[ppsId]) {
    /* Newer PPS */
    if (checkH264PicParametersSetCompliance(handle, pps) < 0)
      return -1;

    handle->picParametersSetIdsPresentNb++;
    handle->picParametersSetIdsPresent[ppsId] = true;
    updatePps = true;
  }
  else {
    /* Already present PPS */
    bool constantPps = constantH264PicParametersSetCheck(
      *(handle->picParametersSet[ppsId]),
      pps
    );

    if (!constantPps) {
      /* PPS if unconstant, check if changes are allowed */
      int ret = checkH264PicParametersSetChangeCompliance(
        handle,
        *(handle->picParametersSet[ppsId]),
        pps
      );
      if (ret < 0)
        LIBBLU_H264_CK_FAIL_RETURN(
          "Forbidden PPS parameters change.\n"
        );

      updatePps = true;
    }
  }

  if (updatePps) {
    if (updatePPSH264Parameters(handle, &pps, ppsId) < 0)
      return -1;
  }

  handle->picParametersSetIdsValid[ppsId] = true;

  return 0;
}

static int _parseH264SeiBufferingPeriod(
  H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod *param
)
{
  /* buffering_period(payloadSize) - Annex D.1.2 */
  uint32_t value;

  H264SPSDataParameters *spsData;

  assert(NULL != handle);
  assert(NULL != param);

  if (
    !handle->sequenceParametersSetPresent
    || !handle->sequenceParametersSet.data.vui_parameters_present_flag
  )
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory VUI parameters in SPS, "
      "Buffering Period SEI message shouldn't be present.\n"
    );

  spsData = &handle->sequenceParametersSet.data;

#if 0 /* Verification in checks */
  if (
    !spsData->vui_parameters.nal_hrd_parameters_present_flag
    && !spsData->vui_parameters.vcl_hrd_parameters_present_flag
  )
    LIBBLU_H264_ERROR_RETURN(
      "NalHrdBpPresentFlag and VclHrdBpPresentFlag are equal to 0b0, "
      "so Buffering Period SEI message shouldn't be present.\n"
    );
#endif

  /* Pre-init values for check purpose : */
  memset(
    param->nal_hrd_parameters, 0,
    H264_MAX_CPB_CONFIGURATIONS * 2 * sizeof(uint32_t)
  );
  memset(
    param->vcl_hrd_parameters, 0,
    H264_MAX_CPB_CONFIGURATIONS * 2 * sizeof(uint32_t)
  );

  /* [ue seq_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->seq_parameter_set_id = value;

  if (spsData->vui_parameters.nal_hrd_parameters_present_flag) {
    H264HrdParameters *hrd = &spsData->vui_parameters.nal_hrd_parameters;
    unsigned SchedSelIdx;

    for (SchedSelIdx = 0; SchedSelIdx <= hrd->cpb_cnt_minus1; SchedSelIdx++) {
      unsigned fieldSize = hrd->initial_cpb_removal_delay_length_minus1 + 1;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fieldSize) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay = value;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fieldSize) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset = value;
    }
  }

  if (spsData->vui_parameters.vcl_hrd_parameters_present_flag) {
    H264HrdParameters *hrd = &spsData->vui_parameters.vcl_hrd_parameters;
    unsigned SchedSelIdx;

    for (SchedSelIdx = 0; SchedSelIdx <= hrd->cpb_cnt_minus1; SchedSelIdx++) {
      unsigned fieldSize = hrd->initial_cpb_removal_delay_length_minus1 + 1;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fieldSize) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay = value;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fieldSize) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset = value;
    }
  }

  return 0;
}

static int _parseH264SeiPicTiming(
  H264ParametersHandlerPtr handle,
  H264SeiPicTiming *param
)
{
  /* pic_timing(payloadSize) - Annex D.1.3 */
  uint32_t value;

  size_t cpb_removal_delay_length;
  size_t dpb_output_delay_length;
  size_t time_offset_length;

  H264SPSDataParameters *sps;
  H264VuiParameters *vui;

  assert(NULL != handle);
  assert(NULL != param);

  if (!handle->sequenceParametersSetPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Missing SPS and VUI parameters, "
      "Pic Timing SEI message shouldn't be present.\n"
    );

  sps = &handle->sequenceParametersSet.data;

  if (!sps->vui_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Missing VUI parameters in SPS, "
      "Pic Timing SEI message shouldn't be present.\n"
    );

  vui = &sps->vui_parameters;

  if (
    !vui->CpbDpbDelaysPresentFlag
    && !vui->pic_struct_present_flag
  )
    LIBBLU_H264_ERROR_RETURN(
      "CpbDpbDelaysPresentFlagand pic_struct_present_flag are equal to 0b0, "
      "so Pic Timing SEI message shouldn't be present."
    );

  /* Recovering cpb_removal_delay_length_minus1 and dpb_output_delay_length_minus1 values. */
  if (vui->nal_hrd_parameters_present_flag) {
    cpb_removal_delay_length =
      vui->nal_hrd_parameters.cpb_removal_delay_length_minus1 + 1;
    dpb_output_delay_length =
      vui->nal_hrd_parameters.dpb_output_delay_length_minus1 + 1;
    time_offset_length = vui->nal_hrd_parameters.time_offset_length;
  }
  else if (vui->vcl_hrd_parameters_present_flag) {
    cpb_removal_delay_length =
      vui->vcl_hrd_parameters.cpb_removal_delay_length_minus1 + 1;
    dpb_output_delay_length =
      vui->vcl_hrd_parameters.dpb_output_delay_length_minus1 + 1;
    time_offset_length = vui->vcl_hrd_parameters.time_offset_length;
  }
  else { /* Default values, shall not happen */
    cpb_removal_delay_length = 24;
    dpb_output_delay_length = 24;
    time_offset_length = 24;
  }

  if (vui->CpbDpbDelaysPresentFlag) {
    /* [u<initial_cpb_removal_delay_length_minus1> cpb_removal_delay] */
    if (readBitsNal(handle, &value, cpb_removal_delay_length) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->cpb_removal_delay = value;

    /* [u<initial_cpb_removal_delay_length_minus1> dpb_output_delay] */
    if (readBitsNal(handle, &value, dpb_output_delay_length) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->dpb_output_delay = value;
  }

  if (vui->pic_struct_present_flag) {
    unsigned i;

    /* [u4 pic_struct] */
    if (readBitsNal(handle, &value, 4) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->pic_struct = value;

    // handle->curProgParam.lastPicStruct = param->pic_struct;

    switch (param->pic_struct) {
    case H264_PIC_STRUCT_FRAME:
    case H264_PIC_STRUCT_TOP_FIELD:
    case H264_PIC_STRUCT_BOTTOM_FIELD:
      param->NumClockTS = 1;
      break;

    case H264_PIC_STRUCT_TOP_FLWD_BOTTOM:
    case H264_PIC_STRUCT_BOTTOM_FLWD_TOP:
    case H264_PIC_STRUCT_DOUBLED_FRAME:
      param->NumClockTS = 2;
      break;

    case H264_PIC_STRUCT_TOP_FLWD_BOT_TOP:
    case H264_PIC_STRUCT_BOT_FLWD_TOP_BOT:
    case H264_PIC_STRUCT_TRIPLED_FRAME:
      param->NumClockTS = 3;
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Reserved 'pic_struct' == %d value in Picture Timing SEI message.\n",
        param->pic_struct
      );
    }

    memset(param->clock_timestamp, 0x00, param->NumClockTS *sizeof(H264ClockTimestampParam));

    for (i = 0; i < param->NumClockTS; i++) {
      /* [b1 clock_timestamp_flag[i]] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->clock_timestamp_flag[i] = value;

      if (param->clock_timestamp_flag[i]) {
        /* [u2 ct_type] */
        if (readBitsNal(handle, &value, 2) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->clock_timestamp[i].ct_type = value;

        /* [b1 nuit_field_based_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->clock_timestamp[i].nuit_field_based_flag = value;

        /* [u5 counting_type] */
        if (readBitsNal(handle, &value, 5) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->clock_timestamp[i].counting_type = value;

        /* [b1 full_timestamp_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[i].full_timestamp_flag = value;

        /* [b1 discontinuity_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[i].discontinuity_flag = value;

        /* [b1 cnt_dropped_flag] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[i].cnt_dropped_flag = value;

        /* [u8 n_frames] */
        if (readBitsNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->ClockTimestamp[i].n_frames = value;

        param->ClockTimestamp[i].seconds_flag = false;
        param->ClockTimestamp[i].minutes_flag = false;
        param->ClockTimestamp[i].hours_flag = false;

        if (param->ClockTimestamp[i].full_timestamp_flag) {
          param->ClockTimestamp[i].seconds_flag = true;
          param->ClockTimestamp[i].minutes_flag = true;
          param->ClockTimestamp[i].hours_flag = true;

          /* [u6 seconds_value] */
          if (readBitsNal(handle, &value, 6) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[i].seconds_value = value;

          /* [u6 minutes_value] */
          if (readBitsNal(handle, &value, 6) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[i].minutes_value = value;

          /* [u5 hours_value] */
          if (readBitsNal(handle, &value, 5) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[i].hours_value = value;
        }
        else {
          param->ClockTimestamp[i].seconds_value = 0;
          param->ClockTimestamp[i].minutes_value = 0;
          param->ClockTimestamp[i].hours_value = 0;

          /* [b1 seconds_flag] */
          if (readBitsNal(handle, &value, 1) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[i].seconds_flag = value;

          if (param->ClockTimestamp[i].seconds_flag) {
            /* [u6 seconds_value] */
            if (readBitsNal(handle, &value, 6) < 0)
              LIBBLU_H264_READING_FAIL_RETURN();
            param->ClockTimestamp[i].seconds_value = value;

            /* [b1 minutes_flag] */
            if (readBitsNal(handle, &value, 1) < 0)
              LIBBLU_H264_READING_FAIL_RETURN();
            param->ClockTimestamp[i].minutes_flag = value;

            if (param->ClockTimestamp[i].minutes_flag) {
              /* [u6 minutes_value] */
              if (readBitsNal(handle, &value, 6) < 0)
                LIBBLU_H264_READING_FAIL_RETURN();
              param->ClockTimestamp[i].minutes_value = value;

              /* [b1 hours_flag] */
              if (readBitsNal(handle, &value, 1) < 0)
                LIBBLU_H264_READING_FAIL_RETURN();
              param->ClockTimestamp[i].hours_flag = value;

              if (param->ClockTimestamp[i].hours_flag) {
                /* [u5 minutes_value] */
                if (readBitsNal(handle, &value, 5) < 0)
                  LIBBLU_H264_READING_FAIL_RETURN();
                param->ClockTimestamp[i].hours_value = value;
              }
            }
          }
        }

        if (0 < time_offset_length) {
          /* [u<time_offset_length> time_offset] */
          if (readBitsNal(handle, &value, time_offset_length) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->ClockTimestamp[i].time_offset = value;
        }
        else
          param->ClockTimestamp[i].time_offset = 0;
      }
    }
  }

  return 0;
}

static int _parseH264SeiUserDataUnregistered(
  H264ParametersHandlerPtr handle,
  H264SeiUserDataUnregistered *param,
  const size_t payloadSize
)
{
  /* user_data_unregistered(payloadSize) - Annex D.1.7 */

  assert(NULL != handle);
  assert(NULL != param);

  /* [u128 uuid_iso_iec_11578] */
  if (readBytesNal(handle, param->uuid_iso_iec_11578, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* [v<8 * (payloadSize - 16)> uuid_iso_iec_11578] */
  param->userDataPayloadOverflow = (
    H264_MAX_USER_DATA_PAYLOAD_SIZE
    < payloadSize - 16
  );
  param->userDataPayloadLength = MIN(
    payloadSize - 16,
    H264_MAX_USER_DATA_PAYLOAD_SIZE
  );

  return readBytesNal(
    handle,
    param->userDataPayload,
    param->userDataPayloadLength
  );
}

static int _parseH264SeiRecoveryPoint(
  H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint *param
)
{
  /* recovery_point(payloadSize) - Annex D.1.8 */
  uint32_t value;

  assert(NULL != handle);
  assert(NULL != param);

  /* [ue recovery_frame_cnt] */
  if (readExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->recovery_frame_cnt = value; /* Max: MaxFrameNum - 1 = 2 ** (12 + 4) - 1 */

  /* [b1 exact_match_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->exact_match_flag = value;

  /* [b1 broken_link_flag] */
  if (readBitsNal(handle, &value, 1) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->broken_link_flag = value;

  /* [u2 changing_slice_group_idc] */
  if (readBitsNal(handle, &value, 2) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->changing_slice_group_idc = value;

  return 0;
}

static int _parseH264SeiReservedSeiMessage(
  H264ParametersHandlerPtr handle,
  size_t payloadSize
)
{
  /* [v<8 * payloadSize> reserved_sei_message_payload_byte] */
  return skipBitsNal(handle, 8 * payloadSize);
}

static int _parseH264SupplementalEnhancementInformationMessage(
  H264ParametersHandlerPtr handle,
  H264SeiMessageParameters *param
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
    if (readBitsNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
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
    ret = _parseH264SeiBufferingPeriod(
      handle,
      &param->bufferingPeriod
    );
    break;

  case H264_SEI_TYPE_PIC_TIMING:
    ret = _parseH264SeiPicTiming(
      handle,
      &param->picTiming
    );
    break;

  case H264_SEI_TYPE_USER_DATA_UNREGISTERED:
    ret = _parseH264SeiUserDataUnregistered(
      handle,
      &param->userDataUnregistered,
      param->payloadSize
    );
    break;

  case H264_SEI_TYPE_RECOVERY_POINT:
    ret = _parseH264SeiRecoveryPoint(
      handle,
      &param->recoveryPoint
    );
    break;

  default:
    LIBBLU_H264_DEBUG_SEI(
      "Presence of reserved SEI message "
      "(Unsupported message, not checked).\n"
    );
    ret = _parseH264SeiReservedSeiMessage(
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
  // int ret;
  // bool is_constant;

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
    if (_parseH264SupplementalEnhancementInformationMessage(handle, &seiMsg) < 0)
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
    switch (seiMsg.payloadType) {
    case H264_SEI_TYPE_BUFFERING_PERIOD:
      if (!handle->sei.bufferingPeriodPresent) {
        if (checkH264SeiBufferingPeriodCompliance(handle, seiMsg.bufferingPeriod) < 0)
          return -1;
      }
      else {
        bool is_constant = constantH264SeiBufferingPeriodCheck(
          handle,
          handle->sei.bufferingPeriod,
          seiMsg.bufferingPeriod
        );

        if (!is_constant) {
          int ret = checkH264SeiBufferingPeriodChangeCompliance(
            handle,
            handle->sei.bufferingPeriod,
            seiMsg.bufferingPeriod
          );
          if (ret < 0)
            return -1;
        }
      }

      handle->sei.bufferingPeriod = seiMsg.bufferingPeriod; /* Update */
      handle->sei.bufferingPeriodPresent = true;
      handle->sei.bufferingPeriodValid = true;
      break;

    case H264_SEI_TYPE_PIC_TIMING:
      /* Always update */
      if (checkH264SeiPicTimingCompliance(handle, seiMsg.picTiming) < 0)
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

      handle->sei.picTiming = seiMsg.picTiming; /* Always update */
      handle->sei.picTimingPresent = true;
      handle->sei.picTimingValid = true;
      break;

    case H264_SEI_TYPE_RECOVERY_POINT:

      if (!handle->sei.recoveryPointPresent) {
        if (checkH264SeiRecoveryPointCompliance(handle, seiMsg.recoveryPoint) < 0)
          return -1;
      }
      else {
        bool is_constant = constantH264SeiRecoveryPointCheck(
          handle->sei.recoveryPoint,
          seiMsg.recoveryPoint
        );
        if (!is_constant) {
          /* Check changes */
          if (checkH264SeiRecoveryPointChangeCompliance(handle, seiMsg.recoveryPoint) < 0)
            return -1;
        }
      }

      handle->sei.recoveryPoint = seiMsg.recoveryPoint; /* Update */
      handle->sei.recoveryPointPresent = true;
      handle->sei.recoveryPointValid = true;
      break;

    default:
      LIBBLU_H264_DEBUG_SEI("  NOTE: Unchecked SEI message type.\n");
    }

  } while (moreRbspDataNal(handle));

  /* rbsp_trailing_bits() */
  if (_parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  return 0;
}

static int _parseH264RefPicListModification(
  H264ParametersHandlerPtr handle,
  H264RefPicListModification *param,
  H264SliceTypeValue slice_type
)
{
  uint32_t value;

  assert(NULL != handle);
  assert(NULL != param);

  if (!is_I_H264SliceTypeValue(slice_type) && !is_SI_H264SliceTypeValue(slice_type)) {
    /* [b1 ref_pic_list_modification_flag_l0] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->ref_pic_list_modification_flag_l0 = value;

    if (param->ref_pic_list_modification_flag_l0) {
      unsigned listLen = 0;

      while (1) {
        H264RefPicListModificationPictureIndex *index =
          &param->refPicListModificationl0[listLen]
        ;

        /* [ue modification_of_pic_nums_idc] */
        if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        index->modification_of_pic_nums_idc = value;

        if (3 < index->modification_of_pic_nums_idc)
          LIBBLU_H264_ERROR_RETURN(
            "Unexpected 'modification_of_pic_nums_idc' == %d value.\n",
            index->modification_of_pic_nums_idc
          );

        if (
          H264_MOD_OF_PIC_IDC_SUBSTRACT_ABS_DIFF == index->modification_of_pic_nums_idc
          || H264_MOD_OF_PIC_IDC_ADD_ABS_DIFF == index->modification_of_pic_nums_idc
        ) {
          /* [ue abs_diff_pic_num_minus1] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->abs_diff_pic_num_minus1 = value;
        }
        else if (index->modification_of_pic_nums_idc == H264_MOD_OF_PIC_IDC_LONG_TERM) {
          /* [ue long_term_pic_num] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->long_term_pic_num = value;
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

  if (is_B_H264SliceTypeValue(slice_type)) {
    /* [b1 ref_pic_list_modification_flag_l1] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->refPicListModificationFlagl1 = value;

    if (param->refPicListModificationFlagl1) {
      unsigned listLen = 0;

      while (1) {
        H264RefPicListModificationPictureIndex *index =
          &param->refPicListModificationl1[listLen]
        ;

        /* [ue modification_of_pic_nums_idc] */
        if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        index->modification_of_pic_nums_idc = value;

        if (3 < index->modification_of_pic_nums_idc)
          LIBBLU_H264_ERROR_RETURN(
            "Invalid 'modification_of_pic_nums_idc' == %u value in "
            "ref_pic_list_modification() section of slice header.\n"
          );

        if (
          H264_MOD_OF_PIC_IDC_SUBSTRACT_ABS_DIFF == index->modification_of_pic_nums_idc
          || H264_MOD_OF_PIC_IDC_ADD_ABS_DIFF == index->modification_of_pic_nums_idc
        ) {
          /* [ue abs_diff_pic_num_minus1] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->abs_diff_pic_num_minus1 = value;
        }
        else if (index->modification_of_pic_nums_idc == H264_MOD_OF_PIC_IDC_LONG_TERM) {
          /* [ue long_term_pic_num] */
          if (readExpGolombCodeNal(handle, &value, 32) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          index->long_term_pic_num = value;
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

static int _parseH264PredWeightTable(
  H264ParametersHandlerPtr handle,
  H264PredWeightTable *param,
  H264SliceHeaderParameters *sliceHeaderParam
)
{
  /* pred_weight_table() - 7.3.3.2 Prediction weight table syntax */
  uint32_t value;
  unsigned i, j;

  H264SPSDataParameters *spsData;

  assert(NULL != handle);
  assert(NULL != param);
  assert(NULL != sliceHeaderParam);

  spsData = &handle->sequenceParametersSet.data;

  /* [ue luma_log2_weight_denom] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->luma_log2_weight_denom = value;

  if (spsData->ChromaArrayType != 0) {
    /* [ue chroma_log2_weight_denom] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->chroma_log2_weight_denom = value;
  }

  for (i = 0; i <= sliceHeaderParam->num_ref_idx_l0_active_minus1; i++) {
    /* [b1 luma_weight_l0_flag[i]] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->weightL0[i].luma_weight_l0_flag = value;

    if (param->weightL0[i].luma_weight_l0_flag) {
      /* [se luma_weight_l0[i]] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL0[i].luma_weight_l0 = (int) value;

      /* [se luma_offset_l0[i]] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL0[i].luma_offset_l0 = (int) value;
    }

    if (spsData->ChromaArrayType != 0) {
      /* [b1 chroma_weight_l0_flag[i]] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL0[i].chroma_weight_l0_flag = value;

      if (param->weightL0[i].chroma_weight_l0_flag) {
        for (j = 0; j < H264_MAX_CHROMA_CHANNELS_NB; j++) {
          /* [se chroma_weight_l0[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL0[i].chroma_weight_l0[j] = (int) value;

          /* [se chroma_offset_l0[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL0[i].chroma_offset_l0[j] = (int) value;
        }
      }
    }
  }

  if (is_B_H264SliceTypeValue(sliceHeaderParam->slice_type)) {
    for (i = 0; i <= sliceHeaderParam->num_ref_idx_l1_active_minus1; i++) {
      /* [b1 luma_weight_l1_flag[i]] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->weightL1[i].luma_weight_l1_flag = value;

      if (param->weightL1[i].luma_weight_l1_flag) {
        /* [se luma_weight_l1[i]] */
        if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->weightL1[i].luma_weight_l1 = (int) value;

        /* [se luma_offset_l1[i]] */
        if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->weightL1[i].luma_offset_l1 = (int) value;
      }

      if (spsData->ChromaArrayType != 0) {
        /* [b1 chroma_weight_l1_flag[i]] */
        if (readBitsNal(handle, &value, 1) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->weightL1[i].chroma_weight_l1_flag = value;

        for (j = 0; j < H264_MAX_CHROMA_CHANNELS_NB; j++) {
          /* [se chroma_weight_l1[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL1[i].chroma_weight_l1[j] = (int) value;

          /* [se chroma_offset_l1[i][j]] */
          if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          param->weightL1[i].chroma_offset_l1[j] = (int) value;
        }
      }
    }
  }

  return 0;
}

static int _checkAndSetPresentOperationsH264DecRefPicMarking(
  H264DecRefPicMarking *param,
  H264MemoryManagementControlOperationValue operation
)
{
  switch (operation) {
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
      operation
    );
  }

  return 0;
}

static int _parseH264DecRefPicMarking(
  H264ParametersHandlerPtr handle,
  H264DecRefPicMarking *param,
  bool IdrPicFlag
)
{
  /* dec_ref_pic_marking() */
  /* 7.3.3.3 Decoded reference picture marking syntax */
  uint32_t value;

  assert(NULL != handle);
  assert(NULL != param);

  param->IdrPicFlag = IdrPicFlag;

  if (param->IdrPicFlag) {
    /* [b1 no_output_of_prior_pics_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->no_output_of_prior_pics_flag = value;

    /* [b1 long_term_reference_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->long_term_reference_flag = value;
  }
  else {
    /* [b1 adaptive_ref_pic_marking_mode_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->adaptive_ref_pic_marking_mode_flag = value;

    if (param->adaptive_ref_pic_marking_mode_flag) {
      H264MemMngmntCtrlOpBlk *opBlk;

      H264MemoryManagementControlOperationValue operation;

      param->nbMemMngmntCtrlOp = 0;
      do {
        if (H264_MAX_SUPPORTED_MEM_MGMNT_CTRL_OPS <= param->nbMemMngmntCtrlOp)
          opBlk = NULL;
        else
          opBlk = &param->MemMngmntCtrlOp[param->nbMemMngmntCtrlOp++];

        /* [ue memory_management_control_operation] */
        if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        operation = value;

        if (NULL != opBlk)
          opBlk->operation = operation;

        if (_checkAndSetPresentOperationsH264DecRefPicMarking(param, operation) < 0)
          return -1;

        if (
          H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED == operation
          || H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM == operation
        ) {
          /* [ue difference_of_pic_nums_minus1] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          if (NULL != opBlk)
            opBlk->difference_of_pic_nums_minus1 = value;
        }

        if (H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED == operation) {
          /* [ue long_term_pic_num] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          if (NULL != opBlk)
            opBlk->long_term_pic_num = value;
        }

        if (
          H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM == operation
          || H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM == operation
        ) {
          /* [ue long_term_frame_idx] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          if (NULL != opBlk)
            opBlk->long_term_frame_idx = value;
        }

        if (H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM == operation) {
          /* [ue max_long_term_frame_idx_plus1] */
          if (readExpGolombCodeNal(handle, &value, 16) < 0)
            LIBBLU_H264_READING_FAIL_RETURN();
          if (NULL != opBlk)
            opBlk->max_long_term_frame_idx_plus1 = value;
        }
      } while (H264_MEM_MGMNT_CTRL_OP_END != operation);
    }
  }

  return 0;
}

static int _parseH264SliceHeader(
  H264ParametersHandlerPtr handle,
  H264SliceHeaderParameters *param
)
{
  /* slice_header() - 7.3.3 Slice header syntax */
  uint32_t value;

  H264SequenceParametersSetDataParameters *sps;
  H264PicParametersSetParameters *pps;

  bool isRefPic = isReferencePictureH264NalRefIdcValue(getNalRefIdc(handle));
  bool IdrPicFlag = isIdrPictureH264NalUnitTypeValue(getNalUnitType(handle));
  bool isExt = isCodedSliceExtensionH264NalUnitTypeValue(getNalUnitType(handle));

  /* bool frameNumEqualPrevRefFrameNum; */  /* TODO */

  assert(NULL != handle);
  assert(NULL != param);

  if (!handle->sequenceParametersSetPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory SPS parameters, Unable to decode slice_header().\n"
    );
  sps = &handle->sequenceParametersSet.data;

  if (0 == handle->picParametersSetIdsPresentNb)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory PPS parameters, Unable to decode slice_header().\n"
    );

  /* ppsData will be set when know active ID */

  /* Get informations from context (current AU) */

  /* [ue first_mb_in_slice] */
  if (readExpGolombCodeNal(handle, &value, 32) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->first_mb_in_slice = value;

  /* [ue slice_type] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->slice_type = value;

  if (H264_SLICE_TYPE_SI < param->slice_type)
    LIBBLU_H264_ERROR_RETURN(
      "Unknown 'slice_type' == %u in slice header.\n",
      param->slice_type
    );

  /* [ue pic_parameter_set_id] */
  if (readExpGolombCodeNal(handle, &value, 8) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->pic_parameter_set_id = value;

  /* Checking PPS id */
#if 0
  if (H264_MAX_PPS <= param->pic_parameter_set_id)
    LIBBLU_H264_ERROR_RETURN(
      "Slice header pic_parameter_set_id value overflow.\n"
    );
#endif
  if (!handle->picParametersSetIdsPresent[param->pic_parameter_set_id])
    LIBBLU_H264_ERROR_RETURN(
      "Slice header referring unknown PPS id %u.\n",
      param->pic_parameter_set_id
    );
  pps = handle->picParametersSet[param->pic_parameter_set_id];

  if (sps->separate_colour_plane_flag) {
    /* [u2 colour_plane_id] */
    if (readBitsNal(handle, &value, 2) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->colour_plane_id = value;
  }
  else
    param->colour_plane_id = H264_COLOUR_PLANE_ID_Y; /* Default, not used, only for checking purposes */

  /* [u<log2_max_frame_num_minus4 + 4> frame_num] */
  if (readBitsNal(handle, &value, sps->log2_max_frame_num_minus4 + 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->frame_num = value;

#if 0
  if (IdrPicFlag) {
    if (0 != param->frame_num) {
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
      (param->frame_num == handle->curProgParam.PrevRefFrameNum)
    ;

    if (frameNumEqualPrevRefFrameNum) {

    }

    if (!frameNumEqualPrevRefFrameNum) {
      handle->curProgParam.gapsInFrameNum = (
        param->frame_num
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

        LIBBLU_H264_CK_FAIL_RETURN(
          "Forbidden gaps in 'frame_num' value "
          "(last: %" PRIu16 ", cur: %" PRIu16 ").\n",
          handle->curProgParam.PrevRefFrameNum,
          param->frame_num
        );
      }
    }

#if 0
    else if (param->frame_num != handle->curProgParam.PrevRefFrameNum) {
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
  handle->curProgParam.gapsInFrameNum = false;

  if (!sps->frame_mbs_only_flag) {
    /* [b1 field_pic_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->field_pic_flag = value;

    if (param->field_pic_flag) {
      /* [b1 bottom_field_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->bottom_field_flag = value;
    }
    else
      param->bottom_field_flag = false;
  }
  else {
    param->field_pic_flag = false;
    param->bottom_field_flag = false;
  }

  if (IdrPicFlag) {
    /* [ue idr_pic_id] */
    if (readExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->idr_pic_id = value;
  }
  else
    param->idr_pic_id = 0; /* For test purpose */

  if (sps->pic_order_cnt_type == 0) {
    unsigned fieldSize = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;

    /* [u<log2_max_pic_order_cnt_lsb_minus4 + 4> pic_order_cnt_lsb] */
    if (readBitsNal(handle, &value, fieldSize) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->pic_order_cnt_lsb = value;

    if (
      pps->bottom_field_pic_order_in_frame_present_flag
      && !param->field_pic_flag
    ) {
      /* [se delta_pic_order_cnt_bottom] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->delta_pic_order_cnt_bottom = (int) value;
    }
    else
      param->delta_pic_order_cnt_bottom = 0;
  }
  else {
    param->pic_order_cnt_lsb = 0; /* For test purpose */
    param->delta_pic_order_cnt_bottom = 0;
  }

  if (
    sps->pic_order_cnt_type == 1
    && !sps->delta_pic_order_always_zero_flag
  ) {
    /* [se delta_pic_order_cnt[0]] */
    if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->delta_pic_order_cnt[0] = (int) value;

    if (
      pps->bottom_field_pic_order_in_frame_present_flag
      && !param->field_pic_flag
    ) {
      /* [se delta_pic_order_cnt[1]] */
      if (readSignedExpGolombCodeNal(handle, &value, 32) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->delta_pic_order_cnt[1] = (int) value;
    }
    else
      param->delta_pic_order_cnt[1] = 0;
  }
  else
    param->delta_pic_order_cnt[0] = param->delta_pic_order_cnt[1] = 0;

  if (pps->redundant_pic_cnt_present_flag) {
    /* [ue redundant_pic_cnt] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->redundant_pic_cnt = value;
  }
  else
    param->redundant_pic_cnt = 0;

  if (is_B_H264SliceTypeValue(param->slice_type)) {
    /* [b1 direct_spatial_mv_pred_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->direct_spatial_mv_pred_flag = value;
  }

  if (
    is_P_H264SliceTypeValue(param->slice_type)
    || is_SP_H264SliceTypeValue(param->slice_type)
    || is_B_H264SliceTypeValue(param->slice_type)
  ) {
    unsigned maxRefIdx;

    /* [b1 num_ref_idx_active_override_flag] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->num_ref_idx_active_override_flag = value;

    if (param->num_ref_idx_active_override_flag) {
      /* [ue num_ref_idx_l0_active_minus1] */
      if (readExpGolombCodeNal(handle, &value, 8) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->num_ref_idx_l0_active_minus1 = value;

      maxRefIdx =
        (param->field_pic_flag) ?
          H264_REF_PICT_LIST_MAX_NB_FIELD
        :
          H264_REF_PICT_LIST_MAX_NB
      ;

      if (maxRefIdx <= param->num_ref_idx_l0_active_minus1)
        LIBBLU_H264_ERROR_RETURN(
          "Maximum reference index for reference picture list 0 exceed %u in "
          "slice header.\n",
          maxRefIdx
        );

      if (is_B_H264SliceTypeValue(param->slice_type)) {
        /* [ue num_ref_idx_l1_active_minus1] */
          if (readExpGolombCodeNal(handle, &value, 8) < 0)
          LIBBLU_H264_READING_FAIL_RETURN();
        param->num_ref_idx_l1_active_minus1 = value;

        if (maxRefIdx <= param->num_ref_idx_l0_active_minus1)
          LIBBLU_H264_ERROR_RETURN(
            "Maximum reference index for reference picture list 0 exceed %u in "
            "slice header.\n",
            maxRefIdx
          );
      }
    }
    else {
      param->num_ref_idx_l0_active_minus1 =
        pps->num_ref_idx_l0_default_active_minus1
      ;
      param->num_ref_idx_l1_active_minus1 =
        pps->num_ref_idx_l1_default_active_minus1
      ;
    }
  }

  if (isExt) {
    /* Present if nal_unit_type is equal to 20 or 21. */
    /* ref_pic_list_mvc_modification() */
    /* Annex H */

    /* TODO: Add 3D MVC support. */
    LIBBLU_TODO_MSG("Unsupported 3D MVC bitstream.\n");
  }
  else {
    /* ref_pic_list_modification() */
    int ret = _parseH264RefPicListModification(
      handle,
      &param->refPicListMod,
      param->slice_type
    );
    if (ret < 0)
      return -1;
  }

  if (
    (
      pps->weighted_pred_flag
      && (
        is_P_H264SliceTypeValue(param->slice_type)
        || is_SP_H264SliceTypeValue(param->slice_type)
      )
    )
    || (
      (
        pps->weighted_bipred_idc
        == H264_WEIGHTED_PRED_B_SLICES_EXPLICIT
      )
      && is_B_H264SliceTypeValue(param->slice_type)
    )
  ) {
    /* pred_weight_table() */
    if (_parseH264PredWeightTable(handle, &param->pred_weight_table, param) < 0)
      return -1;
  }

  param->decRefPicMarkingPresent = getNalRefIdc(handle) != 0;

  memset(&param->dec_ref_pic_marking, 0, sizeof(H264DecRefPicMarking));

  if (param->decRefPicMarkingPresent) {
    /* dec_ref_pic_marking() */
    if (_parseH264DecRefPicMarking(handle, &param->dec_ref_pic_marking, IdrPicFlag) < 0)
      return -1;
  }

  if (
    pps->entropy_coding_mode_flag
    && !is_I_H264SliceTypeValue(param->slice_type)
    && !is_SI_H264SliceTypeValue(param->slice_type)
  ) {
    /* [ue cabac_init_idc] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->cabac_init_idc = value;
  }
  else
    param->cabac_init_idc = 0; /* For check purpose */

  /* [se slice_qp_delta] */
  if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->slice_qp_delta = (int) value;

  if (
    is_SP_H264SliceTypeValue(param->slice_type)
    || is_SI_H264SliceTypeValue(param->slice_type)
  ) {
    if (is_SP_H264SliceTypeValue(param->slice_type)) {
      /* [b1 sp_for_switch_flag] */
      if (readBitsNal(handle, &value, 1) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->sp_for_switch_flag = value;
    }

    /* [se slice_qs_delta] */
    if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->slice_qs_delta = (int) value;
  }

  if (pps->deblocking_filter_control_present_flag) {
    /* [ue disable_deblocking_filter_idc] */
    if (readExpGolombCodeNal(handle, &value, 8) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->disable_deblocking_filter_idc = value;

    if (2 < param->disable_deblocking_filter_idc)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved value 'disable_deblocking_filter_idc' == %u "
        "in slice header.\n",
        param->disable_deblocking_filter_idc
      );

    if (param->disable_deblocking_filter_idc != 1) {
      /* [se slice_alpha_c0_offset_div2] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->slice_alpha_c0_offset_div2 = (int) value;

      /* [se slice_beta_offset_div2] */
      if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->slice_beta_offset_div2 = (int) value;
    }
    else
      param->slice_alpha_c0_offset_div2 = param->slice_beta_offset_div2 = 0;
  }
  else {
    param->disable_deblocking_filter_idc = 0;
    param->slice_alpha_c0_offset_div2 = 0;
    param->slice_beta_offset_div2 = 0;
  }

  if (1 <= pps->num_slice_groups_minus1) {
    H264SliceGroupMapTypeValue sliceGroupMapType =
      pps->slice_groups.slice_group_map_type
    ;

    if (
      sliceGroupMapType == H264_SLICE_GROUP_TYPE_CHANGING_1
      || sliceGroupMapType == H264_SLICE_GROUP_TYPE_CHANGING_2
      || sliceGroupMapType == H264_SLICE_GROUP_TYPE_CHANGING_3
    ) {
      /* [ue slice_group_change_cycle] */
      if (readExpGolombCodeNal(handle, &value, 32) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->slice_group_change_cycle = value;
    }
  }

  /* Computed parameters: */
  param->isRefPic = isRefPic;
  param->isIdrPic = IdrPicFlag;
  param->isCodedSliceExtension = isExt;
  param->sps_pic_order_cnt_type = sps->pic_order_cnt_type;
  param->presenceOfMemManCtrlOp5 = param->dec_ref_pic_marking.presenceOfMemManCtrlOp5;

  param->MbaffFrameFlag =
    sps->mb_adaptive_frame_field_flag
    && !param->field_pic_flag
  ;
  param->PicHeightInMbs =
    sps->FrameHeightInMbs
    / (1 + param->field_pic_flag)
  ;
  param->PicHeightInSamplesL =
    param->PicHeightInMbs * 16
  ;
  param->PicHeightInSamplesC =
    param->PicHeightInMbs
    * sps->MbHeightC
  ;
  param->PicSizeInMbs =
    param->PicHeightInMbs
    * sps->PicWidthInMbs
  ;

  return 0;
}

typedef enum {
  H264_FRST_VCL_NALU_PCP_RET_ERROR  = -1,  /**< Fatal error happen.          */
  H264_FRST_VCL_NALU_PCP_RET_FALSE  =  0,  /**< VCL NALU is not the start of
    a new primary coded picture.                                             */
  H264_FRST_VCL_NALU_PCP_RET_TRUE   =  1   /**< Successful detection of the
    first VCL NALU of a primary coded picture.                               */
} H264FirstVclNaluPCPRet;

static H264FirstVclNaluPCPRet _detectFirstVclNalUnitOfPrimCodedPicture(
  H264SliceHeaderParameters last,
  H264SliceHeaderParameters cur
)
{
  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'frame_num' differs in value ? : %u => %u.\n",
    last.frame_num, cur.frame_num
  );

  if (last.frame_num != cur.frame_num)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'pic_parameter_set_id' differs in value ? : %u => %u.\n",
    last.pic_parameter_set_id, cur.pic_parameter_set_id
  );

  if (last.pic_parameter_set_id != cur.pic_parameter_set_id)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'field_pic_flag' differs in value ? : %u => %u.\n",
    last.field_pic_flag, cur.field_pic_flag
  );

  if (last.field_pic_flag != cur.field_pic_flag)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'bottom_field_flag' differs in value ? : %u => %u.\n",
    last.bottom_field_flag, cur.bottom_field_flag
  );

  if (last.field_pic_flag && last.bottom_field_flag != cur.bottom_field_flag)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'nal_ref_idc' == 0 (IDR-pic) differs in value ? : %s => %s.\n",
    BOOL_STR(last.isRefPic),
    BOOL_STR(cur.isRefPic)
  );

  if (last.isRefPic ^ cur.isRefPic)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'pic_order_cnt_type' == 0 for both ? : %s => %s.\n",
    BOOL_STR(last.sps_pic_order_cnt_type == 0),
    BOOL_STR(cur.sps_pic_order_cnt_type == 0)
  );

  if ((last.sps_pic_order_cnt_type == 0) && (cur.sps_pic_order_cnt_type == 0)) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'pic_order_cnt_lsb' differs in value ? : %u => %u.\n",
      last.pic_order_cnt_lsb, cur.pic_order_cnt_lsb
    );

    if (last.pic_order_cnt_lsb != cur.pic_order_cnt_lsb)
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;

    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'delta_pic_order_cnt_bottom' differs in value ? : "
      "%d => %d.\n",
      last.delta_pic_order_cnt_bottom, cur.delta_pic_order_cnt_bottom
    );

    if (last.delta_pic_order_cnt_bottom != cur.delta_pic_order_cnt_bottom)
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;
  }

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'pic_order_cnt_type' == 1 for both ? : %s => %s.\n",
    BOOL_STR(last.sps_pic_order_cnt_type == 1),
    BOOL_STR(cur.sps_pic_order_cnt_type == 1)
  );

  if ((last.sps_pic_order_cnt_type == 1) && (cur.sps_pic_order_cnt_type == 1)) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'delta_pic_order_cnt[0]' differs in value ? : %ld => %ld.\n",
      last.delta_pic_order_cnt[0], cur.delta_pic_order_cnt[0]
    );

    if (last.delta_pic_order_cnt[0] != cur.delta_pic_order_cnt[0])
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;

    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'delta_pic_order_cnt[1]' differs in value ? : %ld => %ld.\n",
      last.delta_pic_order_cnt[1], cur.delta_pic_order_cnt[1]
    );

    if (last.delta_pic_order_cnt[1] != cur.delta_pic_order_cnt[1])
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;
  }

  LIBBLU_H264_DEBUG_AU_PROCESSING(
    " -> Does 'IdrPicFlag' differs in value ? : %u => %u.\n",
    last.isIdrPic,
    cur.isIdrPic
  );

  if (last.isIdrPic != cur.isIdrPic)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  if (cur.isIdrPic) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      "  -> Does 'idr_pic_id' differs in value ? : %u => %u.\n",
      last.idr_pic_id, cur.idr_pic_id
    );

    if (last.idr_pic_id != cur.idr_pic_id)
      return H264_FRST_VCL_NALU_PCP_RET_TRUE;
  }

  return H264_FRST_VCL_NALU_PCP_RET_FALSE;
}

int decodeH264SliceLayerWithoutPartitioning(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
)
{
  int ret;

  H264SliceLayerWithoutPartitioningParameters sliceParam;

  /* slice_header() */
  if (_parseH264SliceHeader(handle, &sliceParam.header) < 0)
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

  /**
   * NOTE: If no Coded Slice VCL NALU is present before, this NALU is necessary
   * the start of a new primary coded picture.
   */
  ret = H264_FRST_VCL_NALU_PCP_RET_TRUE;

  if (handle->slicePresent) {
    ret = _detectFirstVclNalUnitOfPrimCodedPicture(
      sliceParam.header,
      handle->slice.header
    );
  }

  switch (ret) {
  case H264_FRST_VCL_NALU_PCP_RET_FALSE:
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      " => This VCL NALU is part of the pending primary picture.\n"
    );
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
      handle, options, sliceParam
    );
  }
  else {
    ret = checkH264SliceLayerWithoutPartitioningChangeCompliance(
      handle, options, handle->slice, sliceParam
    );
  }
  if (ret < 0)
    return -1;

  /* Update current slice : */
#if 0
  if (
    handle->slicePresent
    && handle->slice.header.decRefPicMarkingPresent
  ) {
    /* Free previous memory_management_control_operation storage structures : */
    if (
      !handle->slice.header.dec_ref_pic_marking.IdrPicFlag
      && handle->slice.header.dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag
    ) {
      closeH264MemoryManagementControlOperations(
        handle->slice.header.dec_ref_pic_marking.MemMngmntCtrlOp
      );
    }
  }
#endif

  handle->slice.header = sliceParam.header;
  handle->slicePresent = true;

  return 0;
}

int decodeH264FillerData(
  H264ParametersHandlerPtr handle
)
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
  if (_parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  LIBBLU_H264_CK_WCOND_WARNING(
    LIBBLU_WARN_COUNT_CHECK_INC(&handle->warningFlags, AU_filler_data),
    "Presence of filling data bytes, reducing compression efficiency.\n"
  );

  return 0;
}

int decodeH264EndOfSequence(
  H264ParametersHandlerPtr handle
)
{
  assert(NULL != handle);

  if (isInitNal(handle))
    LIBBLU_H264_ERROR_RETURN("Unknown data in end of sequence NAL Unit.\n");

  return 0;
}

int analyzeH264(
  LibbluESParsingSettings *settings
)
{
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
    appendSourceFileWithCrcEsmsHandler(
      h264Infos, settings->esFilepath,
      CRC32_USED_BYTES, lb_compute_crc32(crcBuf, 0, CRC32_USED_BYTES),
      &handle->esmsSrcFileIdx
    ) < 0
  )
    goto free_return;
#endif

  H264ParametersHandlerPtr handle;
  if (NULL == (handle = initH264ParametersHandler(settings)))
    return -1;

  H264HrdVerifierContextPtr hrd_verif_ctx = NULL;
  bool sei_section = false; // Used to know when inserting SEI custom NALU.

  while (!noMoreNal(handle) && !settings->askForRestart) {
    /* Main NAL units parsing loop. */
    H264NalUnitTypeValue nal_unit_type;

    /* Progress bar : */
    printFileParsingProgressionBar(handle->file.inputFile);

    /* Initialize in deserializer the next NALU */
    if (_initNALU(handle) < 0)
      goto free_return;

    nal_unit_type = getNalUnitType(handle);

    if (_isStartOfANewAU(&handle->curProgParam, nal_unit_type)) {
      LIBBLU_H264_DEBUG_AU_PROCESSING(
        "Detect the start of a new Access Unit.\n"
      );
      if (_processAccessUnit(handle, &hrd_verif_ctx, settings) < 0)
        goto free_return;
    }

    switch (nal_unit_type) {
    case NAL_UNIT_TYPE_NON_IDR_SLICE: /* 0x01 / 1 */
    case NAL_UNIT_TYPE_IDR_SLICE:     /* 0x05 / 5 */
      if (decodeH264SliceLayerWithoutPartitioning(handle, settings->options) < 0)
        goto free_return;

      /* Compute PicOrderCnt: */
      if (_computeAuPicOrderCnt(handle, settings->options) < 0)
        goto free_return;
      break;

    case NAL_UNIT_TYPE_SLICE_PART_A_LAYER: /* 0x02 / 2 */
    case NAL_UNIT_TYPE_SLICE_PART_B_LAYER: /* 0x03 / 3 */
    case NAL_UNIT_TYPE_SLICE_PART_C_LAYER: /* 0x04 / 4 */
      LIBBLU_TODO_MSG("Unsupported partitioned slice data NALUs.\n");
      // handle->constraints.forbiddenSliceDataPartitionLayersNal
      // LIBBLU_H264_CK_FAIL_FRETURN(
      //   "Unsupported partitioned slice data NALUs.\n"
      // );
      break;

    case NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION: /* 6 - SEI */
      if (settings->options.discard_sei) {
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
          settings->options.force_rebuild_sei
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

    case NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET: /* 7 - SPS */
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

        if (!settings->options.disable_fixes) {
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
          !settings->options.disable_fixes
        ;
      }

      if (patchH264SequenceParametersSet(handle, settings->options) < 0)
        goto free_return;
      break;

    case NAL_UNIT_TYPE_PIC_PARAMETER_SET: /* 8 - PPS */
      if (decodeH264PicParametersSet(handle) < 0)
        goto free_return;

      sei_section = true; /* Made SEI insertion available. */
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

        if (!settings->options.disable_fixes) {
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
        nal_unit_type
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
    if (sei_section) {
      if (settings->options.force_rebuild_sei) {
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

      sei_section = false;
    }
  }

  if (settings->askForRestart || handle->curProgParam.restartRequired) {
    LIBBLU_INFO(
      "Parsing guessing error found, "
      "restart parsing with corrected parameters.\n"
    );

    settings->options.second_pass = true;
    settings->options.half_PicOrderCnt = handle->curProgParam.half_PicOrderCnt;
    settings->options.initDecPicNbCntShift = handle->curProgParam.initDecPicNbCntShift;

    /* Quick exit. */
    destroyH264ParametersHandler(handle);
    destroyH264HrdVerifierContext(hrd_verif_ctx);
    return 0;
  }

  if (0 < handle->curProgParam.nbSlicesInPic) {
    /* Process remaining pending slices. */
    if (_processPESFrame(handle, settings->options) < 0)
      goto free_return;
  }

  if (!handle->curProgParam.nbPics)
    LIBBLU_ERROR_FRETURN("No picture in bitstream.\n");

  if (handle->curProgParam.nbPics == 1) {
    /* Only one picture = Still picture. */
    LIBBLU_INFO("Stream is detected as still picture.\n");
    setStillPictureEsmsHandler(handle->esms);
  }

  /* Complete Buffering Period SEI computation if needed. */
#if 0
  if (completeH264SeiBufferingPeriodComputation(handle, h264Infos) < 0)
    return -1;
#endif

  lbc_printf(
    lbc_str(" === Parsing finished with success. ===              \n")
  );

  /* Display infos : */
  lbc_printf(
    lbc_str("== Stream Infos =======================================================================\n")
  );
  lbc_printf(
    lbc_str("Codec: Video/H.264-AVC, %ux%u%c, "),
    handle->sequenceParametersSet.data.FrameWidth,
    handle->sequenceParametersSet.data.FrameHeight,
    (handle->sequenceParametersSet.data.frame_mbs_only_flag ? 'p' : 'i')
  );
  lbc_printf(lbc_str("%.3f Im/s.\n"), handle->curProgParam.frameRate);
  printStreamDurationEsmsHandler(handle->esms);
  lbc_printf(
    lbc_str("=======================================================================================\n")
  );

  if (completeH264ParametersHandler(handle, settings) < 0)
    goto free_return;
  destroyH264ParametersHandler(handle);
  destroyH264HrdVerifierContext(hrd_verif_ctx);

  return 0;

free_return:
  destroyH264ParametersHandler(handle);
  destroyH264HrdVerifierContext(hrd_verif_ctx);

  return -1;
}

