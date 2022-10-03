#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "bufferingModel.h"

const char * predefinedBufferTypesName(BufModelBufferName name)
{
  switch (name) {
    /* Systems */
    case TRANSPORT_BUFFER:
      return "TB";
    case MAIN_BUFFER:
      return "B";
    case MULTIPLEX_BUFFER:
      return "MB";
    case ELEMENTARY_BUFFER:
      return "EB";

    case CUSTOM_BUFFER:
      break;
  }

  return NULL;
}

bool isLinkedBufModelNode(
  const BufModelNode node
)
{
  switch (node.type) {
    case NODE_VOID:
      break;

    case NODE_BUFFER:
      return node.linkedElement.buffer->header.isLinked;

    case NODE_FILTER:
      return node.linkedElement.filter->isLinked;
  }

  return false;
}

void setLinkedBufModelNode(
  BufModelNode node
)
{
  switch (node.type) {
    case NODE_VOID:
      break;

    case NODE_BUFFER:
      node.linkedElement.buffer->header.isLinked = true;
      break;

    case NODE_FILTER:
      node.linkedElement.filter->isLinked = true;
  }
}

bool checkBufModelNode(
  BufModelNode node,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  switch (node.type) {
    case NODE_VOID:
      break; /* Data sent to void */

    case NODE_BUFFER:
      return checkBufModelBuffer(
        node.linkedElement.buffer,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext
      );

    case NODE_FILTER:
      return checkBufModelFilter(
        node.linkedElement.filter,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext
      );
  }

  return true;
}

int updateBufModelNode(
  BufModelNode node,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  switch (node.type) {
    case NODE_VOID:
      if (0 < inputData)
        LIBBLU_DEBUG_COM("Discarding %zu bytes.\n", inputData);
      break;

    case NODE_BUFFER:
      return updateBufModelBuffer(
        node.linkedElement.buffer,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext
      );

    case NODE_FILTER:
      return updateBufModelFilter(
        node.linkedElement.filter,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext
      );
  }

  return 0;
}

void destroyBufModelBuffer(
  BufModelBufferPtr buf
)
{
  if (NULL == buf)
    return;

  switch (buf->header.type) {
    case LEAKING_BUFFER:
      return destroyLeakingBuffer((BufModelLeakingBufferPtr) buf);

    case TIME_REMOVAL_BUFFER:
      return destroyRemovalBuffer((BufModelRemovalBufferPtr) buf);
  }
}

bool matchBuffer(
  const BufModelBufferPtr buf,
  const BufModelBufferName name,
  const char * customName,
  uint32_t customNameHash
)
{
  assert(NULL != buf);

  if (buf->header.param.name != name)
    return false;

  if (buf->header.param.name == CUSTOM_BUFFER) {
    if (NULL == customName) {
      LIBBLU_ERROR(
        "Unexpected NULL buffer name string as matchBuffer() argument."
      );
      exit(-1);
    }

    if (!customNameHash)
      customNameHash = fnv1aStrHash(customName);

    return
      buf->header.param.customNameHash == customNameHash
      && 0 == strcmp(buf->header.param.customName, customName)
    ;
  }

  return true;
}

int setBufferOutput(
  BufModelNode buffer,
  BufModelNode output
)
{
  if (!BUF_MODEL_NODE_IS_BUFFER(buffer))
    LIBBLU_ERROR_RETURN("Expect a buffer as destination setBufferOutput().\n");

  if (isLinkedBufModelNode(output))
    LIBBLU_ERROR_RETURN(
      "Unable to use specified buffer output node, "
      "it is has already been linked to another route.\n"
    );
  setLinkedBufModelNode(output);

  buffer.linkedElement.buffer->header.output = output;
  return 0;
}

int addFrameToBuffer(
  BufModelBufferPtr buf,
  BufModelBufferFrame frame
)
{
  BufModelBufferFrame * newFrame;

  if (NULL == buf)
    LIBBLU_ERROR_RETURN("NULL pointer buffer on addFrameToBuffer().\n");

  newFrame = (BufModelBufferFrame *) newEntryCircularBuffer(
    ((BufModelLeakingBufferPtr) buf)->header.storedFrames
  );
  if (NULL == newFrame)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  *newFrame = frame;
  return 1;
}

static int computeBufferDataInput(
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  size_t * resultInputBandwidth,
  size_t * resultBufferPendingData
)
{
  uint64_t elapsedTime;
  size_t bufferInputData, inputDataBandwidth;

  assert(buf->header.lastUpdate <= timestamp);
  elapsedTime = timestamp - buf->header.lastUpdate;

  /* LIBBLU_DEBUG_COM(
    "Compute data input from buffer %s with %" PRIu64 " ticks elapsed.\n",
    BUFFER_NAME(buf), elapsedTime
  ); */

  bufferInputData = buf->header.bufferInputData;

  /* Insert data in buffer */
  if (buf->header.param.instantFilling) {
    inputDataBandwidth = bufferInputData + inputData;
    bufferInputData = 0;
  }
  else {
    bufferInputData += inputData;

    inputDataBandwidth = MIN(
      bufferInputData,
      elapsedTime * fillingBitrate
    );
    bufferInputData -= inputDataBandwidth;
  }

  if (NULL != resultInputBandwidth)
    *resultInputBandwidth = inputDataBandwidth;
  if (NULL != resultBufferPendingData)
    *resultBufferPendingData = bufferInputData;

  return 0;
}

