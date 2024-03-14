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
#include "macros.h"

/* ### Types : ############################################################# */

typedef struct _XmlCtx *XmlCtxPtr;
typedef struct _xmlXPathObject *XmlXPathObjectPtr;
typedef struct _xmlNode *XmlNodePtr;

/* ### Context initialization/destruction : ################################ */

XmlCtxPtr createXmlContext(
  void
);

void destroyXmlContext(
  XmlCtxPtr ctx
);

/** \~english
 * \brief Loads in given context IGS XML description file.
 *
 * \param ctx Used Context for loading.
 * \param filePath Filepath to XML file.
 * \return int A zero value on success, otherwise a negative value.
 */
int loadXmlCtx(
  XmlCtxPtr ctx,
  const lbc *filePath
);

/** \~english
 * \brief Close and release loaded IGS XML description file.
 *
 * \param ctx Context containing a XML file opened by #loadXmlCtx().
 */
void releaseXmlCtx(
  XmlCtxPtr ctx
);

/* ### Context status : #################################################### */

unsigned lastParsedNodeLineXmlCtx(
  const XmlCtxPtr ctx
);

/* ### XML Node : ########################################################## */

const lbc *getNameXmlNode(
  const XmlNodePtr node
);

/* ### XML XPath : ######################################################### */

/** \~english
 * \brief Get number of nodes in given XML XPath object.
 *
 * If given pointer is NULL, value is zero.
 * Otherwise, value is a positive int.
 */
unsigned getNbNodesXmlXPathObject(
  const XmlXPathObjectPtr xpath_obj
);

/** \~english
 * \brief Return true if given node exists in given XML XPath object.
 *
 * \param xpath_obj XPath object.
 * \param node_idx Index of the looked node.
 * \return True if the node exists. False otherwise if the node does not exists
 * or given object pointer is NULL.
 */
static inline bool existsNodeXmlXPathObject(
  const XmlXPathObjectPtr xpath_obj,
  unsigned node_idx
)
{
  return (node_idx < getNbNodesXmlXPathObject(xpath_obj));
}

/** \~english
 * \brief Get a node from given XML XPath object.
 *
 * \param xpath_obj XPath object.
 * \param node_idx Index of the fetched node.
 * \return XmlNodePtr Looked XML node on sucess, otherwise a NULL pointer is
 * returned on error.
 */
XmlNodePtr getNodeXmlXPathObject(
  const XmlXPathObjectPtr xpath_obj,
  unsigned node_idx
);

/** \~english
 * \brief Return path object referenced by given XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return XmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
XmlXPathObjectPtr getPathObjectXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path
);

/** \~english
 * \brief Return path object referenced by given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return XmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
XmlXPathObjectPtr getPathObjectFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  va_list args
);

/** \~english
 * \brief Return path object referenced by given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return XmlXPathObjectPtr PathObject of request (or NULL pointer).
 */
XmlXPathObjectPtr getPathObjectFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
);

bool existsPathObjectFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
);

void destroyXmlXPathObject(
  XmlXPathObjectPtr xpath_obj
);

/* ### Root definition : ################################################### */

/** \~english
 * \brief Define an intermediate new root of XML path.
 *
 * \param ctx Context to use.
 * \param xpath_obj Path object to use as a new root.
 * \param idx Path node set index to use.
 * \return int A zero value on success, otherwise a negative value.
 *
 * Old root is stored in context and can be retrived by
 * #restoreLastRootXmlCtx().
 * Multiple recursive roots can be used, older ones are stored
 * in a LIFO pile (Last In First Out pile).
 */
int setRootPathFromPathObjectXmlCtx(
  XmlCtxPtr ctx,
  XmlXPathObjectPtr xpath_obj,
  int idx
);

/** \~english
 * \brief Restore last old #setRootPathFromPathObjectXmlCtx() saved root.
 *
 * \param ctx Context to use.
 * \return int A zero value on success, otherwise a negative value.
 */
int restoreLastRootXmlCtx(
  XmlCtxPtr ctx
);

/* ### Values parsing : #################################################### */

/** \~english
 * \brief Fetch and return a string from given XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \param idx Path node set index to use.
 * \return lbc* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use.
 */
lbc *getStringXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path,
  int idx
);

