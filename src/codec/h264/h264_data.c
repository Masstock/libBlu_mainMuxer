#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "h264_util.h"
#include "h264_data.h"

const char * h264PrimaryPictureTypeStr(
  const H264PrimaryPictureType val
)
{
  static const char * types[] = {
    "I",
    "I, P",
    "I, P, B",
    "SI",
    "SI, SP",
    "I, SI",
    "I, P, SI, SP",
    "I, P, B, SI, SP"
  };

  if (val < ARRAY_SIZE(types))
    return types[val];
  return "Unknown";
}

const char * h264ProfileIdcValueStr (
  const H264ProfileIdcValue val,
  const H264ContraintFlags constraints
)
{
  switch (val) {
    case H264_PROFILE_CAVLC_444_INTRA:
      return "CAVLC 4:4:4 Intra";

    case H264_PROFILE_BASELINE:
      if (constraints.set1)
        return "Constrained Baseline";
      else
        return "Baseline";

    case H264_PROFILE_MAIN:
      return "Main";

    case H264_PROFILE_EXTENDED:
      return "Extended";

    case H264_PROFILE_SCALABLE_BASELINE:
      return "Scalable Baseline";

    case H264_PROFILE_SCALABLE_HIGH:
      return "Scalable High";

    case H264_PROFILE_HIGH:
      if (constraints.set4) {
        if (constraints.set1)
          return "Constrained High";
        return "Progressive High";
      }
      return "High";

    case H264_PROFILE_MULTIVIEW_HIGH:
      return "Multiview High";

    case H264_PROFILE_HIGH_10:
      if (constraints.set4)
        return "Progressive High 10";
      return "High 10";

    case H264_PROFILE_HIGH_422:
      return "High 4:2:2";

    case H264_PROFILE_MFC_HIGH:
      return "MFC High";

    case H264_PROFILE_DEPTH_MFC_HIGH:
      return "MFC Depth High";

    case H264_PROFILE_MULTIVIEW_DEPTH_HIGH:
      return "Multiview Depth High";

    case H264_PROFILE_ENHANCED_MULTIVIEW_DEPTH_HIGH:
      return "Enhanced Multiview Depth High";

    case H264_PROFILE_HIGH_444_PREDICTIVE:
      return "High 4:4:4 Intra";

    default:
      break;
  }

  return "Unknown";
}

const char * h264ChromaFormatIdcValueStr(
  const H264ChromaFormatIdcValue val
)
{
  switch (val) {
    case H264_CHROMA_FORMAT_400:
      return "4:0:0";

    case H264_CHROMA_FORMAT_420:
      return "4:2:0";

    case H264_CHROMA_FORMAT_422:
      return "4:2:2";

    case H264_CHROMA_FORMAT_444:
      return "4:4:4";
  }

  return "Unknown";
}

const char * h264AspectRatioIdcValueStr(
  const H264AspectRatioIdcValue val
)
{
  static const char * aspectRatios[] = {
    "Unspecified",
    "1:1",
    "12:11",
    "10:11",
    "16:11",
    "40:33",
    "24:11",
    "20:11",
    "32:11",
    "80:33",
    "18:11",
    "15:11",
    "64:33",
    "160:99",
    "4:3",
    "3:2",
    "2:1"
  };

  if ((unsigned) val < ARRAY_SIZE(aspectRatios))
    return aspectRatios[val];
  if (val == H264_ASPECT_RATIO_IDC_EXTENDED_SAR)
    return "Extended_SAR";
  return "Unknown";
}

const char * h264VideoFormatValueStr(
  const H264VideoFormatValue val
)
{
  static const char * videoFormats[] = {
    "Component",
    "PAL",
    "NTSC",
    "SECAM",
    "MAC",
    "Unspecified"
  };

  if (val < ARRAY_SIZE(videoFormats))
    return videoFormats[val];
  return "Unknown";
}

