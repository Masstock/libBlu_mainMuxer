/** \~english
 * \file xmlParsing.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief XML files parsing utilities module.
 *
 * \note Require inclusion of libxml headers (libxml/xmlmemory.h,
 * libxml/parser.h and libxml/xpath.h).
 */

#ifndef __LIBBLU_MUXER__UTIL__XML_PARSING_H__
#define __LIBBLU_MUXER__UTIL__XML_PARSING_H__

#include "common.h"
#include "messages.h"

/** \~english
 * \brief libXML boolean true macro.
 */
#define XML_TRUE  1

/** \~english
 * \brief libXML boolean false macro.
 */
#define XML_FALSE 0

/** \~english
 * \brief Get number of set nodes in given xmlPathObject.
 *
 * If given pointer is NULL, value is zero.
 * Otherwise, value is a positive int.
 */
#define XML_PATH_NODE_NB(xmlXPathObjectPtr)                                   \
  (                                                                           \
    (                                                                         \
      NULL != (xmlXPathObjectPtr)                                             \
      && NULL != (xmlXPathObjectPtr)->nodesetval                              \
    ) ?                                                                       \
    (xmlXPathObjectPtr)->nodesetval->nodeNr                                   \
    : 0                                                                       \
  )

/** \~english
 * \brief Set boolean true if given set node int idx exists in
 * given xmlPathObject.
 *
 * If given pointer is NULL, does not carry any node or if
 * given index is out of range, value is false.
 * Otherwise, value is true.
 */
#define XML_PATH_NODE_EXISTS(xmlXPathObjectPtr, idx)                          \
  (                                                                           \
    NULL != (xmlXPathObjectPtr)                                               \
    && NULL != (xmlXPathObjectPtr)->nodesetval                                \
    && 0 <= (idx)                                                             \
    && (idx) < XML_PATH_NODE_NB(xmlXPathObjectPtr)                            \
  )

/** \~english
 * \brief Get set node of given int idx from given xmlPathObject.
 *
 * If given node does not exists, value is the NULL pointer.
 * Otherwise, value is a xmlNodePtr.
 */
#define XML_PATH_NODE(xmlXPathObjectPtr, idx)                                 \
  (                                                                           \
    (                                                                         \
      XML_PATH_NODE_EXISTS(xmlXPathObjectPtr, idx)                            \
    ) ?                                                                       \
    (xmlXPathObjectPtr)->nodesetval->nodeTab[(idx)]                           \
    : NULL                                                                    \
  )

/** \~english
 * \brief Get xmlDocPtr doc field from xmlNodePtr.
 */
#define XML_DOC(xmlNodePtr)                                                   \
  (                                                                           \
    (xmlNodePtr)->doc                                                         \
  )

/** \~english
 * \brief Get xmlNodePtr children field from xmlNodePtr.
 */
#define XML_CHILDREN_NODE(xmlNodePtr)                                         \
  (                                                                           \
    (xmlNodePtr)->children                                                    \
  )

/** \~english
 * \brief Accessor to #XmlCtxPtr libXml context.
 *
 * Value is of type xmlDocPtr.
 */
#define LIBXML_CONTEXT(XmlCtxPtr)                                             \
  (                                                                           \
    (XmlCtxPtr)->libxmlCtx                                                    \
  )

#define XML_MAX_SAVED_PATHS 5

typedef struct {
  xmlDocPtr libxmlCtx;

  void (* errorLogFun) (const lbc * format, ...);
  void (* debugLogFun) (const lbc * format, ...);

  /* Used when call setRootPathFromPathObjectIgsXmlFile(): */
  xmlNodePtr savedNodesStack[XML_MAX_SAVED_PATHS];
  unsigned savedNodesStackHeight;

  unsigned lastParsedNodeLine;
} XmlCtx, *XmlCtxPtr;

