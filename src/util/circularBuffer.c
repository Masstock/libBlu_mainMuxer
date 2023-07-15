
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "circularBuffer.h"

CircularBufferPtr createCircularBuffer(
  void
)
{
  CircularBufferPtr buf;
  if (NULL == (buf = (CircularBufferPtr) calloc(1, sizeof(CircularBuffer))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  return buf;
}

void * newEntryCircularBuffer(
  CircularBufferPtr buf,
  size_t slot_size
)
{
  assert(NULL != buf);

  if (NULL == buf->array) {
    assert(0 < slot_size);
    buf->slot_size = slot_size;
  }
  else
    lb_assert_npd(buf->slot_size == slot_size);
    // assert(buf->slot_size == slot_size);

  size_t allocated_size = CB_DEF_SIZE << buf->alloc_shft_size;
  if (NULL == buf->array || allocated_size <= buf->used_size) {
    size_t cur_size = CB_DEF_SIZE << (buf->alloc_shft_size);
    size_t new_size = CB_DEF_SIZE << (buf->alloc_shft_size + 1);

    if (!new_size || lb_mul_overflow(new_size, slot_size))
      LIBBLU_ERROR_NRETURN("Circular buffer FIFO overflow.\n");

    uint8_t * new_buf;
    if (NULL == (new_buf = realloc(buf->array, new_size * slot_size)))
      LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

    /* Moving indexes from buffer. */
    size_t dI = cur_size; // Destination Index
    size_t bI = cur_size - buf->top; // Buffer Index
    for (; bI < buf->used_size; bI++, dI++) {
      size_t sI = (buf->top + bI) & (cur_size - 1); // Source Index
      memcpy(&new_buf[dI * slot_size], &new_buf[sI * slot_size], slot_size);
    }

    buf->array = new_buf;
    buf->alloc_shft_size++;
  }

  allocated_size = CB_DEF_SIZE << buf->alloc_shft_size; // Update
  size_t dI = (buf->top + (buf->used_size++)) & (allocated_size - 1);
    // Destination Index

  return &buf->array[dI * slot_size];
}
