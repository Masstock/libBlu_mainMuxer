
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "circularBuffer.h"

CircularBufferPtr createCircularBuffer(
  size_t entrySize
)
{
  CircularBufferPtr buf;

  if (!entrySize)
    LIBBLU_ERROR_NRETURN(
      "Expect at least 1 byte sized entries in circular buffer.\n"
    );

  if (NULL == (buf = (CircularBufferPtr) malloc(sizeof(CircularBuffer))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  *buf = (CircularBuffer) {
    .allocatedSizeShift = 1,
    .entrySize = entrySize,
  };

  return buf;
}

void * newEntryCircularBuffer(
  CircularBufferPtr buf
)
{
  size_t dI, bI, sI;
  /* dI: Destination Index, bI: Buffer Index, sI: Source Index. */

  assert(NULL != buf);

  if (NULL == buf->array || (1ul << buf->allocatedSizeShift) <= buf->usedSize) {
    void * newBuffer;
    size_t curSize, newSize;

    curSize = 1ul << buf->allocatedSizeShift;
    newSize = 1ul << (buf->allocatedSizeShift + 1);

    if (!newSize || lb_mul_overflow(newSize, buf->entrySize))
      LIBBLU_ERROR_NRETURN("Circular buffer FIFO overflow.\n");

    if (NULL == (newBuffer = realloc(buf->array, newSize * buf->entrySize)))
      LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

    /* Moving indexes from buffer. */
    for (
      dI = curSize, bI = curSize - buf->top;
      bI < buf->usedSize;
      bI++, dI++
    ) {
      sI = (buf->top + bI) & (curSize - 1);
      memcpy(
        newBuffer + dI * buf->entrySize,
        newBuffer + sI * buf->entrySize,
        buf->entrySize
      );
    }

    buf->array = newBuffer;
    buf->allocatedSizeShift++;
  }

  dI = (buf->top + (buf->usedSize++)) & ((1ul << buf->allocatedSizeShift) - 1);

  return buf->array + dI * buf->entrySize;
}

#if 0

/* DEMO */

int main(void)
{
  CircularBufferPtr buf;
  int i, * entry;

  if (NULL == (buf = createCircularBuffer(sizeof(int))))
    return -1;

  for (i = 0; i < 32; i++) {
    if (NULL == (entry = (int *) newEntryCircularBuffer(buf)))
      return -1;
    *entry = i;
  }

  for (i = 0; i < 16; i++) {
    if (popCircularBuffer(buf, (void *) &entry) < 0)
      return -1;
    lbc_printf("%d\n", *entry);
  }

  for (i = 0; i < 32; i++) {
    if (NULL == (entry = (int *) newEntryCircularBuffer(buf)))
      return -1;
    *entry = i;
  }

  while (0 <= popCircularBuffer(buf, (void *) &entry))
    lbc_printf("%d\n", *entry);

  destroyCircularBuffer(buf);

  return 0;
}

#endif