const char * h264ColorPrimariesValueStr(
  const H264ColourPrimariesValue val
)
{
  static const char * colour_primaries[] = {
    "Unknown",
    "BT.709",
    "Unspecified",
    "Unknown",
    "BT.470M",
    "BR.470BG",
    "SMPTE 170M",
    "SMPTE 240M",
    "Generic",
    "BT.2020",
    "CIE XYZ",
    "DCI-P3",
    "Display P3"
  };

  if (val < ARRAY_SIZE(colour_primaries))
    return colour_primaries[val];
  if (val == H264_COLOR_PRIM_EBU3213)
    return "EBU 3213-E";
  return "Unknown";
}

const char * h264TransferCharacteristicsValueStr(
  const H264TransferCharacteristicsValue val
)
{
  static const char * transfer_characteristics[] = {
    "Unknown",
    "BT.709",
    "Unspecified",
    "Unknown",
    "BT.470M",
    "BR.470BG",
    "SMPTE 170M",
    "SMPTE 240M",
    "Linear",
    "Log 100:1",
    "Log 100 * Sqrt(10):1",
    "xvYCC",
    "BT.1361-0",
    "sRGB",
    "BT.2020 10 bits",
    "BT.2020 12 bits",
    "SMPTE 2084PQ",
    "SMPTE ST428-1",
    "BT.2100-2 HLG"
  };

  if (val < ARRAY_SIZE(transfer_characteristics))
    return transfer_characteristics[val];
  return "Unknown";
}

const char * h264MatrixCoefficientsValueStr(
  const H264MatrixCoefficientsValue val
)
{
  static const char * matrix_coefficients[] = {
    "sRGB/XYZ",
    "BT.709",
    "Unspecified",
    "Unknown",
    "BT.470M",
    "BR.470BG",
    "SMPTE 170M",
    "SMPTE 240M",
    "YCoCg",
    "BT.2100-2 Y'CbCr",
    "BT.2020",
    "SMPTE ST 2085",
    "Chromaticity-derived non-constant luminance system",
    "Chromaticity-derived constant luminance system",
    "BT.2100-2 ICtCp"
  };

  if (val < ARRAY_SIZE(matrix_coefficients))
    return matrix_coefficients[val];
  return "Unknown";
}

const char * h264WeightedBipredIdcStr(
  const H264WeightedBipredIdc val
)
{
  static const char * idcs[] = {
    "Default",
    "Explicit",
    "Implicit"
  };

  if (val < ARRAY_SIZE(idcs))
    return idcs[val];
  return "Unknown";
}

const char * h264PicStructValueStr(
  const H264PicStructValue val
)
{
  static const char * picStructs[] = {
    "(Progressive) Frame",
    "Top field",
    "Bottom field",
    "Top field, Bottom field",
    "Bottom field, Top field",
    "Top field, Bottom field, Top field repeated",
    "Bottom field, Top field, Bottom field repeated",
    "Frame doubling",
    "Frame tripling"
  };

  if (0 <= val && val < ARRAY_SIZE(picStructs))
    return picStructs[val];
  return "Unknown";
}

const char * h264CtTypeValueStr(
  const H264CtTypeValue val
)
{
  static const char * ctTypes[] = {
    "Progressive",
    "Interlaced",
    "Unknown"
  };

  if (0 <= val && val < ARRAY_SIZE(ctTypes))
    return ctTypes[val];
  return "Unknown";
}

const char * h264CountingTypeValueStr(
  const H264CountingTypeValue val
)
{
  static const char * countingTypes[] = {
    "No dropping of n_frames count values and no use of time_offset",
    "No dropping of n_frames count values",
    "Dropping of individual zero values of n_frames count",
    "Dropping of individual MaxFPS - 1 values of n_frames count",
    "dropping of the two lowest (value 0 and 1) n_frames counts when "
    "seconds_value is equal to 0 and minutes_value is not an integer "
    "multiple of 10",
    "dropping of unspecified individual n_frames count values",
    "dropping of unspecified numbers of unspecified n_frames count values"
  };

  if (0 <= val && val < ARRAY_SIZE(countingTypes))
    return countingTypes[val];
  return "Unknown";
}

