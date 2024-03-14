#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "util.h"

#if USE_LOW_LEVEL_FILE_HANDLING
#  define _POSIX_SOURCE
#  define _LARGEFILE64_SOURCE
#  define _FILE_OFFSET_BITS 64
#  include <sys/types.h>
#  include <unistd.h> /* For low-level IO */
#  include <fcntl.h>
#endif

const lbc * streamTypeStr(
  StreamType typ
)
{
  switch (typ) {
  case TYPE_ES:
    return lbc_str("Elementary Stream");
  case TYPE_PAT:
    return lbc_str("PAT");
  case TYPE_PMT:
    return lbc_str("PMT");
  case TYPE_SIT:
    return lbc_str("SIT");
  case TYPE_PCR:
    return lbc_str("PCR");
  case TYPE_NULL:
    return lbc_str("NULL");
  default:
    break;
  }

  return lbc_str("Error type");
}

#if 0
uint8_t getHdmvVideoFormat(int screenHorizontalDefinition, int screenVerticalDefinition, bool isInterlaced)
{
  if (
    (screenHorizontalDefinition == 1920 || screenHorizontalDefinition == 1440) &&
    screenVerticalDefinition == 1080
  )
    return (isInterlaced) ? 0x4 : 0x6; /* HD 1080p/i */

  if (screenHorizontalDefinition == 1280 && screenVerticalDefinition == 720 && !isInterlaced)
    return 0x5; /* HD 720p */

  if (screenHorizontalDefinition == 720 && screenVerticalDefinition == 576)
    return (isInterlaced) ? 0x2 : 0x7; /* SD 576p/i */

  if (screenHorizontalDefinition == 720 && screenVerticalDefinition == 480)
    return (isInterlaced) ? 0x1 : 0x3; /* SD 480p/i */

  if (screenHorizontalDefinition == 3840 && screenVerticalDefinition == 2160 && !isInterlaced)
    return 0x8; /* UHD 2160p */

  return 0x0; /* Unknown */
}
#endif

void printFileParsingProgressionBar(BitstreamReaderPtr bitStream)
{
  unsigned percentage;

  static unsigned oldPercentage = 100;
  static uint64_t refBitStreamId = 0;

  if (isDebugEnabledLibbbluStatus())
    return; /* Don't print progress bar in debug mode for readability. */

  assert(NULL != bitStream);

  if (refBitStreamId != bitStream->identifier)
    refBitStreamId = bitStream->identifier, oldPercentage = 100;

  percentage = (unsigned) MIN(
    ABS(
      100 * tellPos(bitStream) / MAX(1, bitStream->fileSize)
    ),
    100
  );

  if (percentage != oldPercentage) {
    lbc_printf(
      lbc_str("Opening source file... [%.*s%.*s] %3d%%\r"),
      percentage / 5,
      "====================",
      20 - (percentage / 5),
      "                    ",
      percentage
    );
    fflush(stdout);

    oldPercentage = percentage;
  }
}

int str_time(
  lbc *buf,
  size_t buf_size,
  str_time_format format_mode,
  uint64_t clock_value
)
{
  switch (format_mode) {
  case STRT_H_M_S_MS:
    return lbc_snprintf(
      buf, buf_size,
      lbc_str("%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64),
      (uint64_t) MAIN_CLOCK_HOURS      (clock_value),
      (uint64_t) MAIN_CLOCK_MINUTES    (clock_value),
      (uint64_t) MAIN_CLOCK_SECONDS    (clock_value),
      (uint64_t) MAIN_CLOCK_MILISECONDS(clock_value)
    );
  }

  return 0;
}