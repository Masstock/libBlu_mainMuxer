#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "elementaryStreamProperties.h"

int initLibbluESFmtSpecProp(
  LibbluESFmtProp * dst,
  LibbluESFmtSpecPropType type
)
{
  LibbluESFmtProp prop = {0};

  assert(NULL != dst);

  if (type != FMT_SPEC_INFOS_NONE) {
    size_t propSize;

    if (0 == (propSize = sizeofLibbluESFmtSpecPropType(type)))
      LIBBLU_ERROR_RETURN("Unknown ES Format properties type %u.\n", type);

    if (NULL == (prop.shared_ptr = malloc(propSize)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  *dst = prop;
  return 0;
}

HdmvVideoFormat getHdmvVideoFormat(
  unsigned width,
  unsigned height,
  bool isInterlaced
)
{
  unsigned i;

  static const struct {
    HdmvVideoFormat fmt;
    unsigned width;
    unsigned height;
    bool interlaced;
  } configs[] = {
    { HDMV_VIDEO_FORMAT_480I,  720,  480, 1},
    { HDMV_VIDEO_FORMAT_576I,  720,  576, 1},
    { HDMV_VIDEO_FORMAT_480P,  720,  480, 0},
    {HDMV_VIDEO_FORMAT_1080I, 1440, 1080, 1},
    {HDMV_VIDEO_FORMAT_1080I, 1920, 1080, 1},
    { HDMV_VIDEO_FORMAT_720P, 1280,  720, 0},
    {HDMV_VIDEO_FORMAT_1080P, 1440, 1080, 0},
    {HDMV_VIDEO_FORMAT_1080P, 1920, 1080, 0},
    { HDMV_VIDEO_FORMAT_576P,  720,  576, 1},
    {HDMV_VIDEO_FORMAT_2160P, 3840, 2160, 0}
  };

  for (i = 0; i < ARRAY_SIZE(configs); i++) {
    if (
      width == configs[i].width
      && height == configs[i].height
      && isInterlaced == configs[i].interlaced
    ) {
      return configs[i].fmt;
    }
  }

  return 0;
}

HdmvFrameRateCode getHdmvFrameRateCode(
  float frameRate
)
{
  if (FLOAT_COMPARE(frameRate, 24000.0f / 1001.0f))
    return FRAME_RATE_CODE_23976;
  if (FLOAT_COMPARE(frameRate, 24.0f))
    return FRAME_RATE_CODE_24;
  if (FLOAT_COMPARE(frameRate, 25.0f))
    return FRAME_RATE_CODE_25;
  if (FLOAT_COMPARE(frameRate, 30000.0f / 1001.0f))
    return FRAME_RATE_CODE_29970;
  if (FLOAT_COMPARE(frameRate, 50.0f))
    return FRAME_RATE_CODE_50;
  if (FLOAT_COMPARE(frameRate, 50000.0f / 1001.0f))
    return FRAME_RATE_CODE_59940;

  return FRAME_RATE_CODE_UNSPC;
}

double frameRateCodeToDouble(
  HdmvFrameRateCode code
)
{
  double val;

  static const double frameRates[] = {
    -1.0, /* < Reserved */
    24000.0 / 1001.0,
    24.0,
    25.0,
    30000.0 / 1001.0,
    -1.0, /* < Reserved */
    50.0,
    50000.0 / 1001.0
  };

  val = -1.0;
  if (code < ARRAY_SIZE(frameRates))
    val = frameRates[code];
  return val;
}

#define DISPLAY_MODE_PROGRESSIVE  0x01
#define DISPLAY_MODE_INTERLACED  0x02
#define DISPLAY_MODE_UNRESTRICTED  0x03

static CheckVideoConfigurationRet _checkVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frameRate,
  bool interlaced,

  unsigned nbConfigs,
  const unsigned screenSizeConfigs[][2],
  const unsigned allowedFrameRates[][4],
  const unsigned interlacingMode[][4]
)
{
  unsigned i;

  unsigned configId, frId, displayMode;

  for (i = 0; i < nbConfigs; i++) {
    if (CHECK_ARRAY_2VALS(screenSizeConfigs[i], width, height))
      break;
  }

  if (nbConfigs <= i)
    return CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE;
  configId = i;

  for (i = 0; i < 4; i++) {
    if (allowedFrameRates[configId][i] == frameRate)
      break;
  }

  if (4 <= i)
    return CHK_VIDEO_CONF_RET_ILL_FRAME_RATE;
  frId = i;

  displayMode = 1 << (interlaced);
  /**
   * NOTE: According to ISO/IEC 9899:1999, bool type value is 0 or 1.
   */

  if (0 == (interlacingMode[configId][frId] & displayMode))
    return CHK_VIDEO_CONF_RET_ILL_DISP_MODE;

  return CHK_VIDEO_CONF_RET_OK;
}

CheckVideoConfigurationRet checkPrimVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frameRate,
  bool interlaced
)
{
  static const unsigned screenSizeConfigs[][2] = {
    {1920, 1080},
    {1440, 1080},
    {1280, 720},
    {720, 576},
    {720, 480}
  };

  static const HdmvFrameRateCode allowedFrameRates[][4] = {
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_25,
      FRAME_RATE_CODE_29970
    },
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_25,
      FRAME_RATE_CODE_29970
    },
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_50,
      FRAME_RATE_CODE_59940
    },
    {
      FRAME_RATE_CODE_25
    },
    {
      FRAME_RATE_CODE_29970
    }
  };

  static const unsigned allowedDisplayModes[][4] = {
    {
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_INTERLACED,
      DISPLAY_MODE_INTERLACED
    },
    {
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_INTERLACED,
      DISPLAY_MODE_INTERLACED
    },
    {
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE
    },
    {
      DISPLAY_MODE_INTERLACED
    },
    {
      DISPLAY_MODE_INTERLACED
    }
  };

  assert(ARRAY_SIZE(allowedFrameRates) == ARRAY_SIZE(screenSizeConfigs));
  assert(ARRAY_SIZE(allowedDisplayModes) == ARRAY_SIZE(screenSizeConfigs));

  return _checkVideoConfiguration(
    width,
    height,
    frameRate,
    interlaced,

    ARRAY_SIZE(screenSizeConfigs),
    screenSizeConfigs,
    allowedFrameRates,
    allowedDisplayModes
  );
}