static int computeBufferDataOutput(
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t fillingLevel,
  uint64_t fillingBitrate,
  void * streamContext,
  size_t * resultOutputBandwidth
)
{
  size_t dataBandwidth, outputBufferFillingLevel;
  BufModelBufferPtr outputBuf;

  switch (buf->header.type) {
    case LEAKING_BUFFER:
      /* LIBBLU_DEBUG_COM(
        "Compute data output from buffer %s with %" PRIu64 " ticks elapsed "
        "(removal bitrate: %" PRIu64 " bps %f bptick).\n",
        BUFFER_NAME(buf),
        timestamp - buf->header.lastUpdate,
        ((BufModelLeakingBufferPtr) buf)->removalBitratePerSec,
        ((BufModelLeakingBufferPtr) buf)->removalBitrate
      ); */

      dataBandwidth = computeLeakingBufferDataBandwidth(
        (BufModelLeakingBufferPtr) buf,
        fillingLevel,
        timestamp
      );
      break;

    case TIME_REMOVAL_BUFFER:
      dataBandwidth = computeRemovalBufferDataBandwidth(
        (BufModelRemovalBufferPtr) buf, timestamp
      );
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unable to update buffer model, unknown buffer type."
      );
  }

  if (buf->header.param.dontOverflowOutput) {
    /* Update now output buffer */
    if (BUF_MODEL_NODE_IS_VOID(buf->header.output))
      outputBufferFillingLevel = dataBandwidth;
    else {
      if (!BUF_MODEL_NODE_IS_BUFFER(buf->header.output))
        LIBBLU_ERROR_RETURN(
          "Controlled output buffer from %s must be linked to another "
          "buffer in output (output node type value: %d).",
          BUFFER_NAME(buf), buf->header.output.type
        );

      if (
        updateBufModelNode(
          buf->header.output, timestamp, 0, fillingBitrate, streamContext
        ) < 0
      )
        return -1;

      outputBuf = buf->header.output.linkedElement.buffer;

      outputBufferFillingLevel =
        outputBuf->header.param.bufferSize
        - outputBuf->header.bufferFillingLevel
      ;
    }

    dataBandwidth = MIN(
      dataBandwidth,
      outputBufferFillingLevel
    );
  }

  if (NULL != resultOutputBandwidth)
    *resultOutputBandwidth = dataBandwidth;

  return 0;
}

uint64_t computeLeakingBufferFlushingDuration(
  BufModelLeakingBufferPtr buf,
  size_t inputData
)
{
  return ceil(inputData / buf->removalBitrate);
}

uint64_t computeRemovalBufferFlushingDuration(
  BufModelRemovalBufferPtr buf,
  uint64_t timestamp,
  size_t inputData
)
{
  uint64_t minDuration;
  size_t nbFrames, frameIdx;
  BufModelBufferFrame * frame;

  minDuration = 0;
  nbFrames = getNbEntriesCircularBuffer(buf->header.storedFrames);
  for (frameIdx = 0; frameIdx < nbFrames && 0 < inputData; frameIdx++) {
    frame = (BufModelBufferFrame *) getEntryCircularBuffer(
      buf->header.storedFrames, frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unable to extract frame, broken circular buffer.\n"
      );

    inputData -= MIN(inputData, frame->headerSize + frame->dataSize);
    if (timestamp < frame->removalTimestamp)
      minDuration = frame->removalTimestamp - timestamp;
  }

  return minDuration;
}

uint64_t computeMinFlushingDurationBufModelBuffer(
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t inputData
)
{
  uint64_t requiredDuration;

  switch (buf->header.type) {
    case LEAKING_BUFFER:
      requiredDuration = computeLeakingBufferFlushingDuration(
        (BufModelLeakingBufferPtr) buf,
        inputData
      );
      break;

    case TIME_REMOVAL_BUFFER:
      requiredDuration = computeRemovalBufferFlushingDuration(
        (BufModelRemovalBufferPtr) buf,
        timestamp,
        inputData
      );
      break;

    default:
      requiredDuration = 0;
      LIBBLU_ERROR_RETURN(
        "Unable to update buffer model, unknown buffer type."
      );
  }

  return requiredDuration;
}

