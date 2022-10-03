
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "circularBuffer.h"

CircularBufferPtr createCircularBuffer(size_t entrySize)
{
  CircularBufferPtr buf;

  if (!entrySize)
    LIBBLU_ERROR_NRETURN(
      "Expect at least 1 byte sized entries in circular buffer.\n"
    );

  if (NULL == (buf = (CircularBufferPtr) malloc(sizeof(CircularBuffer))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  buf->buffer = NULL;
  buf->allocatedSize = buf->usedSize = 0;
  buf->top = 0;
  buf->entrySize = entrySize;

  return buf;
}

void destroyCircularBuffer(CircularBufferPtr buf)
{
  if (NULL == buf)
    return;

  free(buf->buffer);
  free(buf);
}

void * newEntryCircularBuffer(CircularBufferPtr buf)
{
  void * newBuffer;
  size_t newLength;
  size_t dI, bI, sI;
  /* dI: Destination Index, bI: Buffer Index, sI: Source Index. */

  assert(NULL != buf);

  if (buf->allocatedSize <= buf->usedSize) {
    newLength = GROW_CIRCULAR_BUFFER_SIZE(
      buf->allocatedSize
    );

    if (newLength <= buf->usedSize)
      LIBBLU_ERROR_NRETURN("Circular buffer FIFO overflow.\n");

    if (NULL == (newBuffer = realloc(buf->buffer, buf->entrySize * newLength)))
      LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

    /* Moving indexes from buffer. */
    for (
      dI = buf->allocatedSize, bI = buf->allocatedSize - buf->top;
      bI < buf->usedSize;
      bI++, dI++
    ) {
      sI = (buf->top + bI) % buf->allocatedSize;
      memcpy(
        newBuffer + dI * buf->entrySize,
        newBuffer + sI * buf->entrySize,
        buf->entrySize
      );
    }

    buf->buffer = newBuffer;
    buf->allocatedSize = newLength;
  }

  dI = (buf->top + (buf->usedSize++)) % buf->allocatedSize;

  return buf->buffer + dI * buf->entrySize;
}

size_t getNbEntriesCircularBuffer(CircularBufferPtr buf)
{
  assert(NULL != buf);

  return buf->usedSize;
}

void * getEntryCircularBuffer(CircularBufferPtr buf, size_t index)
{
  size_t entry;

  assert(NULL != buf);

  if (buf->usedSize <= index)
    return NULL; /* Out of circular buffer index. */

  entry = (buf->top + index) % buf->allocatedSize;

  return buf->buffer + entry * buf->entrySize;
}

int popCircularBuffer(CircularBufferPtr buf, void ** entry)
{
  assert(NULL != buf);

  if (!buf->usedSize)
    return -1; /* Empty circular buffer. */

  if (NULL != entry)
    *entry = buf->buffer + buf->top * buf->entrySize;

  buf->top = (buf->top + 1) % buf->allocatedSize;
  buf->usedSize--;
  return 0;
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