#define XML_ERROR_LOG_FUN(format, ...)                                        \
  ((*(ctx->errorLogFun))(lbc_str(format), ##__VA_ARGS__))

#define XML_DEBUG_LOG_FUN(format, ...)                                        \
  ((*(ctx->debugLogFun))(lbc_str(format), ##__VA_ARGS__))

XmlCtxPtr createXmlContext(
  void (* errorLogFun) (const lbc * format, ...),
  void (* debugLogFun) (const lbc * format, ...)
);

static inline void destroyXmlContext(
  XmlCtxPtr ctx
)
{
  if (NULL == ctx)
    return;

  free(ctx);
}

/** \~english
 * \brief Loads in given context IGS XML description file.
 *
 * \param ctx Used Context for loading.
 * \param filePath Filepath to XML file.
 * \return int A zero value on success, otherwise a negative value.
 */
int loadIgsXmlFile(
  XmlCtxPtr ctx,
  const lbc * filePath
);

/** \~english
 * \brief Close and release loaded IGS XML description file.
 *
 * \param ctx Context containing a XML file opened by #loadIgsXmlFile().
 */
void releaseIgsXmlFile(
  XmlCtxPtr ctx
);

/** \~english
 * \brief Return path object referenced by given XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return xmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
xmlXPathObjectPtr getPathObjectIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * path
);

/** \~english
 * \brief Return path object referenced by given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return xmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
xmlXPathObjectPtr getPathObjectFromExprVaIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Return path object referenced by given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return xmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
xmlXPathObjectPtr getPathObjectFromExprIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * pathFormat,
  ...
);

/** \~english
 * \brief Define an intermediate new root of XML path.
 *
 * \param ctx Context to use.
 * \param pathObj Path object to use as a new root.
 * \param idx Path node set index to use.
 * \return int A zero value on success, otherwise a negative value.
 *
 * Old root is stored in context and can be retrived by
 * #restoreLastRootIgsXmlFile().
 * Multiple recursive roots can be used, older ones are stored
 * in a LIFO pile (Last In First Out pile).
 */
int setRootPathFromPathObjectIgsXmlFile(
  XmlCtxPtr ctx,
  xmlXPathObjectPtr pathObj,
  int idx
);

/** \~english
 * \brief Restore last old #setRootPathFromPathObjectIgsXmlFile() saved root.
 *
 * \param ctx Context to use.
 * \return int A zero value on success, otherwise a negative value.
 */
int restoreLastRootIgsXmlFile(
  const XmlCtxPtr ctx
);

/** \~english
 * \brief Fetch and return a string from given XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \param idx Path node set index to use.
 * \return xmlChar* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use using #freeXmlCharPtr().
 */
xmlChar * getStringIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * path,
  int idx
);

/** \~english
 * \brief Fetch and return a string from given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return xmlChar* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use using #freeXmlCharPtr().
 */
xmlChar * getStringFromExprVaIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Fetch and return a string from given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return xmlChar* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use using #freeXmlCharPtr().
 */
xmlChar * getStringFromExprIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * pathFormat,
  ...
);

/** \~english
 * \brief Release and reset given string result.
 *
 * \param string String result to release.
 *
 * Set pointer to NULL after free.
 */
void freeXmlCharPtr(
  xmlChar ** string
);

/** \~english
 * \brief Return the number of objects corresponding to given
 * XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int Positive or zero on success (or a negative value otherwise).
 */
int getNbObjectsIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * path
);

/** \~english
 * \brief Return the number of objects corresponding to given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int Positive or zero on success (or a negative value otherwise).
 */
int getNbObjectsFromExprVaIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Return the number of objects corresponding to given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int Positive or zero on success (or a negative value otherwise).
 */
int getNbObjectsFromExprIgsXmlFile(
  const XmlCtxPtr ctx,
  const char * pathFormat,
  ...
);

/** \~english
 * \brief Fetch a string from given XPath expression (or use default value).
 *
 * \param string String result of request.
 * \param def Default string pointer used if no object is founded (can be NULL).
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int A zero value on success, otherwise a negative value.
 *
 * String pointer must be freed after use using #freeXmlCharPtr().
 */
int getIfExistsStringIgsXmlFile(
  XmlCtxPtr ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path
);

/** \~english
 * \brief Fetch a string from given XPath expression (or use default value)
 * with arguments pile.
 *
 * \param string String result of request.
 * \param def Default string pointer used if no object is founded (can be NULL).
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int A zero value on success, otherwise a negative value.
 *
 * String pointer must be freed after use using #freeXmlCharPtr().
 */
int getIfExistsStringFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Fetch a string from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param string String result of request.
 * \param def Default string pointer used if no object is founded (can be NULL).
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 *
 * String pointer must be freed after use using #freeXmlCharPtr().
 */
int getIfExistsStringFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * pathFormat,
  ...
);