CheckVideoConfigurationRet checkSecVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frameRate,
  bool interlaced
)
{
  static const unsigned screenSizeConfigs[][2] = {
    {1920, 1080},
    {1440, 1080},
    {1280, 720},
    {720, 576},
    {720, 480}
  };

  static const HdmvFrameRateCode allowedFrameRates[][4] = {
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_25,
      FRAME_RATE_CODE_29970
    },
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_25,
      FRAME_RATE_CODE_29970
    },
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_50,
      FRAME_RATE_CODE_59940
    },
    {
      FRAME_RATE_CODE_25
    },
    {
      FRAME_RATE_CODE_23976,
      FRAME_RATE_CODE_24,
      FRAME_RATE_CODE_29970
    }
  };

  static const unsigned allowedDisplayModes[][4] = {
    {
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_INTERLACED,
      DISPLAY_MODE_INTERLACED
    },
    {
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_INTERLACED,
      DISPLAY_MODE_INTERLACED
    },
    {
      DISPLAY_MODE_PROGRESSIVE
    },
    {
      DISPLAY_MODE_UNRESTRICTED
    },
    {
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_PROGRESSIVE,
      DISPLAY_MODE_UNRESTRICTED
    }
  };

  assert(ARRAY_SIZE(allowedFrameRates) == ARRAY_SIZE(screenSizeConfigs));
  assert(ARRAY_SIZE(allowedDisplayModes) == ARRAY_SIZE(screenSizeConfigs));

  return _checkVideoConfiguration(
    width,
    height,
    frameRate,
    interlaced,

    ARRAY_SIZE(screenSizeConfigs),
    screenSizeConfigs,
    allowedFrameRates,
    allowedDisplayModes
  );
}