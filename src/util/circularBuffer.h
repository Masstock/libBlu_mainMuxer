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

/** \~english
 * \brief CircularBuffer buffer array initialization size in entries.
 */
#define CIRCULAR_BUFFER_DEFAULT_SIZE 16

/** \~english
 * \brief CircularBuffer buffer array entries growing macro.
 *
 * \param size size_t Current allocation size of buffer array (can be zero).
 * \return size_t Incremented allocation size. If supplied size is lower than
 * #CIRCULAR_BUFFER_DEFAULT_SIZE value, it is returned.
 */
#define GROW_CIRCULAR_BUFFER_SIZE(size)                                       \
  (                                                                           \
    ((size) < CIRCULAR_BUFFER_DEFAULT_SIZE) ?                                 \
      CIRCULAR_BUFFER_DEFAULT_SIZE                                            \
    :                                                                         \
      (size) << 1                                                             \
  )

/** \~english
 * \brief Modular circular buffer FIFO structure.
 */
typedef struct {
  uint8_t * array;             /**< Circular buffer array.                   */
  unsigned allocatedSizeShift; /**< Buffer current allocated size as a power
    of 2.                                                                    */
  size_t usedSize;             /**< Buffer number of stored entries.         */

  size_t top;                  /**< Buffer first stored element index.       */
  size_t entrySize;            /**< Size in bytes of a buffer entry.         */
} CircularBuffer, *CircularBufferPtr;

/** \~english
 * \brief Creates and initialize a circular buffer.
 *
 * \param entrySize Buffer array entries size in bytes.
 * \return CircularBufferPtr On sucess, created object is returned. Otherwise,
 * a NULL pointer is returned.
 *
 * Supplied entrySize is the size of buffer saved entries. This value can't
 * be equal to zero.
 * For exemple, if buffer will contain integers, supplied size must be
 * sizeof(int).
 *
 * Created object must be passed to #destroyCircularBuffer() after use.
 */
CircularBufferPtr createCircularBuffer(
  size_t entrySize
);

/** \~english
 * \brief Release all memory allocation done by #createCircularBuffer().
 *
 * \param buf Circulat buffer object to free.
 */
static inline void destroyCircularBuffer(
  CircularBufferPtr buf
)
{
  if (NULL == buf)
    return;

  free(buf->array);
  free(buf);
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
  CircularBufferPtr buf
);

static inline size_t getNbEntriesCircularBuffer(
  CircularBufferPtr buf
)
{
  assert(NULL != buf);

  return buf->usedSize;
}

static inline void * getEntryCircularBuffer(
  CircularBufferPtr buf,
  size_t index
)
{
  size_t entry;

  assert(NULL != buf);

  if (buf->usedSize <= index)
    return NULL; /* Out of circular buffer index. */

  entry = (buf->top + index) & ((1 << buf->allocatedSizeShift) - 1);

  return (void *) (buf->array + entry * buf->entrySize);
}

/** \~english
 * \brief Pop first entry from supplied circular buffer.
 *
 * \param buf Target circulat buffer.
 * \param entry Suppressed entry returning pointer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Returned entry is the oldest inserted value (First In First Out).
 * If buffer is empty, an error is returned.
 */
static inline int popCircularBuffer(
  CircularBufferPtr buf,
  void ** entry
)
{
  assert(NULL != buf);

  if (!buf->usedSize)
    return -1; /* Empty circular buffer. */

  if (NULL != entry)
    *entry = (void *) (buf->array + buf->top * buf->entrySize);

  buf->top = (buf->top + 1) & ((1 << buf->allocatedSizeShift) - 1);
  buf->usedSize--;
  return 0;
}

#endif