/* ### Signed integers : ################################################### */

int getIfExistsInt64FromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  int64_t * dst,
  int64_t def,
  const char * pathFormat,
  va_list args
);

int getIfExistsInt64FromExprIgsXmlFile(
  XmlCtxPtr ctx,
  int64_t * dst,
  int64_t def,
  const char * pathFormat,
  ...
);

/** \~english
 * \brief Fetch a long from given XPath expression (or use default value).
 *
 * \param dst Long result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsLongFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  long * dst,
  long def,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Fetch a long from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param dst Long result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsLongFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  long * dst,
  long def,
  const char * pathFormat,
  ...
);

/** \~english
 * \brief Fetch an int from given XPath expression (or use default value)
 * with arguments pile.
 *
 * \param dst Int result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsIntegerFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  int * dst,
  int def,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Fetch an int from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param dst Int result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsIntegerFromExprIgsXmlFile(
  const XmlCtxPtr ctx,
  int * dst,
  int def,
  const char * pathFormat,
  ...
);

/* ### Boolean value : ##################################################### */

/** \~english
 * \brief Fetch a bool from given XPath expression (or use default value)
 * with arguments pile.
 *
 * \param dst Boolean result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsBooleanFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  bool * dst,
  bool def,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Fetch a bool from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param dst Boolean result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsBooleanFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  bool * dst,
  bool def,
  const char * pathFormat,
  ...
);

/* ### Floating point : #################################################### */

int getIfExistsDoubleFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  double * dst,
  double def,
  const char * pathFormat,
  va_list args
);

int getIfExistsDoubleFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  double * dst,
  double def,
  const char * pathFormat,
  ...
);

int getIfExistsFloatFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  float * dst,
  float def,
  const char * pathFormat,
  va_list args
);

int getIfExistsFloatFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  float * dst,
  float def,
  const char * pathFormat,
  ...
);

/* ### Unsigned integers : ################################################# */

int getIfExistsUint64FromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  uint64_t * dst,
  uint64_t def,
  const char * pathFormat,
  va_list args
);

int getIfExistsUint64FromExprIgsXmlFile(
  XmlCtxPtr ctx,
  uint64_t * dst,
  uint64_t def,
  const char * pathFormat,
  ...
);

/** \~english
 * \brief Fetch a unsigned int from given XPath expression (or use default
 * value).
 *
 * \param dst Unsigned int result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsUnsignedFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  unsigned * dst,
  unsigned def,
  const char * pathFormat,
  va_list args
);

/** \~english
 * \brief Fetch a unsigned int from given XPath expression (or use default
 * value) accepting arguments.
 *
 * \param dst Unsigned int result destination of request.
 * \param def Default value used if no object is founded.
 * \param ctx Context to use.
 * \param pathFormat XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsUnsignedFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  unsigned * dst,
  unsigned def,
  const char * pathFormat,
  ...
);

/* ######################################################################### */

/** \~english
 * \brief #getNbObjectsFromExprIgsXmlFile() error printing function.
 *
 * \param ret #getNbObjectsFromExprIgsXmlFile() returned value.
 * \param pathFormat XPath expression.
 * \param ... Additional path formatting arguments.
 *
 * This function can be used if expected number of objects is
 * not satified (zero for exemple).
 */
void printNbObjectsFromExprErrorIgsXmlFile(
  int ret,
  const char * pathFormat,
  ...
);

#endif