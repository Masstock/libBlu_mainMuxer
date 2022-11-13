/** \~english
 * \file common.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Common utility functions.
 *
 * \xrefitem references "References" "References list"
 *  [1] https://www.reddit.com/r/C_Programming/comments/irettz/comment/g4zeeh3
 */

#ifndef __LIBBLU_MUXER__UTIL__COMMON_H__
#define __LIBBLU_MUXER__UTIL__COMMON_H__

#include "macros.h"
#include "errorCodes.h"

#include "../libs/cwalk/include/cwalk.h"
#if defined(ARCH_WIN32)
#  include "../libs/cwalk/include/cwalk_wchar.h"
#endif
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#if defined(ARCH_WIN32)
#  include <windef.h>

void getWindowsError(
  DWORD * err,
  lbc * buf,
  size_t size
);

int lb_init_iconv(
  void
);

char * lb_iconv_wchar_to_utf8(
  const wchar_t * src
);

wchar_t * lb_iconv_utf8_to_wchar(
  const unsigned char * src
);

int lb_close_iconv(
  void
);

#endif

static inline uint32_t fnv1aStrHash(
  const char * key
)
{
  /* 32 bits FNV-1a hash */
  size_t length, i;
  uint32_t hash;

  if (NULL == key)
    return 0;

  length = strlen(key);

  hash = 2166136261u;
  for (i = 0; i < length; i++) {
    hash ^= ((uint8_t *) key)[i];
    hash *= 16777619;
  }

  return hash;
}

#if defined(ARCH_WIN32)

static inline uint32_t wfnv1aStrHash(
  const wchar_t * key
)
{
  /* 32 bits FNV-1a hash */
  size_t length, i;
  uint32_t hash;

  if (NULL == key)
    return 0;

  length = wcslen(key);

  hash = 2166136261u;
  for (i = 0; i < length; i++) {
    size_t j;

    for (j = 0; j < sizeof(wchar_t); j++) {
      hash ^= ((uint8_t *) (key + i))[j];
      hash *= 16777619;
    }
  }

  return hash;
}

#endif

/** \~english
 * \brief Unsigned Greater Common Divisor.
 *
 * \param x
 * \param y
 * \return unsigned
 */
static inline unsigned lb_gcd_unsigned(
  unsigned x,
  unsigned y
)
{
  unsigned tmp;

  while (y) {
    tmp = y;
    y = x % y;
    x = tmp;
  }

  return x;
}

/** \~english
 * \brief Unsigned Least Common Multiple.
 *
 * \param x
 * \param y
 * \return unsigned
 */
static inline unsigned lb_lcm_unsigned(
  unsigned x,
  unsigned y
)
{
  return (x * y) / lb_gcd_unsigned(x, y);
}

/** \~english
 * \brief Unsigned Nearest Multiple of x to y.
 *
 * \param x
 * \param y
 * \return unsigned
 */
static inline unsigned lb_nm_unsigned(
  unsigned x,
  unsigned y
)
{
  x = x + y / 2;
  x = x - x % y;
  return x;
}

static inline int8_t lb_conv_signed8(
  uint8_t value,
  unsigned size
)
{
  uint8_t mask, digits;

  mask = ~(1 << (size-1));
  digits = value & mask;

  return (value >> (size-1)) ? mask - ~digits : digits;
}

static inline unsigned lb_ceil_log2(
  unsigned value
)
{
  unsigned mask, ret;

  ret = 0;
  for (mask = value - 1; 0 < mask; mask >>= 1)
    ret++;

  return ret;
}

/** \~english
 * \brief Fast 32-bits log 2.
 *
 * \param value
 * \return unsigned
 *
 * Based on De Bruijn suite algorithm.
 * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
 */
static inline unsigned lb_fast_log2_32(
  uint32_t value
)
{
  static const unsigned logs[32] = {
    0,   9,  1, 10, 13, 21,  2, 29,
    11, 14, 16, 18, 22, 25,  3, 30,
    8,  12, 20, 28, 15, 17, 24,  7,
    19, 27, 23,  6, 26,  5,  4, 31
  };

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  return logs[(uint32_t) (value * 0x07C4ACDD) >> 27];
}

/** \~english
 * \brief
 *
 * \param left
 * \param right
 * \return int Non-zero if overflow occurs, zero otherwise if product is safe.
 */
static inline int lb_mul_overflow(
  size_t left,
  size_t right
)
{
  size_t res = left * right;

  return left != 0 && res / left != right;
}

static inline int lb_add_overflow(
  size_t left,
  size_t right
)
{
  return SIZE_MAX - left < right;
}

static inline int lb_uadd_overflow(
  unsigned left,
  unsigned right
)
{
  return UINT_MAX - left < right;
}

