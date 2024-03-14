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
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
);

static int _updateBufModelFilter(
  BufModelOptions options,
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
);

/** \~english
 * \brief Update buffering model from given node.
 *
 * \param root_node Buffering model node.
 * \param timestamp Current update timestamp.
 * \param input_data Optionnal input data in bits.
 * \param filling_bitrate Input data filling bitrate.
 * \param stream_context Input data stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _updateBufModelNode(
  BufModelOptions options,
  BufModelNode node,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
)
{
  switch (node.type) {
  case NODE_VOID:
    if (0 < input_data)
      LIBBLU_DEBUG_COM("Discarding %zu bytes.\n", input_data);
    break;

  case NODE_BUFFER:
    return _updateBufModelBuffer(
      options,
      node.linkedElement.buffer,
      timestamp,
      input_data,
      filling_bitrate,
      stream_context
    );

  case NODE_FILTER:
    return _updateBufModelFilter(
      options,
      node.linkedElement.filter,
      timestamp,
      input_data,
      filling_bitrate,
      stream_context
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
  if (NULL == buf)
    LIBBLU_ERROR_RETURN("NULL pointer buffer on addFrameToBuffer().\n");

  BufModelBufferFrame *new_frame = (BufModelBufferFrame *) newEntryCircularBuffer(
    ((BufModelLeakingBufferPtr) buf)->header.stored_frames,
    sizeof(BufModelBufferFrame)
  );
  if (NULL == new_frame)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  *new_frame = frame;
  return 1;
}

/** \~english
 * \brief Return true if given buffer name matches supplied one.
 *
 * \param buf Buffer to test.
 * \param name Pre-defined buffer name.
 * \param custom_name Custom buffer name string if name value is set to
 * #CUSTOM_BUFFER value (otherwise shall be NULL).
 * \param custom_name_hash Optionnal custom buffer name hash value (if set to
 * 0, the value is computed if required from custom_name).
 * \return true The buffer name matches name and/or custom_name.
 * \return false The buffer name does not matches.
 */
static bool _isNamesMatchBufModelBuffer(
  const BufModelBufferPtr buf,
  const BufModelBufferName name,
  const lbc *custom_name,
  uint32_t custom_name_hash
)
{
  assert(NULL != buf);

  if (buf->header.param.name != name)
    return false;

  if (buf->header.param.name == CUSTOM_BUFFER) {
    if (NULL == custom_name)
      LIBBLU_ERROR_EXIT(
        "Unexpected NULL buffer name string as "
        "_isNamesMatchBufModelBuffer() argument.\n"
      );

    if (!custom_name_hash)
      custom_name_hash = lbc_fnv1aStrHash(custom_name);

    return
      buf->header.param.custom_name_hash == custom_name_hash
      && lbc_equal(buf->header.param.custom_name, custom_name)
    ;
  }

  return true;
}

static void _computeBufferDataInput(
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  size_t *result_input_bandwidth_ret,
  size_t *result_buffer_pending_data_ret
)
{
  assert(buf->header.last_update <= timestamp);

  uint64_t elapsed_time = timestamp - buf->header.last_update;

  /* LIBBLU_DEBUG_COM(
    "Compute data input from buffer %s with %" PRIu64 " ticks elapsed.\n",
    bufferNameBufModelBuffer(buf), elapsed_time
  ); */

  size_t buffer_input_data = buf->header.buffer_input_data;

  /* Insert data in buffer */
  size_t input_data_bandwidth = 0;
  if (buf->header.param.instant_filling) {
    input_data_bandwidth = buffer_input_data + input_data;
    buffer_input_data = 0;
  }
  else {
    buffer_input_data += input_data;

    input_data_bandwidth = MIN(
      buffer_input_data,
      elapsed_time *filling_bitrate
    );
    buffer_input_data -= input_data_bandwidth;
  }

  if (NULL != result_input_bandwidth_ret)
    *result_input_bandwidth_ret = input_data_bandwidth;
  if (NULL != result_buffer_pending_data_ret)
    *result_buffer_pending_data_ret = buffer_input_data;
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
  size_t buf_filling_level,
  uint64_t timestamp
)
{
  assert(NULL != buf);

  return MIN(
    buf_filling_level,
    ceil((timestamp - buf->header.last_update) * buf->removal_br)
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
  size_t data_length = 0;
  size_t nb_frames = nbEntriesCircularBuffer(buf->header.stored_frames);
  for (size_t frame_idx = 0; frame_idx < nb_frames; frame_idx++) {
    BufModelBufferFrame *frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.stored_frames, frame_idx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unable to extract frame, broken circular buffer.\n"
      );

    if (frame->removal_timestamp <= timestamp)
      data_length += frame->header_size + frame->data_size;
    else
      break; /* Unreached timestamp, no more frames. */

    LIBBLU_T_STD_VERIF_TEST_DEBUG(
      "%zu) %zu/%zu - %zu bits.\n",
      frame_idx, frame->removal_timestamp, timestamp, frame->data_size
    );
  }
  if (0 < data_length)
    LIBBLU_T_STD_VERIF_TEST_DEBUG("=> %zu bits.\n", data_length);

  return data_length;
}

