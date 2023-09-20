/** \~english
 * \file circularBuffer.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Circular buffers (FIFO) structures implementation module.
 */

#ifndef __LIBBLU_MUXER__UTIL__CIRCULAR_BUFFER_H__
#define __LIBBLU_MUXER__UTIL__CIRCULAR_BUFFER_H__

#include "common.h"
#include "errorCodes.h"

#define CB_DEF_SIZE  4ull

/** \~english
 * \brief Modular circular buffer FIFO structure.
 */
typedef struct {
  uint8_t * array;             /**< Circular buffer array.                   */
  unsigned alloc_shft_size;    /**< Buffer current allocated size as a power
    of 2.                                                                    */
  size_t used_size;            /**< Buffer number of stored entries.         */

  size_t top;                  /**< Buffer first stored element index.       */
  size_t slot_size;            /**< Size in bytes of a buffer slot.          */
} CircularBuffer;

/** \~english
 * \brief Creates and initialize a circular buffer.
 *
 * \param entrySize Buffer array entries size in bytes.
 * \return CircularBuffer * On sucess, created object is returned. Otherwise,
 * a NULL pointer is returned.
 *
 * Supplied entrySize is the size of buffer saved entries. This value can't
 * be equal to zero.
 * For exemple, if buffer will contain integers, supplied size must be
 * sizeof(int).
 *
 * Created object must be passed to #destroyCircularBuffer() after use.
 */
CircularBuffer * createCircularBuffer(
  void
);

/** \~english
 * \brief Release all memory allocation done by #createCircularBuffer().
 *
 * \param buf Circulat buffer object to free.
 */
static inline void destroyCircularBuffer(
  CircularBuffer * buf
)
{
  if (NULL == buf)
    return;

  free(buf->array);
  free(buf);
}

static inline void cleanCircularBuffer(
  CircularBuffer buffer
)
{
  free(buffer.array);
}

/** \~english
 * \brief Create a new entry in supplied circular buffer and return a pointer
 * to this created entry.
 *
 * \param buf Target circular buffer.
 * \return void* On success, a pointer to the created entry is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * Created entry size is defined by #createCircularBuffer() supplied entrySize
 * value.
 * If buffer is full, its size is growed. If buffer is too big or if a memory
 * allocation occurs, an error is returned.
 */
void * newEntryCircularBuffer(
  CircularBuffer * buf,
  size_t slot_size
);

static inline size_t nbEntriesCircularBuffer(
  const CircularBuffer * buf
)
{
  assert(NULL != buf);

  return buf->used_size;
}

static inline bool isEmptyCircularBuffer(
  const CircularBuffer * buf
)
{
  return 0 == buf->used_size;
}

static inline void * getCircularBuffer(
  const CircularBuffer * buf,
  size_t index
)
{
  assert(NULL != buf);
  assert(0 < buf->slot_size);

  if (buf->used_size <= index)
    return NULL; /* Out of circular buffer index. */

  size_t mask = ((CB_DEF_SIZE << buf->alloc_shft_size) - 1);
  size_t entry_idx = (buf->top + index) & mask;

  return (void *) &buf->array[entry_idx * buf->slot_size];
}

/** \~english
 * \brief Pop first entry from supplied circular buffer.
 *
 * \param buf Target circular buffer.
 * \return int If non empty, pointer to the olded inserted entry is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * \warning Returned pointer addresses memory managed by the CircularBuffer.
 * Calls to #newEntryCircularBuffer() might invalidate previously returned
 * pointers.
 */
static inline void * popCircularBuffer(
  CircularBuffer * buf
)
{
  assert(NULL != buf);

  if (!buf->used_size)
    return NULL; // Empty circular buffer.

  void * entry = &buf->array[buf->top * buf->slot_size];

  size_t mask = ((CB_DEF_SIZE << buf->alloc_shft_size) - 1);
  buf->top = (buf->top + 1) & mask;
  buf->used_size--;

  return entry;
}

#endif