static inline bool lb_data_equal(
  const void * dat1,
  const void * dat2,
  size_t size
)
{
  return 0 == memcmp(dat1, dat2, size);
}

static inline void lb_str_cat(
  char ** dst,
  const char * src
)
{
  while (*src != '\0')
    *((*dst)++) = *(src++);
}

static inline bool lb_str_equal(
  const char * str1,
  const char * str2
)
{
  return
    *str1 == *str2
    && 0 == strcmp(str1, str2)
  ;
}

static inline bool lb_strn_equal(
  const char * str1,
  const char * str2,
  size_t size
)
{
  return
    *str1 == *str2
    && 0 == strncmp(str1, str2, size)
  ;
}

static inline char * lb_strn_dup(
  const char * string,
  size_t size
)
{
  char * dup;

  if (NULL == string)
    return NULL;

  if (NULL == (dup = malloc(size + 1)))
    return NULL;
  snprintf(dup, size+1, "%s", string);

  return dup;
}

static inline char * lb_str_dup(
  const char * string
)
{
  char * dup;
  size_t size;

  if (NULL == string)
    return NULL;

  size = strlen(string);
  if (NULL == (dup = malloc(size + 1)))
    return NULL;
  strcpy(dup, string);

  return dup;
}

static inline char * lb_str_dup_strip(
  const char * string
)
{
  size_t start, end;

  if (NULL == string)
    return NULL;

  start = strspn(string, "\t\n\v\f\r ");
  end = strlen(string) - 1;
  while (0 < end && isspace((int) string[end]))
    end--;
  end = MAX(start, end);

  return lb_strn_dup(string + start, end - start + 1);
}

static inline char * lb_str_dup_to_upper(
  const char * string
)
{
  char * copy, * cp;

  if (NULL == (copy = lb_str_dup(string)))
    return NULL;

  for (cp = copy; *cp != '\0'; cp++)
    *cp = (char) toupper((int) *cp);

  return copy;
}

static inline void lb_strncat(
  char * s1,
  const char * s2,
  size_t size
)
{
  while (*s1 != '\0')
    s1++;

  while (*s2 != '\0' && 0 < --size)
    *(s1++) = *(s2++);
  *s1 = '\0';
}

#if defined(ARCH_WIN32)

static inline bool lb_wstr_equal(
  const wchar_t * str1,
  const wchar_t * str2
)
{
  return
    *str1 == *str2
    && 0 == wcscmp(str1, str2)
  ;
}

static inline bool lb_wstrn_equal(
  const wchar_t * str1,
  const wchar_t * str2,
  size_t size
)
{
  return
    *str1 == *str2
    && 0 == wcsncmp(str1, str2, size)
  ;
}

static inline wchar_t * lb_wstrn_dup(
  const wchar_t * string,
  size_t size
)
{
  wchar_t * dup;
  size_t length;

  length = size + 1;
  if (NULL == (dup = malloc(sizeof(wchar_t) * length)))
    return NULL;
  memcpy(dup, string, sizeof(wchar_t) * size);
  dup[size] = L'\0';

  return dup;
}

static inline wchar_t * lb_wstr_dup(
  const wchar_t * string
)
{
  wchar_t * dup;
  size_t length;

  length = wcslen(string) + 1;
  if (NULL == (dup = malloc(sizeof(wchar_t) * length)))
    return NULL;
  wcscpy(dup, string);

  return dup;
}

static inline wchar_t * lb_char_to_wchar(
  const char * string
)
{
  size_t size;
  wchar_t * dst;

  size = mbstowcs(NULL, string, 0) + 1;
  if (NULL == (dst = (wchar_t *) malloc(size * sizeof(wchar_t))))
    return NULL;
  mbstowcs(dst, string, size);

  return dst;
}

static inline void lb_wstrncat(
  wchar_t * s1,
  const wchar_t * s2,
  size_t size
)
{
  while (*s1 != L'\0')
    s1++;

  while (*s2 != L'\0' && 0 < --size)
    *(s1++) = *(s2++);
  *s1 = L'\0';
}

#else
#  define lb_str_conv  lb_str_dup
#endif

#if defined(_GNU_SOURCE)
#  define lb_vasprintf vasprintf
#  define lb_asprintf asprintf
#else
int lb_vasprintf(
  char ** string,
  const char * format,
  va_list ap
);
int lb_asprintf(
  char ** string,
  const char * format,
  ...
);
#endif

#if 0
static inline void lb_cleanup_eol(
  char * string,
  size_t size
)
{
  while (size && iscntrl(string[--size]))
    string[size] = '\0';
  if (string[size] == '.')
    string[size] = '\0';
}
#endif

#if defined(ARCH_WIN32)
int lb_wasprintf(
  wchar_t ** string,
  const wchar_t * format,
  ...
);
#endif