static int _computeBufferDataOutput(
  BufModelOptions options,
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t filling_level,
  uint64_t filling_bitrate,
  void *stream_context,
  size_t *output_bandwidth_ret
)
{
  size_t data_bandwidth = 0;
  switch (buf->header.type) {
  case LEAKING_BUFFER:
    data_bandwidth = _computeLeakingBufferDataBandwidth(
      (BufModelLeakingBufferPtr) buf,
      filling_level,
      timestamp
    );
    break;

  case TIME_REMOVAL_BUFFER:
    data_bandwidth = _computeRemovalBufferDataBandwidth(
      (BufModelRemovalBufferPtr) buf,
      timestamp
    );
    break;

  default:
    LIBBLU_ERROR_RETURN(
      "Unable to update buffer model, unknown buffer type."
    );
  }

  if (buf->header.param.dont_overflow_output) {
    /* Update now output buffer */
    size_t output_buffer_filling_level = 0;

    if (isVoidBufModelNode(buf->header.output))
      output_buffer_filling_level = data_bandwidth;
    else {
      if (!isBufferBufModelNode(buf->header.output))
        LIBBLU_ERROR_RETURN(
          "Controlled output buffer from %s must be linked to another "
          "buffer in output (output node type value: %d).",
          bufferNameBufModelBuffer(buf), buf->header.output.type
        );

      int ret = _updateBufModelNode(
        options,
        buf->header.output,
        timestamp,
        0,
        filling_bitrate,
        stream_context
      );
      if (ret < 0)
        return -1;
      assert(0 == ret);

      BufModelBufferPtr output_buf = buf->header.output.linkedElement.buffer;

      output_buffer_filling_level = (
        output_buf->header.param.buffer_size
        - output_buf->header.buffer_filling_level
      );
    }

    data_bandwidth = MIN(
      data_bandwidth,
      output_buffer_filling_level
    );
  }

  if (NULL != output_bandwidth_ret)
    *output_bandwidth_ret = data_bandwidth;

  return 0;
}

static uint64_t _computeLeakingBufferDelayForBufferDataOutput(
  BufModelLeakingBufferPtr buf,
  size_t requested_output
)
{
  assert(NULL != buf);

  return requested_output / buf->removal_br;
}

static uint64_t _computeRemovalBufferDelayForBufferDataOutput(
  BufModelRemovalBufferPtr buf,
  size_t requested_output,
  uint64_t timestamp
)
{
  uint64_t delay = 0;

  size_t nb_frames = nbEntriesCircularBuffer(buf->header.stored_frames);
  for (size_t frame_idx = 0; frame_idx < nb_frames; frame_idx++) {
    BufModelBufferFrame *frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.stored_frames, frame_idx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unable to extract frame, broken circular buffer.\n"
      );

    if (!requested_output)
      break;
    delay = frame->removal_timestamp - timestamp;
    requested_output -= MIN(requested_output, frame->data_size);
  }

  return delay;
}