bool checkBufModelBuffer(
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  size_t fillingLevel, inputDataBandwidth, outputDataBandwidth;
  bool ret;

  if (
    computeBufferDataInput(
      buf, timestamp, inputData, fillingBitrate,
      &inputDataBandwidth, NULL
    ) < 0
  )
    return true;

  fillingLevel = buf->header.bufferFillingLevel + inputDataBandwidth;

  /* Compute how many time elapsed since last update and current packet
  transfer finish. */
  if (
    computeBufferDataOutput(
      buf, timestamp, fillingLevel,
      fillingBitrate, streamContext,
      &outputDataBandwidth
    ) < 0
  )
    return true;

  /* assert(outputDataBandwidth <= fillingLevel); */

  if (
    !checkBufModelNode(
      buf->header.output,
      timestamp,
      outputDataBandwidth,
      fillingBitrate,
      streamContext
    )
  )
    return false;

  ret = (
    outputDataBandwidth > fillingLevel
    || (fillingLevel - outputDataBandwidth) <= buf->header.param.bufferSize
  );
  if (!ret)
    LIBBLU_T_STD_VERIF_TEST_DEBUG(
      "Full buffer (%zu - %zu = %zu < %zu bits).\n",
      fillingLevel,
      outputDataBandwidth,
      fillingLevel - outputDataBandwidth,
      buf->header.param.bufferSize
    );

#if 0
  if (!ret && NULL != minimalRequiredFlushingTime) {
    /* Compute hypothetical required flushing duration. */
    *minimalRequiredFlushingTime = computeMinFlushingDurationBufModelBuffer(
      buf, timestamp,
      (fillingLevel - outputDataBandwidth) - buf->header.param.bufferSize
    );
  }
#endif

  return ret;
}

