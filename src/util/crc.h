
#ifndef __LIBBLU_MUXER__UTIL__CRC_H__
#define __LIBBLU_MUXER__UTIL__CRC_H__

#include "crcLookupTables.h"

typedef struct {
  const uint32_t *table;
  bool shifted;
  unsigned length;
  uint32_t poly;
} CrcParam;

typedef struct {
  bool in_use;     /**< Set current usage of CRC computation.            */
  uint32_t buf;    /**< CRC computation buffer.                          */
  CrcParam param;  /**< Attached CRC generation parameters.              */
} CrcContext;

static inline void resetCrcContext(
  CrcContext *ctx
)
{
  *ctx = (CrcContext) {
    0
  };
}

static inline void initCrcContext(
  CrcContext *ctx,
  CrcParam param,
  uint32_t initial_value
)
{
  assert(NULL != param.table || 0x0 != param.poly);
  assert(0 < param.length);

  *ctx = (CrcContext) {
    .in_use = true,
    .buf = initial_value,
    .param = param
  };
}

static inline void setUseCrcContext(
  CrcContext *ctx,
  bool use
)
{
  ctx->in_use = use;
}

void applyNoTableShiftedCrcContext(
  CrcContext *ctx,
  uint8_t byte
);

void applyNoTableCrcContext(
  CrcContext *ctx,
  uint8_t byte
);

static inline void applySingleByteTableCrcContext(
  CrcContext *ctx,
  const uint32_t *table,
  uint8_t byte
)
{
  unsigned length = ctx->param.length;
  uint32_t buf    = ctx->buf;

  if (ctx->param.shifted)
    ctx->buf = ((buf << 8) ^ byte) ^ table[(buf >> (length - 8)) & 0xFF];
  else
    ctx->buf = (buf >> 8) ^ table[(buf & 0xFF) ^ byte];
}

static inline void applySingleByteCrcContext(
  CrcContext *ctx,
  uint8_t byte
)
{
  if (!ctx->in_use)
    return;

  if (NULL != ctx->param.table) {
    /* Applying CRC look-up table */
    applySingleByteTableCrcContext(ctx, ctx->param.table, byte);
  }
  else {
    if (ctx->param.shifted)
      applyNoTableShiftedCrcContext(ctx, byte);
    else
      applyNoTableCrcContext(ctx, byte);
  }
}

static inline void applyTableCrcContext(
  CrcContext *ctx,
  const uint8_t *array,
  size_t size
)
{
  const uint32_t *table = ctx->param.table;

  assert(NULL != table);

  if (!ctx->in_use)
    return;

  for (size_t i = 0; i < size; i++) {
    applySingleByteTableCrcContext(ctx, table, array[i]);
  }
}

static inline void applyCrcContext(
  CrcContext *ctx,
  const uint8_t *array,
  size_t size
)
{
  if (!ctx->in_use)
    return;

  for (size_t i = 0; i < size; i++) {
    applySingleByteCrcContext(ctx, array[i]);
  }
}

static inline uint32_t completeCrcContext(
  CrcContext *ctx
)
{
  assert(ctx->in_use);

  ctx->in_use = false;
  return (ctx->buf & ((1ull << ctx->param.length) - 1));
}

#endif