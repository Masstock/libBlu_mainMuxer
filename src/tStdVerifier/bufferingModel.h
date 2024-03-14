/** \~english
 * \file bufferingModel.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief MPEG-TS T-STD buffering modelization utilities module.
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__
#define __LIBLU_MUXER__BUFFERING_MODEL__

#include "tStdVerifierError.h"
#include "../util.h"

typedef struct {
  bool abort_on_underflow;          /**< Cancel mux on buffer underflow
    error.                                                                   */
  uint64_t underflow_warn_timeout;  /**< Duration in #MAIN_CLOCK_27MHZ ticks
    during which no additional underflow warning message is printed since
    the last printed one.                                                    */
} BufModelOptions;

/** \~english
 * \brief Buffering model node types.
 */
typedef enum {
  NODE_VOID,    /**< void type, data arriving at this node is discarded.     */
  NODE_BUFFER,  /**< Buffer type, data arriving at this node fills a buffer. */
  NODE_FILTER   /**< Filter type, data arriving at this node is routed to
    a sub-branch of the buffering model.                                     */
} BufModelNodeType;

struct BufModelBuffer;
struct BufModelFilter;

/** \~english
 * \brief Buffering model node.
 *
 * Buffering model is builded on the form of a tree composed of nodes of
 * different types (buffer, filter...) representing the different components
 * of the model.
 * Data entering the model from a root and is spread on the model according
 * to nodes configuration.
 */
typedef struct {
  BufModelNodeType type;  /**< Type of the node.                             */
  union {
    struct BufModelBuffer *buffer;  /**< Node attached buffer object if
      type == #NODE_BUFFER.                                                  */
    struct BufModelFilter *filter;  /**< Node attached filter object if
      type == #NODE_FILTER.                                                  */
  } linkedElement;        /**< Node linked element.                          */
} BufModelNode;

/** \~english
 * \brief Define a #NODE_VOID #BufModelNode value.
 */
static inline BufModelNode newVoidBufModelNode(
  void
)
{
  return (BufModelNode) {
    .type = NODE_VOID
  };
}

/** \~english
 * \brief Destroy memory allocation used by node attached object.
 *
 * \param node Node whose content is to be freed.
 */
void cleanBufModelNode(
  BufModelNode node
);

/** \~english
 * \brief Destroy whole buffering model from given root node.
 *
 * \param root_node Root node of buffering model to free.
 *
 * Every element present in buffering model tree is destroyed.
 */
static inline void destroyBufModel(
  BufModelNode root_node
)
{
  return cleanBufModelNode(root_node);
}

/** \~english
 * \brief Return true if supplied node is a #NODE_VOID node.
 *
 * \param bufModelNode BufModelNode Buffering model node to check.
 * \return bool true Given node is a NODE_VOID node.
 * \return bool false Given node is not a NODE_VOID node.
 */
static inline bool isVoidBufModelNode(
  BufModelNode node
)
{
  return (NODE_VOID == node.type);
}

/** \~english
 * \brief Return true if supplied node is a #NODE_BUFFER node.
 *
 * \param bufModelNode BufModelNode Buffering model node to check.
 * \return bool true Given node is a NODE_BUFFER node.
 * \return bool false Given node is not a NODE_BUFFER node.
 */
static inline bool isBufferBufModelNode(
  BufModelNode node
)
{
  return (NODE_BUFFER == node.type);
}

/** \~english
 * \brief Return true if supplied node is a #NODE_FILTER node.
 *
 * \param bufModelNode BufModelNode Buffering model node to check.
 * \return bool true Given node is a NODE_FILTER node.
 * \return bool false Given node is not a NODE_FILTER node.
 */
static inline bool isFilterBufModelNode(
  BufModelNode node
)
{
  return (NODE_FILTER == node.type);
}

/** \~english
 * \brief Buffer output method types.
 */
typedef enum {
  LEAKING_BUFFER,      /**< Leaking method buffer.                           */
  TIME_REMOVAL_BUFFER  /**< Removal timestamp method buffer.                 */
} BufModelBufferType;

/** \~english
 * \brief Pre-defined buffer names values.
 */
