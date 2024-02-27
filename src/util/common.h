/** \~english
 * \file common.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Common utility functions.
 */

#ifndef __LIBBLU_MUXER__UTIL__COMMON_H__
#define __LIBBLU_MUXER__UTIL__COMMON_H__

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"
#include "errorCodes.h"
#include "string.h"

#if defined(ARCH_WIN32)
typedef unsigned __LONG32 DWORD;

lbc * getLastWindowsErrorString(
  DWORD * err
);

#endif

uint32_t lbc_fnv1aStrHash(
  const lbc * str
);

/** \~english
 * \brief Byte order swap a two bytes word.
 *
 * \param value Word to be byte order swapped.
 * \return uint16_t Resulting byte order swapped word.
 */
static inline uint16_t bswap16(
  uint16_t value
)
{
  return (value << 8) | (value >> 8);
}

/** \~english
 * \brief Byte order swap a four bytes double word.
 *
 * \param value Dword to be byte order swapped.
 * \return uint32_t Resulting byte order swapped dword.
 */
static inline uint32_t bswap32(
  uint32_t value
)
{
  return
    (value << 24)
    | ((value >> 8) & 0xFF00)
    | ((value << 8) & 0xFF0000)
    | (value >> 24)
  ;
}

/** \~english
 * \brief Swap byte order of each value in given four bytes double word.
 *
 * \param value Dword containing values.
 * \param length Length of each value (8, 16 or 32 bits).
 * \return uint32_t Resulting byte order swapped dword.
 */
static inline uint32_t bswapn32(
  uint32_t value,
  unsigned length
)
{
  if (8 == length)
    return value; // Do nothing.
  if (16 == length)
    return bswap16(value);
  if (32 == length)
    return bswap32(value);
  LIBBLU_ERROR_EXIT(
    "%s:%u: Invalid swap length %u.\n",
    __func__, __LINE__, length
  );
}

/** \~english
 * \brief Swap bits from given byte.
 *
 * \param value Byte to swap bits from.
 * \return uint8_t Resulting bit-reversed byte.
 */
static inline uint8_t binswap8(
  uint8_t value
)
{
  const uint8_t nibble_lut[] = {
    0x0, 0x8, 0x4, 0xC,
    0x2, 0xA, 0x6, 0xE,
    0x1, 0x9, 0x5, 0xD,
    0x3, 0xB, 0x7, 0xF
  };
  return (nibble_lut[value & 0xF] << 4) | nibble_lut[value >> 4];
}

/** \~english
 * \brief Swap bits of each byte of a two bytes word.
 *
 * \param value Word of two bytes to binary swap.
 * \return uint16_t Resulting binary swapped word.
 *
 * Bytes order is not touched.
 */
static inline uint16_t binswap16(
  uint16_t value
)
{
  return (binswap8(value >> 8) << 8) | binswap8(value);
  ;
}

/** \~english
 * \brief Swap bits of each byte of a four bytes dword.
 *
 * \param value Double word of four bytes to binary swap.
 * \return uint32_t Resulting binary swapped dword.
 *
 * Bytes order is not touched.
 */
static inline uint32_t binswap32(
  uint32_t value
)
{
  return
    (binswap8(value >> 24) << 24)
    | (binswap8(value >> 16) << 16)
    | (binswap8(value >>  8) <<  8)
    | binswap8(value)
  ;
}

static inline uint8_t lb_xor_32_to_8(
  uint32_t value
)
{
  value ^= value >> 16;
  value ^= value >> 8;
  return value;
}

static inline uint8_t lb_xor_16_to_8(
  uint16_t value
)
{
  value ^= value >> 8;
  return value;
}

static inline uint8_t lb_xor_16_to_4(
  uint16_t value
)
{
  value ^= value >> 8;
  value ^= value >> 4;
  return value & 0xF;
}

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

static inline int lb_sign_extend(
  int value,
  unsigned size
)
{
  const int mask = 1u << (size - 1);
  return (value ^ mask) - mask;
}

#if 0
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
#endif

static inline uint64_t lb_ceil_pow2_64(
  uint64_t value
)
{
  value--;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value |= value >> 32;
  return value + 1;
}

static inline uint32_t lb_ceil_pow2_32(
  uint32_t value
)
{
  value--;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  return value + 1;
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
static inline int lb_mul_overflow_size_t(
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

static inline int lb_u32add_overflow(
  uint32_t left,
  uint32_t right
)
{
  return UINT32_MAX - left < right;
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
  *(*dst) = '\0';
}

static inline void lb_str_cat_comma(
  char ** dst,
  const char * src,
  bool append_prefix_comma
)
{
  if (append_prefix_comma)
    lb_str_cat(dst, ", ");
  while (*src != '\0')
    *((*dst)++) = *(src++);
  *(*dst) = '\0';
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

static inline uint32_t lb_compute_crc32(
  uint8_t * data,
  size_t offset,
  size_t length
)
{
  uint32_t crc = 0xFFFFFFFF;

  for (size_t pos = offset; pos < offset + length; pos++) {
    crc ^= ((uint32_t) data[pos]) << 24;
    for (unsigned i = 0; i < 8; i++) {
      crc = (crc << 1) ^ ((0 - (crc >> 31)) & 0x04C11DB7);
    }
  }

  return crc;
}

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
int lb_get_relative_fp_from_anchor(
  lbc * buf,
  size_t size,
  const lbc * filepath,
  const lbc * anchor
);

int lb_gen_anchor_absolute_fp(
  lbc ** dst,
  const lbc * base,
  const lbc * path
);

int lb_gen_absolute_fp(
  lbc ** dst,
  const lbc * path
);

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

#if 0
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

#endif