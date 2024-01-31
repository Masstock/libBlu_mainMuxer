/** \~english
 * \file util.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxer common utilities main header.
 */

#ifndef __LIBBLU_MUXER__UTIL_H__
#define __LIBBLU_MUXER__UTIL_H__

#include "util/bitReader.h"
#include "util/bitWriter.h"
#include "util/bitStreamHandling.h"
#include "util/circularBuffer.h"
#include "util/common.h"
#include "util/crcLookupTables.h"
#include "util/errorCodes.h"
#include "util/errorCodesVa.h"
#include "util/hashTables.h"
#include "util/libraries.h"
#include "util/macros.h"
#include "util/textFilesHandling.h"

#define PROG_INFOS  "libBlu Muxer Exp. 0.5"
#define PROG_CONF_FILENAME  lbc_str("settings.ini")

#define MAX_NB_STREAMS  32

#define PAT_DELAY  (90 * 27000ull)
#define PMT_DELAY  (90 * 27000ull)
#define SIT_DELAY  (900 * 27000ull)
#define PCR_DELAY  (70  * 27000ull)

/* UI Parameters :                                                           */
#define PROGRESSION_BAR_LATENCY 200

/** \~english
 * \brief MPEG-TS main clock frequency (27MHz).
 *
 * Number of clock ticks per second.
 */
#define MAIN_CLOCK_27MHZ  27000000ull

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to hours number.
 *
 * MAIN_CLOCK_HOURS(MAIN_CLOCK_27MHZ * 60 * 60) == 1.
 */
#define MAIN_CLOCK_HOURS(clk)                                                 \
  ((clk) / (MAIN_CLOCK_27MHZ * 3600ull) % 60ull)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to minutes number.
 *
 * MAIN_CLOCK_MINUTES(MAIN_CLOCK_27MHZ * 60) == 1.
 */
#define MAIN_CLOCK_MINUTES(clk)                                               \
  ((clk) / (MAIN_CLOCK_27MHZ * 60) % 60)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to seconds number.
 *
 * MAIN_CLOCK_SECONDS(MAIN_CLOCK_27MHZ) == 1.
 */
#define MAIN_CLOCK_SECONDS(clk)                                               \
  ((clk) / MAIN_CLOCK_27MHZ % 60)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to milliseconds number.
 *
 * MAIN_CLOCK_MILISECONDS(MAIN_CLOCK_27MHZ / 10) == 100.
 */
#define MAIN_CLOCK_MILISECONDS(clk)                                           \
  ((clk) / (MAIN_CLOCK_27MHZ / 1000) % 1000)

/** \~english
 * \brief MPEG-TS sub clock frequency (90kHz).
 */
#define SUB_CLOCK_90KHZ  (MAIN_CLOCK_27MHZ / 300)

/** \~english
 * \brief Rounds supplied 27MHz clock value to the highest 90kHz sub-clock
 * common tick.
 */
#define ROUND_90KHZ_CLOCK(x)  ((x) / 300 * 300)

/** \~english
 * \brief MPEG-TS packet header synchronization byte.
 */
#define TP_SYNC_BYTE  0x47

/** \~english
 * \brief MPEG-TS packet length in bytes.
 */
#define TP_SIZE  188

/** \~english
 * \brief MPEG-TS packet TS header length in bytes.
 */
#define TP_HEADER_SIZE  4

#define TP_PAYLOAD_SIZE  (TP_SIZE - TP_HEADER_SIZE)

/** \~english
 * \brief BDAV TP_extra_header length in bytes.
 */
#define TP_EXTRA_HEADER_SIZE  4

/* in b/s : */
#define BDAV_VIDEO_MAX_BITRATE  40000000
#define BDAV_VIDEO_MAX_BITRATE_SEC_V  7600000
#define BDAV_VIDEO_MAX_BITRATE_DVD_OUT  15000000
#define BDAV_UHD_VIDEO_MAX_BITRATE  100000000

/* In bytes : */
#define BDAV_VIDEO_MAX_BUFFER_SIZE  240000000
#define BDAV_VIDEO_MAX_BUFFER_SIZE_DVD_OUT  120000000

typedef enum {
  TYPE_ES,

  TYPE_PAT,
  TYPE_PMT,
  TYPE_SIT,
  TYPE_PCR,
  TYPE_NULL,

  TYPE_ERROR = 0xFF
} StreamType;

const lbc * streamTypeStr(
  StreamType typ
);

static inline int isEsStreamType(
  StreamType typ
)
{
  return typ == TYPE_ES;
}

void printFileParsingProgressionBar(
  BitstreamReaderPtr bitStream
);

/** \~english
 * \brief str_time() clock representation formatting mode.
 *
 * Given maximum sizes in char include terminating null-character.
 */
typedef enum {
  STRT_H_M_S_MS  /**< HH:MM:SS.mmm format, use at most 13 charcters.         */
} str_time_format;

#define STRT_H_M_S_MS_LEN  13

/** \~english
 * \brief Copies into buf a string representation of the time from given
 * clock timestamp in #MAIN_CLOCK_27MHZ ticks.
 *
 * \param buf Destination pointer to the destination array where the
 * resulting C string is copied.
 * \param buf_size Maximum number of characters to be copied to buf,
 * including the terminating null-character.
 * \param format_mode Result string time representation format mode.
 * \param clock_value Clock value to be represented, exprimed in
 * #MAIN_CLOCK_27MHZ ticks.
 * \return int Return the number of characters written (excluding the null-
 * character).
 *
 * The output string format (and maximum size) is defined by given format
 * argument.
 *
 * \note If given maxsize value is shorter than the specified #str_time_format
 * one for the given mode, the output is truncated (and may not countain
 * terminating null-character)
 */
int str_time(
  lbc * buf,
  size_t buf_size,
  str_time_format format_mode,
  uint64_t clock_value
);

#endif