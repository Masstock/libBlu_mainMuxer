#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "bufferingModel.h"

static int _updateBufModelBuffer(
  BufModelOptions options,
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
);

static int _updateBufModelFilter(
  BufModelOptions options,
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
);

/** \~english
 * \brief Update buffering model from given node.
 *
 * \param rootNode Buffering model node.
 * \param timestamp Current update timestamp.
 * \param inputData Optionnal input data in bits.
 * \param fillingBitrate Input data filling bitrate.
 * \param streamContext Input data stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _updateBufModelNode(
  BufModelOptions options,
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
      return _updateBufModelBuffer(
        options,
        node.linkedElement.buffer,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext
      );

    case NODE_FILTER:
      return _updateBufModelFilter(
        options,
        node.linkedElement.filter,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext
      );
  }

  return 0;
}

void cleanBufModelNode(
  BufModelNode node
)
{
  switch (node.type) {
    case NODE_VOID:
      break;
    case NODE_BUFFER:
      return destroyBufModelBuffer(node.linkedElement.buffer);
    case NODE_FILTER:
      return destroyBufModelFilter(node.linkedElement.filter);
  }
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

int setBufferOutput(
  BufModelNode buffer,
  BufModelNode output
)
{
  if (!isBufferBufModelNode(buffer))
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
    ((BufModelLeakingBufferPtr) buf)->header.storedFrames,
    sizeof(BufModelBufferFrame)
  );
  if (NULL == newFrame)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  *newFrame = frame;
  return 1;
}

/** \~english
 * \brief Return true if given buffer name matches supplied one.
 *
 * \param buf Buffer to test.
 * \param name Pre-defined buffer name.
 * \param customName Custom buffer name string if name value is set to
 * #CUSTOM_BUFFER value (otherwise shall be NULL).
 * \param customNameHash Optionnal custom buffer name hash value (if set to
 * 0, the value is computed if required from customName).
 * \return true The buffer name matches name and/or customName.
 * \return false The buffer name does not matches.
 */
static bool _isNamesMatchBufModelBuffer(
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
    if (NULL == customName)
      LIBBLU_ERROR_EXIT(
        "Unexpected NULL buffer name string as "
        "_isNamesMatchBufModelBuffer() argument.\n"
      );

    if (!customNameHash)
      customNameHash = fnv1aStrHash(customName);

    return
      buf->header.param.customNameHash == customNameHash
      && lb_str_equal(buf->header.param.customName, customName)
    ;
  }

  return true;
}

static void _computeBufferDataInput(
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
    bufferNameBufModelBuffer(buf), elapsedTime
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
}

/** \~english
 * \brief Return leaking buffer data amount ready to be removed from buffer.
 *
 * \param buf Leaking buffer.
 * \param timestamp Current timestamp.
 * \return size_t Buffer output data bandwidth in bits.
 *
 * Evaluates leakable data from buffer at current timestamp.
 */