static uint64_t _computeDelayForBufferDataOutput(
  BufModelBufferPtr buf,
  size_t requested_output,
  uint64_t timestamp
)
{
  switch (buf->header.type) {
  case LEAKING_BUFFER:
    return _computeLeakingBufferDelayForBufferDataOutput(
      (BufModelLeakingBufferPtr) buf,
      requested_output
    );

  case TIME_REMOVAL_BUFFER:
    return _computeRemovalBufferDelayForBufferDataOutput(
      (BufModelRemovalBufferPtr) buf,
      requested_output,
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
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context,
  size_t total_input_data,
  uint64_t *delay
);

/** \~english
 * \brief Check if input data can fill given buffer without error.
 *
 * Compute minimal time required to hypotheticaly
 *
 * \param buf Updated buffer.
 * \param timestamp Current updating timestamp.
 * \param input_data Input data amount in bits.
 * \param filling_bitrate Input data filling bitrate in bits per second.
 * \param stream_context Input data source stream context.
 * \return true Input data can fill given buffer without error.
 * \return false Input data cannot fill given buffer, leading buffer overflow
 * or other error.
 */
static bool _checkBufModelBuffer(
  BufModelOptions options,
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context,
  size_t total_input_data,
  uint64_t *delay
)
{
  size_t input_data_bandwidth;
  _computeBufferDataInput(
    buf,
    timestamp,
    input_data,
    filling_bitrate,
    &input_data_bandwidth,
    NULL
  );

  size_t filling_level = buf->header.buffer_filling_level + input_data_bandwidth;

  /* Compute how many time elapsed since last update and current packet
  transfer finish. */
  size_t output_data_bandwidth;
  int ret = _computeBufferDataOutput(
    options,
    buf,
    timestamp,
    filling_level,
    filling_bitrate,
    stream_context,
    &output_data_bandwidth
  );
  if (ret < 0)
    return true;

  /* assert(output_data_bandwidth <= filling_level); */
  bool output_check = _checkBufModelNode(
    options,
    buf->header.output,
    timestamp,
    output_data_bandwidth,
    filling_bitrate,
    stream_context,
    total_input_data,
    delay
  );
  if (!output_check)
    return false;

  bool not_full_buffer = (
    filling_level < output_data_bandwidth
    || (filling_level - output_data_bandwidth) <= buf->header.param.buffer_size
  );

  if (!not_full_buffer) {
    LIBBLU_T_STD_VERIF_TEST_DEBUG(
      "Full buffer (%zu - %zu = %zu < %zu bits).\n",
      filling_level,
      output_data_bandwidth,
      filling_level - output_data_bandwidth,
      buf->header.param.buffer_size
    );

    if (!buf->header.param.instant_filling) {
      /** Speed trick:
       * Compute delay to empty whole buffer rather than only required space
       * for heading buffers.
       */
      *delay = _computeDelayForBufferDataOutput(
        buf,
        filling_level,
        timestamp
      );
    }
    else
      *delay = _computeDelayForBufferDataOutput(buf, total_input_data, timestamp);
  }

  return not_full_buffer;
}

static int _signalUnderflow(
  BufModelOptions options,
  size_t buffer_filling_level,
  size_t bufferOutput,
  uint64_t timestamp
)
{
  static uint64_t last_warning_timestamp;

  if (options.abort_on_underflow)
    LIBBLU_ERROR_RETURN(
      "Buffer underflow (%zu < %zu) at STC %" PRIu64 " ticks @ 27MHz "
      "(~%" PRIu64 " ticks @ 90kHz).\n",
      buffer_filling_level,
      bufferOutput,
      timestamp,
      timestamp / 300
    );

  if (last_warning_timestamp + options.underflow_warn_timeout < timestamp)
    LIBBLU_WARNING(
      "Buffer underflow (%zu < %zu) at STC %" PRIu64 " ticks @ 27MHz "
      "(~%" PRIu64 " ticks @ 90kHz).\n",
      buffer_filling_level,
      bufferOutput,
      timestamp,
      timestamp / 300
    );

  last_warning_timestamp = timestamp;
  return 0;
}

/** \~english
 * \brief Update buffering chain state from given buffer.
 *
 * \param options Buffer model options.
 * \param buf Buffer to update.
 * \param timestamp Current updating timestamp.
 * \param input_data Optionnal input data amount in bits.
 * \param filling_bitrate Input data filling bitrate in bits per second.
 * \param stream_context Input data source stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _updateBufModelBuffer(
  BufModelOptions options,
  BufModelBufferPtr buf,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
)
{
  assert(buf->header.last_update <= timestamp);

  size_t data_bandwidth, input_pending_data;
  _computeBufferDataInput(
    buf,
    timestamp,
    input_data,
    filling_bitrate,
    &data_bandwidth,
    &input_pending_data
  );

  /* LIBBLU_DEBUG_COM(
    "%zu bytes enter %s (remain %zu bytes pending).\n",
    data_bandwidth,
    bufferNameBufModelBuffer(buf),
    input_pending_data
  ); */

  buf->header.buffer_filling_level += data_bandwidth;
  buf->header.buffer_input_data = input_pending_data;

  /* Compute how many time elapsed since last update and current packet
  transfer finish. */
  int ret = _computeBufferDataOutput(
    options,
    buf,
    timestamp,
    buf->header.buffer_filling_level,
    filling_bitrate,
    stream_context,
    &data_bandwidth
  );
  if (ret < 0)
    return -1;

  /* Check if output data is greater than buffer occupancy. */
  if (buf->header.buffer_filling_level < data_bandwidth) {
    size_t buf_filling_level = buf->header.buffer_filling_level;

    if (_signalUnderflow(options, buf_filling_level, data_bandwidth, timestamp) < 0)
      return -1;
    // data_bandwidth = buf_filling_level; // TODO: Might be more accurate without.
  }

  /* Remove ouput data bandwidth */
  buf->header.buffer_filling_level -= MIN(
    buf->header.buffer_filling_level,
    data_bandwidth
  );

  /* LIBBLU_DEBUG_COM(
    "%zu bytes removed from %s.\n",
    data_bandwidth, bufferNameBufModelBuffer(buf)
  ); */

  /* Check if buffer occupancy after update is greater than its size. */
  if (buf->header.param.buffer_size < buf->header.buffer_filling_level) {
    /* buf Overflow, this shall never happen */
    LIBBLU_ERROR_RETURN(
      "Buffer overflow happen in %s (%zu < %zu).\n",
      bufferNameBufModelBuffer(buf),
      buf->header.param.buffer_size,
      buf->header.buffer_filling_level
    );
  }

  /* Set update time */
  buf->header.last_update = timestamp;

  /* Remove bandwidth from buffer stored frames */
  /* And compute emited data to following buffer */
  size_t emitted_data = 0;

  for (size_t frame_idx = 0; 0 < data_bandwidth; frame_idx++) {
    BufModelBufferFrame *frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.stored_frames,
      frame_idx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN(
        "Unexpected data in buffer %s, "
        "buf->buffer_filling_level shall be lower or equal to "
        "the sum of all frames header_size + removed_data (extra %zu bytes).\n",
        bufferNameBufModelBuffer(buf),
        data_bandwidth
      );

    size_t removed_data = MIN(frame->header_size, data_bandwidth);
    frame->header_size = frame->header_size - removed_data;
    data_bandwidth -= removed_data;

    removed_data = MIN(frame->data_size, data_bandwidth);
    frame->data_size = frame->data_size - removed_data;
    emitted_data += (0 < frame->output_data_size) ?
      frame->output_data_size : removed_data
    ;
    data_bandwidth -= removed_data;
  }

  /* Transfer data to output buffer and update it */
  if (!isVoidBufModelNode(buf->header.output)) {
    /* LIBBLU_DEBUG_COM(
      "Emit %zu bytes from %s.\n",
      emitted_data, bufferNameBufModelBuffer(buf)
    ); */

    ret = _updateBufModelNode(
      options,
      buf->header.output,
      timestamp,
      emitted_data,
      filling_bitrate,
      stream_context
    );
    if (ret < 0)
      return -1;
  }
  else if (0 < emitted_data) {
    /* Otherwise, no output buffer, data is discarted */
    /* LIBBLU_DEBUG_COM(
      "Discard %zu bytes at output of %s.\n",
      emitted_data, bufferNameBufModelBuffer(buf)
    ); */
  }

  /* Delete empty useless frames */
  for (
    size_t frame_idx = 0;
    frame_idx < nbEntriesCircularBuffer(buf->header.stored_frames);
    frame_idx++
  ) {
    BufModelBufferFrame *frame = (BufModelBufferFrame *) getCircularBuffer(
      buf->header.stored_frames,
      frame_idx
    );
    if (NULL == frame)
      LIBBLU_ERROR_RETURN("No frame to get, broken circular buffer.\n");

    if (!frame->header_size && !frame->data_size) {
      /* Frame header and payload has already been fully transfered */
      if (NULL == popCircularBuffer(buf->header.stored_frames))
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
  BufModelBuffersListPtr list = (BufModelBuffersListPtr) malloc(
    sizeof(BufModelBuffersList)
  );
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
  assert(NULL != list);
  assert(NULL != buf);

  if (list->nb_allocated_buffers <= list->nb_used_buffers) {
    /* Need realloc */
    unsigned new_length = GROW_BUF_MODEL_BUFFERS_LIST_LENGTH(
      list->nb_allocated_buffers
    );

    if (lb_mul_overflow_size_t(new_length, sizeof(BufModelBufferPtr)))
      LIBBLU_ERROR_RETURN(
        "Buffers list array length value overflow, array is too big.\n"
      );

    BufModelBufferPtr *new_array = (BufModelBufferPtr *) realloc(
      list->buffers,
      new_length *sizeof(BufModelBufferPtr)
    );
    if (NULL == new_array)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    list->buffers = new_array;
    list->nb_allocated_buffers = new_length;
  }

  list->buffers[list->nb_used_buffers++] = buf;

  return 0;
}

int addFrameToBufferFromList(
  BufModelBuffersListPtr bufList,
  BufModelBufferName name,
  const lbc *custom_buf_name,
  BufModelBufferFrame frame
)
{
  assert(NULL != bufList);
  assert(0 < bufList->nb_used_buffers);

  uint32_t custom_name_hash = 0;
  if (NULL != custom_buf_name)
    custom_name_hash = lbc_fnv1aStrHash(custom_buf_name);

  for (unsigned i = 0; i < bufList->nb_used_buffers; i++) {
    if (
      _isNamesMatchBufModelBuffer(
        bufList->buffers[i],
        name,
        custom_buf_name,
        custom_name_hash
      )
    ) {
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
 * \param initial_timestamp Initial zero timestamp value.
 * \param removal_br Buffer output leaking bitrate in bits per second.
 * \return BufferPtr On success, created object is returned. Otherwise a
 * NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffer() function after use
 * (this function cast the buffer to #destroyLeakingBuffer() function).
 */
static BufModelBufferPtr _createLeakingBuffer(
  BufModelBufferParameters param,
  uint64_t initial_timestamp,
  uint64_t removal_br
)
{
  BufModelLeakingBufferPtr buf = (BufModelLeakingBufferPtr) malloc(
    sizeof(BufModelLeakingBuffer)
  );
  if (NULL == buf)
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  CircularBuffer *frame_buffer = createCircularBuffer();
  if (NULL == frame_buffer)
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  *buf = (BufModelLeakingBuffer) {
    .header = {
      .type = LEAKING_BUFFER,
      .param = param,
      .output = newVoidBufModelNode(),
      .last_update = initial_timestamp,
      .stored_frames = frame_buffer
    },
    .removal_br_per_sec = removal_br,
    .removal_br = (double) removal_br / MAIN_CLOCK_27MHZ
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
 * \param initial_timestamp Initial zero timestamp value.
 * \param removal_br Buffer output leaking bitrate in bits per second.
 * \return BufferPtr On success, created object is returned. Otherwise a
 * NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffer() function after use
 * (this function cast the buffer to #destroyLeakingBuffer() function).
 */
int createLeakingBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode *node,
  BufModelBufferParameters param,
  uint64_t initial_timestamp,
  uint64_t removal_br
)
{
  assert(NULL != node);

  BufModelBufferPtr buf = _createLeakingBuffer(param, initial_timestamp, removal_br);
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
 * \param initial_timestamp Initial zero timestamp value.
 * \return BufferPtr On success, created object is returned. Otherwise a
 * NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffer() function after use
 * (this function cast the buffer to #destroyRemovalBuffer() function).
 */
static BufModelBufferPtr createRemovalBuffer(
  BufModelBufferParameters param,
  uint64_t initial_timestamp
)
{
  BufModelRemovalBufferPtr buf = (BufModelRemovalBufferPtr) malloc(
    sizeof(BufModelRemovalBuffer)
  );
  if (NULL == buf)
    goto free_return; /* Memory allocation error */
  CircularBuffer *frame_buffer = createCircularBuffer();
  if (NULL == frame_buffer)
    goto free_return; /* Memory allocation error */

  *buf = (BufModelRemovalBuffer) {
    .header = {
      .type = TIME_REMOVAL_BUFFER,
      .param = param,
      .output = newVoidBufModelNode(),
      .last_update = initial_timestamp,
      .stored_frames = frame_buffer
    }
  };

  return (BufModelBufferPtr) buf;

free_return:
  destroyRemovalBuffer(buf);
  return NULL;
}

int createRemovalBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode *node,
  BufModelBufferParameters param,
  uint64_t initial_timestamp
)
{
  assert(NULL != node);

  BufModelBufferPtr buf = createRemovalBuffer(param, initial_timestamp);
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
      left.string_hash == right.string_hash
      && !lbc_strcmp(left.string, right.string)
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
  for (unsigned i = 0; i < list.list_length; i++) {
    if (
      areEqualBufModelFilterLblValues(
        list.list[i], value, list.list_item_type
      )
    )
      return true;
  }

  return false;
}

int setBufModelFilterLblList(
  BufModelFilterLbl *dst,
  BufModelFilterLblType values_type,
  const void *values_list,
  const unsigned values_nb
)
{
  /* Check parameters */
  if (NULL == dst)
    LIBBLU_ERROR_RETURN("NULL pointer filter label list destination.\n");
  if (NULL == values_list)
    LIBBLU_ERROR_RETURN("NULL pointer filter label list source values.\n");
  if (!values_nb)
    LIBBLU_ERROR_RETURN("Zero-sized list label.\n");

  /* Allocate values array */
  BufModelFilterLblValue *array = (BufModelFilterLblValue *) malloc(
    values_nb *sizeof(BufModelFilterLblValue)
  );
  if (NULL == array)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  /* Build values array */
  switch (values_type) {
  case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
    for (unsigned i = 0; i < values_nb; i++)
      array[i] = BUF_MODEL_FILTER_NUMERIC_LABEL_VALUE(
        ((int *) values_list)[i]
      );
    break;

  case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
    for (unsigned i = 0; i < values_nb; i++) {
      array[i] = BUF_MODEL_FILTER_STRING_LABEL_VALUE(
        ((lbc **) values_list)[i]
      );

      if (NULL == array[i].string) {
        /* Release already allocated memory */
        while (i--)
          cleanBufModelFilterLblValue(array[i], values_type);
        LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
      }
    }
    break;

  default:
    LIBBLU_ERROR_FRETURN(
      "Unexpected list label values type '%s', "
      "expect numeric or string value type.",
      getBufModelFilterLblTypeString(values_type)
    );
  }

  *dst = BUF_MODEL_FILTER_LIST_LABEL(
    values_type,
    array,
    values_nb
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
  if (left.type == BUF_MODEL_FILTER_LABEL_TYPE_LIST) {
    if (
      left.value.list_item_type != right.type
      && (
        right.type != BUF_MODEL_FILTER_LABEL_TYPE_LIST
        || left.value.list_item_type != right.value.list_item_type
      )
    )
      return false;

    for (unsigned i = 0; i < left.value.list_length; i++) {
      bool ret = areEqualBufModelFilterLblValues(
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
 * \param labels_type Type of filter nodes attached labels.
 * \return BufModelFilterPtr On success, created object is returned. Otherwise
 * a NULL pointer is returned.
 *
 * See #BufModelFilter documentation for more details about filters.
 * Created object must be passed to #destroyBufModelFilter() function after
 * use.
 */
BufModelFilterPtr _createBufModelFilter(
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labels_type
)
{
  if (NULL == fun)
    LIBBLU_ERROR_NRETURN(
      "_createBufModelFilter() receive NULL decision function pointer.\n"
    );

  if (labels_type == BUF_MODEL_FILTER_LABEL_TYPE_LIST)
    LIBBLU_ERROR_NRETURN(
      "_createBufModelFilter() labels type cannot be list, "
      "use list content type.\n"
    );

  BufModelFilterPtr filter = (BufModelFilterPtr) malloc(sizeof(BufModelFilter));
  if (NULL == filter)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  *filter = (BufModelFilter) {
    .labels_type = labels_type,
    .apply = fun
  };

  return filter;
}

int createBufModelFilterNode(
  BufModelNode *node,
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labels_type
)
{
  assert(NULL != node);

  BufModelFilterPtr filter = _createBufModelFilter(fun, labels_type);
  if (NULL == filter)
    return -1;

  node->type = NODE_FILTER;
  node->linkedElement.filter = filter;

  return 0;
}

void destroyBufModelFilter(
  BufModelFilterPtr filter
)
{
  if (NULL == filter)
    return;

  for (unsigned i = 0; i < filter->nb_used_nodes; i++) {
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
  assert(NULL != filter);

  if (isLinkedBufModelNode(node))
    LIBBLU_ERROR_RETURN(
      "Unable to use specified filter output node, "
      "it is has already been linked to another route.\n"
    );
  setLinkedBufModelNode(node);

  if (
    filter->labels_type != label.type
    && (
      label.type != BUF_MODEL_FILTER_LABEL_TYPE_LIST
      || filter->labels_type != label.value.list_item_type
    )
  ) {
    LIBBLU_ERROR_RETURN(
      "New filter node label type mismatch with buffer one (%s <> %s).\n",
      getBufModelFilterLblTypeString(label.type),
      getBufModelFilterLblTypeString(filter->labels_type)
    );
  }

  if (filter->nb_allocated_nodes <= filter->nb_used_nodes) {
    /* Need realloc */
    unsigned new_length = GROW_BUF_MODEL_FILTER_LENGTH(
      filter->nb_allocated_nodes
    );

    if (lb_mul_overflow_size_t(new_length, sizeof(BufModelFilterLbl)))
      LIBBLU_ERROR_RETURN(
        "Buffering model filter output nodes overflow. "
        "Too many defined output.\n"
      );

    BufModelNode *new_array_nodes = (BufModelNode *) realloc(
      filter->nodes,
      new_length *sizeof(BufModelNode)
    );
    if (NULL == new_array_nodes)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    BufModelFilterLbl *new_array_labels = (BufModelFilterLbl *) realloc(
      filter->labels,
      new_length *sizeof(BufModelFilterLbl)
    );
    if (NULL == new_array_labels) {
      free(new_array_labels);
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    }

    filter->nodes  = new_array_nodes;
    filter->labels = new_array_labels;
    filter->nb_allocated_nodes = new_length;
  }

  /* Check for label duplicates */
  for (unsigned i = 0; i < filter->nb_used_nodes; i++) {
    if (_areEqualBufModelFilterLbls(filter->labels[i], label))
      LIBBLU_ERROR_RETURN(
        "Buffering model filter duplicate labels.\n"
      );
  }

  unsigned node_index = filter->nb_used_nodes++;
  filter->labels[node_index] = label;
  filter->nodes[node_index]  = node;

  return 0;
}

int addNodeToBufModelFilterNode(
  BufModelNode filter_node,
  BufModelNode node,
  BufModelFilterLbl label
)
{
  assert(isFilterBufModelNode(filter_node));

  return _addNodeToBufModelFilter(
    filter_node.linkedElement.filter,
    node,
    label
  );
}

/** \~english
 * \brief Check if input data can fill given filter without error.
 *
 * \param filter Buffering model filter to update.
 * \param timestamp Current update timestamp.
 * \param input_data Optionnal input data in bits.
 * \param filling_bitrate Input data filling bitrate.
 * \param stream_context Input data stream context.
 * \return true Input data can fill given buffering model without error.
 * \return false Input data cannot fill given buffering model, leading buffer
 * overflow or other error.
 */
static bool _checkBufModelFilter(
  BufModelOptions options,
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context,
  size_t total_input_data,
  uint64_t *delay
)
{
  assert(NULL != filter);
  assert(NULL != filter->apply);

  unsigned dest_index = filter->nb_used_nodes;

  if (0 < input_data) {
    if (filter->apply(filter, &dest_index, stream_context) < 0)
      LIBBLU_ERROR_BRETURN(
        "Buffering model filter apply() method returns error.\n"
      );

    if (filter->nb_used_nodes <= dest_index)
      LIBBLU_ERROR_BRETURN(
        "Buffering model filter method returns destination index %u out of "
        "filter destination array of length %u.\n",
        dest_index,
        filter->nb_used_nodes
      );
  }
#if !defined(BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES)
  else
    return true; /* No data to transmit, useless decision. */
#endif

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  unsigned nb_used_nodes = filter->nb_used_nodes;
  BufModelNode *nodes = filter->nodes;

  for (unsigned i = 0; i < nb_used_nodes; i++) {
    /* Only transmit input_data to destination node */
    bool ret = _checkBufModelNode(
      options,
      nodes[i],
      timestamp,
      (i == dest_index) ? input_data : 0,
      filling_bitrate,
      stream_context,
      total_input_data,
      delay
    );

    if (!ret)
      return false;
  }

  return true;
#else
  return _checkBufModelNode(
    options,
    filter->nodes[dest_index],
    timestamp,
    input_data,
    filling_bitrate,
    stream_context,
    total_input_data,
    delay
  );
#endif
}

/** \~english
 * \brief Update buffering chain state from given filter.
 *
 * \param filter Buffering model filter to update.
 * \param timestamp Current update timestamp.
 * \param input_data Optionnal input data in bits.
 * \param filling_bitrate Input data filling bitrate.
 * \param stream_context Input data stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _updateBufModelFilter(
  BufModelOptions options,
  BufModelFilterPtr filter,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
)
{
  unsigned destIndex;

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  int ret;
  unsigned i;

  BufModelNode *nodes;
  unsigned nb_used_nodes;
#endif

  assert(NULL != filter);
  assert(NULL != filter->apply);

  if (0 < input_data) {
    if (filter->apply(filter, &destIndex, stream_context) < 0)
      LIBBLU_ERROR_RETURN(
        "Buffering model filter apply() method returns error.\n"
      );

    if (filter->nb_used_nodes <= destIndex)
      LIBBLU_ERROR_RETURN(
        "Buffering model filter method returns destination index %u out of "
        "filter destination array of length %u.\n",
        destIndex,
        filter->nb_used_nodes
      );
  }
  else {
#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
    destIndex = filter->nb_used_nodes; /* Set impossible index */
#else
    return 0; /* No data to transmit, useless decision. */
#endif
  }

#if BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES
  nb_used_nodes = filter->nb_used_nodes;
  nodes = filter->nodes;

  for (i = 0; i < nb_used_nodes; i++) {
    /* Only transmit input_data to destination node */
    ret = _updateBufModelNode(
      nodes[i],
      timestamp,
      (i == destIndex) ? input_data : 0,
      filling_bitrate,
      stream_context
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
    input_data,
    filling_bitrate,
    stream_context
  );
#endif
}

/** \~english
 * \brief Check if input data can fill given buffering model from given node
 * without error.
 *
 * \param root_node Buffering model node.
 * \param timestamp Current updating timestamp.
 * \param input_data Input data amount in bits.
 * \param filling_bitrate Input data filling bitrate in bits per second.
 * \param stream_context Input data source stream context.
 * \return true Input data can fill given buffering model without error.
 * \return false Input data cannot fill given buffering model, leading buffer
 * overflow or other error.
 */
static bool _checkBufModelNode(
  BufModelOptions options,
  BufModelNode node,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context,
  size_t total_input_data,
  uint64_t *delay
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
      input_data,
      filling_bitrate,
      stream_context,
      total_input_data,
      delay
    );

  case NODE_FILTER:
    return _checkBufModelFilter(
      options,
      node.linkedElement.filter,
      timestamp,
      input_data,
      filling_bitrate,
      stream_context,
      total_input_data,
      delay
    );
  }

  return true;
}

int checkBufModel(
  BufModelOptions options,
  BufModelNode root_node,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context,
  uint64_t *delay
)
{
  return _checkBufModelNode(
    options,
    root_node,
    timestamp,
    input_data,
    filling_bitrate,
    stream_context,
    input_data,
    delay
  );
}

int updateBufModel(
  BufModelOptions options,
  BufModelNode root_node,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
)
{
  LIBBLU_DEBUG_COM(
    "Updating buffering chain at %" PRIu64 " with %zu bytes.\n",
    timestamp, input_data
  );

  return _updateBufModelNode(
    options,
    root_node,
    timestamp,
    input_data,
    filling_bitrate,
    stream_context
  );
}

void printBufModelBuffer(
  const BufModelBufferPtr buf
)
{
  lbc_printf(
    lbc_str("[%s %" PRIu64 " / %" PRIu64 "]"),
    bufferNameBufModelBuffer(buf),
    buf->header.buffer_filling_level,
    buf->header.param.buffer_size
  );

  if (buf->header.type == LEAKING_BUFFER)
    lbc_printf(lbc_str(" -/-> "));
  else
    lbc_printf(lbc_str(" -|-> "));
  printBufModelBufferingChain(buf->header.output);
}

void printBufModelFilterLbl(
  const BufModelFilterLbl label
)
{
  switch (label.type) {
  case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
    lbc_printf(lbc_str("0x%X"), label.value.number);
    break;

  case BUF_MODEL_FILTER_LABEL_TYPE_STRING:
    lbc_printf(lbc_str("%s"), label.value.string);
    break;

  case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
    lbc_printf(lbc_str("["));
    for (unsigned i = 0; i < label.value.list_length; i++) {
      if (0 < i)
        lbc_printf(lbc_str(", "));

      printBufModelFilterLbl(
        (BufModelFilterLbl) {
          .type = label.value.list_item_type,
          .value = label.value.list[i]
        }
      );
    }
    lbc_printf(lbc_str("]"));
  }
}


void printBufModelFilter(const BufModelFilterPtr filter)
{
  lbc_printf(lbc_str("Filter {\n"));
  for (unsigned i = 0; i < filter->nb_used_nodes; i++) {
    lbc_printf(lbc_str(" - "));
    printBufModelFilterLbl(filter->labels[i]);
    lbc_printf(lbc_str(" -> "));
    printBufModelBufferingChain(filter->nodes[i]);
  }
  lbc_printf(lbc_str("}\n"));
}

void printBufModelBufferingChain(const BufModelNode node)
{
  switch (node.type) {
  case NODE_VOID:
    lbc_printf(lbc_str("*void*\n"));
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
  BufModelNode node = newVoidBufModelNode();

  const BufModelBufferParameters buf_param = {
    .name = TRANSPORT_BUFFER,
    .instant_filling = false,
    .dont_overflow_output = false,
    .buffer_size = 512*8
  };

  const BufModelBufferFrame frame_param = {
    .header_size = 0,
    .data_size = 188 * 8,
    .output_data_size = 0,
    .do_not_remove = false
  };

  BufModelBuffersListPtr bufList = createBufModelBuffersList();
  if (NULL == bufList)
    return -1;

  if (createLeakingBufferNode(bufList, &node, buf_param, 0, 1000000) < 0)
    return -1;
  if (addFrameToBufferFromList(bufList, TRANSPORT_BUFFER, NULL, frame_param) < 0)
    return -1;

  // if (updateBufModel(node, 0, 188 * 8, 48000000, NULL) < 0)
  //   return -1;
  // if (updateBufModel(node, 846, 0, 48000000, NULL) < 0)
  //   return -1;

  printBufModelBufferingChain(node);

  destroyBufModel(node);
  destroyBufModelBuffersList(bufList);
  return 0;
}

#endif