typedef enum {
  TRANSPORT_BUFFER,  /**< Transport Buffer.                                  */
  MAIN_BUFFER,       /**< Main Buffer for System information / Elementary
    Stream (non-video).                                                      */
  MULTIPLEX_BUFFER,  /**< Multiplexing buffer for video Elementary Stream.   */
  ELEMENTARY_BUFFER, /**< Elementary Stream buffer for video Elementary
    Stream.                                                                  */

  CUSTOM_BUFFER      /**< Custom buffer name value, enable
    BufferParameters.custom_name field containing custom name.                */
} BufModelBufferName;

/** \~english
 * \brief Return a string representation of #BufferName values.
 *
 * \param name Pre-defined name value.
 * \return const lbc* String representation.
 */
static inline const lbc * predefinedBufferTypesName(
  BufModelBufferName name
)
{
  static const lbc *strings[] = {
    lbc_str("TB"),
    lbc_str("B"),
    lbc_str("MB"),
    lbc_str("EB")
  };

  if (name < ARRAY_SIZE(strings))
    return strings[name];
  return lbc_str("Unknown");
}


/** \~english
 * \brief Buffer parameters structure.
 */
typedef struct {
  BufModelBufferName name;    /**< Pre-defined name of the buffer.           */
  lbc *custom_name;          /**< Custom buffer name field (only used if
    BufferParameters.name == CUSTOM_BUFFER).                                 */
  uint32_t custom_name_hash;  /**< Hash of the custom name field.            */

  bool instant_filling;       /**< Buffer fills instantly with input_data.
    Otherwise supplied filling bitrate is applyed.                           */
  bool dont_overflow_output;  /**< Buffer only output data if following
    buffer is empty enought to carry it without overflowing.                 */

  size_t buffer_size;  /**< Size of the buffer in bits.                      */
} BufModelBufferParameters;

/** \~english
 * \brief Buffer carried data frames.
 */
typedef struct {
  size_t header_size;  /**< Header data of frame in bits, which will be
    discarded at frame removal from buffer.                                  */
  size_t data_size;    /**< Payload data of frame in bits, which will be
    transfered at frame removal to the following buffer (unless
    output_data_size value is not zero).                                     */

  uint64_t removal_timestamp;  /**< Removal timestamp of frame only used if
    buffer use removal timestamps ontput method.                             */

  size_t output_data_size;  /**< Optionnal output data size modifier in
    bits. If this value is non-zero, it will be transfered at frame removal
    rather than data_size value.                                             */
  bool do_not_remove;       /**< Optionnal flag, if set to true, data is not
    removed from current buffer at frame removal (as if it is copied).       */
} BufModelBufferFrame;

/** \~english
 * \brief Common buffers header structure.
 */
typedef struct {
  BufModelBufferType type;         /**< Type of the buffer.                  */
  BufModelBufferParameters param;  /**< Parameters of the buffer.            */
  BufModelNode output;             /**< Following output node. If this value
    is #NODE_VOID, data is discarded at removal.                             */

  bool is_linked;  /**< Buffer has a parent route.                           */

  size_t buffer_input_data;     /**< Buffer pending input data in bits.
    Used when BufferParameters.instant_filling == false.                     */
  size_t buffer_filling_level;  /**< Buffer filling level in bits. If this
    value exceeds BufferParameters.buffer_size value, an overflow situation
    occurs.                                                                  */

  uint64_t last_update;  /**< Last buffer updating timestamp.                */

  CircularBuffer *stored_frames;  /**< Buffer pending data frames. FIFO of
    #BufferFrame.                                                            */
} BufModelBufferCommonHeader;

/** \~english
 * \brief Generic buffer object.
 */
typedef struct BufModelBuffer {
  BufModelBufferCommonHeader header;
} BufModelBuffer, *BufModelBufferPtr;

/** \~english
 * \brief Returns buffer name.
 *
 * \param bufferPtr BufferPtr buffer.
 * \return lbc *buffer name.
 */
static inline const lbc * bufferNameBufModelBuffer(
  const BufModelBufferPtr buf
)
{
  if (CUSTOM_BUFFER == buf->header.param.name)
    return buf->header.param.custom_name;
  return predefinedBufferTypesName(buf->header.param.name);
}

/** \~english
 * \brief Destroy memory allocation used by buffer object.
 *
 * \param buf Buffer to free.
 *
 * Supplied buffer may have been created by functions #_createLeakingBuffer()
 * or #destroyRemovalBuffer().
 */
