#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "streamHeap.h"

static int increaseSizeStreamHeap(
  StreamHeapPtr heap
)
{
  int newSize;
  StreamHeapNode *newArray;

  assert(heap->allocatedSize <= heap->usedSize);

  newSize = GROW_ALLOCATION(heap->allocatedSize, STREAM_HEAP_DEFAULT_SIZE);
  if (newSize <= heap->usedSize)
    LIBBLU_ERROR_RETURN("Stream heap size overflow.\n");

  newArray = (StreamHeapNode *) realloc(
    heap->content,
    newSize *sizeof(StreamHeapNode)
  );
  if (NULL == newArray)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  heap->content = newArray;
  heap->allocatedSize = newSize;

  return 0;
}

StreamHeapPtr createStreamHeap(
  void
)
{
  StreamHeapPtr heap;

  if (NULL == (heap = (StreamHeapPtr) malloc(sizeof(StreamHeap))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  heap->content = NULL;
  heap->allocatedSize = heap->usedSize = 0;

  return heap;
}

void destroyStreamHeap(
  StreamHeapPtr heap
)
{
  if (NULL == heap)
    return;

  free(heap->content);
  free(heap);
}

static int cmpNodesStreamHeap(
  StreamHeapNode *left,
  StreamHeapNode *right
)
{
  assert(NULL != left);
  assert(NULL != right);

  if (left->timer.next_TP_pres_time == right->timer.next_TP_pres_time) {
    /* Same timing, check priority */
    if (left->timer.priority_lvl == right->timer.priority_lvl)
      return 0; /* Equals */
    return (left->timer.priority_lvl > right->timer.priority_lvl) ? -1 : 1;
  }

  return (left->timer.next_TP_pres_time < right->timer.next_TP_pres_time) ? -1 : 1;
}

static int childStreamHeap(
  StreamHeapPtr heap,
  int idx
)
{
  int comp;

  assert(NULL != heap);

  if (idx < 0 || heap->usedSize <= idx * 2 + 1)
    return -1; /* Out of heap */

  if (heap->usedSize <= idx * 2 + 2)
    return idx * 2 + 1;

  comp = cmpNodesStreamHeap(
    &heap->content[idx * 2 + 1],
    &heap->content[idx * 2 + 2]
  );

  return idx * 2 + ((comp >= 0) + 1);
}

static void shiftDownStreamHeap(
  StreamHeapPtr heap,
  int idx
)
{
  StreamHeapNode tmp;
  int child, parent;

  assert(NULL != heap);

  parent = idx;
  child = childStreamHeap(heap, idx);

  while (
    0 <= child
    && cmpNodesStreamHeap(
      &heap->content[child],
      &heap->content[parent]
    ) < 0
  ) {
    /* Perform swap between child and parent. */
    tmp = heap->content[child];
    heap->content[child] = heap->content[parent];
    heap->content[parent] = tmp;

    parent = child;
    child = childStreamHeap(heap, parent);
  }
}

static void shiftUpStreamHeap(
  StreamHeapPtr heap,
  int idx
)
{
  StreamHeapNode tmp;
  int child, parent;

  child = idx;
  parent = (idx - 1) / 2;

  while (
    0 < child
    && cmpNodesStreamHeap(&heap->content[child], &heap->content[parent]) < 0
  ) {
    /* Perform swap between child and parent. */
    tmp = heap->content[child];
    heap->content[child] = heap->content[parent];
    heap->content[parent] = tmp;

    child = parent;
    parent = (child - 1) / 2;
  }
}

static void editStreamHeap(
  StreamHeapPtr heap,
  int idx,
  StreamHeapTimingInfos timer,
  LibbluStreamPtr stream
)
{
  int child, parent;

  assert(NULL != heap);
  assert(NULL != stream);

  assert(0 <= idx && idx < heap->usedSize);

  child = childStreamHeap(heap, idx);
  parent = (idx - 1) / 2;

  heap->content[idx].timer = timer;
  heap->content[idx].stream = stream;

  if (
    0 < child
    && cmpNodesStreamHeap(heap->content + child, heap->content + idx) < 0
  )
    shiftDownStreamHeap(heap, idx);

  else if (
    cmpNodesStreamHeap(heap->content + idx, heap->content + parent) < 0
  )
    shiftUpStreamHeap(heap, idx);
}

int addStreamHeap(
  StreamHeapPtr heap,
  StreamHeapTimingInfos timer,
  LibbluStreamPtr stream
)
{
  assert(NULL != heap);
  assert(NULL != stream);

  if (heap->allocatedSize <= heap->usedSize) {
    /* Heap limit reached, increase heap size. */
    if (increaseSizeStreamHeap(heap) < 0)
      return -1;
  }

  /* Insert stream at queue: */
  int idx = heap->usedSize++;
  heap->content[idx].timer = timer;
  heap->content[idx].stream = stream;

  /* Shift up in heap if required: */
  shiftUpStreamHeap(heap, idx);

  return 0;
}

void extractStreamHeap(
  StreamHeapPtr heap,
  StreamHeapTimingInfos *timer,
  LibbluStreamPtr *stream
)
{
  assert(NULL != heap);

  if (heap->usedSize <= 0)
    LIBBLU_ERROR_EXIT(
      "Aborded execution, "
      "attempt to extract from a corrupted stream heap.\n"
    );

  if (NULL != timer)
    *timer = heap->content[0].timer;
  if (NULL != stream)
    *stream  = heap->content[0].stream;
  heap->usedSize--;

  if (!heap->usedSize)
    return;

  assert(0 < heap->usedSize);

  editStreamHeap(
    heap, 0,
    heap->content[heap->usedSize].timer,
    heap->content[heap->usedSize].stream
  );
}