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
  bool abortOnUnderflow;          /**< Cancel mux on buffer underflow error. */
  uint64_t underflowWarnTimeout;  /**< Duration in #MAIN_CLOCK_27MHZ ticks
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
    struct BufModelBuffer * buffer;  /**< Node attached buffer object if
      type == #NODE_BUFFER.                                                  */
    struct BufModelFilter * filter;  /**< Node attached filter object if
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
 * \param rootNode Root node of buffering model to free.
 *
 * Every element present in buffering model tree is destroyed.
 */
static inline void destroyBufModel(
  BufModelNode rootNode
)
{
  return cleanBufModelNode(rootNode);
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
    BufferParameters.customName field containing custom name.                */
} BufModelBufferName;

/** \~english
 * \brief Return a string representation of #BufferName values.
 *
 * \param name Pre-defined name value.
 * \return const char* String representation.
 */
static inline const char * predefinedBufferTypesName(
  BufModelBufferName name
)
{
  static const char * strings[] = {
    "TB", "B", "MB", "EB"
  };

  if (name < ARRAY_SIZE(strings))
    return strings[name];
  return "Unknown";
}


/** \~english
 * \brief Buffer parameters structure.
 */
typedef struct {
  BufModelBufferName name;          /**< Pre-defined name of the buffer.     */
  char * customName;        /**< Custom buffer name field (only used if
    BufferParameters.name == CUSTOM_BUFFER).                                 */
  uint32_t customNameHash;  /**< Hash of the custom name field.              */

  bool instantFilling;      /**< Buffer fills instantly with inputData.
    Otherwise supplied filling bitrate is applyed.                           */
  bool dontOverflowOutput;  /**< Buffer only output data if following
    buffer is empty enought to carry it without overflowing.                 */

  size_t bufferSize;        /**< Size of the buffer in bits.                 */
} BufModelBufferParameters;

/** \~english
 * \brief Buffer carried data frames.
 */
typedef struct {
  size_t headerSize;          /**< Header data of frame in bits, which
    will be discarded at frame removal from buffer.                          */
  size_t dataSize;            /**< Payload data of frame in bits, which
    will be transfered at frame removal to the following buffer (unless
    outputDataSize value is not zero).                                       */

  uint64_t removalTimestamp;  /**< Removal timestamp of frame only used if
    buffer use removal timestamps ontput method.                             */

  size_t outputDataSize;      /**< Optionnal output data size modifier
    in bits. If this value is non-zero, it will be transfered at frame
    removal rather than dataSize value.                                      */
  bool doNotRemove;           /**< Optionnal flag, if set to true, data is
    not removed from current buffer at frame removal (as if it is copied).   */
} BufModelBufferFrame;

/** \~english
 * \brief Common buffers header structure.
 */
