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
  uint32_t value;

  assert(NULL != param);

  if (param->file.packetInitialized)
    LIBBLU_H264_ERROR_RETURN(
      "Double initialisation of a NAL unit at 0x%" PRIx64 " offset.\n",
      tellPos(param->file.inputFile)
    );

  int64_t NAL_unit_start_offset = tellPos(param->file.inputFile);

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
        NAL_unit_start_offset
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
        NAL_unit_start_offset
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
    NAL_unit_start_offset,
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
  H264AUNalUnitPtr access_unit_NALU_cell = createNewNalCell(
    param, param->file.nal_unit_type
  );
  if (NULL == access_unit_NALU_cell)
    return -1;

  access_unit_NALU_cell->startOffset = NAL_unit_start_offset;

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
  const H264CurrentProgressParam *param = &handle->cur_prog_param;

  return
    (param->au_content.bottom_field_present && param->au_content.top_field_present)
    || param->au_content.frame_present
  ;
}

static inline bool _complementaryFieldPairPresent(
  const H264ParametersHandlerPtr handle
)
{
  const H264CurrentProgressParam *param = &handle->cur_prog_param;

  return
    param->au_content.bottom_field_present
    && param->au_content.top_field_present
  ;
}

static int32_t _computePicOrderCntType0(
  H264ParametersHandlerPtr handle
)
{
  /* Mode 0 - 8.2.1.1 Decoding process for picture order count type 0 */
  /* Input: PicOrderCntMsb of previous picture */

  const H264SPSDataParameters *sps       = &handle->sequence_parameter_set.data;
  const H264SliceHeaderParameters *slice = &handle->slice.header;
  H264LastPictureProperties *last_pic    = &handle->cur_prog_param.last_pic;

  int32_t MaxPicOrderCntLsb = (int32_t) sps->MaxPicOrderCntLsb;

  /* Derivation of variables prevPicOrderCntMsb and prevPicOrderCntLsb : */
  int32_t prevPicOrderCntMsb, prevPicOrderCntLsb;
  if (slice->is_IDR_pic) {
    prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
  }
  else {
    if (last_pic->pres_of_mem_man_ctrl_op_5) {
      if (!last_pic->bottom_field_flag) {
        prevPicOrderCntMsb = 0;
        prevPicOrderCntLsb = last_pic->TopFieldOrderCnt;
      }
      else {
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
      }
    }
    else {
      prevPicOrderCntMsb = last_pic->PicOrderCntMsb;
      prevPicOrderCntLsb = last_pic->pic_order_cnt_lsb;
    }
  }

  /* Derivation of PicOrderCntMsb of the current picture ([1] 8-3) : */
  int32_t PicOrderCntMsb;
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
  int32_t TopFieldOrderCnt    = last_pic->TopFieldOrderCnt;
  int32_t BottomFieldOrderCnt = 0;

  if (!slice->bottom_field_flag)
    TopFieldOrderCnt = PicOrderCntMsb + slice->pic_order_cnt_lsb;

  if (!slice->field_pic_flag)
    BottomFieldOrderCnt = TopFieldOrderCnt + slice->delta_pic_order_cnt_bottom;
  else
    BottomFieldOrderCnt = PicOrderCntMsb + slice->pic_order_cnt_lsb;

  /* Derivation of PicOrderCnt for the current picture ([1] 8.2.1, 8-1) */
  int32_t PicOrderCnt;
  if (!slice->field_pic_flag || _complementaryFieldPairPresent(handle))
    PicOrderCnt = MIN(TopFieldOrderCnt, BottomFieldOrderCnt);
  else if (!slice->bottom_field_flag)
    PicOrderCnt = TopFieldOrderCnt;
  else
    PicOrderCnt = BottomFieldOrderCnt;

  /* Update last picture properties */
  last_pic->TopFieldOrderCnt  = TopFieldOrderCnt;
  last_pic->PicOrderCntMsb    = PicOrderCntMsb;
  last_pic->pic_order_cnt_lsb = slice->pic_order_cnt_lsb;

  /* fprintf(
    stderr, "is_IDR_pic: %d, PicOrderCnt: %d, MaxPicOrderCntLsb: %d, BottomFieldOrderCnt: %d, TopFieldOrderCnt: %d, PicOrderCntMsb: %d, pic_order_cnt_lsb: %d\n",
    slice->is_IDR_pic,
    PicOrderCnt,
    MaxPicOrderCntLsb,
    BottomFieldOrderCnt,
    TopFieldOrderCnt,
    PicOrderCntMsb,
    slice->pic_order_cnt_lsb
  ); */

  return PicOrderCnt;
}

static int32_t _calcCurPicOrderCnt(
  H264ParametersHandlerPtr handle
)
{
  int32_t PicOrderCnt;

  const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;

  assert(handle->sequence_parameter_set_present);

  if (
    !handle->access_unit_delimiter_valid
    || !handle->sequence_parameter_set_GOP_valid
    || !handle->slice_valid
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
      "Broken Picture Order Count on field/picture %" PRId64 ", "
      "computation result is negative (PicOrderCnt = %" PRId32 ").\n",
      handle->cur_prog_param.nb_pictures,
      PicOrderCnt
    );
  }

  return PicOrderCnt;
}

