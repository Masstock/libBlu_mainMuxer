/** \~english
 * \file streamHeap.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxer streams order computation heap module.
 *
 * To determine the order of the packets to be muxed, the streams are ordered
 * in a heap according to the timestamp of the next transport packet to be
 * injected. Timestamp calculated externally according to the individual
 * bit rate of each stream.
 */

#ifndef __LIBBLU_MUXER__STREAM_HEAP_H__
#define __LIBBLU_MUXER__STREAM_HEAP_H__

#include "stream.h"

/** \~english
 * \brief Default stream heap size.
 *
 * This value is doubled if exceeded.
 */
#define STREAM_HEAP_DEFAULT_SIZE  4

/** \~english
 * \brief Stream heap node timing informations.
 *
 * Timing values are in #MAIN_CLOCK_27MHZ ticks if unit not precised.
 */
typedef struct {
  uint64_t tsPt;   /**< Next Transport Packet presentation time.             */
  int tsPriority;  /**< Stream priority level, higher means higher priority
    than other streams sharing same tsPt value.                              */

  /* Used for computation parameters: */
  uint64_t pesDuration; /**< PES packet duration (commonly frame-rate for
    video, a fixed number of samples for audio...)                           */
  double pesNb;         /**< Number of PES frames per second. Rounded up.    */
  double bitrate;       /**< Stream bit-rate in bps (Max bitrate for VBR,
    bitrate for CBR streams).                                                */
  uint64_t pesTsNb;     /**< Maximum number of TP per PES packet.            */
  uint64_t tsDuration;  /**< Transport Packet duration
    (PES duration / nb TP per PES).                                          */
} StreamHeapTimingInfos;

static inline void incrementTPTimestampStreamHeapTimingInfos(
  StreamHeapTimingInfos * dst
)
{
  dst->tsPt += dst->tsDuration;
}

/** \~english
 * \brief Stream heap node.
 *
 */
typedef struct {
  StreamHeapTimingInfos timer;  /**< Node timing properties.                 */
  LibbluStreamPtr stream;       /**< Node attached stream.                   */
} StreamHeapNode;

/** \~english
 * \brief Stream heap structure.
 *
 */
typedef struct {
  StreamHeapNode * content;  /**< Heap content.                              */
  int usedSize;              /**< Heap used size.                            */
  int allocatedSize;         /**< heap array allocated size.                 */
} StreamHeap, *StreamHeapPtr;

/** \~english
 * \brief Create an empty stream heap.
 *
 * \return StreamHeapPtr Upon success, created object is returned. Otherwise,
 * a NULL pointer is returned.
 *
 * Created object must be released using #destroyStreamHeap().
 */
StreamHeapPtr createStreamHeap(
  void
);

/** \~english
 * \brief Release memory allocation created by #createStreamHeap().
 *
 * \param heap Heap object to free.
 */
void destroyStreamHeap(
  StreamHeapPtr heap
);

/** \~english
 * \brief Add a new node to supplied heap.
 *
 * \param heap Destination heap.
 * \param timer New node attached timing properties.
 * \param stream New node attached stream.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int addStreamHeap(
  StreamHeapPtr heap,
  StreamHeapTimingInfos timer,
  LibbluStreamPtr stream
);

/** \~english
 * \brief Return true if the heap top stream timing value as been reached.
 *
 * \param heap Heap to check.
 * \param currentStcTs Current timing value in #MAIN_CLOCK_27MHZ ticks.
 * \return true Time value reached.
 * \return false Empty heap or not reached time value.
 */
static inline bool streamIsReadyStreamHeap(
  StreamHeapPtr heap,
  uint64_t currentStcTs
)
{
  assert(NULL != heap);

  return
    (0 < heap->usedSize)
    && (heap->content[0].timer.tsPt <= currentStcTs)
  ;
}

/** \~english
 * \brief Extract the stream heap top.
 *
 * \param heap Heap to edit.
 * \param timer Extracted top node timing properties return pointer (or NULL).
 * \param stream Extracted top node attached stream return pointer (or NULL).
 *
 * The extracted node is removed from the heap.
 *
 * \warning This function MUST be used only after a control with the function
 * #streamIsReadyStreamHeap(). Otherwise this function will fail and abort
 * program execution.
 */
void extractStreamHeap(
  StreamHeapPtr heap,
  StreamHeapTimingInfos * timer,
  LibbluStreamPtr * stream
);

#endif