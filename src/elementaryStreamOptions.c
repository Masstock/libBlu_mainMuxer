#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "elementaryStreamOptions.h"

uint64_t computeFlagsLibbluESSettingsOptions(
  LibbluESSettingsOptions options
)
{
  return
    (options.secondaryStream          << FLAG_SHFT_SEC_STREAM)
    | (options.extractCore            << FLAG_SHFT_EXTRACT_CORE)
    | (options.dvdMedia               << FLAG_SHFT_DVD_OUTPUT)
    | (options.disableFixes           << FLAG_SHFT_DISABLE_FIXES)
    | (options.forceRebuildSei        << FLAG_SHFT_FORCE_REBUILD_SEI)
    | ((options.fpsChange != 0x00)    << FLAG_SHFT_CHANGE_FPS)
    | (isUsedLibbluAspectRatioMod(options.arChange) << FLAG_SHFT_CHANGE_AR)
    | ((options.levelChange != 0x00)  << FLAG_SHFT_CHANGE_LEVEL)
    | (options.discardSei             << FLAG_SHFT_REMOVE_SEI)
  ;
}

int parseFpsChange(
  const lbc * expr,
  HdmvFrameRateCode * val
)
{
  float valueFloat;
  HdmvFrameRateCode value;

  if (lbc_sscanf(expr, " %f", &valueFloat) < 1)
    return -1;
  if (0x0 == (value = getHdmvFrameRateCode(valueFloat)))
    return -1;

  *val = value;
  return 0;
}

int parseArChange(
  const lbc * expr,
  LibbluAspectRatioMod * val
)
{
  unsigned x, y;

  H264AspectRatioIdcValue idc;

  if (lbc_sscanf(expr, " %u:%u", &x, &y) < 2)
    return -1;
  if (!x ^ !y)
    return -1;

  idc = H264_ASPECT_RATIO_IDC_EXTENDED_SAR;
  switch (x * y) {
    case 0:
      x = y = (unsigned) -1;
      idc = H264_ASPECT_RATIO_IDC_UNSPECIFIED;
      break;

    case 1*1:
      idc = H264_ASPECT_RATIO_IDC_1_BY_1;
      break;

    case 12*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_12_BY_11;
      break;

    case 10*11:
      if (x < y)
        idc = H264_ASPECT_RATIO_IDC_10_BY_11;
      break;

    case 16*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_16_BY_11;
      break;

    case 40*33:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_40_BY_33;
      break;

    case 24*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_24_BY_11;
      break;

    case 20*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_20_BY_11;
      break;

    case 32*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_32_BY_11;
      break;

    case 80*33:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_80_BY_33;
      break;

    case 18*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_18_BY_11;
      break;

    case 15*11:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_15_BY_11;
      break;

    case 64*33:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_64_BY_33;
      break;

    case 160*99:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_160_BY_99;
      break;

    case 4*3:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_4_BY_3;
      break;

    case 3*2:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_3_BY_2;
      break;

    case 2*1:
      if (x > y)
        idc = H264_ASPECT_RATIO_IDC_2_BY_1;
  }

  *val = (LibbluAspectRatioMod) {
    .idc = idc,
    .x = x,
    .y = y
  };
  return 0;
}

int parseLevelChange(
  const lbc * expr,
  uint8_t * val
)
{
  unsigned prefix, suffix;

  switch (lbc_sscanf(expr, " %u.%u", &prefix, &suffix)) {
    case 1: /* No separator, xx format */
      if (prefix < 10) /* Only one character for one-digit levels (1 => 1.0) */
        prefix *= 10;

      /* Split value to prefix.suffix */
      suffix = prefix % 10;
      prefix = prefix / 10;
      break;

    case 2: /* x.x format */
      break;

    default: /* Wrong format */
      return -1;
  }

  if (6 < prefix || (((prefix == 1) ? 3 : 2) < suffix))
    return -1; /* Error, unknown profile level */

  *val = prefix * 10 + suffix;
  return 0;
}