int updateBufModelBuffer(
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  int ret;

  size_t inputPendingData, dataBandwidth;
  size_t removedData, emitedData;

  size_t frameIdx;
  BufModelBufferFrame * frame;

  assert(buf->header.lastUpdate <= timestamp);

  if (
    computeBufferDataInput(
      buf, timestamp, inputData, fillingBitrate,
      &dataBandwidth, &inputPendingData
    ) < 0
  )
    return -1;

  /* LIBBLU_DEBUG_COM(
    "%zu bytes enter %s (remain %zu bytes pending).\n",
    dataBandwidth,
    BUFFER_NAME(buf),
    inputPendingData
  ); */

  buf->header.bufferFillingLevel += dataBandwidth;
  buf->header.bufferInputData = inputPendingData;

  /* Compute how many time elapsed since last update and current packet
  transfer finish. */
  if (
    computeBufferDataOutput(
      buf, timestamp, buf->header.bufferFillingLevel,
      fillingBitrate, streamContext,
      &dataBandwidth
    ) < 0
  )
    return -1;

  /* Check if output data is greater than buffer occupancy. */
  if (buf->header.bufferFillingLevel < dataBandwidth) {
    LIBBLU_ERROR_RETURN(
      "Buffer underflow (%zu < %zu) at %" PRIu64 ".\n",
      buf->header.bufferFillingLevel,
      dataBandwidth,
      timestamp
    );
  }

  /* Remove ouput data bandwidth */
  buf->header.bufferFillingLevel -= MIN(
    buf->header.bufferFillingLevel,
    dataBandwidth
  );

  /* LIBBLU_DEBUG_COM(
    "%zu bytes removed from %s.\n",
    dataBandwidth, BUFFER_NAME(buf)
  ); */

  /* Check if buffer occupancy after update is greater than its size. */
  if (buf->header.param.bufferSize < buf->header.bufferFillingLevel) {
    /* buf Overflow, this shall never happen */
    LIBBLU_ERROR_RETURN(
      "Buffer overflow happen in %s (%zu < %zu).\n",
      BUFFER_NAME(buf),
      buf->header.param.bufferSize,
      buf->header.bufferFillingLevel
    );
  }

  /* Set update time */
  buf->header.lastUpdate = timestamp;

  /* Remove bandwidth from buffer stored frames */
  /* And compute emited data to following buffer */
  emitedData = 0;

  for (frameIdx = 0; 0 < dataBandwidth; frameIdx++) {
    frame = (BufModelBufferFrame *) getEntryCircularBuffer(
      buf->header.storedFrames, frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unexpected data in buffer %s, "
        "buf->bufferFillingLevel shall be lower or equal to "
        "the sum of all frames headerSize + removedData (extra %zu bytes).\n",
        BUFFER_NAME(buf),
        dataBandwidth
      );

    removedData = MIN(frame->headerSize, dataBandwidth);
    frame->headerSize = frame->headerSize - removedData;
    dataBandwidth -= removedData;

    removedData = MIN(frame->dataSize, dataBandwidth);
    frame->dataSize = frame->dataSize - removedData;
    emitedData += (0 < frame->outputDataSize) ?
      frame->outputDataSize : removedData
    ;
    dataBandwidth -= removedData;
  }

  /* Transfer data to output buffer and update it */
  if (!BUF_MODEL_NODE_IS_VOID(buf->header.output)) {
    /* LIBBLU_DEBUG_COM(
      "Emit %zu bytes from %s.\n",
      emitedData, BUFFER_NAME(buf)
    ); */

    ret = updateBufModelNode(
      buf->header.output,
      timestamp,
      emitedData,
      fillingBitrate,
      streamContext
    );
    if (ret < 0)
      return -1;
  }
  else if (0 < emitedData) {
    /* Otherwise, no output buffer, data is discarted */
    /* LIBBLU_DEBUG_COM(
      "Discard %zu bytes at output of %s.\n",
      emitedData, BUFFER_NAME(buf)
    ); */
  }

  /* Delete empty useless frames */
  for (
    frameIdx = 0;
    frameIdx < getNbEntriesCircularBuffer(buf->header.storedFrames);
    frameIdx++
  ) {
    frame = (BufModelBufferFrame *) getEntryCircularBuffer(
      buf->header.storedFrames, frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN("No frame to get, broken circular buffer.\n");

    if (!frame->headerSize && !frame->dataSize) {
      /* Frame header and payload has already been fully transfered */
      if (popCircularBuffer(buf->header.storedFrames, NULL) < 0)
        LIBBLU_ERROR_RETURN("Unable to pop, broken circular buffer.\n");
    }
    else
      break; /* Frame is not empty, following ones too, stop here */
  }

  return 0;
}

BufModelBuffersListPtr createBufModelBuffersList(void)
{
  BufModelBuffersListPtr list;

  if (NULL == (list = (BufModelBuffersListPtr) malloc(sizeof(BufModelBuffersList))))
    return NULL;

  list->nbAllocatedBuffers = list->nbUsedBuffers = 0;
  list->buffers = NULL;

  return list;
}

void destroyBufModelBuffersList(
  BufModelBuffersListPtr list
)
{
  if (NULL == list)
    return;

  free(list->buffers);
  free(list);
}

int addBufferBufModelBuffersList(
  BufModelBuffersListPtr list,
  BufModelBufferPtr buf
)
{
  BufModelBufferPtr * newArray;
  unsigned newLength;

  assert(NULL != list);
  assert(NULL != buf);

  if (list->nbAllocatedBuffers <= list->nbUsedBuffers) {
    /* Need realloc */
    newLength = GROW_BUF_MODEL_BUFFERS_LIST_LENGTH(
      list->nbAllocatedBuffers
    );

    if (lb_mul_overflow(newLength, sizeof(BufModelBufferPtr)))
      LIBBLU_ERROR_RETURN(
        "Buffers list array length value overflow, array is too big.\n"
      );

    newArray = (BufModelBufferPtr *) realloc(
      list->buffers,
      newLength * sizeof(BufModelBufferPtr)
    );
    if (NULL == newArray)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    list->buffers = newArray;
    list->nbAllocatedBuffers = newLength;
  }

  list->buffers[list->nbUsedBuffers++] = buf;

  return 0;
}

int addFrameToBufferFromList(
  BufModelBuffersListPtr bufList,
  BufModelBufferName name,
  const char * customBufName,
  BufModelBufferFrame frame
)
{
  unsigned i;
  uint32_t customNameHash;

  assert(NULL != bufList);
  assert(0 < bufList->nbUsedBuffers);

  if (NULL != customBufName)
    customNameHash = fnv1aStrHash(customBufName);
  else
    customNameHash = 0;

  for (i = 0; i < bufList->nbUsedBuffers; i++) {
    if (matchBuffer(bufList->buffers[i], name, customBufName, customNameHash)) {
      /* Found buffer */
      return addFrameToBuffer(
        bufList->buffers[i],
        frame
      );
    }
  }

  LIBBLU_ERROR_RETURN(
    "Unknown destination buffer %s addFrameToBufferFromList().\n",
    predefinedBufferTypesName(name)
  );
}

BufModelBufferPtr createLeakingBuffer(
  BufModelBufferParameters param,
  uint64_t initialTimestamp,
  uint64_t removalBitrate
)
{
  BufModelLeakingBufferPtr buf;
  CircularBufferPtr frmBuf;

  if (NULL == (buf = (BufModelLeakingBufferPtr) malloc(sizeof(BufModelLeakingBuffer))))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  buf->header.type = LEAKING_BUFFER;
  buf->header.param = param;
  buf->header.output = BUF_MODEL_NEW_NODE();
  buf->header.isLinked = false;
  buf->header.bufferInputData = 0;
  buf->header.bufferFillingLevel = 0;
  buf->header.lastUpdate = initialTimestamp;
  buf->header.storedFrames = NULL;
  buf->removalBitratePerSec = removalBitrate;
  buf->removalBitrate = (double) removalBitrate / MAIN_CLOCK_27MHZ;

  if (NULL == (frmBuf = createCircularBuffer(sizeof(BufModelBufferFrame))))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  buf->header.storedFrames = frmBuf;

  return (BufModelBufferPtr) buf;

free_return:
  destroyLeakingBuffer(buf);
  return NULL;
}

int createLeakingBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode * node,
  BufModelBufferParameters param,
  uint64_t initialTimestamp,
  uint64_t removalBitrate
)
{
  BufModelBufferPtr buf;

  assert(NULL != node);

  buf = createLeakingBuffer(param, initialTimestamp, removalBitrate);
  if (NULL == buf)
    return -1;

  if (addBufferBufModelBuffersList(list, buf) < 0)
    return -1;

  node->type = NODE_BUFFER;
  node->linkedElement.buffer = buf;

  return 0;
}

