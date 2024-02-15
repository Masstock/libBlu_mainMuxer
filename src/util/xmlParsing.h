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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

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
  ((NULL == (xmlXPathObjectPtr)                                               \
    && NULL == (xmlXPathObjectPtr)->nodesetval                                \
   ) ? 0 : (xmlXPathObjectPtr)->nodesetval->nodeNr)

/** \~english
 * \brief Set boolean true if given set node int idx exists in
 * given xmlPathObject.
 *
 * If given pointer is NULL, does not carry any node or if
 * given index is out of range, value is false.
 * Otherwise, value is true.
 */
#define XML_PATH_NODE_EXISTS(xmlXPathObjectPtr, idx)                          \
  (0 <= (idx) && (idx) < XML_PATH_NODE_NB(xmlXPathObjectPtr))

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

#define XML_MAX_SAVED_PATHS  16

typedef struct {
  xmlDocPtr libxml_ctx;

  /* Used when call setRootPathFromPathObjectIgsXmlFile(): */
  xmlNodePtr saved_nodes_stack[XML_MAX_SAVED_PATHS];
  unsigned saved_nodes_stack_height;

  unsigned last_parsed_node_line;
} XmlCtx;

static inline void cleanXmlContext(
  XmlCtx ctx
)
{
  while (0 < ctx.saved_nodes_stack_height--) {
    xmlNodePtr node = ctx.saved_nodes_stack[ctx.saved_nodes_stack_height];
    xmlFreeNode(xmlDocSetRootElement(ctx.libxml_ctx, node));
  }
}

/** \~english
 * \brief Loads in given context IGS XML description file.
 *
 * \param ctx Used Context for loading.
 * \param filePath Filepath to XML file.
 * \return int A zero value on success, otherwise a negative value.
 */
int loadIgsXmlFile(
  XmlCtx * ctx,
  const lbc * filePath
);

/** \~english
 * \brief Close and release loaded IGS XML description file.
 *
 * \param ctx Context containing a XML file opened by #loadIgsXmlFile().
 */
void releaseIgsXmlFile(
  XmlCtx * ctx
);

/** \~english
 * \brief Return path object referenced by given XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return xmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
xmlXPathObjectPtr getPathObjectIgsXmlFile(
  XmlCtx * ctx,
  const char * path
);

/** \~english
 * \brief Return path object referenced by given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return xmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
xmlXPathObjectPtr getPathObjectFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Return path object referenced by given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return xmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
xmlXPathObjectPtr getPathObjectFromExprIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  ...
);

/** \~english
 * \brief Define an intermediate new root of XML path.
 *
 * \param ctx Context to use.
 * \param xpath_obj Path object to use as a new root.
 * \param idx Path node set index to use.
 * \return int A zero value on success, otherwise a negative value.
 *
 * Old root is stored in context and can be retrived by
 * #restoreLastRootIgsXmlFile().
 * Multiple recursive roots can be used, older ones are stored
 * in a LIFO pile (Last In First Out pile).
 */
int setRootPathFromPathObjectIgsXmlFile(
  XmlCtx * ctx,
  xmlXPathObjectPtr xpath_obj,
  int idx
);

/** \~english
 * \brief Restore last old #setRootPathFromPathObjectIgsXmlFile() saved root.
 *
 * \param ctx Context to use.
 * \return int A zero value on success, otherwise a negative value.
 */
int restoreLastRootIgsXmlFile(
  XmlCtx * ctx
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
  XmlCtx * ctx,
  const char * path,
  int idx
);

/** \~english
 * \brief Fetch and return a string from given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return xmlChar* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use using #freeXmlCharPtr().
 */
xmlChar * getStringFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Fetch and return a string from given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return xmlChar* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use using #freeXmlCharPtr().
 */
xmlChar * getStringFromExprIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
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
  XmlCtx * ctx,
  const char * path
);

/** \~english
 * \brief Return the number of objects corresponding to given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int Positive or zero on success (or a negative value otherwise).
 */
int getNbObjectsFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Return the number of objects corresponding to given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int Positive or zero on success (or a negative value otherwise).
 */
int getNbObjectsFromExprIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  ...
);

