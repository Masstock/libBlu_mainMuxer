

#ifndef __LIBBLU_MUXER__UTIL__BIT_WRITER_H__
#define __LIBBLU_MUXER__UTIL__BIT_WRITER_H__

#include "common.h"
#include "crc.h"

typedef struct {
  uint8_t * buf;  /**< Byte array content.                                   */
  size_t offset;  /**< Writing offset in bits.                               */
  size_t size;    /**< Current buffer size in bits.                          */
} LibbluBitWriter;

static inline void initLibbluBitWriter(
  LibbluBitWriter * br
)
{
  *br = (LibbluBitWriter) {
    0
  };
}

static inline void cleanLibbluBitWriter(
  LibbluBitWriter br
)
{
  free(br.buf);
}

static inline size_t offsetLibbluBitWriter(
  const LibbluBitWriter * br
)
{
  return br->offset;
}

static inline const uint8_t * accessLibbluBitWriter(
  const LibbluBitWriter * br
)
{
  return br->buf;
}

int increaseAllocLibbluBitWriter(
  LibbluBitWriter * br,
  size_t required_size
);

static inline int putLibbluBitWriter(
  LibbluBitWriter * br,
  uint64_t value,
  unsigned bits
)
{
  assert(bits <= 64);

  if (br->size < br->offset + bits) {
    if (increaseAllocLibbluBitWriter(br, br->offset + bits) < 0)
      return -1;
  }

  value &= (1ull << bits) - 1;

  while (8 <= (br->offset & 0x7) + bits) {
    unsigned consumed_bits = 8 - (br->offset & 0x7);
    br->buf[br->offset >> 3] |= value >> (bits - consumed_bits);
    br->offset += consumed_bits;
    bits -= consumed_bits;
  }

  br->buf[br->offset >> 3] |= value << (8 - (bits & 0x7) - (br->offset & 0x7));
  br->offset += bits;

  return 0;
}

static inline void padByteLibbluBitWriter(
  LibbluBitWriter * br
)
{
  br->offset += (~(br->offset - 1)) & 0x7;
}

static inline int padBlockBoundaryByteLibbluBitWriter(
  LibbluBitWriter * br,
  size_t start_offset
)
{
  size_t padding = (~(br->offset - 1) + start_offset) & 0x7;
  return putLibbluBitWriter(br, 0x0, padding);
}

#endif