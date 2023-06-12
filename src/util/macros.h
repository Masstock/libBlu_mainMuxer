/** \~english
 * \file macros.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Internal parameters and macro functions module.
 */

#ifndef __LIBBLU_MUXER__UTIL__BIT_MACROS_H__
#define __LIBBLU_MUXER__UTIL__BIT_MACROS_H__

/* Linux */
#if defined(__linux__)
#  define ARCH_UNIX
#  define ARCH_LINUX

#  if defined(__x86_64__)
#    define PROG_ARCH "Linux 64 bits"
#  elif defined(__i386__)
#    define PROG_ARCH "Linux 32 bits"
#    define _FILE_OFFSET_BITS 64
#    define _LARGEFILE64_SOURCE
#  else
#    define PROG_ARCH "Linux Unknown"
#  endif
#  define PRI_SIZET "zu"

/* Windows */
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(__CYGWIN__)
#  define ARCH_WIN32

#  if defined(_WIN64) || defined(__CYGWIN__)
#    define PROG_ARCH "Windows 64 bits"
#    define PRI_SIZET PRIu64
#  else
#    define PROG_ARCH "Windows 32 bits"
#    define _FILE_OFFSET_BITS 64
#    define _LARGEFILE64_SOURCE
#    define fopen fopen64
#    define fseek fseeko64
#    define ftell ftello64

#    define PRI_SIZET PRIu64
#    define size_t uint64_t
#  endif

/* Mac-OS */
#elif defined(__APPLE__)
#  define ARCH_UNIX
#  define ARCH_APPLE

#  define define PRI_SIZET "zu"
#  if defined(__x86_64__)
#    define PROG_ARCH "MacOS 64 bits"
#  elif defined(__i386__)
#    define PROG_ARCH "MacOS 32 bits"
#  else
#    define PROG_ARCH "MacOS Unknown"
#  endif

/* Error */
#else
#  error "Unknown architecture"
#endif

#if defined(__GNUC__)
/* Inspired from ffmpeg/linux kernel */
#  define GCC_VERSION_AT_LEAST(x,y) (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#  define GCC_VERSION_AT_MOST(x,y)  (__GNUC__ < (x) || __GNUC__ == (x) && __GNUC_MINOR__ <= (y))

#  if GCC_VERSION_AT_LEAST(2,96)
#    define likely(x)  __builtin_expect((x) != 0, 1)
#    define unlikely(x)  __builtin_expect((x) != 0, 0)
#  else
#    define likely(x)  (x)
#    define unlikely(x)  (x)
#  endif
#else
#  define likely(x)  (x)
#  define unlikely(x)  (x)
#endif

/* Debugging macros: */
#define TOC()  fprintf(stderr, "TOC\n");
#define PING()  fprintf(stderr, "PING\n");
#define PONG()  fprintf(stderr, "PONG\n");

#if defined(ARCH_WIN32)
#  include <wchar.h>
#  define lbc  wchar_t
#  define lbc_int  wint_t

#  define lbc_locale_convto  lb_char_to_wchar
#  define lbc_utf8_convto  lb_iconv_utf8_to_wchar
#  define lbc_convfrom  lb_iconv_wchar_to_utf8

#  define lbc_isblank  iswblank
#  define lbc_iscntrl  iswcntrl
#  define lbc_isspace  iswspace