void destroyLeakingBuffer(BufModelLeakingBufferPtr buf)
{
  if (NULL == buf)
    return;

  destroyBufModel(buf->header.output);
  destroyCircularBuffer(buf->header.storedFrames);
  free(buf);
}

size_t computeLeakingBufferDataBandwidth(
  BufModelLeakingBufferPtr buf,
  size_t bufFillingLevel,
  uint64_t timestamp
)
{
  assert(NULL != buf);

  return MIN(
    bufFillingLevel,
    ceil((timestamp - buf->header.lastUpdate) * buf->removalBitrate)
  );
}

BufModelBufferPtr createRemovalBuffer(
  BufModelBufferParameters param,
  uint64_t initialTimestamp
)
{
  BufModelRemovalBufferPtr buf;
  CircularBufferPtr frmBuf;

  if (NULL == (buf = (BufModelRemovalBufferPtr) malloc(sizeof(BufModelRemovalBuffer))))
    goto free_return; /* Memory allocation error */

  buf->header.type = TIME_REMOVAL_BUFFER;
  buf->header.param = param;
  buf->header.output = BUF_MODEL_NEW_NODE();
  buf->header.isLinked = false;
  buf->header.bufferInputData = 0;
  buf->header.bufferFillingLevel = 0;
  buf->header.lastUpdate = initialTimestamp;
  buf->header.storedFrames = NULL;

  if (NULL == (frmBuf = createCircularBuffer(sizeof(BufModelBufferFrame))))
    goto free_return; /* Memory allocation error */
  buf->header.storedFrames = frmBuf;

  return (BufModelBufferPtr) buf;

free_return:
  destroyRemovalBuffer(buf);
  return NULL;
}

int createRemovalBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode * node,
  BufModelBufferParameters param,
  uint64_t initialTimestamp
)
{
  BufModelBufferPtr buf;

  assert(NULL != node);

  buf = createRemovalBuffer(param, initialTimestamp);
  if (NULL == buf)
    return -1;

  if (addBufferBufModelBuffersList(list, buf) < 0)
    return -1;

  node->type = NODE_BUFFER;
  node->linkedElement.buffer = buf;

  return 0;
}

void destroyRemovalBuffer(BufModelRemovalBufferPtr buf)
{
  if (NULL == buf)
    return;

  destroyBufModel(buf->header.output);
  destroyCircularBuffer(buf->header.storedFrames);
  free(buf);
}

size_t computeRemovalBufferDataBandwidth(
  BufModelRemovalBufferPtr buf,
  uint64_t timestamp
)
{
  size_t dataLength, nbFrames, frameIdx;
  BufModelBufferFrame * frame;

  dataLength = 0;
  nbFrames = getNbEntriesCircularBuffer(buf->header.storedFrames);
  for (frameIdx = 0; frameIdx < nbFrames; frameIdx++) {
    frame = (BufModelBufferFrame *) getEntryCircularBuffer(
      buf->header.storedFrames, frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unable to extract frame, broken circular buffer.\n"
      );

    if (frame->removalTimestamp <= timestamp)
      dataLength += frame->headerSize + frame->dataSize;
    else
      break; /* Unreached timestamp, no more frames. */

    LIBBLU_T_STD_VERIF_TEST_DEBUG(
      "%zu) %zu/%zu - %zu bits.\n",
      frameIdx, frame->removalTimestamp, timestamp, frame->dataSize
    );
  }
  if (0 < dataLength)
    LIBBLU_T_STD_VERIF_TEST_DEBUG("=> %zu bits.\n", dataLength);

  return dataLength;
}

const char * getBufModelFilterLblTypeString(
  const BufModelFilterLblType type
)
{
  switch (type) {
    case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
      return "numeric";

    case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
      return "string";

    case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
      return "list";
  }

  return "unknown";
}

void cleanBufModelFilterLblValue(
  BufModelFilterLblValue val,
  BufModelFilterLblType type
)
{
  unsigned i;

  switch (type) {
    case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
      break;

    case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
      free(val.string);
      break;

    case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
      for (i = 0; i < val.listLength; i++)
        cleanBufModelFilterLblValue(val.list[i], val.listItemsType);
      free(val.list);
      break;
  }
}