void destroyBufModelBuffer(
  BufModelBufferPtr buf
);

/** \~english
 * \brief Set output route of target buffer object.
 *
 * \param buffer Edited buffer.
 * \param output Newer destination buffering route.
 */
int setBufferOutput(
  BufModelNode buffer,
  BufModelNode output
);

/** \~english
 * \brief Add given frame to supplied buffer object.
 *
 * \param buf Destination buffer object.
 * \param frame Frame to store in destination buffer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int addFrameToBuffer(
  BufModelBufferPtr buf,
  BufModelBufferFrame frame
);

/** \~english
 * \brief Default length of #BufModelBuffersList array.
 */
#define DEFAULT_BUF_MODEL_BUFFERS_LIST_LENGTH  4

/** \~english
 * \brief #BufModelBuffersList array allocation growing macro.
 */
#define GROW_BUF_MODEL_BUFFERS_LIST_LENGTH(length)                            \
  (                                                                           \
    (length) < DEFAULT_BUF_MODEL_BUFFERS_LIST_LENGTH ?                        \
      DEFAULT_BUF_MODEL_BUFFERS_LIST_LENGTH                                   \
    :                                                                         \
      DEFAULT_BUF_MODEL_BUFFERS_LIST_LENGTH * 2                               \
  )

/** \~english
 * \typedef BufModelBuffersListPtr
 * \brief #BufModelBuffersList pointer type.
 */
/** \~english
 * \brief Buffer indexation list handling.
 */
typedef struct {
  BufModelBufferPtr *buffers;    /**< Indexed buffers.                      */
  unsigned nb_used_buffers;       /**< Number of used indexes.               */
  unsigned nb_allocated_buffers;  /**< Number of allocated indexes.          */
} BufModelBuffersList, *BufModelBuffersListPtr;

/** \~english
 * \brief Create a buffers list object.
 *
 * \return BufModelBuffersListPtr On success, created object is returned.
 * Otherwise, a NULL pointer is returned.
 *
 * Created object must be passed to #destroyBufModelBuffersList() function
 * after use.
 */
BufModelBuffersListPtr createBufModelBuffersList(
  void
);

/** \~english
 * \brief Destroy memory allocation done by #createBufModelBuffersList().
 *
 * \param list List object to free.
 *
 * \note Indexed buffers aren't destroyed. These shall be released using
 * #destroyBufModel() function.
 */
void destroyBufModelBuffersList(
  BufModelBuffersListPtr list
);

/** \~english
 * \brief Add given frame to a specified buffer in supplied list.
 *
 * \param buf_list Buffer list to fetch destination buffer from.
 * \param name Pre-defined buffer name value.
 * \param custom_buf_name Custom buffer name string if name value is set to
 * #CUSTOM_BUFFER value (otherwise shall be NULL).
 * \param frame Frame to store in destination buffer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int addFrameToBufferFromList(
  BufModelBuffersListPtr buf_list,
  BufModelBufferName name,
  const lbc *custom_buf_name,
  BufModelBufferFrame frame
);

/** \~english
 * \brief Leaking output based buffer structure.
 *
 * A leaking buffer is defined as a buffer using leaking method as output.
 */
typedef struct BufModelLeakingBuffer {
  BufModelBufferCommonHeader header;  /**< Common header.                    */

  uint64_t removal_br_per_sec;  /**< Leaking output bitrate from buffer in
    bits per second.                                                         */
  double removal_br;            /**< Leaking output bitrate from buffer in
    bits per #MAIN_CLOCK_27MHZ tick.                                         */
} BufModelLeakingBuffer, *BufModelLeakingBufferPtr;

/** \~english
 * \brief Create a buffer using leaking output method on supplied node.
 *
 * \param list Buffering model branch buffers list.
 * \param node Destination buffering model node.
 * \param param Buffer parameters.
 * \param initial_timestamp Initial zero timestamp value.
 * \param removal_br Buffer output leaking bitrate in bits per second.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int createLeakingBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode *node,
  BufModelBufferParameters param,
  uint64_t initial_timestamp,
  uint64_t removal_br
);

/** \~english
 * \brief Destroy memory allocation done by #_createLeakingBuffer().
 *
 * \param buf Buffer to free.
 */