static size_t _computeLeakingBufferDataBandwidth(
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

/** \~english
 * \brief Return removal timestamps buffer data amount ready to be removed
 * from buffer.
 *
 * \param buf Removal timestamps based buffer.
 * \param timestamp Current timestamp.
 * \return size_t Buffer output data bandwidth in bits.
 *
 * Evaluates data from in-buffer frames width removal timestamp lower or equal
 * to current timestamp.
 */
static size_t _computeRemovalBufferDataBandwidth(
  BufModelRemovalBufferPtr buf,
  uint64_t timestamp
)
{
  size_t dataLength, nbFrames, frameIdx;
  BufModelBufferFrame * frame;

  dataLength = 0;
  nbFrames = nbEntriesCircularBuffer(buf->header.storedFrames);
  for (frameIdx = 0; frameIdx < nbFrames; frameIdx++) {
    frame = (BufModelBufferFrame *) getCircularBuffer(
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

static int _computeBufferDataOutput(
  BufModelOptions options,
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

  int ret = 0;

  switch (buf->header.type) {
    case LEAKING_BUFFER:
      dataBandwidth = _computeLeakingBufferDataBandwidth(
        (BufModelLeakingBufferPtr) buf,
        fillingLevel,
        timestamp
      );
      break;

    case TIME_REMOVAL_BUFFER:
      dataBandwidth = _computeRemovalBufferDataBandwidth(
        (BufModelRemovalBufferPtr) buf,
        timestamp
      );
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unable to update buffer model, unknown buffer type."
      );
  }

  if (buf->header.param.dontOverflowOutput) {
    /* Update now output buffer */
    if (isVoidBufModelNode(buf->header.output))
      outputBufferFillingLevel = dataBandwidth;
    else {
      if (!isBufferBufModelNode(buf->header.output))
        LIBBLU_ERROR_RETURN(
          "Controlled output buffer from %s must be linked to another "
          "buffer in output (output node type value: %d).",
          bufferNameBufModelBuffer(buf), buf->header.output.type
        );

      ret = _updateBufModelNode(
        options,
        buf->header.output,
        timestamp,
        0,
        fillingBitrate,
        streamContext
      );
      if (ret < 0)
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

  return ret;
}

static uint64_t _computeLeakingBufferDelayForBufferDataOutput(
  BufModelLeakingBufferPtr buf,
  size_t requestedOutput
)
{
  assert(NULL != buf);

  return requestedOutput / buf->removalBitrate;
}

static uint64_t _computeRemovalBufferDelayForBufferDataOutput(
  BufModelRemovalBufferPtr buf,
  size_t requestedOutput,
  uint64_t timestamp
)
{
  BufModelBufferFrame * frame;
  uint64_t delay = 0;

  size_t nbFrames = nbEntriesCircularBuffer(buf->header.storedFrames);
  for (size_t frameIdx = 0; frameIdx < nbFrames; frameIdx++) {
    frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.storedFrames, frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unable to extract frame, broken circular buffer.\n"
      );

    if (!requestedOutput)
      break;
    delay = frame->removalTimestamp - timestamp;
    requestedOutput -= MIN(requestedOutput, frame->dataSize);
  }

  return delay;
}

static uint64_t _computeDelayForBufferDataOutput(
  BufModelBufferPtr buf,
  size_t requestedOutput,
  uint64_t timestamp
)
{
  switch (buf->header.type) {
    case LEAKING_BUFFER:
      return _computeLeakingBufferDelayForBufferDataOutput(
        (BufModelLeakingBufferPtr) buf,
        requestedOutput
      );

    case TIME_REMOVAL_BUFFER:
      return _computeRemovalBufferDelayForBufferDataOutput(
        (BufModelRemovalBufferPtr) buf,
        requestedOutput,
        timestamp
      );

    default:
      break;
  }

  return 0;
}

static bool _checkBufModelNode(
  BufModelOptions options,
  BufModelNode node,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext,
  size_t totalInputData,
  uint64_t * delay
);

/** \~english
 * \brief Check if input data can fill given buffer without error.
 *
 * Compute minimal time required to hypotheticaly
 *
 * \param buf Updated buffer.
 * \param timestamp Current updating timestamp.
 * \param inputData Input data amount in bits.
 * \param fillingBitrate Input data filling bitrate in bits per second.
 * \param streamContext Input data source stream context.
 * \return true Input data can fill given buffer without error.
 * \return false Input data cannot fill given buffer, leading buffer overflow
 * or other error.
 */
static bool _checkBufModelBuffer(
  BufModelOptions options,
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext,
  size_t totalInputData,
  uint64_t * delay
)
{
  size_t fillingLevel, inputDataBandwidth, outputDataBandwidth;
  bool outputCheck, notFullBuffer;
  int ret;

  _computeBufferDataInput(
    buf,
    timestamp,
    inputData,
    fillingBitrate,
    &inputDataBandwidth,
    NULL
  );

  fillingLevel = buf->header.bufferFillingLevel + inputDataBandwidth;

  /* Compute how many time elapsed since last update and current packet
  transfer finish. */
  ret = _computeBufferDataOutput(
    options,
    buf,
    timestamp,
    fillingLevel,
    fillingBitrate,
    streamContext,
    &outputDataBandwidth
  );
  if (ret < 0)
    return true;

  /* assert(outputDataBandwidth <= fillingLevel); */
  outputCheck = _checkBufModelNode(
    options,
    buf->header.output,
    timestamp,
    outputDataBandwidth,
    fillingBitrate,
    streamContext,
    totalInputData,
    delay
  );
  if (!outputCheck)
    return false;

  notFullBuffer = (
    fillingLevel < outputDataBandwidth
    || (fillingLevel - outputDataBandwidth) <= buf->header.param.bufferSize
  );

  if (!notFullBuffer) {
    LIBBLU_T_STD_VERIF_TEST_DEBUG(
      "Full buffer (%zu - %zu = %zu < %zu bits).\n",
      fillingLevel,
      outputDataBandwidth,
      fillingLevel - outputDataBandwidth,
      buf->header.param.bufferSize
    );

    if (!buf->header.param.instantFilling) {
      /** Speed trick:
       * Compute delay to empty whole buffer rather than only required space
       * for heading buffers.
       */
      *delay = _computeDelayForBufferDataOutput(
        buf,
        fillingLevel,
        timestamp
      );
    }
    else
      *delay = _computeDelayForBufferDataOutput(buf, totalInputData, timestamp);
  }

  return notFullBuffer;
}

static int _signalUnderflow(
  BufModelOptions options,
  size_t bufferFillingLevel,
  size_t bufferOutput,
  uint64_t timestamp
)
{
  static uint64_t lastWarningTimestamp;

  if (options.abortOnUnderflow)
    LIBBLU_ERROR_RETURN(
      "Buffer underflow (%zu < %zu) at STC %" PRIu64 " ticks @ 27MHz "
      "(~%" PRIu64 " ticks @ 90kHz).\n",
      bufferFillingLevel,
      bufferOutput,
      timestamp,
      timestamp / 300
    );

  if (lastWarningTimestamp + options.underflowWarnTimeout < timestamp)
    LIBBLU_WARNING(
      "Buffer underflow (%zu < %zu) at STC %" PRIu64 " ticks @ 27MHz "
      "(~%" PRIu64 " ticks @ 90kHz).\n",
      bufferFillingLevel,
      bufferOutput,
      timestamp,
      timestamp / 300
    );

  lastWarningTimestamp = timestamp;
  return 0;
}

/** \~english
 * \brief Update buffering chain state from given buffer.
 *
 * \param options Buffer model options.
 * \param buf Buffer to update.
 * \param timestamp Current updating timestamp.
 * \param inputData Optionnal input data amount in bits.
 * \param fillingBitrate Input data filling bitrate in bits per second.
 * \param streamContext Input data source stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _updateBufModelBuffer(
  BufModelOptions options,
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

  assert(buf->header.lastUpdate <= timestamp);

  _computeBufferDataInput(
    buf,
    timestamp,
    inputData,
    fillingBitrate,
    &dataBandwidth,
    &inputPendingData
  );

  /* LIBBLU_DEBUG_COM(
    "%zu bytes enter %s (remain %zu bytes pending).\n",
    dataBandwidth,
    bufferNameBufModelBuffer(buf),
    inputPendingData
  ); */

  buf->header.bufferFillingLevel += dataBandwidth;
  buf->header.bufferInputData = inputPendingData;

  /* Compute how many time elapsed since last update and current packet
  transfer finish. */
  ret = _computeBufferDataOutput(
    options,
    buf,
    timestamp,
    buf->header.bufferFillingLevel,
    fillingBitrate,
    streamContext,
    &dataBandwidth
  );
  if (ret < 0)
    return -1;

  /* Check if output data is greater than buffer occupancy. */
  if (buf->header.bufferFillingLevel < dataBandwidth) {
    size_t bufFillingLevel = buf->header.bufferFillingLevel;

    if (_signalUnderflow(options, bufFillingLevel, dataBandwidth, timestamp) < 0)
      return -1;
    // dataBandwidth = bufFillingLevel; // TODO: Might be more accurate without.
  }

  /* Remove ouput data bandwidth */
  buf->header.bufferFillingLevel -= MIN(
    buf->header.bufferFillingLevel,
    dataBandwidth
  );

  /* LIBBLU_DEBUG_COM(
    "%zu bytes removed from %s.\n",
    dataBandwidth, bufferNameBufModelBuffer(buf)
  ); */

  /* Check if buffer occupancy after update is greater than its size. */
  if (buf->header.param.bufferSize < buf->header.bufferFillingLevel) {
    /* buf Overflow, this shall never happen */
    LIBBLU_ERROR_RETURN(
      "Buffer overflow happen in %s (%zu < %zu).\n",
      bufferNameBufModelBuffer(buf),
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
    BufModelBufferFrame * frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.storedFrames,
      frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unexpected data in buffer %s, "
        "buf->bufferFillingLevel shall be lower or equal to "
        "the sum of all frames headerSize + removedData (extra %zu bytes).\n",
        bufferNameBufModelBuffer(buf),
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
  if (!isVoidBufModelNode(buf->header.output)) {
    /* LIBBLU_DEBUG_COM(
      "Emit %zu bytes from %s.\n",
      emitedData, bufferNameBufModelBuffer(buf)
    ); */

    ret = _updateBufModelNode(
      options,
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
      emitedData, bufferNameBufModelBuffer(buf)
    ); */
  }

  /* Delete empty useless frames */
  for (
    frameIdx = 0;
    frameIdx < nbEntriesCircularBuffer(buf->header.storedFrames);
    frameIdx++
  ) {
    BufModelBufferFrame * frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.storedFrames,
      frameIdx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN("No frame to get, broken circular buffer.\n");

    if (!frame->headerSize && !frame->dataSize) {
      /* Frame header and payload has already been fully transfered */
      if (NULL == popCircularBuffer(buf->header.storedFrames))
        LIBBLU_ERROR_RETURN("Unable to pop, broken circular buffer.\n");
    }
    else
      break; /* Frame is not empty, following ones too, stop here */
  }

  return 0;
}

BufModelBuffersListPtr createBufModelBuffersList(
  void
)
{
  BufModelBuffersListPtr list;

  list = (BufModelBuffersListPtr) malloc(sizeof(BufModelBuffersList));
  if (NULL == list)
    return NULL;
  *list = (BufModelBuffersList) {0};

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

/** \~english
 * \brief Add given buffer to buffers list.
 *
 * \param list Destination list.
 * \param buf Added buffer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _addBufferBufModelBuffersList(
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
    if (_isNamesMatchBufModelBuffer(bufList->buffers[i], name, customBufName, customNameHash)) {
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

/** \~english
 * \brief Create a buffer using leaking output method.
 *
 * \param param Buffer parameters.
 * \param initialTimestamp Initial zero timestamp value.
 * \param removalBitrate Buffer output leaking bitrate in bits per second.
 * \return BufferPtr On success, created object is returned. Otherwise a
 * NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffer() function after use
 * (this function cast the buffer to #destroyLeakingBuffer() function).
 */
static BufModelBufferPtr _createLeakingBuffer(
  BufModelBufferParameters param,
  uint64_t initialTimestamp,
  uint64_t removalBitrate
)
{
  BufModelLeakingBufferPtr buf;
  if (NULL == (buf = (BufModelLeakingBufferPtr) malloc(sizeof(BufModelLeakingBuffer))))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  CircularBuffer * frmBuf;
  if (NULL == (frmBuf = createCircularBuffer()))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  *buf = (BufModelLeakingBuffer) {
    .header = {
      .type = LEAKING_BUFFER,
      .param = param,
      .output = newVoidBufModelNode(),
      .lastUpdate = initialTimestamp,
      .storedFrames = frmBuf
    },
    .removalBitratePerSec = removalBitrate,
    .removalBitrate = (double) removalBitrate / MAIN_CLOCK_27MHZ
  };

  return (BufModelBufferPtr) buf;

free_return:
  destroyLeakingBuffer(buf);
  return NULL;
}

/** \~english
 * \brief Create a buffer using leaking output method.
 *
 * \param param Buffer parameters.
 * \param initialTimestamp Initial zero timestamp value.
 * \param removalBitrate Buffer output leaking bitrate in bits per second.
 * \return BufferPtr On success, created object is returned. Otherwise a
 * NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffer() function after use
 * (this function cast the buffer to #destroyLeakingBuffer() function).
 */
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

  buf = _createLeakingBuffer(param, initialTimestamp, removalBitrate);
  if (NULL == buf)
    return -1;

  if (_addBufferBufModelBuffersList(list, buf) < 0)
    return -1;

  node->type = NODE_BUFFER;
  node->linkedElement.buffer = buf;

  return 0;
}

/** \~english
 * \brief Create a buffer using removal timestamps output method.
 *
 * \param param Buffer parameters.
 * \param initialTimestamp Initial zero timestamp value.
 * \return BufferPtr On success, created object is returned. Otherwise a
 * NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffer() function after use
 * (this function cast the buffer to #destroyRemovalBuffer() function).
 */
static BufModelBufferPtr createRemovalBuffer(
  BufModelBufferParameters param,
  uint64_t initialTimestamp
)
{
  BufModelRemovalBufferPtr buf;
  CircularBuffer * frmBuf;

  if (NULL == (buf = (BufModelRemovalBufferPtr) malloc(sizeof(BufModelRemovalBuffer))))
    goto free_return; /* Memory allocation error */
  if (NULL == (frmBuf = createCircularBuffer()))
    goto free_return; /* Memory allocation error */

  *buf = (BufModelRemovalBuffer) {
    .header = {
      .type = TIME_REMOVAL_BUFFER,
      .param = param,
      .output = newVoidBufModelNode(),
      .lastUpdate = initialTimestamp,
      .storedFrames = frmBuf
    }
  };

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

  if (_addBufferBufModelBuffersList(list, buf) < 0)
    return -1;

  node->type = NODE_BUFFER;
  node->linkedElement.buffer = buf;

  return 0;
}

static bool _isInListBufModelFilterLblValue(
  BufModelFilterLblValue list,
  BufModelFilterLblValue value
);

/** \~english
 * \brief Return true if both supplied label values are equal.
 *
 * \param left First value.
 * \param right Second value.
 * \param type Type of values.
 * \return true Both label values are evaluated as equal.
 * \return false Label values are different.
 */
static bool areEqualBufModelFilterLblValues(
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
      return _isInListBufModelFilterLblValue(
        left, right
      );
  }

  /* Shall never happen */
  LIBBLU_ERROR_EXIT(
    "Unexpected buffering model filter label type %u.\n",
    type
  );
}

static bool _isInListBufModelFilterLblValue(
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

/** \~english
 * \brief Return true if both supplied labels are equal.
 *
 * \param left First label.
 * \param right Second label.
 * \return true Both labels values are evaluated as equal.
 * \return false Labels are different.
 */
bool _areEqualBufModelFilterLbls(
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

/** \~english
 * \brief Create a buffering model filter.
 *
 * \param fun Stream context filtering function pointer.
 * \param labelsType Type of filter nodes attached labels.
 * \return BufModelFilterPtr On success, created object is returned. Otherwise
 * a NULL pointer is returned.
 *
 * See #BufModelFilter documentation for more details about filters.
 * Created object must be passed to #destroyBufModelFilter() function after
 * use.
 */
BufModelFilterPtr _createBufModelFilter(
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labelsType
)
{
  BufModelFilterPtr filter;

  if (NULL == fun)
    LIBBLU_ERROR_NRETURN(
      "_createBufModelFilter() receive NULL decision function pointer.\n"
    );

  if (labelsType == BUF_MODEL_FILTER_LABEL_TYPE_LIST)
    LIBBLU_ERROR_NRETURN(
      "_createBufModelFilter() labels type cannot be list, "
      "use list content type.\n"
    );

  if (NULL == (filter = (BufModelFilterPtr) malloc(sizeof(BufModelFilter))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  *filter = (BufModelFilter) {
    .labelsType = labelsType,
    .apply = fun
  };

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

  if (NULL == (filter = _createBufModelFilter(fun, labelsType)))
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

/** \~english
 * \brief Add supplied buffering model node to filter.
 *
 * \param filter Destination filter.
 * \param node Node to add in filter outputs.
 * \param label Node attached label.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Label must be of type defined at filter creation using
 * #createBufModelFilterNode(), otherwise an error is raised.
 */
static int _addNodeToBufModelFilter(
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
    if (_areEqualBufModelFilterLbls(filter->labels[i], label))
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
  assert(isFilterBufModelNode(filterNode));

  return _addNodeToBufModelFilter(
    filterNode.linkedElement.filter,
    node,
    label
  );
}

/** \~english
 * \brief Check if input data can fill given filter without error.
 *
 * \param filter Buffering model filter to update.
 * \param timestamp Current update timestamp.
 * \param inputData Optionnal input data in bits.
 * \param fillingBitrate Input data filling bitrate.
 * \param streamContext Input data stream context.
 * \return true Input data can fill given buffering model without error.
 * \return false Input data cannot fill given buffering model, leading buffer
 * overflow or other error.
 */
static bool _checkBufModelFilter(
  BufModelOptions options,
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext,
  size_t totalInputData,
  uint64_t * delay
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
    ret = _checkBufModelNode(
      options,
      nodes[i],
      timestamp,
      (i == destIndex) ? inputData : 0,
      fillingBitrate,
      streamContext,
      totalInputData,
      delay
    );

    if (!ret)
      return false;
  }

  return true;
#else
  return _checkBufModelNode(
    options,
    filter->nodes[destIndex],
    timestamp,
    inputData,
    fillingBitrate,
    streamContext,
    totalInputData,
    delay
  );
#endif
}

/** \~english
 * \brief Update buffering chain state from given filter.
 *
 * \param filter Buffering model filter to update.
 * \param timestamp Current update timestamp.
 * \param inputData Optionnal input data in bits.
 * \param fillingBitrate Input data filling bitrate.
 * \param streamContext Input data stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _updateBufModelFilter(
  BufModelOptions options,
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
  else {
#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
    destIndex = filter->nbUsedNodes; /* Set impossible index */
#else
    return 0; /* No data to transmit, useless decision. */
#endif
  }

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  nbUsedNodes = filter->nbUsedNodes;
  nodes = filter->nodes;

  for (i = 0; i < nbUsedNodes; i++) {
    /* Only transmit inputData to destination node */
    ret = _updateBufModelNode(
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
  return _updateBufModelNode(
    options,
    filter->nodes[destIndex],
    timestamp,
    inputData,
    fillingBitrate,
    streamContext
  );
#endif
}

/** \~english
 * \brief Check if input data can fill given buffering model from given node
 * without error.
 *
 * \param rootNode Buffering model node.
 * \param timestamp Current updating timestamp.
 * \param inputData Input data amount in bits.
 * \param fillingBitrate Input data filling bitrate in bits per second.
 * \param streamContext Input data source stream context.
 * \return true Input data can fill given buffering model without error.
 * \return false Input data cannot fill given buffering model, leading buffer
 * overflow or other error.
 */
static bool _checkBufModelNode(
  BufModelOptions options,
  BufModelNode node,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext,
  size_t totalInputData,
  uint64_t * delay
)
{
  switch (node.type) {
    case NODE_VOID:
      break; /* Data sent to void */

    case NODE_BUFFER:
      return _checkBufModelBuffer(
        options,
        node.linkedElement.buffer,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext,
        totalInputData,
        delay
      );

    case NODE_FILTER:
      return _checkBufModelFilter(
        options,
        node.linkedElement.filter,
        timestamp,
        inputData,
        fillingBitrate,
        streamContext,
        totalInputData,
        delay
      );
  }

  return true;
}

int checkBufModel(
  BufModelOptions options,
  BufModelNode rootNode,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext,
  uint64_t * delay
)
{
  return _checkBufModelNode(
    options,
    rootNode,
    timestamp,
    inputData,
    fillingBitrate,
    streamContext,
    inputData,
    delay
  );
}

int updateBufModel(
  BufModelOptions options,
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

  return _updateBufModelNode(
    options,
    rootNode,
    timestamp,
    inputData,
    fillingBitrate,
    streamContext
  );
}

void printBufModelBuffer(const BufModelBufferPtr buf)
{
  lbc_printf(
    "[%s %" PRIu64 " / %" PRIu64 "]",
    bufferNameBufModelBuffer(buf),
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
  BufModelNode node = newVoidBufModelNode();
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