bool isInListBufModelFilterLblValue(
  BufModelFilterLblValue list,
  BufModelFilterLblValue value
)
{
  unsigned i;

  for (i = 0; i < list.listLength; i++) {
    if (
      areEqualBufModelFilterLblValues(
        list.list[i], value, list.listItemsType
      )
    )
      return true;
  }

  return false;
}

bool areEqualBufModelFilterLblValues(
  BufModelFilterLblValue left,
  BufModelFilterLblValue right,
  BufModelFilterLblType type
)
{
  switch (type) {
    case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
      return left.number == right.number;

    case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
      return
        left.stringHash == right.stringHash
        && !strcmp(left.string, right.string)
      ;

    case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
      return isInListBufModelFilterLblValue(
        left, right
      );
  }

  /* Shall never happen */
  LIBBLU_ERROR("Unexpected buffering model filter label type %u.\n", type);
  exit(-1);

  return false;
}

void cleanBufModelFilterLbl(
  BufModelFilterLbl label
)
{
  cleanBufModelFilterLblValue(
    label.value,
    label.type
  );
}

int setBufModelFilterLblList(
  BufModelFilterLbl * dst,
  BufModelFilterLblType valuesType,
  const void * valuesList,
  const unsigned valuesNb
)
{
  unsigned i;
  BufModelFilterLblValue * array;

  /* Check parameters */
  if (NULL == dst)
    LIBBLU_ERROR_RETURN("NULL pointer filter label list destination.\n");
  if (NULL == valuesList)
    LIBBLU_ERROR_RETURN("NULL pointer filter label list source values.\n");
  if (!valuesNb)
    LIBBLU_ERROR_RETURN("Zero-sized list label.\n");

  /* Allocate values array */
  array = (BufModelFilterLblValue *) malloc(
    valuesNb * sizeof(BufModelFilterLblValue)
  );
  if (NULL == array)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  /* Build values array */
  switch (valuesType) {
    case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
      for (i = 0; i < valuesNb; i++)
        array[i] = BUF_MODEL_FILTER_NUMERIC_LABEL_VALUE(
          ((int *) valuesList)[i]
        );
      break;

    case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
      for (i = 0; i < valuesNb; i++) {
        array[i] = BUF_MODEL_FILTER_STRING_LABEL_VALUE(
          ((char **) valuesList)[i]
        );

        if (NULL == array[i].string) {
          /* Release already allocated memory */
          while (i--)
            cleanBufModelFilterLblValue(array[i], valuesType);
          LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
        }
      }
      break;

    default:
      LIBBLU_ERROR_FRETURN(
        "Unexpected list label values type '%s', "
        "expect numeric or string value type.",
        getBufModelFilterLblTypeString(valuesType)
      );
  }

  *dst = BUF_MODEL_FILTER_LIST_LABEL(
    valuesType,
    array,
    valuesNb
  );

  return 0;

free_return:
  free(array);
  return -1;
}

bool areEqualBufModelFilterLbls(
  BufModelFilterLbl left,
  BufModelFilterLbl right
)
{
  bool ret;
  unsigned i;

  if (left.type == BUF_MODEL_FILTER_LABEL_TYPE_LIST) {
    if (
      left.value.listItemsType != right.type
      && (
        right.type != BUF_MODEL_FILTER_LABEL_TYPE_LIST
        || left.value.listItemsType != right.value.listItemsType
      )
    )
      return false;

    for (i = 0; i < left.value.listLength; i++) {
      ret = areEqualBufModelFilterLblValues(
        right.value, left.value.list[i], right.type
      );
      if (ret)
        return true;
    }

    return false;
  }

  if (left.type != right.type)
    return false;
  return areEqualBufModelFilterLblValues(
    left.value,
    right.value,
    left.type
  );
}