static inline void destroyLeakingBuffer(
  BufModelLeakingBufferPtr buf
)
{
  if (NULL == buf)
    return;

  destroyBufModel(buf->header.output);
  destroyCircularBuffer(buf->header.stored_frames);
  free(buf);
}

/** \~english
 * \typedef BufModelRemovalBufferPtr
 * \brief #BufModelRemovalBuffer pointer type.
 */
/** \~english
 * \brief Removal timestamps output based buffer structure.
 *
 * Removal of buffer data is based on stored frames timestamps resolution.
 */
typedef struct {
  BufModelBufferCommonHeader header;  /**< Common header.                    */
} BufModelRemovalBuffer, *BufModelRemovalBufferPtr;

/** \~english
 * \brief Create a buffer using removal timestamps output method on
 * supplied node.
 *
 * \param list Buffering model branch buffers list.
 * \param node Destination buffering model node.
 * \param param Buffer parameters.
 * \param initial_timestamp Initial zero timestamp value.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int createRemovalBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode *node,
  BufModelBufferParameters param,
  uint64_t initial_timestamp
);

/** \~english
 * \brief Destroy memory allocation done by #createRemovalBuffer().
 *
 * \param buf Buffer to free.
 */
static inline void destroyRemovalBuffer(
  BufModelRemovalBufferPtr buf
)
{
  if (NULL == buf)
    return;

  destroyBufModel(buf->header.output);
  destroyCircularBuffer(buf->header.stored_frames);
  free(buf);
}

/** \~english
 * \brief #BufModelFilter filtering decision function prototype.
 *
 * Filtering function must check stream is non-NULL.
 * Other parameters are supposed valid.
 */
typedef int (*BufModelFilterDecisionFun) (
  struct BufModelFilter *filter,
  unsigned *idx,
  void *stream
);

/** \~english
 * \brief Default #BufModelFilter output nodes array length.
 */
#define DEFAULT_BUF_MODEL_FILTER_LENGTH 2

/** \~english
 * \brief #BufModelFilter output nodes array length growing macro.
 */
#define GROW_BUF_MODEL_FILTER_LENGTH(length)                                  \
  (                                                                           \
    (length) < DEFAULT_BUF_MODEL_FILTER_LENGTH ?                              \
      DEFAULT_BUF_MODEL_FILTER_LENGTH                                         \
    :                                                                         \
      (length) * 2                                                            \
  )

/** \~english
 * \brief Buffering model filter label types.
 */
typedef enum {
  BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC,  /**< Numeric label.                  */
  BUF_MODEL_FILTER_LABEL_TYPE_STRING,   /**< String label.                   */
  BUF_MODEL_FILTER_LABEL_TYPE_LIST      /**< List label.                     */
} BufModelFilterLblType;

/** \~english
 * \brief Return a constant string representation of #BufModelFilterLblType.
 *
 * \param type Type to return.
 * \return const lbc* String representation.
 */
static inline const char * getBufModelFilterLblTypeString(
  BufModelFilterLblType type
)
{
  static const char *strings[] = {
    "numeric",
    "string",
    "list"
  };

  if (type < ARRAY_SIZE(strings))
    return strings[type];
  return "unknown";
}

/** \~english
 * \brief Buffering model filter node label value.
 */
typedef union BufModelFilterLblValue {
  int number;             /**< Numeric value.
    Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC.                    */
  struct {
    lbc *string;          /**< String value chars.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_STRING.                   */
    uint32_t string_hash;  /**< String value hash.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_STRING.                   */
  };
  struct {
    union BufModelFilterLblValue *list;  /**< List of labels array.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_LIST.                     */
    unsigned list_length;                 /**< Length of the list array.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_LIST.                     */
    BufModelFilterLblType list_item_type; /**< Type of the list items.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_LIST.                     */
  };
} BufModelFilterLblValue;

#define BUF_MODEL_FILTER_NUMERIC_LABEL_VALUE(numeric_val)                     \
  (                                                                           \
    (BufModelFilterLblValue) {                                                \
      .number = numeric_val                                                   \
    }                                                                         \
  )

#define BUF_MODEL_FILTER_STRING_LABEL_VALUE(string_val)                       \
  (                                                                           \
    (BufModelFilterLblValue) {                                                \
      .string      = lbc_strdup(string_val),                                  \
      .string_hash = lbc_fnv1aStrHash(string_val)                             \
    }                                                                         \
  )