static int64_t _computeAUPicOrderCnt(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
)
{
  assert(NULL != handle);

  int64_t PicOrderCnt = _calcCurPicOrderCnt(handle);
  if (PicOrderCnt < 0)
    return -1;

  if (
    handle->cur_prog_param.half_PicOrderCnt
    && (PicOrderCnt & 0x1)
    && _completePicturePresent(handle)
  ) {
    /* Unexpected Odd PicOrderCnt, value shall not be divided. */
    if (options.second_pass)
      LIBBLU_H264_ERROR_RETURN(
        "Unexpected odd picture order count value, "
        "values are broken.\n"
      );

    handle->cur_prog_param.half_PicOrderCnt = false;
    handle->cur_prog_param.restart_required = true;
    LIBBLU_H264_DEBUG_AU_TIMINGS("DISABLE HALF\n");
  }

  int64_t DivPicOrderCnt = PicOrderCnt >> handle->cur_prog_param.half_PicOrderCnt;

  LIBBLU_H264_DEBUG_AU_TIMINGS(
    "PicOrderCnt=%" PRId64 " DivPicOrderCnt=%" PRId64 "\n",
    PicOrderCnt,
    DivPicOrderCnt
  );

  handle->cur_prog_param.PicOrderCnt = DivPicOrderCnt;

  return PicOrderCnt;
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
    cpb_removal_time = handle->cur_prog_param.auCpbRemovalTime;
    dpb_output_time = handle->cur_prog_param.auDpbOutputTime;
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
    handle->script, param
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
  LIBBLU_H264_DEBUG_AU_PROCESSING("Check if this NAL unit mark the start of a new Access Unit.\n");
  bool AU_start_NAL_unit_type = false;

  if (!curState->is_VCL_NALU_reached) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      " => No VCL NAL unit of a primary coded picture has been reached.\n"
    );
    return false;
  }

  if (curState->is_new_prim_coded_pic_first_VCL_NALU_reached) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      " => A new first VCL NAL unit of a primary coded picture as been reached.\n"
    );
    curState->is_new_prim_coded_pic_first_VCL_NALU_reached = true;
    return true;
  }

  static const H264NalUnitTypeValue NALU_start_types[] = {
    NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION,
    NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET,
    NAL_UNIT_TYPE_PIC_PARAMETER_SET,
    NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER,
    NAL_UNIT_TYPE_PREFIX_NAL_UNIT,
    NAL_UNIT_TYPE_SUBSET_SEQUENCE_PARAMETER_SET,
    NAL_UNIT_TYPE_DEPTH_PARAMETER_SET,
    NAL_UNIT_TYPE_RESERVED_17,
    NAL_UNIT_TYPE_RESERVED_18
  };

  for (unsigned i = 0; !AU_start_NAL_unit_type && i < ARRAY_SIZE(NALU_start_types); i++) {
    if (NALU_start_types[i] == nal_unit_type) {
      LIBBLU_H264_DEBUG_AU_PROCESSING(
        "NALU type '%s' if one of the AU start expected ones.\n",
        H264NalUnitTypeStr(nal_unit_type)
      );

      AU_start_NAL_unit_type = true;
    }
  }

  if (AU_start_NAL_unit_type) {
    curState->is_VCL_NALU_reached = false; // Reset
    LIBBLU_H264_DEBUG_AU_PROCESSING(" => Start of a new Access Unit detected.\n");
  }
  else
    LIBBLU_H264_DEBUG_AU_PROCESSING(" => This NALU is part of pending Access Unit.\n");

  return AU_start_NAL_unit_type;
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
  int64_t frame_duration = handle->cur_prog_param.frameDuration;
  int64_t field_duration = handle->cur_prog_param.fieldDuration;

  if (handle->sei.pic_timing_valid) {
    /* Presence of picture timing SEI message */

    switch (handle->sei.pic_timing.pic_struct) {
    case H264_PIC_STRUCT_TOP_FLWD_BOT_TOP:
    case H264_PIC_STRUCT_BOT_FLWD_TOP_BOT:
      return frame_duration + field_duration;
    case H264_PIC_STRUCT_DOUBLED_FRAME:
      return 2 * frame_duration;
    case H264_PIC_STRUCT_TRIPLED_FRAME:
      return 3 * frame_duration;
    default:
      break;
    }
  }

  /* Otherwise, every picture is considered as during one frame. */
  return frame_duration;
}

static void setEsmsPtsRef(
  EsmsHandlerPtr script,
  const H264CurrentProgressParam *cur_prog
)
{
  uint64_t reference = (uint64_t) (
    cur_prog->init_dec_pic_nb_cnt_shift
    * cur_prog->frameDuration
  );

  script->PTS_reference = DIV_ROUND_UP(reference, 300ull);

  LIBBLU_H264_DEBUG_AU_TIMINGS(
    "Set PTS_reference: %" PRIu64 " ticks@90kHz %" PRIu64 " ticks@27MHz "
    "(init_dec_pic_nb_cnt_shift=%" PRIu64 ", frame duration=%" PRIu64 ").\n",
    script->PTS_reference,
    reference,
    cur_prog->init_dec_pic_nb_cnt_shift,
    cur_prog->frameDuration
  );
}

static int64_t _computeStreamPicOrderCnt(
  H264ParametersHandlerPtr handle
)
{
  H264CurrentProgressParam *cp = &handle->cur_prog_param;

  if (
    /* handle->slice.header.is_IDR_pic && */ 0 == cp->PicOrderCnt
    && 0 < cp->last_max_stream_PicOrderCnt
  ) {
    cp->max_stream_PicOrderCnt = cp->last_max_stream_PicOrderCnt + 1;
    cp->last_max_stream_PicOrderCnt = 0;
  }

  LIBBLU_H264_DEBUG_AU_TIMINGS(
    "PicOrderCnt=%" PRId64 " max_stream_PicOrderCnt=%" PRId64 "\n",
    cp->PicOrderCnt,
    cp->max_stream_PicOrderCnt
  );

  int64_t stream_PicOrderCnt = cp->PicOrderCnt + cp->max_stream_PicOrderCnt;

  cp->last_max_stream_PicOrderCnt = MAX(
    cp->last_max_stream_PicOrderCnt,
    stream_PicOrderCnt
  );

  return stream_PicOrderCnt;
}

static int _initProperties(
  EsmsHandlerPtr esms,
  H264CurrentProgressParam *cur_prog,
  const H264SPSDataParameters *sps,
  H264ConstraintsParam constraints
)
{
  /* Convert to BDAV syntax. */
  HdmvVideoFormat video_format = getHdmvVideoFormat(
    sps->FrameWidth,
    sps->FrameHeight,
    !sps->frame_mbs_only_flag
  );
  if (!video_format)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Resolution %ux%u%c is unsupported.\n",
      sps->FrameWidth,
      sps->FrameHeight,
      (sps->frame_mbs_only_flag) ? "p" : "i"
    );

  if (cur_prog->frameRate < 0)
    LIBBLU_H264_ERROR_RETURN("Missing Frame-rate information (not present in bitstream).\n");

  HdmvFrameRateCode frame_rate_code = getHdmvFrameRateCode(cur_prog->frameRate);
  if (!frame_rate_code)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Frame-rate %.3f is unsupported.\n",
      cur_prog->frameRate
    );

  esms->prop = (LibbluESProperties) {
    .coding_type   = STREAM_CODING_TYPE_AVC,
    .video_format  = video_format,
    .frame_rate    = frame_rate_code,
    .profile_IDC   = sps->profile_idc,
    .level_IDC     = sps->level_idc
  };

  LibbluESH264SpecProperties *h264_fmt_prop = esms->fmt_prop.h264;
  h264_fmt_prop->constraint_flags = valueH264ContraintFlags(sps->constraint_set_flags);
  if (sps->NalHrdBpPresentFlag) {
    const H264HrdParameters *hrd = &sps->vui_parameters.nal_hrd_parameters;
    h264_fmt_prop->CpbSize = hrd->SchedSel[hrd->cpb_cnt_minus1].CpbSize;
    h264_fmt_prop->BitRate = hrd->SchedSel[hrd->cpb_cnt_minus1].BitRate;

    esms->bitrate  = hrd->SchedSel[hrd->cpb_cnt_minus1].BitRate;
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
    h264_fmt_prop->CpbSize = 1200 * constraints.MaxCPB;
    h264_fmt_prop->BitRate = constraints.max_bit_rate;

    esms->bitrate  = constraints.max_bit_rate;
  }

  cur_prog->frameDuration = MAIN_CLOCK_27MHZ / cur_prog->frameRate;
  cur_prog->fieldDuration = cur_prog->frameDuration / 2;

  setEsmsPtsRef(esms, cur_prog);
  return 0;
}