const char * h264SliceTypeValueStr(
  const H264SliceTypeValue val
)
{
  static const char * types[] = {
    "P Slice Unrestricted",
    "B Slice Unrestricted",
    "I Slice Unrestricted",
    "SP Slice Unrestricted",
    "SI Slice Unrestricted",
    "P Slice",
    "B Slice",
    "I Slice",
    "SP Slice",
    "SI Slice"
  };

  if (0 <= val && val < ARRAY_SIZE(types))
    return types[val];
  return "Unknown";
}

const char * h264ColourPlaneIdValueStr(
  const H264ColourPlaneIdValue val
)
{
  static const char * colourPlane[] = {
    "Y (luma)",
    "Cb (blue differential chroma)",
    "Cr (red differential chroma)"
  };

  if (0 <= val && val < ARRAY_SIZE(colourPlane))
    return colourPlane[val];
  return "Unknown";
}

const char * h264ModificationOfPicNumsIdcValueStr(
  const H264ModificationOfPicNumsIdcValue val
)
{
  static const char * indices[] = {
    "Difference to subtract from picture number prediction value",
    "Difference to add from picture number prediction value",
    "Long-term picture used as reference picture",
    "End of loop indicator"
  };

  if (0 <= val && val < ARRAY_SIZE(indices))
    return indices[val];
  return "Unknown";
}

H264MemMngmntCtrlOpBlkPtr createH264MemoryManagementControlOperations(
  void
)
{
  H264MemMngmntCtrlOpBlkPtr op;

  op = (H264MemMngmntCtrlOpBlkPtr) calloc(1, sizeof(H264MemMngmntCtrlOpBlk));
  if (NULL == op)
    LIBBLU_H264_ERROR_NRETURN("Memory allocation error.\n");

  op->nextOperation = NULL;

  return op;
}

void closeH264MemoryManagementControlOperations(
  H264MemMngmntCtrlOpBlkPtr op
)
{
  H264MemMngmntCtrlOpBlkPtr nextOp;

  while (NULL != op) {
    nextOp = op->nextOperation;
    free(op->nextOperation);
    op = nextOp;
  }
}

H264MemMngmntCtrlOpBlkPtr copyH264MemoryManagementControlOperations(
  H264MemMngmntCtrlOpBlkPtr op
)
{
  H264MemMngmntCtrlOpBlkPtr copiedOp;

  if (NULL == op)
    return NULL;

  copiedOp = (H264MemMngmntCtrlOpBlkPtr) malloc(sizeof(H264MemMngmntCtrlOpBlk));
  if (NULL == copiedOp)
    LIBBLU_H264_ERROR_NRETURN("Memory allocation error.\n");

  memcpy(copiedOp, op, sizeof(H264MemMngmntCtrlOpBlk));

  if (NULL != op->nextOperation) {
    copiedOp->nextOperation = copyH264MemoryManagementControlOperations(
      op->nextOperation
    );
    if (NULL == copiedOp->nextOperation) {
      closeH264MemoryManagementControlOperations(copiedOp);
      return NULL;
    }
  }
  else
    copiedOp->nextOperation = NULL;

  return copiedOp;
}

const char * h264DeblockingFilterIdcStr(
  const H264DeblockingFilterIdc val
)
{
  static const char * mode[] = {
    "Enable",
    "Disable",
    "Disabled at slice boundaries"
  };

  if (0 <= val && val < ARRAY_SIZE(mode))
    return mode[val];
  return "Unknown";
}

int initH264NalDeserializerContext(
  H264NalDeserializerContext * dst,
  const lbc * filepath
)
{
  BitstreamReaderPtr inputFile;

  if (NULL == (inputFile = createBitstreamReaderDefBuf(filepath)))
    return -1;
  *dst = (H264NalDeserializerContext) {
    .inputFile = inputFile
  };

  return 0;
}

