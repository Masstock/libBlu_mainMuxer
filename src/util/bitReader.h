

#ifndef __LIBBLU_MUXER__UTIL__BIT_READER_H__
#define __LIBBLU_MUXER__UTIL__BIT_READER_H__

#include "common.h"

typedef struct {
  const uint8_t * buf;  /**< Byte array content. */
  size_t offset;  /**< Reading offset in bits. */
  size_t size;    /**< Total buffer size in bits. */
} LibbluBitReader, *LibbluBitReaderPtr;

static inline void initLibbluBitReader(
  LibbluBitReader * br,
  const uint8_t * buf,
  size_t size
)
{
  *br = (LibbluBitReader) {
    .buf = buf,
    .size = size
  };
}

static inline size_t offsetLibbluBitReader(
  const LibbluBitReader * br
)
{
  return br->offset;
}

static inline size_t remainingBitsLibbluBitReader(
  const LibbluBitReader * br
)
{
  return br->size - br->offset;
}

static inline int getLibbluBitReader(
  LibbluBitReader * br,
  uint32_t * value,
  unsigned bits
)
{
  const uint8_t * buf = br->buf;
  size_t off  = br->offset;
  size_t size = br->size;

  assert(bits <= 32);

  if (size < off + bits)
    LIBBLU_ERROR_RETURN("Prematurate end of file.\n");

  unsigned required_bytes = (bits + 7 + (off & 0x7)) >> 3;
  uint32_t next_bits = 0;
  for (unsigned i = 0; i < required_bytes; i++) {
    next_bits = (next_bits << 8) | buf[(off + 8 * i) >> 3];
  }
  next_bits <<= off & 0x7;

  *value = (next_bits >> ((required_bytes << 3) - bits)) & ((1ull << bits) - 1);

  return 0;
}

static inline int readLibbluBitReader(
  LibbluBitReader * br,
  uint32_t * value,
  unsigned bits
)
{
  if (getLibbluBitReader(br, value, bits) < 0)
    return -1;
  br->offset += bits;
  return 0;
}

static void skipLibbluBitReader(
  LibbluBitReader * br,
  unsigned bits
)
{
  assert(bits <= remainingBitsLibbluBitReader(br));
  br->offset += bits;
}

static inline int padByteLibbluBitReader(
  LibbluBitReader * br
)
{
  unsigned padding_size = (~(br->offset - 1)) & 0x7;

  if (remainingBitsLibbluBitReader(br) < padding_size)
    LIBBLU_ERROR_RETURN("Prematurate end of file.\n");
  skipLibbluBitReader(br, padding_size);
  return 0;
}

static inline int padWordLibbluBitReader(
  LibbluBitReader * br
)
{
  unsigned padding_size = (~(br->offset - 1)) & 0xF;

  if (remainingBitsLibbluBitReader(br) < padding_size)
    LIBBLU_ERROR_RETURN("Prematurate end of file.\n");
  skipLibbluBitReader(br, padding_size);
  return 0;
}

#endif