typedef struct {
  BufModelBufferType type;         /**< Type of the buffer.                  */
  BufModelBufferParameters param;  /**< Parameters of the buffer.            */
  BufModelNode output;             /**< Following output node. If this value
    is #NODE_VOID, data is discarded at removal.                             */

  bool isLinked;                   /**< Buffer has a parent route.           */

  size_t bufferInputData;          /**< Buffer pending input data in bits.
    Used when BufferParameters.instantFilling == false.                      */
  size_t bufferFillingLevel;       /**< Buffer filling level in bits. If
    this value exceeds BufferParameters.bufferSize value, an overflow
    situation occurs.                                                        */

  uint64_t lastUpdate;             /**< Last buffer updating timestamp.      */

  CircularBuffer * storedFrames;  /**< Buffer pending data frames. FIFO
    of #BufferFrame.                                                         */
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
 * \return char * buffer name.
 */
static inline const char * bufferNameBufModelBuffer(
  const BufModelBufferPtr buf
)
{
  if (CUSTOM_BUFFER == buf->header.param.name)
    return buf->header.param.customName;
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
#define DEFAULT_BUF_MODEL_BUFFERS_LIST_LENGTH 4

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
  BufModelBufferPtr * buffers;  /**< Indexed buffers.                        */
  unsigned nbUsedBuffers;       /**< Number of used indexes.                 */
  unsigned nbAllocatedBuffers;  /**< Number of allocated indexes.            */
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
 * \param bufList Buffer list to fetch destination buffer from.
 * \param name Pre-defined buffer name value.
 * \param customBufName Custom buffer name string if name value is set to
 * #CUSTOM_BUFFER value (otherwise shall be NULL).
 * \param frame Frame to store in destination buffer.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int addFrameToBufferFromList(
  BufModelBuffersListPtr bufList,
  BufModelBufferName name,
  const char * customBufName,
  BufModelBufferFrame frame
);

/** \~english
 * \brief Leaking output based buffer structure.
 *
 * A leaking buffer is defined as a buffer using leaking method as output.
 */
typedef struct BufModelLeakingBuffer {
  BufModelBufferCommonHeader header;  /**< Common header.                    */

  uint64_t removalBitratePerSec;      /**< Leaking output bitrate from
    buffer in bits per second.                                               */
  double removalBitrate;              /**< Leaking output bitrate from
    buffer in bits per #MAIN_CLOCK_27MHZ tick.                                */
} BufModelLeakingBuffer, *BufModelLeakingBufferPtr;

/** \~english
 * \brief Create a buffer using leaking output method on supplied node.
 *
 * \param list Buffering model branch buffers list.
 * \param node Destination buffering model node.
 * \param param Buffer parameters.
 * \param initialTimestamp Initial zero timestamp value.
 * \param removalBitrate Buffer output leaking bitrate in bits per second.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int createLeakingBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode * node,
  BufModelBufferParameters param,
  uint64_t initialTimestamp,
  uint64_t removalBitrate
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
  destroyCircularBuffer(buf->header.storedFrames);
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
 * \param initialTimestamp Initial zero timestamp value.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int createRemovalBufferNode(
  BufModelBuffersListPtr list,
  BufModelNode * node,
  BufModelBufferParameters param,
  uint64_t initialTimestamp
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
  destroyCircularBuffer(buf->header.storedFrames);
  free(buf);
}

/** \~english
 * \brief #BufModelFilter filtering decision function prototype.
 *
 * Filtering function must check stream is non-NULL.
 * Other parameters are supposed valid.
 */
typedef int (*BufModelFilterDecisionFun) (
  struct BufModelFilter * filter,
  unsigned * idx,
  void * stream
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
 * \return const char* String representation.
 */
static inline const char * getBufModelFilterLblTypeString(
  BufModelFilterLblType type
)
{
  static const char * strings[] = {
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
    char * string;        /**< String value chars.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_STRING.                   */
    uint32_t stringHash;  /**< String value hash.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_STRING.                   */
  };
  struct {
    union BufModelFilterLblValue * list;  /**< List of labels array.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_LIST.                     */
    unsigned listLength;                  /**< Length of the list array.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_LIST.                     */
    BufModelFilterLblType listItemsType;  /**< Type of the list items.
      Used if type == #BUF_MODEL_FILTER_LABEL_TYPE_LIST.                     */
  };
} BufModelFilterLblValue;

#define BUF_MODEL_FILTER_NUMERIC_LABEL_VALUE(numericVal)                      \
  (                                                                           \
    (BufModelFilterLblValue) {                                                \
      .number = numericVal                                                    \
    }                                                                         \
  )

#define BUF_MODEL_FILTER_STRING_LABEL_VALUE(stringVal)                        \
  (                                                                           \
    (BufModelFilterLblValue) {                                                \
      .string     = lb_str_dup(stringVal),                                     \
      .stringHash = fnv1aStrHash(stringVal)                                   \
    }                                                                         \
  )

#define BUF_MODEL_FILTER_LIST_LABEL_VALUE(itemsType, itemsArray, arrayLength) \
  (                                                                           \
    (BufModelFilterLblValue) {                                                \
      .list          = itemsArray,                                            \
      .listLength    = arrayLength,                                           \
      .listItemsType = itemsType                                              \
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
    for (i = 0; i < val.listLength; i++)
      cleanBufModelFilterLblValue(val.list[i], val.listItemsType);
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
 * \param numericVal int Numeric value.
 * \return BufModelFilterLbl Numeric label.
 */
#define BUF_MODEL_FILTER_NUMERIC_LABEL(numericVal)                            \
  (                                                                           \
    (BufModelFilterLbl) {                                                     \
      .type  = BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC,                           \
      .value = BUF_MODEL_FILTER_NUMERIC_LABEL_VALUE(numericVal)               \
    }                                                                         \
  )

/** \~english
 * \brief Return a string #BufModelFilterLbl.
 *
 * \param stringVal const char * String value.
 * \return BufModelFilterLbl String label.
 *
 * \note Supplied string is duplicated.
 */
#define BUF_MODEL_FILTER_STRING_LABEL(stringVal)                              \
  (                                                                           \
    (BufModelFilterLbl) {                                                     \
      .type  = BUF_MODEL_FILTER_LABEL_TYPE_STRING,                            \
      .value = BUF_MODEL_FILTER_STRING_LABEL_VALUE(stringVal)                 \
    }                                                                         \
  )

#define BUF_MODEL_FILTER_LIST_LABEL(itemsType, itemsArray, arrayLength)       \
  (                                                                           \
    (BufModelFilterLbl) {                                                     \
      .type  = BUF_MODEL_FILTER_LABEL_TYPE_LIST,                              \
      .value = BUF_MODEL_FILTER_LIST_LABEL_VALUE(                             \
        itemsType, itemsArray, arrayLength                                    \
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
 * \param valuesType Type of list values (see details).
 * \param valuesList Values list.
 * \param valuesNb Size of the list in indexes.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * If specified value type is #BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC, supplied
 * array shall be of type int *, if value type is
 * #BUF_MODEL_FILTER_LABEL_TYPE_STRING, supplied array shall be of type
 * char **. Other types are forbidden.
 */
int setBufModelFilterLblList(
  BufModelFilterLbl * dst,
  BufModelFilterLblType valuesType,
  const void * valuesList,
  const unsigned valuesNb
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
  bool isLinked;                     /**< Filter buffering object has a
    parent route. And shall not be used as output of another route.          */

  BufModelNode * nodes;              /**< Output nodes array.                */
  BufModelFilterLbl * labels;        /**< Output nodes labels array. Type is
    defined by labelsType.                                                   */
  unsigned nbAllocatedNodes;         /**< Output nodes array allocation
    size.                                                                    */
  unsigned nbUsedNodes;              /**< Used output nodes array entries.   */

  BufModelFilterLblType labelsType;  /**< Node labels value type.            */

  BufModelFilterDecisionFun apply;   /**< Filter application function.       */
} BufModelFilter, *BufModelFilterPtr;

/** \~english
 * \brief Create a buffering model filter on supplied node.
 *
 * \param node Destination buffering model node.
 * \param fun Stream context filtering function pointer.
 * \param labelsType Type of filter nodes attached labels.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int createBufModelFilterNode(
  BufModelNode * node,
  BufModelFilterDecisionFun fun,
  BufModelFilterLblType labelsType
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
 * \param filterNode Destination filter node.
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
  BufModelNode filterNode,
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
 * \param rootNode Buffering model node root.
 * \param timestamp Current update timestamp.
 * \param inputData Optionnal input data in bits.
 * \param fillingBitrate Input data filling bitrate.
 * \param streamContext Input data stream context.
 * \param delay TODO
 * \return true Input data can fill given buffering model without error.
 * \return false Input data cannot fill given buffering model, leading buffer
 * overflow or other error.
 */
int checkBufModel(
  BufModelOptions options,
  BufModelNode rootNode,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext,
  uint64_t * delay
);

/** \~english
 * \brief Update buffering chain state from given buffering model root.
 *
 * \param rootNode Buffering model node root.
 * \param timestamp Current update timestamp.
 * \param inputData Optionnal input data in bits.
 * \param fillingBitrate Input data filling bitrate.
 * \param streamContext Input data stream context.
 * \return int On success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int updateBufModel(
  BufModelOptions options,
  BufModelNode rootNode,
  uint64_t timestamp,
  size_t inputData,
  uint64_t fillingBitrate,
  void * streamContext
);

static inline bool isLinkedBufModelNode(
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

static inline void setLinkedBufModelNode(
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