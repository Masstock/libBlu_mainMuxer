#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "elementaryStreamProperties.h"

#define DISPLAY_MODE_PROGRESSIVE  0x01
#define DISPLAY_MODE_INTERLACED  0x02
#define DISPLAY_MODE_UNRESTRICTED  0x03

static CheckVideoConfigurationRet checkVideoConfiguration(
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

  return checkVideoConfiguration(
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

  return checkVideoConfiguration(
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