BufModelFilterPtr createBufModelFilter(
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labelsType
)
{
  BufModelFilterPtr filter;

  if (NULL == fun)
    LIBBLU_ERROR_NRETURN(
      "createBufModelFilter() receive NULL decision function pointer.\n"
    );

  if (labelsType == BUF_MODEL_FILTER_LABEL_TYPE_LIST)
    LIBBLU_ERROR_NRETURN(
      "createBufModelFilter() labels type cannot be list, "
      "use list content type.\n"
    );

  if (NULL == (filter = (BufModelFilterPtr) malloc(sizeof(BufModelFilter))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  filter->isLinked = false;
  filter->nodes = NULL;
  filter->labels = NULL;
  filter->nbAllocatedNodes = filter->nbUsedNodes = 0;
  filter->labelsType = labelsType;
  filter->apply = fun;

  return filter;
}

int createBufModelFilterNode(
  BufModelNode * node,
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labelsType
)
{
  BufModelFilterPtr filter;

  assert(NULL != node);

  if (NULL == (filter = createBufModelFilter(fun, labelsType)))
    return -1;

  node->type = NODE_FILTER;
  node->linkedElement.filter = filter;

  return 0;
}

void destroyBufModelFilter(
  BufModelFilterPtr filter
)
{
  unsigned i;

  if (NULL == filter)
    return;

  for (i = 0; i < filter->nbUsedNodes; i++) {
    cleanBufModelNode(filter->nodes[i]);
    cleanBufModelFilterLbl(filter->labels[i]);
  }
  free(filter->nodes);
  free(filter->labels);
  free(filter);
}

int addNodeToBufModelFilter(
  BufModelFilterPtr filter,
  BufModelNode node,
  BufModelFilterLbl label
)
{
  unsigned i;

  BufModelNode * newArrayNodes;
  BufModelFilterLbl * newArrayLabels;
  unsigned newLength;

  assert(NULL != filter);

  if (isLinkedBufModelNode(node))
    LIBBLU_ERROR_RETURN(
      "Unable to use specified filter output node, "
      "it is has already been linked to another route.\n"
    );
  setLinkedBufModelNode(node);

  if (
    filter->labelsType != label.type
    && (
      label.type != BUF_MODEL_FILTER_LABEL_TYPE_LIST
      || filter->labelsType != label.value.listItemsType
    )
  )
    LIBBLU_ERROR_RETURN(
      "New filter node label type mismatch with buffer one (%s <> %s).\n",
      getBufModelFilterLblTypeString(label.type),
      getBufModelFilterLblTypeString(filter->labelsType)
    );

  if (filter->nbUsedNodes <= filter->nbAllocatedNodes) {
    /* Need realloc */
    newLength = GROW_BUF_MODEL_FILTER_LENGTH(
      filter->nbAllocatedNodes
    );

    if (lb_mul_overflow(newLength, sizeof(BufModelNode)))
      LIBBLU_ERROR_RETURN(
        "Buffering model filter output nodes overflow. "
        "Too many defined output.\n"
      );

    newArrayNodes = (BufModelNode *) realloc(
      filter->nodes,
      newLength * sizeof(BufModelNode)
    );
    if (NULL == newArrayNodes)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    newArrayLabels = (BufModelFilterLbl *) realloc(
      filter->labels,
      newLength * sizeof(BufModelFilterLbl)
    );
    if (NULL == newArrayLabels) {
      free(newArrayLabels);
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    }

    filter->nodes = newArrayNodes;
    filter->labels = newArrayLabels;
    filter->nbAllocatedNodes = newLength;
  }

  /* Check for label duplicates */
  for (i = 0; i < filter->nbUsedNodes; i++) {
    if (areEqualBufModelFilterLbls(filter->labels[i], label))
      LIBBLU_ERROR_RETURN(
        "Buffering model filter duplicate labels.\n"
      );
  }

  filter->labels[filter->nbUsedNodes] = label;
  filter->nodes[filter->nbUsedNodes++] = node;

  return 0;
}

int addNodeToBufModelFilterNode(
  BufModelNode filterNode,
  BufModelNode node,
  BufModelFilterLbl label
)
{
  assert(BUF_MODEL_NODE_IS_FILTER(filterNode));

  return addNodeToBufModelFilter(
    filterNode.linkedElement.filter,
    node,
    label
  );
}

bool checkBufModelFilter(
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  unsigned destIndex;

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  bool ret;
  unsigned i;

  BufModelNode * nodes;
  unsigned nbUsedNodes;
#endif

  assert(NULL != filter);
  assert(NULL != filter->apply);

  if (0 < inputData) {
    if (filter->apply(filter, &destIndex, streamContext) < 0)
      LIBBLU_ERROR_BRETURN(
        "Buffering model filter apply() method returns error.\n"
      );

    if (filter->nbUsedNodes <= destIndex)
      LIBBLU_ERROR_BRETURN(
        "Buffering model filter method returns destination index %u out of "
        "filter destination array of length %u.\n",
        destIndex,
        filter->nbUsedNodes
      );
  }
  else
#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
    destIndex = filter->nbUsedNodes; /* Set impossible index */
#else
    return true; /* No data to transmit, useless decision. */
#endif

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  nbUsedNodes = filter->nbUsedNodes;
  nodes = filter->nodes;

  for (i = 0; i < nbUsedNodes; i++) {
    /* Only transmit inputData to destination node */
    ret = checkBufModelNode(
      nodes[i],
      timestamp,
      (i == destIndex) ? inputData : 0,
      fillingBitrate,
      streamContext
    );

    if (!ret)
      return false;
  }

  return true;
#else
  return checkBufModelNode(
    filter->nodes[destIndex],
    timestamp,
    inputData,
    fillingBitrate,
    streamContext
  );
#endif
}

int updateBufModelFilter(
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  unsigned destIndex;

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  int ret;
  unsigned i;

  BufModelNode * nodes;
  unsigned nbUsedNodes;
#endif

  assert(NULL != filter);
  assert(NULL != filter->apply);

  if (0 < inputData) {
    if (filter->apply(filter, &destIndex, streamContext) < 0)
      LIBBLU_ERROR_RETURN(
        "Buffering model filter apply() method returns error.\n"
      );

    if (filter->nbUsedNodes <= destIndex)
      LIBBLU_ERROR_RETURN(
        "Buffering model filter method returns destination index %u out of "
        "filter destination array of length %u.\n",
        destIndex,
        filter->nbUsedNodes
      );
  }
  else
#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
    destIndex = filter->nbUsedNodes; /* Set impossible index */
#else
    return 0; /* No data to transmit, useless decision. */
#endif

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  nbUsedNodes = filter->nbUsedNodes;
  nodes = filter->nodes;

  for (i = 0; i < nbUsedNodes; i++) {
    /* Only transmit inputData to destination node */
    ret = updateBufModelNode(
      nodes[i],
      timestamp,
      (i == destIndex) ? inputData : 0,
      fillingBitrate,
      streamContext
    );

    if (ret < 0)
      return -1;
  }

  return 0;
#else
  return updateBufModelNode(
    filter->nodes[destIndex],
    timestamp, inputData, fillingBitrate, streamContext
  );
#endif
}

int checkBufModel(
  BufModelNode rootNode,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  return checkBufModelNode(
    rootNode,
    timestamp,
    inputData,
    fillingBitrate,
    streamContext
  );
}

int updateBufModel(
  BufModelNode rootNode,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
)
{
  LIBBLU_DEBUG_COM(
    "Updating buffering chain at %" PRIu64 " with %zu bytes.\n",
    timestamp, inputData
  );

  return updateBufModelNode(
    rootNode,
    timestamp,
    inputData,
    fillingBitrate,
    streamContext
  );
}

void destroyBufModel(
  BufModelNode rootNode
)
{
  return cleanBufModelNode(rootNode);
}

void printBufModelBuffer(const BufModelBufferPtr buf)
{
  lbc_printf(
    "[%s %" PRIu64 " / %" PRIu64 "]",
    BUFFER_NAME(buf),
    buf->header.bufferFillingLevel,
    buf->header.param.bufferSize
  );

  if (buf->header.type == LEAKING_BUFFER)
    lbc_printf(" -/-> ");
  else
    lbc_printf(" -|-> ");
  printBufModelBufferingChain(buf->header.output);
}

void printBufModelFilterLbl(const BufModelFilterLbl label)
{
  unsigned i;

  switch (label.type) {
    case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
      lbc_printf("0x%X", label.value.number);
      break;

    case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
      lbc_printf("%s", label.value.string);
      break;

    case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
      lbc_printf("[");
      for (i = 0; i < label.value.listLength; i++) {
        if (0 < i)
          lbc_printf(", ");

        printBufModelFilterLbl(
          (BufModelFilterLbl) {
            .type = label.value.listItemsType,
            .value = label.value.list[i]
          }
        );
      }
      lbc_printf("]");
  }
}

void printBufModelFilter(const BufModelFilterPtr filter)
{
  unsigned i;

  lbc_printf("Filter {\n");
  for (i = 0; i < filter->nbUsedNodes; i++) {
    lbc_printf(" - ");
    printBufModelFilterLbl(filter->labels[i]);
    lbc_printf(" -> ");
    printBufModelBufferingChain(filter->nodes[i]);
  }
  lbc_printf("}\n");
}

void printBufModelBufferingChain(const BufModelNode node)
{
  switch (node.type) {
    case NODE_VOID:
      lbc_printf("*void*\n");
      break;

    case NODE_BUFFER:
      printBufModelBuffer(node.linkedElement.buffer);
      break;

    case NODE_FILTER:
      printBufModelFilter(node.linkedElement.filter);
      break;
  }
}

#if 0

int main(void)
{
  BufModelBuffersListPtr bufList;
  BufModelBufferParameters bufParam;
  BufModelNode node = BUF_MODEL_NEW_NODE();
  BufModelBufferFrame frameParam;

  bufParam = (BufModelBufferParameters) {
    .name = TRANSPORT_BUFFER,
    .instantFilling = false,
    .dontOverflowOutput = false,
    .bufferSize = 512*8
  };

  frameParam = (BufModelBufferFrame) {
    .headerSize = 0,
    .dataSize = 188 * 8,
    .outputDataSize = 0,
    .doNotRemove = false
  };

  if (NULL == (bufList = createBufModelBuffersList()))
    return -1;

  if (createLeakingBufferNode(bufList, &node, bufParam, 0, 1000000) < 0)
    return -1;
  if (addFrameToBufferFromList(bufList, TRANSPORT_BUFFER, NULL, frameParam) < 0)
    return -1;

  if (updateBufModel(node, 0, 188 * 8, 48000000, NULL) < 0)
    return -1;
  if (updateBufModel(node, 846, 0, 48000000, NULL) < 0)
    return -1;

  printBufModelBufferingChain(node);

  destroyBufModel(node);
  destroyBufModelBuffersList(bufList);
  return 0;
}

#endif