#define BUF_MODEL_FILTER_LIST_LABEL_VALUE(item_type, items_arr, arr_len)      \
  (                                                                           \
    (BufModelFilterLblValue) {                                                \
      .list           = items_arr,                                            \
      .list_length    = arr_len,                                              \
      .list_item_type = item_type                                             \
    }                                                                         \
  )

/** \~english
 * \brief Destroy (if required) potential memory allocated by a filter label.
 *
 * \param val Label value to release.
 * \param type Label value type.
 */
static inline void cleanBufModelFilterLblValue(
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
    for (i = 0; i < val.list_length; i++)
      cleanBufModelFilterLblValue(val.list[i], val.list_item_type);
    free(val.list);
    break;
  }
}

/** \~english
 * \brief Buffering Model filter node label structure.
 */
typedef struct {
  BufModelFilterLblType type;    /**< Type of the label value.               */
  BufModelFilterLblValue value;  /**< Label value.                           */
} BufModelFilterLbl;

/** \~english
 * \brief Return a numeric #BufModelFilterLbl.
 *
 * \param numeric_val int Numeric value.
 * \return BufModelFilterLbl Numeric label.
 */
#define BUF_MODEL_FILTER_NUMERIC_LABEL(numeric_val)                           \
  (                                                                           \
    (BufModelFilterLbl) {                                                     \
      .type  = BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC,                           \
      .value = BUF_MODEL_FILTER_NUMERIC_LABEL_VALUE(numeric_val)              \
    }                                                                         \
  )

/** \~english
 * \brief Return a string #BufModelFilterLbl.
 *
 * \param string_val const lbc *String value.
 * \return BufModelFilterLbl String label.
 *
 * \note Supplied string is duplicated.
 */
#define BUF_MODEL_FILTER_STRING_LABEL(string_val)                             \
  (                                                                           \
    (BufModelFilterLbl) {                                                     \
      .type  = BUF_MODEL_FILTER_LABEL_TYPE_STRING,                            \
      .value = BUF_MODEL_FILTER_STRING_LABEL_VALUE(string_val)                \
    }                                                                         \
  )

#define BUF_MODEL_FILTER_LIST_LABEL(item_type, item_arr, arr_len)             \
  (                                                                           \
    (BufModelFilterLbl) {                                                     \
      .type  = BUF_MODEL_FILTER_LABEL_TYPE_LIST,                              \
      .value = BUF_MODEL_FILTER_LIST_LABEL_VALUE(                             \
        item_type, item_arr, arr_len                                          \
      )                                                                       \
    }                                                                         \
  )

static inline void cleanBufModelFilterLbl(
  BufModelFilterLbl label
)
{
  cleanBufModelFilterLblValue(
    label.value,
    label.type
  );
}

/** \~english
 * \brief Create and store on destination label a list-typed label containing
 * a copy of supplied values.
 *
 * \param dst Destination label.
 * \param values_type Type of list values (see details).
 * \param values_list Values list.
 * \param values_nb Size of the list in indexes.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * If specified value type is #BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC, supplied
 * array shall be of type int *, if value type is
 * #BUF_MODEL_FILTER_LABEL_TYPE_STRING, supplied array shall be of type
 * lbc **. Other types are forbidden.
 */
int setBufModelFilterLblList(
  BufModelFilterLbl *dst,
  BufModelFilterLblType values_type,
  const void *values_list,
  const unsigned values_nb
);

/** \~english
 * \brief Buffer model filter object.
 *
 * Buffer dumping data destination filtering object. Allows switching
 * destination of data according to stream properties using user-defined
 * decision function.
 *
 * For exemple, this can be used to filter input TS packets according
 * to their PIDs, making a PID filter.
 */
typedef struct BufModelFilter {
  bool is_linked;                     /**< Filter buffering object has a
    parent route. And shall not be used as output of another route.          */

  BufModelNode *nodes;         /**< Output nodes array.                     */
  BufModelFilterLbl *labels;   /**< Output nodes labels array. Type is
    defined by labels_type.                                                  */
  unsigned nb_allocated_nodes;  /**< Output nodes array allocation
    size.                                                                    */
  unsigned nb_used_nodes;       /**< Used output nodes array entries.        */

  BufModelFilterLblType labels_type;  /**< Node labels value type.           */

  BufModelFilterDecisionFun apply;  /**< Filter application function.        */
} BufModelFilter, *BufModelFilterPtr;