static int _registerAccessUnitNALUs(
  H264ParametersHandlerPtr handle
)
{
  H264CurrentProgressParam *cur_prog = &handle->cur_prog_param;

  if (!cur_prog->cur_access_unit.nb_used_NALUs)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected empty access unit, no NALU received.\n"
    );

  size_t cur_PES_frame_offset = 0;
  for (unsigned NALU_idx = 0; NALU_idx < cur_prog->cur_access_unit.nb_used_NALUs; NALU_idx++) {
    const H264AUNalUnitPtr au_NALU_cell = &cur_prog->cur_access_unit.NALUs[NALU_idx];

    if (au_NALU_cell->replace) {
      size_t written_bytes = 0;

      switch (au_NALU_cell->nal_unit_type) {
#if 0
      case NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION:
        if (
          isH264SeiBufferingPeriodPatchMessage(
            (H264SeiRbspParameters *) au_NALU_cell->replacementParam
          )
        ) {
          written_bytes = appendH264SeiBufferingPeriodPlaceHolder(
            handle,
            cur_PES_frame_offset,
            (H264SeiRbspParameters *) au_NALU_cell->replacementParam
          );
          break;
        }

        written_bytes = appendH264Sei(
          handle,
            cur_PES_frame_offset,
          (H264SeiRbspParameters *) au_NALU_cell->replacementParam
        );
        break;
#endif

      case NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET:
        /* Update SPS */
        written_bytes = appendH264SequenceParametersSet(
          handle,
          cur_PES_frame_offset,
          &au_NALU_cell->replacementParameters.sps
        );
        break;

      default:
        LIBBLU_H264_ERROR_RETURN(
          "Unknown replacementParameters NAL unit type code: 0x%" PRIx8 ".\n",
          au_NALU_cell->nal_unit_type
        );
      }

      if (0 == written_bytes)
        return -1; /* Zero byte written = Error. */

      /* TODO */
      cur_PES_frame_offset += written_bytes /* written bytes */;
    }
    else {
      assert(cur_PES_frame_offset <= UINT32_MAX);

      int ret = appendAddPesPayloadCommandEsmsHandler(
        handle->script,
        handle->script_src_file_idx,
        cur_PES_frame_offset,
        au_NALU_cell->startOffset,
        au_NALU_cell->length
      );
      if (ret < 0)
        return -1;

      cur_PES_frame_offset += au_NALU_cell->length;
    }
  }

  /* Saving biggest picture AU size (for CPB computations): */
  if (cur_prog->largestAUSize < cur_PES_frame_offset) {
    /* Biggest I picture AU. */
    if (is_I_H264SliceTypeValue(handle->slice.header.slice_type))
      cur_prog->largestIPicAUSize = MAX(
        cur_prog->largestIPicAUSize,
        cur_PES_frame_offset
      );

    /* Biggest picture AU. */
    cur_prog->largestAUSize = cur_PES_frame_offset;
  }

  return 0;
}

static int _processPESFrame(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
)
{
  assert(NULL != handle);

  /* Check Access Unit content : */
  if (checkContentH264AccessUnit(handle) < 0)
    return -1;

  /* Check BD constraints : */
  if (handle->bd_constraints_initialized) {
    if (checkBDConstraintsH264AccessUnit(handle) < 0)
      return -1;
  }

  H264CurrentProgressParam *cur = &handle->cur_prog_param;

  if (!cur->initialized_param) {
    /* Set pictures counting mode (apply correction if 2nd pass) */
    if (!options.second_pass) {
      cur->half_PicOrderCnt = true;
        // By default, expect to divide by two the PicOrderCnt.
    }
    else {
      cur->half_PicOrderCnt          = options.half_PicOrderCnt;
      cur->init_dec_pic_nb_cnt_shift = options.init_dec_pic_nb_cnt_shift;
    }

    const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;
    if (_initProperties(handle->script, &handle->cur_prog_param, sps, handle->constraints) < 0)
      return -1;

    cur->initialized_param = true;
  }

  int64_t stream_PicOrderCnt = _computeStreamPicOrderCnt(handle);
  int64_t pts_diff_shift = (
    stream_PicOrderCnt - cur->decoding_PicOrderCnt
    + cur->init_dec_pic_nb_cnt_shift
  );

  if (pts_diff_shift < 0) {
    LIBBLU_H264_DEBUG_AU_TIMINGS(
      "stream_PicOrderCnt=%" PRId64 " pts_diff_shift=%" PRId64 "\n",
      stream_PicOrderCnt,
      pts_diff_shift
    );

    if (options.second_pass)
      LIBBLU_H264_ERROR_RETURN(
        "Broken picture order count, negative decoding delay.\n"
      );

    cur->init_dec_pic_nb_cnt_shift += - pts_diff_shift;
    cur->restart_required = true;

    LIBBLU_H264_DEBUG_AU_TIMINGS(
      "ADJUST init_dec_pic_nb_cnt_shift=%" PRId64 "\n",
      cur->init_dec_pic_nb_cnt_shift
    );
  }

  int64_t dts = cur->last_pic.dts + cur->last_pic.duration;
  int64_t pts = dts + pts_diff_shift *cur->frameDuration;
  uint64_t dts_90kHz = (cur->last_pic.dts / 300) + DIV_ROUND_UP(cur->last_pic.duration, 300);
  uint64_t pts_90kHz = dts_90kHz + DIV_ROUND_UP(pts_diff_shift *cur->frameDuration, 300);

  LIBBLU_H264_DEBUG_AU_TIMINGS(" -> PTS: %" PRIu64 "@90kHz %" PRIu64 "@27MHz\n", pts_90kHz, pts);
  LIBBLU_H264_DEBUG_AU_TIMINGS(" -> DTS: %" PRIu64 "@90kHz %" PRIu64 "@27MHz\n", dts_90kHz, dts);
  LIBBLU_H264_DEBUG_AU_TIMINGS(" -> StreamPicOrderCnt: %" PRId32 "\n", stream_PicOrderCnt);

  cur->last_pic.dts += cur->last_pic.duration;
  cur->last_pic.duration = _computeDtsIncrement(handle);
  cur->decoding_PicOrderCnt++;

  int ret = initVideoPesPacketEsmsHandler(
    handle->script,
    handle->slice.header.slice_type,
    pts_90kHz
  );
  if (ret < 0)
    return -1;

  // Decoding Time Stamp
  if (pts_90kHz != dts_90kHz) {
    if (setDecodingTimeStampPesPacketEsmsHandler(handle->script, dts_90kHz) < 0)
      return -1;
  }

  // Entry point
  bool is_GOP_start = (
    is_I_H264SliceTypeValue(handle->slice.header.slice_type)
    && handle->sequence_parameter_set_valid
  );

  if (is_GOP_start) {
    LIBBLU_H264_DEBUG_AU_PROCESSING(" -> Access unit is marked as an entry point.\n");
    if (markAsEntryPointPesPacketEsmsHandler(handle->script) < 0)
      return -1;
  }

  // H.264 Extension Data
  if (_setBufferingInformationsAccessUnit(handle, options, pts_90kHz, dts_90kHz) < 0)
    return -1;


  if (_registerAccessUnitNALUs(handle) < 0)
    return -1;

  if (writePesPacketEsmsHandler(handle->script_file, handle->script) < 0)
    return -1;

  /* Clean H264AUNalUnit : */
  if (cur->cur_access_unit.is_in_process_NALU) {
    /* Replacing current cell at list top. */
    cur->cur_access_unit.NALUs[0] = (
      cur->cur_access_unit.NALUs[cur->cur_access_unit.nb_used_NALUs]
    );
  }

  cur->cur_access_unit.size = 0;
  cur->cur_access_unit.nb_used_NALUs = 0;

  handle->script->PTS_final = pts_90kHz;

  return 0;
}