/** \~english
 * \brief Fetch and return a string from given XPath expression
 * with arguments pile.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param args Additional path arguments pile.
 * \return lbc* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use.
 */
lbc *getStringFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  va_list args
);

/** \~english
 * \brief Fetch and return a string from given XPath expression
 * accepting arguments.
 *
 * \param ctx Context to use.
 * \param path_format XPath format formatted request.
 * \param ... Additional path formatting arguments.
 * \return lbc* String result of request (or NULL pointer).
 *
 * Returned pointer must be freed after use.
 */
lbc *getStringFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
);

/** \~english
 * \brief Release and reset given string result.
 *
 * \param string String result to release.
 *
 * Set pointer to NULL after free.
 */
void freelbcPtr(
  lbc ** string
);

/** \~english
 * \brief Return the number of objects corresponding to given
 * XPath expression.
 *
 * \param ctx Context to use.
 * \param path XPath format request.
 * \return int Positive or zero on success (or a negative value otherwise).
 */
int getNbObjectsXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path
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
int getNbObjectsFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
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
int getNbObjectsFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
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
 * String pointer must be freed after use using #freelbcPtr().
 */
int getIfExistsStringXmlCtx(
  XmlCtxPtr ctx,
  lbc ** string,
  const lbc *def,
  const lbc *path
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
 * String pointer must be freed after use using #freelbcPtr().
 */
int getIfExistsStringFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  lbc ** string,
  const lbc *def,
  const lbc *path_format,
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
 * String pointer must be freed after use using #freelbcPtr().
 */
int getIfExistsStringFromExprXmlCtx(
  XmlCtxPtr ctx,
  lbc ** string,
  const lbc *def,
  const lbc *path_format,
  ...
);

/* ### Signed integers : ################################################### */

int getIfExistsInt64FromExprVaXmlCtx(
  XmlCtxPtr ctx,
  int64_t *dst,
  int64_t def,
  const lbc *path_format,
  va_list args
);

int getIfExistsInt64FromExprXmlCtx(
  XmlCtxPtr ctx,
  int64_t *dst,
  int64_t def,
  const lbc *path_format,
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
int getIfExistsLongFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  long *dst,
  long def,
  const lbc *path_format,
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
int getIfExistsLongFromExprXmlCtx(
  XmlCtxPtr ctx,
  long *dst,
  long def,
  const lbc *path_format,
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
int getIfExistsIntegerFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  int *dst,
  int def,
  const lbc *path_format,
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
int getIfExistsIntegerFromExprXmlCtx(
  XmlCtxPtr ctx,
  int *dst,
  int def,
  const lbc *path_format,
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
int getIfExistsBooleanFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  bool *dst,
  bool def,
  const lbc *path_format,
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
int getIfExistsBooleanFromExprXmlCtx(
  XmlCtxPtr ctx,
  bool *dst,
  bool def,
  const lbc *path_format,
  ...
);

/* ### Floating point : #################################################### */

int getIfExistsDoubleFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  double *dst,
  double def,
  const lbc *path_format,
  va_list args
);

int getIfExistsDoubleFromExprXmlCtx(
  XmlCtxPtr ctx,
  double *dst,
  double def,
  const lbc *path_format,
  ...
);

int getIfExistsFloatFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  float *dst,
  float def,
  const lbc *path_format,
  va_list args
);

int getIfExistsFloatFromExprXmlCtx(
  XmlCtxPtr ctx,
  float *dst,
  float def,
  const lbc *path_format,
  ...
);

/* ### Unsigned integers : ################################################# */

int getIfExistsUint64FromExprVaXmlCtx(
  XmlCtxPtr ctx,
  uint64_t *dst,
  uint64_t def,
  const lbc *path_format,
  va_list args
);

int getIfExistsUint64FromExprXmlCtx(
  XmlCtxPtr ctx,
  uint64_t *dst,
  uint64_t def,
  const lbc *path_format,
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
int getIfExistsUnsignedFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  unsigned *dst,
  unsigned def,
  const lbc *path_format,
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
int getIfExistsUnsignedFromExprXmlCtx(
  XmlCtxPtr ctx,
  unsigned *dst,
  unsigned def,
  const lbc *path_format,
  ...
);

#endif