/** \~english
 * \brief Fetch a string from given XPath expression (or use default value).
 *
 * \param string String result of request.
 * \param def Default string pointer used if no object is found (can be NULL).
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int A zero value on success, otherwise a negative value.
 *
 * String pointer must be freed after use using #freeXmlCharPtr().
 */
int getIfExistsStringIgsXmlFile(
  XmlCtx * ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path
);

/** \~english
 * \brief Fetch a string from given XPath expression (or use default value)
 * with arguments pile.
 *
 * \param string String result of request.
 * \param def Default string pointer used if no object is found (can be NULL).
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int A zero value on success, otherwise a negative value.
 *
 * String pointer must be freed after use using #freeXmlCharPtr().
 */
int getIfExistsStringFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Fetch a string from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param string String result of request.
 * \param def Default string pointer used if no object is found (can be NULL).
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 *
 * String pointer must be freed after use using #freeXmlCharPtr().
 */
int getIfExistsStringFromExprIgsXmlFile(
  XmlCtx * ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path_format,
  ...
);

/* ### Signed integers : ################################################### */

int getIfExistsInt64FromExprVaIgsXmlFile(
  XmlCtx * ctx,
  int64_t * dst,
  int64_t def,
  const char * path_format,
  va_list args
);

int getIfExistsInt64FromExprIgsXmlFile(
  XmlCtx * ctx,
  int64_t * dst,
  int64_t def,
  const char * path_format,
  ...
);

/** \~english
 * \brief Fetch a long from given XPath expression (or use default value).
 *
 * \param dst Long result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsLongFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  long * dst,
  long def,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Fetch a long from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param dst Long result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsLongFromExprIgsXmlFile(
  XmlCtx * ctx,
  long * dst,
  long def,
  const char * path_format,
  ...
);

/** \~english
 * \brief Fetch an int from given XPath expression (or use default value)
 * with arguments pile.
 *
 * \param dst Int result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsIntegerFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  int * dst,
  int def,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Fetch an int from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param dst Int result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsIntegerFromExprIgsXmlFile(
  XmlCtx * ctx,
  int * dst,
  int def,
  const char * path_format,
  ...
);

/* ### Boolean value : ##################################################### */

/** \~english
 * \brief Fetch a bool from given XPath expression (or use default value)
 * with arguments pile.
 *
 * \param dst Boolean result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsBooleanFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  bool * dst,
  bool def,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Fetch a bool from given XPath expression (or use default value)
 * accepting arguments.
 *
 * \param dst Boolean result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsBooleanFromExprIgsXmlFile(
  XmlCtx * ctx,
  bool * dst,
  bool def,
  const char * path_format,
  ...
);

/* ### Floating point : #################################################### */

int getIfExistsDoubleFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  double * dst,
  double def,
  const char * path_format,
  va_list args
);

int getIfExistsDoubleFromExprIgsXmlFile(
  XmlCtx * ctx,
  double * dst,
  double def,
  const char * path_format,
  ...
);

int getIfExistsFloatFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  float * dst,
  float def,
  const char * path_format,
  va_list args
);

int getIfExistsFloatFromExprIgsXmlFile(
  XmlCtx * ctx,
  float * dst,
  float def,
  const char * path_format,
  ...
);

/* ### Unsigned integers : ################################################# */

int getIfExistsUint64FromExprVaIgsXmlFile(
  XmlCtx * ctx,
  uint64_t * dst,
  uint64_t def,
  const char * path_format,
  va_list args
);

int getIfExistsUint64FromExprIgsXmlFile(
  XmlCtx * ctx,
  uint64_t * dst,
  uint64_t def,
  const char * path_format,
  ...
);

/** \~english
 * \brief Fetch a unsigned int from given XPath expression (or use default
 * value).
 *
 * \param dst Unsigned int result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsUnsignedFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  unsigned * dst,
  unsigned def,
  const char * path_format,
  va_list args
);

/** \~english
 * \brief Fetch a unsigned int from given XPath expression (or use default
 * value) accepting arguments.
 *
 * \param dst Unsigned int result destination of request.
 * \param def Default value used if no object is found.
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return int A zero value on success, otherwise a negative value.
 */
int getIfExistsUnsignedFromExprIgsXmlFile(
  XmlCtx * ctx,
  unsigned * dst,
  unsigned def,
  const char * path_format,
  ...
);

#endif