static bool _areEnoughDataToInitHrdVerifier(
  const H264ParametersHandlerPtr handle
)
{
  if (!handle->sequence_parameter_set_GOP_valid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing valid SPS.\n");

  if (!handle->slice_valid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing slice header.\n");

  if (!handle->sei.pic_timing_valid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing Picture Timing SEI message.\n");

  if (!handle->sei.buffering_period_GOP_valid)
    LIBBLU_H264_HRDV_INFO_BRETURN("Disabled by missing Buffering Period SEI message.\n");

  return true;
}

static bool _areSupportedParametersToInitHrdVerifier(
  const H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions *options
)
{
  return checkH264CpbHrdVerifierAvailablity(
    options,
    &handle->sequence_parameter_set.data
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
    &handle->sequence_parameter_set.data,
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
  assert(hrdVerifierCtx->pesNbAlreadyProcessedBytes <= handle->cur_prog_param.cur_access_unit.size * 8);

  int64_t frameSize = handle->cur_prog_param.cur_access_unit.size * 8
    - hrdVerifierCtx->pesNbAlreadyProcessedBytes
  ;

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
    LIBBLU_H264_DEBUG_AU_PROCESSING("Got a complete picture, processing to PES frame cutting.\n");
    if (_processPESFrame(handle, settings->options) < 0)
      return -1;

    if (NULL != *hrdVerifierCtxPtr)
      (*hrdVerifierCtxPtr)->pesNbAlreadyProcessedBytes = 0;

    if (
      handle->cur_prog_param.restart_required
      && H264_PICTURES_BEFORE_RESTART <= handle->cur_prog_param.nb_frames
    ) {
      settings->askForRestart = true;
    }
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

  if (!handle->access_unit_delimiter_present || !handle->access_unit_delimiter_valid) {
    if (checkH264AccessUnitDelimiterCompliance(&handle->warning_flags, AUD_param) < 0)
      return -1;
  }
  else {
    if (5 < handle->cur_prog_param.last_NALU_type) {
      bool is_constant = constantH264AccessUnitDelimiterCheck(
        handle->access_unit_delimiter, AUD_param
      );

      if (is_constant)
        handle->cur_prog_param.presence_of_unnecessary_AUD = true;
      else {
        LIBBLU_H264_ERROR_RETURN(
          "Presence of an unexpected access unit delimiter NALU "
          "(Rec. ITU-T H.264 7.4.1.2.3 violation).\n"
        );
      }
    }
  }

  handle->access_unit_delimiter_present = true;
  handle->access_unit_delimiter_valid   = true;
  handle->access_unit_delimiter         = AUD_param; /* Update */

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

  if (H264_MAX_CPB_CONFIGURATIONS < param->cpb_cnt_minus1 + 1u)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected number of alternative CPB specifications in bitstream "
      "('cpb_cnt_minus1' + 1 == %u, expect at most %u).\n",
      param->cpb_cnt_minus1 + 1u,
      H264_MAX_CPB_CONFIGURATIONS
    );

  /* [u4 bit_rate_scale] */
  if (readBitsNal(handle, &value, 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->bit_rate_scale = value;

  /* [u4 cpb_size_scale] */
  if (readBitsNal(handle, &value, 4) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();
  param->cpb_size_scale = value;

  memset(param->SchedSel, 0x00, (param->cpb_cnt_minus1 + 1) * sizeof(H264SchedSel));

  for (ShedSelIdx = 0; ShedSelIdx <= param->cpb_cnt_minus1; ShedSelIdx++) {
    /* [ue bit_rate_value_minus1[ShedSelIdx]] */
    if (readExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->SchedSel[ShedSelIdx].bit_rate_value_minus1 = value;

    param->SchedSel[ShedSelIdx].BitRate =
      ((int64_t) param->SchedSel[ShedSelIdx].bit_rate_value_minus1 + 1)
      << (6 + param->bit_rate_scale)
    ;

    /* [ue cpb_size_value_minus1[ShedSelIdx]] */
    if (readExpGolombCodeNal(handle, &value, 32) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->SchedSel[ShedSelIdx].cpb_size_value_minus1 = value;

    param->SchedSel[ShedSelIdx].CpbSize =
      ((int64_t) param->SchedSel[ShedSelIdx].cpb_size_value_minus1 + 1)
      << (4 + param->cpb_size_scale)
    ;

    /* [b1 cbr_flag[ShedSelIdx]] */
    if (readBitsNal(handle, &value, 1) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->SchedSel[ShedSelIdx].cbr_flag = value;
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

  param->CpbDpbDelaysPresentFlag = (
    param->nal_hrd_parameters_present_flag
    || param->vcl_hrd_parameters_present_flag
  );

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
  handle->cur_prog_param.frameRate = -1;
  if (
    sps->vui_parameters_present_flag
    && vui->timing_info_present_flag
  ) {
    /* Frame rate information present */
    handle->cur_prog_param.frameRate = vui->FrameRate;
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

  param->NalHrdBpPresentFlag = param->vui_parameters.nal_hrd_parameters_present_flag;
  param->VclHrdBpPresentFlag = param->vui_parameters.vcl_hrd_parameters_present_flag;

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
  H264SPSDataParameters SPS_data;

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Sequence Parameters Set NAL unit type (receive: %s).\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  /* seq_parameter_set_data() */
  if (_parseH264SequenceParametersSetData(handle, &SPS_data) < 0)
    return -1;

  /* rbsp_trailing_bits() */
  if (_parseH264RbspTrailingBits(handle) < 0)
    LIBBLU_H264_READING_FAIL_RETURN();

  /* Check */
  if (!handle->access_unit_delimiter_valid)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Missing mandatory AUD before SPS.\n"
    );

  if (!handle->sequence_parameter_set_present) {
    if (checkH264SequenceParametersSetCompliance(handle, SPS_data) < 0)
      return -1;

    if (checkH264SpsBitstreamCompliance(options, SPS_data) < 0)
      return -1;
  }
  else {
    bool constant_SPS = constantH264SequenceParametersSetCheck(
      handle->sequence_parameter_set.data,
      SPS_data
    );

    if (!constant_SPS) {
      if (checkH264SequenceParametersSetCompliance(handle, SPS_data) < 0)
        return -1;

      if (checkH264SpsBitstreamCompliance(options, SPS_data) < 0)
        return -1;

      const H264SequenceParametersSetDataParameters old_SPS_data =
        handle->sequence_parameter_set.data
      ;

      if (checkH264SPSChangeBDCompliance(&handle->warning_flags, old_SPS_data, SPS_data) < 0)
        return -1;
    }
    else if (handle->sequence_parameter_set_valid)
      handle->cur_prog_param.presence_of_unnecessary_SPS = true;
  }

  handle->sequence_parameter_set_present   = true;
  handle->sequence_parameter_set_GOP_valid = true;
  handle->sequence_parameter_set_valid     = true;
  handle->sequence_parameter_set.data      = SPS_data; // Update

  return 0;
}

int decodeH264PicParametersSet(
  H264ParametersHandlerPtr handle
)
{
  /* pic_parameter_set_rbsp() */
  uint32_t value;

  H264PicParametersSetParameters pps; /* Output structure */

  assert(NULL != handle);

  if (getNalUnitType(handle) != NAL_UNIT_TYPE_PIC_PARAMETER_SET)
    LIBBLU_H264_ERROR_RETURN(
      "Expected a Picture Parameters Set NAL unit type (receive: '%s').\n",
      H264NalUnitTypeStr(getNalUnitType(handle))
    );

  if (!handle->access_unit_delimiter_present)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory AUD NALU before PPS NALU.\n"
    );

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

  /* Checking SPS id */ // TODO: Support more than one SPS
  if (0x00 != pps.seq_parameter_set_id || !handle->sequence_parameter_set_GOP_valid)
    LIBBLU_H264_ERROR_RETURN(
      "The Picture Parameter Set references an invalid SPS or one that has not been "
      "communicated since the last IDR access unit "
      "('seq_parameter_set_id' == %u).\n",
      pps.seq_parameter_set_id
    );
  const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;

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
      pps.nbScalingMatrix = (
        6
        + ((sps->chroma_format_idc != H264_CHROMA_FORMAT_444) ? 2 : 6)
        * pps.transform_8x8_mode_flag
      );

      /* Initialization */
      memset(&pps.pic_scaling_list_present_flag, false, 12 * sizeof(bool));
      memset(&pps.UseDefaultScalingMatrix4x4Flag, true, 6 * sizeof(bool));
      memset(&pps.UseDefaultScalingMatrix8x8Flag, true, 6 * sizeof(bool));

      for (unsigned i = 0; i < pps.nbScalingMatrix; i++) {
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
  if (H264_MAX_PPS <= pps.pic_parameter_set_id)
    LIBBLU_ERROR_RETURN(
      "Internal error, unexpected PPS id %u.\n",
      pps.pic_parameter_set_id
    );

  if (checkH264PicParametersSetCompliance(handle, pps) < 0)
    return -1;

  if (!handle->picture_parameter_set_present[pps.pic_parameter_set_id]) {
    /* Newer PPS */
    handle->picture_parameter_set_present  [pps.pic_parameter_set_id] = true;
    handle->nb_picture_parameter_set_present++;
  }

  if (1 < handle->nb_picture_parameter_set_present) {
    /* Checking constant parameters between PPS */
    if (handle->cur_prog_param.entropy_coding_mode_flag != pps.entropy_coding_mode_flag)
      LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
        LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, PPS_entropy_coding_mode_flag_change),
        "Entropy coding mode ('entropy_coding_mode_flag') in Picture Parameter "
        "Set shall remain the same accross all PPS.\n"
      );
  }
  else {
    /* Init constant PPS parameters */
    handle->cur_prog_param.entropy_coding_mode_flag = pps.entropy_coding_mode_flag;
  }

  handle->picture_parameter_set_GOP_valid[pps.pic_parameter_set_id] = true;
  handle->picture_parameter_set_valid    [pps.pic_parameter_set_id] = true;
  handle->nb_picture_parameter_set_valid++;

  if (updatePPSH264Parameters(handle, &pps, pps.pic_parameter_set_id) < 0)
    return -1;

  return 0;
}

static int _parseH264SeiBufferingPeriod(
  H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod *param
)
{
  /* buffering_period(payloadSize) - Annex D.1.2 */
  uint32_t value;

  assert(NULL != handle);
  assert(NULL != param);

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

  /* Checking SPS id */ // TODO: Support more than one SPS
  if (0x00 != param->seq_parameter_set_id || !handle->sequence_parameter_set_GOP_valid)
    LIBBLU_H264_ERROR_RETURN(
      "The Buffering Period SEI message references an invalid SPS or one "
      "that has not been communicated since the last IDR access unit "
      "('seq_parameter_set_id' == %u).\n",
      param->seq_parameter_set_id
    );
  const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;

  if (sps->NalHrdBpPresentFlag) {
    const H264HrdParameters *sps_hrd = &sps->vui_parameters.nal_hrd_parameters;

    for (unsigned SchedSelIdx = 0; SchedSelIdx <= sps_hrd->cpb_cnt_minus1; SchedSelIdx++) {
      unsigned fld_size = sps_hrd->initial_cpb_removal_delay_length_minus1 + 1;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fld_size) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay = value;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fld_size) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset = value;
    }
  }

  if (sps->VclHrdBpPresentFlag) {
    const H264HrdParameters *sps_hrd = &sps->vui_parameters.vcl_hrd_parameters;

    for (unsigned SchedSelIdx = 0; SchedSelIdx <= sps_hrd->cpb_cnt_minus1; SchedSelIdx++) {
      unsigned fld_size = sps_hrd->initial_cpb_removal_delay_length_minus1 + 1;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fld_size) < 0)
        LIBBLU_H264_READING_FAIL_RETURN();
      param->vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay = value;

      /* [u<initial_cpb_removal_delay_length_minus1+1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (readBitsNal(handle, &value, fld_size) < 0)
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

  if (!handle->sequence_parameter_set_present)
    LIBBLU_H264_ERROR_RETURN(
      "Missing SPS and VUI parameters, "
      "Pic Timing SEI message shouldn't be present.\n"
    );

  sps = &handle->sequence_parameter_set.data;

  if (!sps->vui_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Missing VUI parameters in SPS, "
      "Pic Timing SEI message shouldn't be present.\n"
    );

  vui = &sps->vui_parameters;

  if (!vui->CpbDpbDelaysPresentFlag && !vui->pic_struct_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "CpbDpbDelaysPresentFlag and pic_struct_present_flag are equal to 0b0, "
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

    // handle->cur_prog.lastPicStruct = param->pic_struct;

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
      &param->buffering_period
    );
    break;

  case H264_SEI_TYPE_PIC_TIMING:
    ret = _parseH264SeiPicTiming(
      handle,
      &param->pic_timing
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
      &param->recovery_point
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
  H264SeiMessageParameters msg;

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
    if (_parseH264SupplementalEnhancementInformationMessage(handle, &msg) < 0)
      return -1;

    if (msg.tooEarlyEnd) {
      /* x264 fix */
      LIBBLU_H264_DEBUG(
        "Warning: Too early end of NAL unit "
        "(x264 user_data_unregistered wrong length fix, skipped).\n"
      );
      return reachNextNal(handle);
    }

    LIBBLU_H264_DEBUG_SEI(
      " SEI Message payload - %s.\n",
      H264SEIMessagePayloadTypeStr(msg.payloadType)
    );
    LIBBLU_H264_DEBUG_SEI(
      "  => Type (payloadType): %zu.\n",
      msg.payloadType
    );
    LIBBLU_H264_DEBUG_SEI(
      "  => Size (payloadSize): %zu byte(s).\n",
      msg.payloadSize
    );
    LIBBLU_H264_DEBUG_SEI(
      "  Content:\n"
    );

    /* Compliance checks and parameters saving : */
    switch (msg.payloadType) {
    case H264_SEI_TYPE_BUFFERING_PERIOD: /* 0 - buffering_period() */
      if (!handle->sei.buffering_period_present) {
        if (checkH264SeiBufferingPeriodCompliance(handle, msg.buffering_period) < 0)
          return -1;
      }
      else {
        bool is_constant = constantH264SeiBufferingPeriodCheck(
          handle,
          handle->sei.buffering_period,
          msg.buffering_period
        );

        if (!is_constant) {
          int ret = checkH264SeiBufferingPeriodChangeCompliance(
            handle,
            handle->sei.buffering_period,
            msg.buffering_period
          );
          if (ret < 0)
            return -1;
        }
      }

      handle->sei.buffering_period_present   = true;
      handle->sei.buffering_period_GOP_valid = true;
      handle->sei.buffering_period_valid     = true;
      handle->sei.buffering_period           = msg.buffering_period; // Update
      break;

    case H264_SEI_TYPE_PIC_TIMING: /* 1 - pic_timing() */
      /* Always update */
      if (checkH264SeiPicTimingCompliance(handle, msg.pic_timing) < 0)
        return -1;

      if (handle->sei.pic_timing_present) {
        /* Check changes */
#if 0
        ret = checkH264SeiPicTimingChangeCompliance(
          handle,
          &handle->sei.pic_timing,
          &msg.pic_timing
        );
#endif
      }

      handle->sei.pic_timing_present = true;
      handle->sei.pic_timing_valid   = true;
      handle->sei.pic_timing         = msg.pic_timing; // Update
      break;

    case H264_SEI_TYPE_RECOVERY_POINT: /* 6 - recovery_point() */

      if (!handle->sei.recovery_point_present) {
        if (checkH264SeiRecoveryPointCompliance(handle, msg.recovery_point) < 0)
          return -1;
      }
      else {
        bool is_constant = constantH264SeiRecoveryPointCheck(
          handle->sei.recovery_point,
          msg.recovery_point
        );
        if (!is_constant) {
          /* Check changes */
          if (checkH264SeiRecoveryPointChangeCompliance(handle, msg.recovery_point) < 0)
            return -1;
        }
      }

      handle->sei.recovery_point_present = true;
      handle->sei.recovery_point_valid   = true;
      handle->sei.recovery_point         = msg.recovery_point; // Update
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

  spsData = &handle->sequence_parameter_set.data;

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
    if (param->pres_of_mem_man_ctrl_op_5)
      LIBBLU_H264_ERROR_RETURN(
        "Presence of multiple 'memory_management_control_operation' "
        "equal to 5 in a slice header.\n"
      );
    param->pres_of_mem_man_ctrl_op_5 = true;
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

static unsigned _calcPrevRefFrameNum(
  const H264SliceHeaderParameters *slice_header,
  const H264CurrentProgressParam *cur
)
{
  if (slice_header->is_IDR_pic)
    return 0u;

  if (cur->gaps_in_frame_num_since_prev_ref_prim_coded_pic)
    return cur->last_non_existing_frame_num;

  return cur->prev_ref_prim_coded_pic.frame_num;
}

static int _parseH264SliceHeader(
  H264ParametersHandlerPtr handle,
  H264SliceHeaderParameters *param
)
{
  /* slice_header() - 7.3.3 Slice header syntax */
  uint32_t value;

  bool is_ref_pic         = isReferencePictureH264NalRefIdcValue(getNalRefIdc(handle));
  bool is_IDR_pic         = isIDRPictureH264NalUnitTypeValue(getNalUnitType(handle)); // IdrPicFlag
  bool is_coded_slice_ext = isCodedSliceExtensionH264NalUnitTypeValue(getNalUnitType(handle));

  /* bool frameNumEqualPrevRefFrameNum; */  /* TODO */

  assert(NULL != handle);
  assert(NULL != param);

  if (!handle->sequence_parameter_set_present)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory SPS parameters, Unable to decode slice_header().\n"
    );
  const H264SequenceParametersSetDataParameters *sps =
    &handle->sequence_parameter_set.data
  ;

  if (!handle->nb_picture_parameter_set_present)
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
  if (!handle->picture_parameter_set_GOP_valid[param->pic_parameter_set_id])
    LIBBLU_H264_ERROR_RETURN(
      "The slice header references an invalid PPS or one that has not been "
      "communicated since the last IDR access unit "
      "('pic_parameter_set_id' == %u).\n",
      param->pic_parameter_set_id
    );
  const H264PicParametersSetParameters *pps = handle->picture_parameter_set[
    param->pic_parameter_set_id
  ];

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

  if (is_IDR_pic) {
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

  if (is_coded_slice_ext) {
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
    if (_parseH264DecRefPicMarking(handle, &param->dec_ref_pic_marking, is_IDR_pic) < 0)
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
    else
      param->sp_for_switch_flag = 0;

    /* [se slice_qs_delta] */
    if (readSignedExpGolombCodeNal(handle, &value, 16) < 0)
      LIBBLU_H264_READING_FAIL_RETURN();
    param->slice_qs_delta = (int) value;
  }
  else
    param->slice_qs_delta = param->sp_for_switch_flag = 0;

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
    else
      param->slice_group_change_cycle = 0;
  }
  else
    param->slice_group_change_cycle = 0;

  /* Computed parameters: */
  param->is_ref_pic                = is_ref_pic;
  param->is_IDR_pic                = is_IDR_pic;
  param->is_coded_slice_ext        = is_coded_slice_ext;
  param->sps_pic_order_cnt_type    = sps->pic_order_cnt_type;
  param->pres_of_mem_man_ctrl_op_5 = param->dec_ref_pic_marking.pres_of_mem_man_ctrl_op_5;

  param->MbaffFrameFlag = sps->mb_adaptive_frame_field_flag && !param->field_pic_flag;

  param->PicHeightInMbs      = sps->FrameHeightInMbs / (1 + param->field_pic_flag);
  param->PicHeightInSamplesL = param->PicHeightInMbs * 16;
  param->PicHeightInSamplesC = param->PicHeightInMbs * sps->MbHeightC;
  param->PicSizeInMbs        = param->PicHeightInMbs * sps->PicWidthInMbs;

  param->PrevRefFrameNum = _calcPrevRefFrameNum(param, &handle->cur_prog_param);

  return 0;
}

typedef enum {
  H264_FRST_VCL_NALU_PCP_RET_ERROR  = -1,  /**< Fatal error happen.          */
  H264_FRST_VCL_NALU_PCP_RET_FALSE  =  0,  /**< VCL NALU is not the start of
    a new primary coded picture.                                             */
  H264_FRST_VCL_NALU_PCP_RET_TRUE   =  1   /**< Successful detection of the
    first VCL NALU of a primary coded picture.                               */
} H264FirstVclNaluPCPRet;

static H264FirstVclNaluPCPRet _detectFirstVCLNalUnitOfPrimCodedPicture(
  H264SliceHeaderParameters last,
  H264SliceHeaderParameters cur
)
{
  /* [1] 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture */
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
    BOOL_STR(last.is_ref_pic),
    BOOL_STR(cur.is_ref_pic)
  );

  if (last.is_ref_pic ^ cur.is_ref_pic)
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
    last.is_IDR_pic,
    cur.is_IDR_pic
  );

  if (last.is_IDR_pic != cur.is_IDR_pic)
    return H264_FRST_VCL_NALU_PCP_RET_TRUE;

  if (cur.is_IDR_pic) {
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
  H264ParametersHandlerPtr handle
)
{
  H264SliceLayerWithoutPartitioningParameters slice;

  /* slice_header() */
  if (_parseH264SliceHeader(handle, &slice.header) < 0)
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
  H264FirstVclNaluPCPRet is_first_VCL_NALU_ret = (
    (!handle->slice_valid)
    ? H264_FRST_VCL_NALU_PCP_RET_TRUE
    : _detectFirstVCLNalUnitOfPrimCodedPicture(
        slice.header,
        handle->slice.header
      )
  );

  switch (is_first_VCL_NALU_ret) {
  case H264_FRST_VCL_NALU_PCP_RET_FALSE:
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      " => This VCL NALU is part of the pending primary picture.\n"
    );
    break;

  case H264_FRST_VCL_NALU_PCP_RET_TRUE:
    LIBBLU_H264_DEBUG_AU_PROCESSING(
      " => This VCL NALU mark a new primary picture.\n"
    );
    if (handle->cur_prog_param.is_VCL_NALU_reached) {
      handle->cur_prog_param.is_new_prim_coded_pic_first_VCL_NALU_reached = true;
      LIBBLU_H264_DEBUG_AU_PROCESSING(
        "Another primary picture is already present, marking the start "
        "of a new Access Unit.\n"
      );
    }
    break;

  default:
    return -1;
  }

  /* Always do checks : */
  LIBBLU_H264_DEBUG_SLICE("  Slice header:\n");

  int ret = 0;
  if (is_first_VCL_NALU_ret)
    ret = checkH264SliceLayerWithoutPartitioningCompliance(handle, slice);
  else
    ret = checkH264SliceLayerWithoutPartitioningChangeCompliance(handle, handle->slice, slice);
  if (ret < 0)
    return -1;

  /* Update current slice : */
  handle->slice_valid  = true;
  handle->slice.header = slice.header;

  return 0;
}

int decodeH264FillerData(
  H264ParametersHandlerPtr handle
)
{
  size_t length;
  uint32_t value;

  assert(NULL != handle);

  if (!handle->slice_valid)
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
    LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, AU_filler_data),
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

  H264ParametersHandlerPtr handle = createH264ParametersHandler(settings);
  if (NULL == handle)
    return -1;

  H264HrdVerifierContextPtr hrd_verif_ctx = NULL;
  bool sei_section = false; // Used to know when inserting SEI custom NALU.

  /* Main NAL units parsing loop. */
  while (!noMoreNal(handle) && !settings->askForRestart) {
    /* Progress bar : */
    printFileParsingProgressionBar(handle->file.inputFile);

    /* Initialize in deserializer the next NALU */
    if (_initNALU(handle) < 0)
      goto free_return;

    H264NalUnitTypeValue nal_unit_type = getNalUnitType(handle);
    H264NalRefIdcValue   nal_ref_idc   = getNalUnitRefIdc(handle);

    if (_isStartOfANewAU(&handle->cur_prog_param, nal_unit_type)) {
      if (_processAccessUnit(handle, &hrd_verif_ctx, settings) < 0)
        goto free_return;

      /* Reset settings for the next Access Unit : */
      resetH264ParametersHandler(handle, isIDRPictureH264NalUnitTypeValue(nal_unit_type));
    }

    switch (nal_unit_type) {
    case NAL_UNIT_TYPE_NON_IDR_SLICE: /* 0x01 / 1 */
    case NAL_UNIT_TYPE_IDR_SLICE:     /* 0x05 / 5 */
      if (decodeH264SliceLayerWithoutPartitioning(handle) < 0)
        goto free_return;
      break;

    case NAL_UNIT_TYPE_SLICE_PART_A_LAYER: /* 0x02 / 2 */
    case NAL_UNIT_TYPE_SLICE_PART_B_LAYER: /* 0x03 / 3 */
    case NAL_UNIT_TYPE_SLICE_PART_C_LAYER: /* 0x04 / 4 */
      LIBBLU_TODO_MSG("Unsupported partitioned slice data NALUs.\n");

      // handle->constraints.forbiddenSliceDataPartitionLayersNal
      handle->cur_prog_param.is_VCL_NALU_reached = true;
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
          && (handle->sei.buffering_period_valid || handle->sei.pic_timing_valid)
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

          handle->sei.buffering_period_valid = false;
          handle->sei.pic_timing_valid = false;

          if (reachNextNal(handle) < 0)
            goto free_return;
          continue;
        }
      }
      break;

    case NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET: /* 7 - SPS */
      if (decodeH264SequenceParametersSet(handle, settings->options) < 0)
        goto free_return;

      if (handle->cur_prog_param.presence_of_unnecessary_SPS) {
        LIBBLU_H264_CK_WCOND_WARNING(
          LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, SPS_duplicated),
          "Presence of duplicated Sequence Parameter Set NAL unit(s) "
          "in Access Unit(s).\n"
        );

        if (!settings->options.disable_fixes) {
          handle->cur_prog_param.presence_of_unnecessary_SPS = false;
          if (discardCurNalCell(handle) < 0)
            goto free_return;
          continue;
        }
      }

      /* Apply SPS VUI patch in case of missing/broken VUI : */
      if (
        handle->cur_prog_param.missing_VUI_video_signal_type
        || handle->cur_prog_param.wrong_VUI
      ) {
        handle->cur_prog_param.rebuild_VUI |= !settings->options.disable_fixes;
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

      if (handle->cur_prog_param.presence_of_unnecessary_AUD) {
        LIBBLU_H264_CK_WCOND_WARNING(
          LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, AUD_duplicated),
          "Presence of duplicated Access Unit Delimiter NALU(s) "
          "in Access Unit(s) (Rec. ITU-T H.264 7.4.1.2.3 violation).\n"
        );

        if (!settings->options.disable_fixes) {
          handle->cur_prog_param.presence_of_unnecessary_AUD = false;
          if (discardCurNalCell(handle) < 0)
            goto free_return;
          continue;
        }
      }
      break;

    case NAL_UNIT_TYPE_END_OF_SEQUENCE:   /* 10 - End of sequence */
    case NAL_UNIT_TYPE_END_OF_STREAM:     /* 11 - End of stream */
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

    if (isVCLH264NalUnitTypeValue(nal_unit_type)) {
      /* Compute Access Unit PicOrderCnt: */
      int64_t PicOrderCnt = _computeAUPicOrderCnt(handle, settings->options);
      if (PicOrderCnt < 0)
        goto free_return;

      /* Save slice properties */
      const H264SliceHeaderParameters *slice_header = &handle->slice.header;
      H264CurrentProgressParam *cur = &handle->cur_prog_param;

      /* Picture */
      H264LastPictureProperties *last_pic = &cur->last_pic;
      last_pic->pres_of_mem_man_ctrl_op_5 = slice_header->pres_of_mem_man_ctrl_op_5;
      last_pic->bottom_field_flag         = slice_header->bottom_field_flag;
      last_pic->PicOrderCnt               = PicOrderCnt;

      if (isReferencePictureH264NalRefIdcValue(nal_ref_idc)) {
        /* Reference picture */
        H264PrevRefPrimCodedPictureProperties *prev_ref_pic = &cur->prev_ref_prim_coded_pic;
        prev_ref_pic->field_picture_number = handle->cur_prog_param.nb_pictures;
        prev_ref_pic->frame_num            = slice_header->frame_num;
      }

      handle->cur_prog_param.is_VCL_NALU_reached = true;
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

        handle->sei.buffering_period_valid = true;
#else
        LIBBLU_H264_ERROR_FRETURN(
          "SEI rebuilding option is currently broken.\n"
        );
#endif
      }

      sei_section = false;
    }
  }

  if (settings->askForRestart || handle->cur_prog_param.restart_required) {
    LIBBLU_INFO(
      "Parsing guessing error found, "
      "restart parsing with corrected parameters.\n"
    );

    settings->options.second_pass = true;
    settings->options.half_PicOrderCnt = handle->cur_prog_param.half_PicOrderCnt;
    settings->options.init_dec_pic_nb_cnt_shift = handle->cur_prog_param.init_dec_pic_nb_cnt_shift;

    /* Quick exit. */
    destroyH264ParametersHandler(handle);
    destroyH264HrdVerifierContext(hrd_verif_ctx);
    return 0;
  }

  if (0 < handle->cur_prog_param.nb_slices_in_picture) {
    /* Process remaining pending slices. */
    if (_processPESFrame(handle, settings->options) < 0)
      goto free_return;
  }

  if (!handle->cur_prog_param.nb_frames)
    LIBBLU_ERROR_FRETURN("No picture in bitstream.\n");

  if (handle->cur_prog_param.nb_frames == 1) {
    /* Only one picture = Still picture. */
    // TODO: Improve detection of still pictures
    LIBBLU_INFO("Stream is detected as still picture.\n");
    setStillPictureEsmsHandler(handle->script);
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
    handle->sequence_parameter_set.data.FrameWidth,
    handle->sequence_parameter_set.data.FrameHeight,
    (handle->sequence_parameter_set.data.frame_mbs_only_flag ? 'p' : 'i')
  );
  lbc_printf(lbc_str("%.3f Im/s.\n"), handle->cur_prog_param.frameRate);
  printStreamDurationEsmsHandler(handle->script);
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

