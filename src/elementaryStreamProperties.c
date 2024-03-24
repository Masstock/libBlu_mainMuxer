#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "elementaryStreamProperties.h"

int initLibbluESFmtSpecProp(
  LibbluESFmtProp *dst,
  LibbluESFmtSpecPropType type
)
{
  LibbluESFmtProp prop = {0};

  assert(NULL != dst);

  if (type != FMT_SPEC_INFOS_NONE) {
    size_t prop_size = sizeofLibbluESFmtSpecPropType(type);
    if (0 == prop_size)
      LIBBLU_ERROR_RETURN("Unknown ES Format properties type %u.\n", type);

    if (NULL == (prop.shared_ptr = malloc(prop_size)))
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
  float frame_rate
)
{
  if (FLOAT_COMPARE(frame_rate, 24000.0f / 1001.0f))
    return FRAME_RATE_CODE_23976;
  if (FLOAT_COMPARE(frame_rate, 24.0f))
    return FRAME_RATE_CODE_24;
  if (FLOAT_COMPARE(frame_rate, 25.0f))
    return FRAME_RATE_CODE_25;
  if (FLOAT_COMPARE(frame_rate, 30000.0f / 1001.0f))
    return FRAME_RATE_CODE_29970;
  if (FLOAT_COMPARE(frame_rate, 50.0f))
    return FRAME_RATE_CODE_50;
  if (FLOAT_COMPARE(frame_rate, 50000.0f / 1001.0f))
    return FRAME_RATE_CODE_59940;

  return FRAME_RATE_CODE_UNSPC;
}

#define DISPLAY_MODE_PROGRESSIVE  0x01
#define DISPLAY_MODE_INTERLACED  0x02
#define DISPLAY_MODE_UNRESTRICTED  0x03

static CheckVideoConfigurationRet _checkVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frame_rate,
  bool interlaced,

  unsigned nb_configs,
  const unsigned screen_size_configs[][2],
  const unsigned allowed_frame_rates[][4],
  const unsigned interlace_modes[][4]
)
{
  unsigned i = 0;
  for (; i < nb_configs; i++) {
    if (CHECK_ARRAY_2VALS(screen_size_configs[i], width, height))
      break;
  }

  if (nb_configs <= i)
    return CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE;
  unsigned screen_size_config_idx = i;

  for (i = 0; i < 4; i++) {
    if (allowed_frame_rates[screen_size_config_idx][i] == frame_rate)
      break;
  }

  if (4 <= i)
    return CHK_VIDEO_CONF_RET_ILL_FRAME_RATE;
  unsigned frame_rate_idx = i;

  unsigned interlace_mode_idx = 1u << (interlaced);

  if (0 == (interlace_modes[screen_size_config_idx][frame_rate_idx] & interlace_mode_idx))
    return CHK_VIDEO_CONF_RET_ILL_DISP_MODE;

  return CHK_VIDEO_CONF_RET_OK;
}

CheckVideoConfigurationRet checkPrimVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frame_rate,
  bool interlaced
)
{
  static const unsigned screen_size_configs[][2] = {
    {1920, 1080},
    {1440, 1080},
    {1280, 720},
    {720, 576},
    {720, 480}
  };

  static const HdmvFrameRateCode allowed_frame_rates[][4] = {
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

  assert(ARRAY_SIZE(allowed_frame_rates) == ARRAY_SIZE(screen_size_configs));
  assert(ARRAY_SIZE(allowedDisplayModes) == ARRAY_SIZE(screen_size_configs));

  return _checkVideoConfiguration(
    width,
    height,
    frame_rate,
    interlaced,

    ARRAY_SIZE(screen_size_configs),
    screen_size_configs,
    allowed_frame_rates,
    allowedDisplayModes
  );
}

CheckVideoConfigurationRet checkSecVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frame_rate,
  bool interlaced
)
{
  static const unsigned screen_size_configs[][2] = {
    {1920, 1080},
    {1440, 1080},
    {1280, 720},
    {720, 576},
    {720, 480}
  };

  static const HdmvFrameRateCode allowed_frame_rates[][4] = {
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

  assert(ARRAY_SIZE(allowed_frame_rates) == ARRAY_SIZE(screen_size_configs));
  assert(ARRAY_SIZE(allowedDisplayModes) == ARRAY_SIZE(screen_size_configs));

  return _checkVideoConfiguration(
    width,
    height,
    frame_rate,
    interlaced,

    ARRAY_SIZE(screen_size_configs),
    screen_size_configs,
    allowed_frame_rates,
    allowedDisplayModes
  );
}