#  define lbc_sscanf(s, format, ...)                                          \
  swscanf(s, lbc_str(format), ##__VA_ARGS__)
#  define lbc_fgets  fgetws

#  define lbc_putchar  _fputwchar
#  define lbc_puts  _putws
#  define lbc_fprintf(stream, format, ...)                                    \
  fwprintf(stream, lbc_str(format), ##__VA_ARGS__)
#  define lbc_printf(format, ...)                                             \
  wprintf(lbc_str(format), ##__VA_ARGS__)
#  define lbc_snprintf(s, n, format, ...)                                     \
  snwprintf(s, n, lbc_str(format), ##__VA_ARGS__)
#  define lbc_asprintf(s, format, ...)                                        \
  lb_wasprintf(s, lbc_str(format), ##__VA_ARGS__)
#  define lbc_vfprintf  vfwprintf
#  define lbc_deb_printf  wprintf

#  define lbc_strlen  wcslen

#  define lbc_fgets  fgetws
#  define lbc_fopen(f, m)  _wfopen(f, lbc_str(m))
#  define lbc_strcspn  wcscspn
#  define lbc_strdup  lb_wstr_dup
#  define lbc_strndup  lb_wstrn_dup

#  define lbc_atob  lb_watob

#  define lbc_equal  lb_wstr_equal
#  define lbc_equaln  lb_wstrn_equal
#  define lbc_strcmp  wcscmp
#  define lbc_strcpy  wcscpy
#  define lbc_strncat  lb_wstrncat
#  define lbc_strncpy  wcsncpy

#  define lbc_access_fp(f, m)  lb_waccess_fp(f, lbc_str(m))
#  define lbc_chdir  _wchdir
#  define lbc_getcwd  lb_wget_cwd

#  define lbc_fnv1aStrHash wfnv1aStrHash

#  define lbc_cwk_path_get_absolute  cwkw_path_get_absolute
#  define lbc_cwk_path_get_basename  cwkw_path_get_basename
#  define lbc_cwk_path_get_dirname  cwkw_path_get_dirname
#  define lbc_cwk_path_get_extension  cwkw_path_get_extension
#  define lbc_cwk_path_get_relative  cwkw_path_get_relative
#  define lbc_cwk_path_get_style  cwkw_path_get_style
#  define lbc_cwk_path_guess_style  cwkw_path_guess_style
#  define lbc_cwk_path_is_absolute  cwkw_path_is_absolute
#  define lbc_cwk_path_join  cwkw_path_join
#  define lbc_cwk_path_set_style  cwkw_path_set_style
#  define lbc_cwk_path_style  cwkw_path_style

#  define PRI_LBC  "lc"
#  define PRI_LBCS  "ls"
#  define PRI_LBCP  "l"
#  define lbc_str(s)  L"" s
#  define lbc_char(c)  L##c
#else
#  define lbc  char
#  define lbc_int  int

#  define lbc_locale_convto  lb_str_dup
#  define lbc_utf8_convto(str)  lb_str_dup((char *) (str))
#  define lbc_convfrom  lb_str_dup

#  define lbc_isblank  isblank
#  define lbc_iscntrl  iscntrl
#  define lbc_isspace  isspace

#  define lbc_sscanf  sscanf
#  define lbc_fgets  fgets

#  define lbc_asprintf  lb_asprintf
#  define lbc_deb_printf  printf
#  define lbc_fprintf  fprintf
#  define lbc_printf  printf
#  define lbc_putchar  putchar
#  define lbc_puts  puts
#  define lbc_snprintf  snprintf
#  define lbc_vfprintf vfprintf

#  define lbc_strlen  strlen

#  define lbc_fgets  fgets
#  define lbc_fopen  fopen
#  define lbc_strcspn  strcspn
#  define lbc_strdup  lb_str_dup
#  define lbc_strndup  lb_strn_dup

#  define lbc_equal  lb_str_equal
#  define lbc_equaln  lb_strn_equal
#  define lbc_strcmp  strcmp
#  define lbc_strcpy  strcpy
#  define lbc_strncat  lb_strncat
#  define lbc_strncpy  strncpy

#  define lbc_atob  lb_atob

#  define lbc_access_fp  lb_access_fp
#  define lbc_chdir  chdir
#  define lbc_getcwd  lb_get_cwd

#  define lbc_fnv1aStrHash fnv1aStrHash

#  define lbc_cwk_path_get_absolute  cwk_path_get_absolute
#  define lbc_cwk_path_get_basename  cwk_path_get_basename
#  define lbc_cwk_path_get_dirname  cwk_path_get_dirname
#  define lbc_cwk_path_get_extension  cwk_path_get_extension
#  define lbc_cwk_path_get_relative  cwk_path_get_relative
#  define lbc_cwk_path_get_style  cwk_path_get_style
#  define lbc_cwk_path_guess_style  cwk_path_guess_style
#  define lbc_cwk_path_is_absolute  cwk_path_is_absolute
#  define lbc_cwk_path_join  cwk_path_join
#  define lbc_cwk_path_set_style  cwk_path_set_style
#  define lbc_cwk_path_style  cwk_path_style

#  define PRI_LBC  "c"
#  define PRI_LBCS  "s"
#  define PRI_LBCP  ""
#  define lbc_str(s)  s
#  define lbc_char(c)  c
#endif

/** \~english
 * \brief Path buffering size. Maximum supported path including NUL character.
 *
 * \warning The real maximum supported path size may be lower depending on
 * system but this value ensure buffering accuracy (if path exceed system/drive
 * capabilities, an error may properly be prompted at file manipulation such
 * as with fopen()).
 */
#define PATH_BUFSIZE  4096

#define STR_BUFSIZE  8192

/* ### ANSI formatting strings : ########################################### */

/** \~english
 * \brief ANSI Bold formatting format string.
 */
#define ANSI_BOLD  "\x1b[1m"

/** \~english
 * \brief ANSI Red color format string.
 */
#define ANSI_COLOR_RED  "\x1b[91m"

/** \~english
 * \brief ANSI Green color format string.
 */
#define ANSI_COLOR_GREEN  "\x1b[92m"

/** \~english
 * \brief ANSI Yellow color format string.
 */
#define ANSI_COLOR_YELLOW  "\x1b[93m"

/** \~english
 * \brief ANSI Blue color format string.
 */
#define ANSI_COLOR_BLUE  "\x1b[94m"

/** \~english
 * \brief ANSI Magenta color format string.
 */
#define ANSI_COLOR_MAGENTA  "\x1b[95m"

/** \~english
 * \brief ANSI Cyan color format string.
 */
#define ANSI_COLOR_CYAN  "\x1b[96m"

/** \~english
 * \brief ANSI reset format string.
 */
#define ANSI_RESET  "\x1b[0m"

/* Macro functions :                                                         */

#define _STR(v)  #v
#define STR(v)  _STR(v)

/** \~english
 * \brief Maximum value macro.
 *
 * \warning NEVER use this macro if x is not a side-effect-free expression.
 */
#define MAX(x, y)  (((x) > (y)) ? (x) : (y))

/** \~english
 * \brief Minimum value macro.
 *
 * \warning NEVER use this macro if x is not a side-effect-free expression.
 */
#define MIN(x, y)  (((x) < (y)) ? (x) : (y))

/** \~english
 * \brief Absolute value macro.
 *
 * \warning NEVER use this macro if x is not a side-effect-free expression.
 */
#define ABS(x)  (0 < (x) ? (x) : -(x))

/** \~english
 * \brief Arithmetic square macro.
 *
 * \param x Numeric operand.
 *
 * \warning NEVER use this macro if x is not a side-effect-free expression.
 */
#define SQUARE(x)  ((x) * (x))

#define DIV_ROUND_UP(num, div)  (((num) + ((div) - 1)) / (div))

#define DIV_ROUND(num, div)  ((((num) + (div)) >> 1) / (div))

/** \~english
 * \brief Shift right and round up.
 */
#define SHF_ROUND_UP(num, exp)  (((num) + ((1 << exp) - 1)) >> (exp))

/** \~english
 * \brief Number of entries in a constant array?
 *
 * \param array Constant array.
 * \return Size of the array.
 */
#define ARRAY_SIZE(array)  (sizeof(array) / sizeof(array[0]))

#define CHECK_ARRAY_2VALS(arr, x, y)                                          \
  ((arr)[0] == (x) && (arr)[1] == (y))

/** \~english
 * \brief Floats comparison epsilon value.
 */
#define EPSILON  0.0001

/** \~english
 * \brief Return true if both float values are considered as equal accoding
 * to #EPSILON value.
 *
 * \param x float Left floating point value operand.
 * \param y float Right floating point value operand.
 * \return bool True Values are considered as equal.
 * \return bool False Values are not equal.
 */
#define FLOAT_COMPARE(x, y)                                                   \
  (                                                                           \
    ABS((x) - (y)) < EPSILON                                                  \
  )

/** \~english
 * \brief
 *
 * Require inclusion of <string.h>.
 */
#define MEMSET_ARRAY(arr, val)                                                \
  memset(arr, val, ARRAY_SIZE(arr) * sizeof(*(arr)))

#define SET_NULL_ARRAY(arr)                                                   \
  do {                                                                        \
    size_t i;                                                                 \
    for (i = 0; i < ARRAY_SIZE(arr); i++)                                     \
      (arr)[i] = NULL;                                                        \
  } while (0)

/** \~english
 * \brief Set byte of a byte array.
 *
 */
#define SB_ARRAY(arr, off, val)                                               \
  ((arr)[off] = (val))

/** \~english
 * \brief Write byte to byte array.
 *
 */
#define WB_ARRAY(arr, off, val)                                               \
  SB_ARRAY(arr, (off)++, val)

#define RB_ARRAY(arr, off)                                                    \
  ((arr)[(off)++])

/** \~english
 * \brief Write byte array to byte array.
 *
 */
#define WA_ARRAY(arr, off, src, len)                                          \
  do {                                                                        \
    memcpy((arr) + (off), src, len);                                          \
    (off) += (len);                                                           \
  } while (0)

/** \~english
 * \brief Read byte array from file.
 *
 */
#define RA_FILE(fd, dst, len)                                                 \
  fread(dst, len, 1, fd)

/** \~english
 * \brief Read byte array from file.
 *
 */
#define WA_FILE(fd, src, len)                                                 \
  fwrite(src, len, 1, fd)

#define GROW_ALLOCATION(old, def)                                             \
  (                                                                           \
    ((old) < def) ?                                                           \
      def                                                                     \
    :                                                                         \
      (old) << 1                                                              \
  )

/** \~english
 * \brief Transform a function return value from a boolean convention to
 * program zero/negative convention.
 *
 * If supplied value is 0, -1 is returned. If supplied value is 1, 0 is
 * returned.*
 */
#define BOOL_TO_INT_RET(x)  ((x) - 1)

#define BOOL_STR(x)  ((x) ? "True" : "False")
#define BOOL_INFO(x)  ((x) ? "Yes" : "No")
#define BOOL_PRESENCE(x)  ((x) ? "Present" : "Absent")

#define UINT8A_TO_UINT16(array)                                               \
  (                                                                           \
    ((uint16_t) (array)[0] <<  8)                                             \
    | ((uint16_t) (array)[1]    )                                             \
  )

#define UINT8A_TO_UINT32(array)                                               \
  (                                                                           \
    ((uint32_t) UINT8A_TO_UINT16(array) << 16)                                \
    | ((uint32_t) UINT8A_TO_UINT16((array)+2))                                \
  )

#define UINT8A_TO_UINT64(array)                                               \
  (                                                                           \
    ((uint64_t) UINT8A_TO_UINT32(array) << 32)                                \
    | ((uint64_t) UINT8A_TO_UINT32((array)+4))                                \
  )


#define LB_DECL_GUID(label)  uint8_t label[16]

#define LB_PRIGUID                                                            \
  "02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X"

#define LB_PRIGUID_EXTENDER(field)                                            \
  field[ 0], field[ 1], field[ 2], field[ 3],                                 \
  field[ 4], field[ 5], field[ 6], field[ 7],                                 \
  field[ 8], field[ 9], field[10], field[11],                                 \
  field[12], field[13], field[14], field[15]

#define lb_guid_equal(l, r)  lb_data_equal(l, r, 16)

/* Multiplexer Macros :                                                      */
#define READ_BUFFER_LEN                                                    8192
#define WRITE_BUFFER_LEN                                                   4096
#define DIFF_TRIGGER_REALLOC_OPTIMIZATION                                131072

/* Multiplex Parameters :                                                    */

/** \~english
 * \brief Initial BDAV multiplex buffering duration in seconds.
 *
 * Delay added to the initial multiplexing timecode (smallest DTS).
 */
#define BDAV_STD_BUF_DURATION                                               0.9

/** \~english
 * \brief Disable interleaving of TS packets from different PIDs.
 *
 * If this macro is set to zero, streams are interleaved according to
 * their buffering timings. For exemple, two video frame TS packets
 * of the same PES packet can be interleaved with an audio TS packet
 * if buffering timings required it.
 * Otherwise, all TS packets of the same PES packet will be send as
 * one continuous burst according to the first TS packet timing.
 *
 * This parameter is disabled by default.
 */
#define DISABLE_STREAM_TYPES_INTERLEAVING                                     0

/** \~english
 * \brief Allows partial memory releasing after PES frames size peaks.
 *
 * If this macro is set to zero, stream PES buffer size can only
 * increase during multiplexing. Ending with a buffer size
 * of the biggest PES frame size, reducing number of memory
 * reallocations.
 * Otherwise, buffer size will be reduced in size if a PES frame
 * size difference is greater than #DIFF_TRIGGER_REALLOC_OPTIMIZATION
 * bytes, minimizing memory usage at cost of memory reallocations.
 *
 * This parameter is enabled by default.
 */
#define USE_OPTIMIZED_REALLOCATION                                            1

/** \~english
 * \brief Allows alternative optimized bit per bit file reading.
 *
 * If this macro is set to zero, reading n bits will call n times
 * #readBit() function.
 * Otherwise, bigger binary blocks will computed within #readBits()
 * function.
 *
 * This parameter is enabled by default.
 */
#define USE_ALTER_BIT_READING                                                 1

/** \~english
 * \brief Allows usage of low level file I/O calls.
 *
 * If this macro is set to zero, file handling will be performed
 * using portable C standard stdio libray.
 * Otherwise, system dependant calls will be used. If a system is
 * not handled, file handling is switched back to standard one.
 *
 * This parameter is disabled by default.
 *
 * \todo This is not currently implemented.
 */
#define USE_LOW_LEVEL_FILE_HANDLING                                           0

#endif