/** \~english
 * \brief Create a buffering model filter on supplied node.
 *
 * \param node Destination buffering model node.
 * \param fun Stream context filtering function pointer.
 * \param labels_type Type of filter nodes attached labels.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int createBufModelFilterNode(
  BufModelNode *node,
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labels_type
);

/** \~english
 * \brief Destroy memory allocation done by #createBufModelFilterNode().
 *
 * \param filter Filter object to free.
 */
void destroyBufModelFilter(
  BufModelFilterPtr filter
);

/** \~english
 * \brief Add supplied buffering model node to filter of supplied node.
 *
 * \param filter_node Destination filter node.
 * \param node Node to add in filter outputs.
 * \param label Node attached label.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * Label must be of type defined at filter creation using
 * #createBufModelFilterNode(), otherwise an error is raised.
 *
 * If supplied node is not a #NODE_FILTER node type, behaviour is undefined.
 * Can be ensured using #isBufferBufModelNode() macro.
 */
int addNodeToBufModelFilterNode(
  BufModelNode filter_node,
  BufModelNode node,
  BufModelFilterLbl label
);

/** \~english
 * \brief Buffering model #BufModelFilter default nodes branch update trigger.
 *
 * If this macro is set to true, unborrowed nodes will be updated (without
 * filling data). Otherwise, only the destination node is updated with
 * input data.
 */
#define BUF_MODEL_UPDATE_FILTER_DEFAULT_NODES  false

/** \~english
 * \brief Check if input data can fill given buffering chain without error.
 *
 * \param root_node Buffering model node root.
 * \param timestamp Current update timestamp.
 * \param input_data Optionnal input data in bits.
 * \param filling_bitrate Input data filling bitrate.
 * \param stream_context Input data stream context.
 * \param delay TODO
 * \return true Input data can fill given buffering model without error.
 * \return false Input data cannot fill given buffering model, leading buffer
 * overflow or other error.
 */
int checkBufModel(
  BufModelOptions options,
  BufModelNode root_node,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context,
  uint64_t *delay
);

/** \~english
 * \brief Update buffering chain state from given buffering model root.
 *
 * \param root_node Buffering model node root.
 * \param timestamp Current update timestamp.
 * \param input_data Optionnal input data in bits.
 * \param filling_bitrate Input data filling bitrate.
 * \param stream_context Input data stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int updateBufModel(
  BufModelOptions options,
  BufModelNode root_node,
  uint64_t timestamp,
  size_t input_data,
  uint64_t filling_bitrate,
  void *stream_context
);

static inline bool isLinkedBufModelNode(
  const BufModelNode node
)
{
  switch (node.type) {
  case NODE_VOID:
    break;

  case NODE_BUFFER:
    return node.linkedElement.buffer->header.is_linked;

  case NODE_FILTER:
    return node.linkedElement.filter->is_linked;
  }

  return false;
}

static inline void setLinkedBufModelNode(
  BufModelNode node
)
{
  switch (node.type) {
  case NODE_VOID:
    break;
  case NODE_BUFFER:
    node.linkedElement.buffer->header.is_linked = true;
    break;
  case NODE_FILTER:
    node.linkedElement.filter->is_linked = true;
  }
}

/** \~english
 * \brief Print buffer representation on terminal for debug.
 *
 * \param buf Buffer object to print.
 */
void printBufModelBuffer(
  const BufModelBufferPtr buf
);

/** \~english
 * \brief Print filter label value representation on terminal for debug.
 *
 * \param label Label to print.
 */
void printBufModelFilterLbl(
  const BufModelFilterLbl label
);

/** \~english
 * \brief Print filter representation on terminal for debug.
 *
 * \param filter Filter object to print.
 */
void printBufModelFilter(
  const BufModelFilterPtr filter
);

/** \~english
 * \brief Print buffering model chain on terminal for debug.
 *
 * \param node Buffering model entry point node.
 */
void printBufModelBufferingChain(
  const BufModelNode node
);

#endif