static inline uint32_t lb_compute_crc32(
  uint8_t * data,
  size_t startOffset,
  size_t length
)
{
  bool msb;
  size_t pos, bitIdx;
  uint32_t crc = 0xFFFFFFFF;

  for (pos = startOffset; pos < startOffset + length; pos++) {
    crc ^= (uint32_t) data[pos] << 24;

    for (bitIdx = 0; bitIdx < 8; bitIdx++) {
      msb = (crc >> 31);
      crc = (crc << 1) ^ ((0 - msb) & 0x04C11DB7);
    }
  }

  return crc;
}

/** \~english
 * \brief
 *
 * \param buf
 * \param size
 * \return int
 *
 * Based on [1].
 */
int lb_get_wd(
  char * buf,
  size_t size
);

#if defined(ARCH_WIN32)
int lb_wget_wd(
  wchar_t * buf,
  size_t size
);
#endif

/** \~english
 * \brief Generate a relative filepath from a anchor filepath.
 *
 * \param buf Path destination buffer.
 * \param size Buffer size.
 * \param filepath Relative filepath from anchor.
 * \param anchor Anchor relative filepath from program working directory.
 * \return int Upon successfull completion, a zero value is returned.
 * Otherwise, a negative value is returned.
 *
 * This function may be used to convert script relative paths to program
 * relative paths using script filepath as anchor.
 */
static inline int lb_get_relative_fp_from_anchor(
  lbc * buf,
  size_t size,
  const lbc * filepath,
  const lbc * anchor
)
{
  size_t anchorDirectorySize;
  lbc anchorDirectory[PATH_BUFSIZE];

  /* Check if path is already absolute */
  if (lbc_cwk_path_is_absolute(filepath))
    return BOOL_TO_INT_RET(
      lbc_snprintf(buf, size, "%" PRI_LBCS, filepath)
      < (int) size
    );

  lbc_cwk_path_get_dirname(anchor, &anchorDirectorySize);
  if (PATH_BUFSIZE <= anchorDirectorySize)
    return 0;
  if (!anchorDirectorySize)
    return BOOL_TO_INT_RET(
      lbc_snprintf(buf, size, "%" PRI_LBCS, filepath)
      < (int) size
    );

  lbc_snprintf(anchorDirectory, anchorDirectorySize, "%" PRI_LBCS, anchor);
  return BOOL_TO_INT_RET(
    lbc_snprintf(
      buf, size, "%" PRI_LBCS "/%" PRI_LBCS, anchorDirectory, filepath
    ) < (int) size
  );
}

static inline int lb_gen_absolute_fp(
  lbc ** dst,
  const lbc * path
)
{
  lbc absolutePath[PATH_BUFSIZE];
  size_t absolutePathSize;

  assert(NULL != dst);
  assert(NULL != path);

  if (lbc_cwk_path_is_absolute(path)) {
    int ret;

    /* The path is already an absolute path */
    ret = lbc_snprintf(
      absolutePath, PATH_BUFSIZE, "%" PRI_LBCS, path
    );
    if (ret < 0)
      LIBBLU_ERROR_RETURN("Too long absolute filepath.\n");
    absolutePathSize = (size_t) ret;
  }
  else {
    /* The path is relative and shall be converted to absolute */
    lbc workingDirectory[PATH_BUFSIZE];

    /* Get the working directory as base */
    if (lbc_getwd(workingDirectory, PATH_BUFSIZE) < 0)
      return -1;

    absolutePathSize = lbc_cwk_path_get_absolute(
      workingDirectory,
      path,
      absolutePath,
      PATH_BUFSIZE
    );
  }

  if (PATH_BUFSIZE <= absolutePathSize)
    LIBBLU_ERROR_RETURN("Too long absolute filepath.\n");

  /* Duplicate the result from stack to heap */
  if (NULL == (*dst = lbc_strdup(absolutePath)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

static inline int lb_access_fp(
  const char * filepath,
  const char * mode
)
{
  FILE * fd;

  if (NULL == (fd = fopen(filepath, mode)))
    return -1;
  return fclose(fd);
}

#if defined(ARCH_WIN32)

static inline int lb_waccess_fp(
  const wchar_t * filepath,
  const wchar_t * mode
)
{
  FILE * fd;

  if (NULL == (fd = _wfopen(filepath, mode)))
    return -1;
  return fclose(fd);
}

#endif

static inline void lb_print_data(
  const uint8_t * buf,
  size_t size
)
{
  size_t off;

  for (off = 0; off < size; off++)
    lbc_printf("0x%02X ", (unsigned char) buf[off]);

  lbc_printf("\n");
}

static inline void lb_send_data(
  const uint8_t * buf,
  size_t size
)
{
  FILE * out = fopen("test.dat", "wb");
  fwrite(buf, sizeof(uint8_t), size, out);
  fclose(out);
}

#endif