unsigned getH264MaxMBPS(
  uint8_t level_idc
)
{
  /* Max macroblock processing rate - MaxMBPS (MB/s) */
  /* Rec. ITU-T H.264 - Table A-1 */

  switch (level_idc) {
    case 10:
    case H264_LEVEL_1B:
      return 1485;
    case 11:
      return 3000;
    case 12:
      return 6000;
    case 13:
      return 11880;

    case 20:
      return 11880;
    case 21:
      return 19800;
    case 22:
      return 20250;

    case 30:
      return 40500;
    case 31:
      return 108000;
    case 32:
      return 216000;

    case 40:
    case 41:
      return 245760;
    case 42:
      return 522240;

    case 50:
      return 589824;
    case 51:
      return 983040;
    case 52:
      return 2073600;

    case 60:
      return 4177920;
    case 61:
      return 8355840;
    case 62:
      return 16711680;
  }

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MaxFS(
  uint8_t level_idc
)
{
  /* Max frame size - MaxFS (MBs) */
  /* Rec. ITU-T H.264 - Table A-1 */

  switch (level_idc) {
    case 10:
    case H264_LEVEL_1B:
      return 99;
    case 11:
    case 12:
    case 13:
    case 20:
      return 396;
    case 21:
      return 792;
    case 22:
    case 30:
      return 1620;
    case 31:
      return 3600;
    case 32:
      return 5120;
    case 40:
    case 41:
      return 8192;
    case 42:
      return 8704;
    case 50:
      return 22080;
    case 51:
    case 52:
      return 36864;
    case 60:
    case 61:
    case 62:
      return 139264;
  }

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MaxDpbMbs(
  uint8_t level_idc
)
{
  /* Max decoded picture buffer size - MaxDpbMbs (MBs) */
  /* Rec. ITU-T H.264 - Table A-1 */

  switch (level_idc) {
    case 10:
    case H264_LEVEL_1B:
      return 396;
    case 11:
      return 900;
    case 12:
    case 13:
    case 20:
      return 2376;
    case 21:
      return 4752;
    case 22:
    case 30:
      return 8100;
    case 31:
      return 18000;
    case 32:
      return 20480;
    case 40:
    case 41:
      return 32768;
    case 42:
      return 34816;
    case 50:
      return 110400;
    case 51:
    case 52:
      return 184320;
    case 60:
    case 61:
    case 62:
      return 696320;
  }

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MaxBR(
  uint8_t level_idc
)
{
  /* Max video bit rate - MaxBR (factor bits/s) */
  /* Rec. ITU-T H.264 - Table A-1 */

  switch (level_idc) {
    case 10:
      return 64;
    case H264_LEVEL_1B:
      return 128;
    case 11:
      return 192;
    case 12:
      return 384;
    case 13:
      return 768;
    case 20:
      return 2000;
    case 21:
    case 22:
      return 4000;
    case 30:
      return 10000;
    case 31:
      return 14000;
    case 32:
    case 40:
      return 20000;
    case 41:
    case 42:
      return 50000;
    case 50:
      return 135000;
    case 51:
    case 52:
    case 60:
      return 240000;
    case 61:
      return 480000;
    case 62:
      return 800000;
  }

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MaxCPB(
  uint8_t level_idc
)
{
  /* Max CPB size - MaxCPB (factor bits) */
  /* Rec. ITU-T H.264 - Table A-1 */

  switch (level_idc) {
    case 10:
      return 175;
    case H264_LEVEL_1B:
      return 350;
    case 11:
      return 500;
    case 12:
      return 1000;
    case 13:
    case 20:
      return 2000;
    case 21:
    case 22:
      return 4000;
    case 30:
      return 10000;
    case 31:
      return 14000;
    case 32:
      return 20000;
    case 40:
      return 25000;
    case 41:
    case 42:
      return 62500;
    case 50:
      return 135000;
    case 51:
    case 52:
    case 60:
      return 240000;
    case 61:
      return 480000;
    case 62:
      return 800000;
  }

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MaxVmvR(
  uint8_t level_idc
)
{
  /* Vertical MV component limit - MaxVmvR (luma frame samples) */
  /* Rec. ITU-T H.264 - Table A-1 */

  switch (level_idc) {
    case 10:
    case H264_LEVEL_1B:
      return 64;
    case 11:
    case 12:
    case 13:
    case 20:
      return 128;
    case 21:
    case 22:
    case 30:
      return 256;
    case 31:
    case 32:
    case 40:
    case 41:
    case 42:
    case 50:
    case 51:
    case 52:
      return 512;
    case 60:
    case 61:
    case 62:
      return 8192;
  }

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MinCR(
  uint8_t level_idc
)
{
  /* Min compression ratio - MinCR */
  /* Rec. ITU-T H.264 - Table A-1 */
  /*
    TODO: Add Blu-ray constrains for movie stream and still picture
    (https://forum.doom9.org/showthread.php?p=1369280#post1369280)
  */

  if (31 <= level_idc && level_idc <= 40)
    return 4;
  return 2;
}

unsigned getH264MaxMvsPer2Mb(
  uint8_t level_idc
)
{
  /* Max number of motion vectors per two consecutive MBs - MaxMvsPer2Mb */
  /* Rec. ITU-T H.264 - Table A-1 */

  if (level_idc == 30)
    return 32;
  if (31 <= level_idc && level_idc <= 62)
    return 16;

  return 0; /* Unknown/Unrestricted level. */
}

unsigned getH264SliceRate(
  uint8_t level_idc
)
{
  /* SliceRate */
  /* Rec. ITU-T H.264 - Table A-4 */

  if (level_idc == 30)
    return 22;
  if (31 <= level_idc && level_idc <= 40)
    return 60;
  if (41 <= level_idc)
    return 24;

  return 0; /* Unknown/Unsupported level. */
}

unsigned getH264MinLumaBiPredSize(
  uint8_t level_idc
)
{
  /* MinLumaBiPredSize */
  /* Rec. ITU-T H.264 - Table A-4 */

  if (31 <= level_idc)
    return 8*8;

  return 0; /* Unknown/Unrestricted level. */
}

bool getH264direct_8x8_inference_flag(
  uint8_t level_idc
)
{
  /* direct_8x8_inference_flag */
  /* Rec. ITU-T H.264 - Table A-4 */

  if (30 <= level_idc)
    return true;

  return false; /* Unknown/Unrestricted level. */
}

bool getH264frame_mbs_only_flag(
  uint8_t level_idc
)
{
  /* frame_mbs_only_flag */
  /* Rec. ITU-T H.264 - Table A-4 */

  if (level_idc <= 20 || 42 <= level_idc)
    return true;

  return false; /* Unknown/Unrestricted level. */
}

unsigned getH264MaxSubMbRectSize(
  uint8_t level_idc
)
{
  /* MaxSubMbRectSize */
  /* Rec. ITU-T H.264 - Table A-4 */

  if (level_idc <= 30)
    return 576;

  return 0; /* Unknown/Unrestricted level. */
}

void updateH264LevelLimits(
  H264ParametersHandlerPtr param,
  uint8_t level_idc
)
{
  assert(NULL != param);

  param->constraints.MaxMBPS = getH264MaxMBPS(level_idc);
  param->constraints.MaxFS = getH264MaxFS(level_idc);
  param->constraints.MaxDpbMbs = getH264MaxDpbMbs(level_idc);
  param->constraints.MaxBR = getH264MaxBR(level_idc);
  param->constraints.MaxCPB = getH264MaxCPB(level_idc);
  param->constraints.MaxVmvR = getH264MaxVmvR(level_idc);
  param->constraints.MinCR = getH264MinCR(level_idc);
  param->constraints.MaxMvsPer2Mb = getH264MaxMvsPer2Mb(level_idc);

  param->constraints.SliceRate = getH264SliceRate(level_idc);
  param->constraints.MinLumaBiPredSize = getH264MinLumaBiPredSize(level_idc);
  param->constraints.direct_8x8_inference_flag = getH264direct_8x8_inference_flag(level_idc);
  param->constraints.frame_mbs_only_flag = getH264frame_mbs_only_flag(level_idc);
  param->constraints.MaxSubMbRectSize = getH264MaxSubMbRectSize(level_idc);
}

unsigned getH264cpbBrVclFactor(
  H264ProfileIdcValue profile_idc
)
{
  /* cpbBrVclFactor */
  /* Rec. ITU-T H.264 - Table A-2 */
  /* And A.3.1 */

  switch (profile_idc) {
    case H264_PROFILE_BASELINE:
    case H264_PROFILE_MAIN:
    case H264_PROFILE_EXTENDED:
      return 1000;

    case H264_PROFILE_HIGH:
      return 1250;

    case H264_PROFILE_HIGH_10:
      return 3000;

    case H264_PROFILE_HIGH_422:
    case H264_PROFILE_HIGH_444_PREDICTIVE:
    case H264_PROFILE_CAVLC_444_INTRA:
      return 4000;

    default:
      break;
  }

  return 0; /* Unconcerned profile. */
}

unsigned getH264cpbBrNalFactor(
  H264ProfileIdcValue profile_idc
)
{
  /* cpbBrNalFactor */
  /* Rec. ITU-T H.264 - Table A-2 */
  /* And A.3.1 */

  switch (profile_idc) {
    case H264_PROFILE_BASELINE:
    case H264_PROFILE_MAIN:
    case H264_PROFILE_EXTENDED:
      return 1200;

    case H264_PROFILE_HIGH:
      return 1500;

    case H264_PROFILE_HIGH_10:
      return 3600;

    case H264_PROFILE_HIGH_422:
    case H264_PROFILE_HIGH_444_PREDICTIVE:
    case H264_PROFILE_CAVLC_444_INTRA:
      return 4800;

    default:
      break;
  }

  return 0; /* Unconcerned profile. */
}

void updateH264ProfileLimits(
  H264ParametersHandlerPtr param,
  H264ProfileIdcValue profile_idc,
  H264ContraintFlags constraintsFlags,
  bool applyBdavConstraints
)
{
  H264ConstraintsParam * constraintsParam;

  assert(NULL != param);

  constraintsParam = &param->constraints;

  constraintsParam->cpbBrVclFactor = getH264cpbBrVclFactor(profile_idc);
  constraintsParam->cpbBrNalFactor = getH264cpbBrNalFactor(profile_idc);

  switch (profile_idc) {
    case H264_PROFILE_BASELINE:
      /* A.2.1 Baseline profile constraints */
      constraintsParam->allowedSliceTypes = H264_RESTRICTED_IP_SLICE_TYPES;
      constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
      constraintsParam->restrictedFrameMbsOnlyFlag = true;
      constraintsParam->forbiddenWeightedPredModesUse = true;
      constraintsParam->restrictedEntropyCodingMode =
        H264_ENTROPY_CODING_MODE_CAVLC_ONLY;
      constraintsParam->maxAllowedNumSliceGroups = 8;
      constraintsParam->forbiddenPPSExtensionParameters = true;
      constraintsParam->maxAllowedLevelPrefix = 15;
      constraintsParam->restrictedPcmSampleValues = true;

      if (constraintsFlags.set1) {
        /* A.2.1.1 Constrained Baseline profile constraints */
        constraintsParam->forbiddenArbitrarySliceOrder = true;
       constraintsParam->maxAllowedNumSliceGroups = 1;
        constraintsParam->forbiddenRedundantPictures = true;
      }
      break;

    case H264_PROFILE_MAIN:
      /* A.2.2 Main profile constraints */
      constraintsParam->allowedSliceTypes = H264_RESTRICTED_IPB_SLICE_TYPES;
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
      constraintsParam->restrictedEntropyCodingMode =
        H264_ENTROPY_CODING_MODE_CAVLC_ONLY;
      constraintsParam->maxAllowedNumSliceGroups = 8;
      constraintsParam->forbiddenPPSExtensionParameters = true;
      constraintsParam->maxAllowedLevelPrefix = 15;
      constraintsParam->restrictedPcmSampleValues = true;
      break;

    case H264_PROFILE_HIGH:
      /* A.2.4 High profile constraints */
      constraintsParam->allowedSliceTypes = H264_RESTRICTED_IPB_SLICE_TYPES;
      constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
      constraintsParam->forbiddenArbitrarySliceOrder = true;
      constraintsParam->maxAllowedNumSliceGroups = 1;
      constraintsParam->forbiddenRedundantPictures = true;
      constraintsParam->restrictedChromaFormatIdc =
        (applyBdavConstraints) ?
          H264_420_CHROMA_FORMAT_IDC
          : H264_MONO_420_CHROMA_FORMAT_IDC
      ;
      constraintsParam->maxAllowedBitDepthMinus8 = 0;
      constraintsParam->forbiddenQpprimeYZeroTransformBypass = true;

      if (constraintsFlags.set4) {
        /* A.2.4.1 Progressive High profile constraints */
        constraintsParam->restrictedFrameMbsOnlyFlag = true;

        if (constraintsFlags.set5) {
          /* A.2.4.2 Constrained High profile constraints */
          constraintsParam->allowedSliceTypes = H264_RESTRICTED_IP_SLICE_TYPES;
        }
      }
      break;

    case H264_PROFILE_HIGH_10:
      /* A.2.5 High 10 profile constraints */
      constraintsParam->allowedSliceTypes = H264_RESTRICTED_IPB_SLICE_TYPES;
      constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
      constraintsParam->forbiddenArbitrarySliceOrder = true;
      constraintsParam->maxAllowedNumSliceGroups = 1;
      constraintsParam->forbiddenRedundantPictures = true;
      constraintsParam->restrictedChromaFormatIdc =
        H264_MONO_420_CHROMA_FORMAT_IDC;
      constraintsParam->maxAllowedBitDepthMinus8 = 0;
      constraintsParam->forbiddenQpprimeYZeroTransformBypass = true;

      if (constraintsFlags.set4) {
        /* A.2.5.1 Progressive High 10 profile constraints */
        constraintsParam->restrictedFrameMbsOnlyFlag = true;
      }
      if (constraintsFlags.set3) {
        /* A.2.8 High 10 Intra profile constraints */
        constraintsParam->idrPicturesOnly = true;
      }
      break;

    case H264_PROFILE_HIGH_422:
      /* A.2.6 High 4:2:2 profile constraints */
      constraintsParam->allowedSliceTypes = H264_RESTRICTED_IPB_SLICE_TYPES;
      constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
      constraintsParam->forbiddenArbitrarySliceOrder = true;
      constraintsParam->maxAllowedNumSliceGroups = 1;
      constraintsParam->forbiddenRedundantPictures = true;
      constraintsParam->restrictedChromaFormatIdc =
        H264_MONO_TO_422_CHROMA_FORMAT_IDC;
      constraintsParam->maxAllowedBitDepthMinus8 = 2;
      constraintsParam->forbiddenQpprimeYZeroTransformBypass = true;

      if (constraintsFlags.set3) {
        /* A.2.9 High 10 Intra profile constraints */
        constraintsParam->idrPicturesOnly = true;
      }
      break;

    case H264_PROFILE_HIGH_444_PREDICTIVE:
    case H264_PROFILE_CAVLC_444_INTRA:
      /* A.2.7 High 4:4:4 Predictive profile constraints */
      constraintsParam->allowedSliceTypes = H264_RESTRICTED_IPB_SLICE_TYPES;
      constraintsParam->forbiddenSliceDataPartitionLayersNal = true;
      constraintsParam->forbiddenArbitrarySliceOrder = true;
      constraintsParam->maxAllowedNumSliceGroups = 1;
      constraintsParam->forbiddenRedundantPictures = true;
      constraintsParam->maxAllowedBitDepthMinus8 = 6;

      if (
        constraintsFlags.set3
        || profile_idc == H264_PROFILE_CAVLC_444_INTRA
      ) {
        /* A.2.10 High 4:4:4 Intra profile constraints */
        constraintsParam->idrPicturesOnly = true;

        if (profile_idc == H264_PROFILE_CAVLC_444_INTRA) {
          /* A.2.11 CAVLC 4:4:4 Intra profile constraints */
          constraintsParam->restrictedEntropyCodingMode =
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

unsigned getH264BrNal(
  H264ConstraintsParam constraints,
  H264ProfileIdcValue profile_idc
)
{
  switch (profile_idc) {
    case H264_PROFILE_BASELINE:
    case H264_PROFILE_MAIN:
    case H264_PROFILE_EXTENDED:
      return 1200 * constraints.MaxBR;

    default:
      if (!constraints.cpbBrNalFactor)
        LIBBLU_ERROR(
          "Warning, no defined cpbBrNalFactor for profile_idc %u.\n",
          profile_idc
        );
  }

  return constraints.cpbBrNalFactor * constraints.MaxBR;
}

/* ### Blu-ray specifications : ############################################ */

H264BdavExpectedAspectRatioRet getH264BdavExpectedAspectRatioIdc(
  const unsigned frameWidth,
  const unsigned frameHeight
)
{
  switch (frameWidth) {
    case 1920:
    case 1280:
      return NEW_H264_BDAV_EXPECTED_ASPECT_RATIO_RET(
        H264_ASPECT_RATIO_IDC_1_BY_1,
        H264_ASPECT_RATIO_IDC_1_BY_1
      );

    case 1440:
      return NEW_H264_BDAV_EXPECTED_ASPECT_RATIO_RET(
        H264_ASPECT_RATIO_IDC_4_BY_3,
        H264_ASPECT_RATIO_IDC_4_BY_3
      );

    case 720:
      if (frameHeight == 576) {
        return NEW_H264_BDAV_EXPECTED_ASPECT_RATIO_RET(
          H264_ASPECT_RATIO_IDC_12_BY_11,
          H264_ASPECT_RATIO_IDC_16_BY_11
        );
      }

      return NEW_H264_BDAV_EXPECTED_ASPECT_RATIO_RET(
        H264_ASPECT_RATIO_IDC_10_BY_11,
        H264_ASPECT_RATIO_IDC_40_BY_33
      );
  }

  return NEW_H264_BDAV_EXPECTED_ASPECT_RATIO_RET(
    H264_ASPECT_RATIO_IDC_12_BY_11,
    H264_ASPECT_RATIO_IDC_16_BY_11
  );
}

H264VideoFormatValue getH264BdavExpectedVideoFormat(
  const double frameRate
)
{
  if (frameRate == 25 || frameRate == 50)
    return H264_VIDEO_FORMAT_PAL;
  return H264_VIDEO_FORMAT_NTSC;
}

H264ColourPrimariesValue getH264BdavExpectedColorPrimaries(
  const unsigned frameHeight
)
{
  switch (frameHeight) {
    case 576:
      return H264_COLOR_PRIM_BT470BG;
    case 480:
      return H264_COLOR_PRIM_SMPTE170M;
  }

  return H264_COLOR_PRIM_BT709;
}

H264TransferCharacteristicsValue getH264BdavExpectedTransferCharacteritics(
  const unsigned frameHeight
)
{
  switch (frameHeight) {
    case 576:
      return H264_TRANS_CHAR_BT470BG;
    case 480:
      return H264_TRANS_CHAR_SMPTE170M;
  }

  return H264_TRANS_CHAR_BT709;
}

H264MatrixCoefficientsValue getH264BdavExpectedMatrixCoefficients(
  const unsigned frameHeight
)
{
  switch (frameHeight) {
    case 576:
      return H264_MATRX_COEF_BT470M;
    case 480:
      return H264_MATRX_COEF_SMPTE170M;
  }

  return H264_MATRX_COEF_BT709;
}