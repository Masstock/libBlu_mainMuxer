#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "xmlParsing.h"
#include "errorCodesVa.h"

/* ========================== */

XmlCtxPtr createXmlContext(
  void (* errorLogFun) (const lbc * format, ...),
  void (* debugLogFun) (const lbc * format, ...)
)
{
  XmlCtxPtr ctx;

  if (NULL == errorLogFun || NULL == debugLogFun)
    LIBBLU_ERROR_NRETURN("Missing suppling of xml parsing log functions.\n");

  if (NULL == (ctx = (XmlCtxPtr) malloc(sizeof(XmlCtx))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  ctx->libxmlCtx = NULL;
  ctx->errorLogFun = errorLogFun;
  ctx->debugLogFun = debugLogFun;
  ctx->savedNodesStackHeight = 0;

  return ctx;
}

int loadIgsXmlFile(
  XmlCtxPtr ctx,
  const lbc * filePath
)
{
  char * filepath_converted;

  assert(NULL != ctx);

  if (NULL != LIBXML_CONTEXT(ctx)) {
    XML_ERROR_LOG_FUN(
      "A libxml context is already in use (and will be freed).\n"
    );

    xmlFreeDoc(LIBXML_CONTEXT(ctx));
  }

  if (NULL == (filepath_converted = lbc_convfrom(filePath)))
    return -1;

  if (NULL == (LIBXML_CONTEXT(ctx) = xmlParseFile(filepath_converted))) {
    free(filepath_converted);

    XML_ERROR_LOG_FUN(
      "Unable to open input '%" PRI_LBCS "' xml file.\n",
      filePath
    );
    LIBBLU_ERROR_RETURN("Parsing error in supplied IGS XML file.\n");
  }

  free(filepath_converted);
  return 0;
}

void releaseIgsXmlFile(
  XmlCtxPtr ctx
)
{
  if (NULL == ctx)
    return;

  if (NULL != LIBXML_CONTEXT(ctx)) {
    xmlFreeDoc(LIBXML_CONTEXT(ctx));
    xmlCleanupParser();
    LIBXML_CONTEXT(ctx) = NULL;
  }
}

xmlXPathObjectPtr getPathObjectFromPathObjIgsXmlFile(
  const xmlXPathObjectPtr pathObj,
  const char * path
)
{
  xmlNodePtr node;
  xmlXPathObjectPtr phObj = NULL;
  xmlXPathContextPtr phCtx = NULL;

  if (NULL == (node = XML_PATH_NODE(pathObj, 0)))
    return NULL;

  if (NULL == (phCtx = xmlXPathNewContext(XML_DOC(node))))
    goto free_return;

  if (NULL == (phObj = xmlXPathEvalExpression((xmlChar *) path, phCtx)))
    goto free_return;

  if (xmlXPathNodeSetIsEmpty(phObj->nodesetval))
    goto free_return;

  xmlXPathFreeContext(phCtx);
  return phObj;

free_return:
  xmlXPathFreeObject(phObj);
  xmlXPathFreeContext(phCtx);

  return NULL;
}

xmlXPathObjectPtr getPathObjectIgsXmlFile(
  XmlCtxPtr ctx,
  const char * path
)
{
  xmlXPathObjectPtr phObj = NULL;
  xmlXPathContextPtr phCtx = NULL;
  xmlNodePtr node;

  if (NULL == (phCtx = xmlXPathNewContext(LIBXML_CONTEXT(ctx))))
    return NULL;

  if (NULL == (phObj = xmlXPathEvalExpression((xmlChar *) path, phCtx)))
    goto free_return;

  /* if (xmlXPathNodeSetIsEmpty(phObj->nodesetval))
    goto free_return; */

  if (!xmlXPathNodeSetIsEmpty(phObj->nodesetval)) {
    node = phObj->nodesetval->nodeTab[0];

    if (node->type == XML_ATTRIBUTE_NODE) {
      /* Getting argument parent (XML_ELEMENT_NODE) line */
      ctx->lastParsedNodeLine = node->parent->line + 1;
    }
    else
      ctx->lastParsedNodeLine = node->line;
  }

  xmlXPathFreeContext(phCtx);
  return phObj;

free_return:
  xmlXPathFreeObject(phObj);
  xmlXPathFreeContext(phCtx);
  return NULL;
}

xmlXPathObjectPtr getPathObjectFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  const char * pathFormat,
  va_list args
)
{
  char * path = NULL;
  xmlXPathObjectPtr objPtr;

  if (lb_vasprintf(&path, pathFormat, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  objPtr = getPathObjectIgsXmlFile(ctx, path);
  free(path);

  return objPtr;
}

xmlXPathObjectPtr getPathObjectFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  const char * pathFormat,
  ...
)
{
  xmlXPathObjectPtr objPtr;
  va_list args;

  va_start(args, pathFormat);
  objPtr = getPathObjectFromExprVaIgsXmlFile(ctx, pathFormat, args);
  va_end(args);

  return objPtr;
}

int setRootPathFromPathObjectIgsXmlFile(
  XmlCtxPtr ctx,
  xmlXPathObjectPtr pathObj,
  int idx
)
{
  xmlNodePtr node;

  if (XML_MAX_SAVED_PATHS <= ctx->savedNodesStackHeight)
    LIBBLU_ERROR_RETURN(
      "Reached XML path nodes saving limit, "
      "broken input file.\n"
    );

  if (NULL == pathObj)
    LIBBLU_ERROR_RETURN(
      "Unable to set XML root, "
      "setRootPathFromPathObjectIgsXmlFile() receive "
      "a NULL path object.\n"
    );

  if (NULL == (node = XML_PATH_NODE(pathObj, idx)))
    LIBBLU_ERROR_RETURN(
      "Unable to set XML root, "
      "given path object does not contain any node.\n"
    );

  /* Removing node from nodes tree, it will be freed when restoring tree */
  pathObj->nodesetval->nodeTab[idx] = NULL;

  ctx->savedNodesStack[ctx->savedNodesStackHeight++] =
    xmlDocSetRootElement(XML_DOC(node), node)
  ;

  return 0;
}

int restoreLastRootIgsXmlFile(
  XmlCtxPtr ctx
)
{
  xmlNodePtr restoredNode;

  if (NULL == ctx || NULL == LIBXML_CONTEXT(ctx))
    LIBBLU_ERROR_RETURN(
      "Unable to restore last saved XML root, receive closed context.\n"
    );

  if (!ctx->savedNodesStackHeight)
    LIBBLU_ERROR_RETURN(
      "Unable to restore last saved XML root, "
      "empty history.\n"
    );

  restoredNode = ctx->savedNodesStack[--ctx->savedNodesStackHeight];

  xmlFreeNode(xmlDocSetRootElement(LIBXML_CONTEXT(ctx), restoredNode));
  return 0;
}

xmlChar * getStringIgsXmlFile(
  XmlCtxPtr ctx,
  const char * path,
  int idx
)
{
  xmlXPathObjectPtr phObj;
  xmlNodePtr node;
  xmlChar * string;

  if (NULL == (phObj = getPathObjectIgsXmlFile(ctx, path))) {
    LIBBLU_ERROR_NRETURN(
      "Error happen during XML nodes parsing.\n"
    );
  }

  if (NULL == (node = XML_PATH_NODE(phObj, idx))) {
    xmlXPathFreeObject(phObj);
    LIBBLU_ERROR_NRETURN(
      "Unable to fetch an XML node from path '%c'.\n",
      path
    );
  }

  string = xmlNodeListGetString(
    LIBXML_CONTEXT(ctx),
    XML_CHILDREN_NODE(node),
    XML_TRUE
  );

  xmlXPathFreeObject(phObj);
  return string;
}

xmlChar * getStringFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  const char * pathFormat,
  va_list args
)
{
  char * path = NULL;
  xmlChar * string;

  if (lb_vasprintf(&path, pathFormat, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error using lb_vasprintf().\n");

  string = getStringIgsXmlFile(ctx, path, 0);
  free(path);

  return string;
}

xmlChar * getStringFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  const char * pathFormat,
  ...
)
{
  va_list args;
  xmlChar * string;

  va_start(args, pathFormat);
  string = getStringFromExprVaIgsXmlFile(ctx, pathFormat, args);
  va_end(args);

  return string;
}

void freeXmlCharPtr(
  xmlChar ** string
)
{
  if (NULL == string)
    return;

  xmlFree(*string);
  *string = NULL;
}

int getNbObjectsIgsXmlFile(
  XmlCtxPtr ctx,
  const char * path
)
{
  xmlXPathObjectPtr phObj;
  int nbNodes;

  if (NULL == (phObj = getPathObjectIgsXmlFile(ctx, path)))
    return -1;
  nbNodes = XML_PATH_NODE_NB(phObj);

  xmlXPathFreeObject(phObj);
  return nbNodes;
}

int getNbObjectsFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  const char * pathFormat,
  va_list args
)
{
  char * path = NULL;
  int nbNodes;

  if (lb_vasprintf(&path, pathFormat, args) < 0)
    LIBBLU_ERROR_RETURN("Memory allocation error using lb_vasprintf().\n");

  nbNodes = getNbObjectsIgsXmlFile(ctx, path);
  free(path);

  return nbNodes;
}

int getNbObjectsFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int nbNodes;

  va_start(args, pathFormat);
  nbNodes = getNbObjectsFromExprVaIgsXmlFile(ctx, pathFormat, args);
  va_end(args);

  return nbNodes;
}

int getIfExistsStringIgsXmlFile(
  XmlCtxPtr ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path
)
{
  xmlXPathObjectPtr phObj;

  *string = NULL;
  if (NULL == (phObj = getPathObjectIgsXmlFile(ctx, path)))
    return -1;

  if (0 < XML_PATH_NODE_NB(phObj))
    *string = getStringIgsXmlFile(ctx, path, 0);
  else {
    /* Copy default string: */
    if (NULL == def)
      *string = NULL;
    else if (NULL == (*string = xmlStrdup(def)))
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  }

  xmlXPathFreeObject(phObj);
  return 0;

free_return:
  xmlXPathFreeObject(phObj);
  return -1;
}

int getIfExistsStringFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * pathFormat,
  va_list args
)
{
  char * path;
  int ret;

  if (lb_vasprintf(&path, pathFormat, args) < 0)
    LIBBLU_ERROR_RETURN("Memory allocation error using lb_vasprintf().\n");

  ret = getIfExistsStringIgsXmlFile(ctx, string, def, path);
  free(path);

  return ret;
}

int getIfExistsStringFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, string, def, pathFormat, args
  );
  va_end(args);

  return ret;
}

/* ### Signed integers : ################################################### */

int getIfExistsInt64FromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  int64_t * dst,
  int64_t def,
  const char * pathFormat,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, &xmlString, NULL, pathFormat, args
  );
  if (ret < 0)
    return -1;

  string = (char *) xmlString;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    long long value;
    char * end;

    errno = 0; /* Clear errno */
    value = strtoll(string, &end, 0);

    if (EINVAL == errno || '\0' != *end)
      LIBBLU_ERROR_RETURN(
        "Expect XML node value '%s' to be a valid integer "
        "(line: %u).\n",
        string,
        ctx->lastParsedNodeLine
      );

    if (ERANGE == errno)
      LIBBLU_ERROR_RETURN(
        "XML node value '%s' is out of range "
        "(line: %u).\n",
        string,
        ctx->lastParsedNodeLine
      );

    if (0 != errno)
      LIBBLU_ERROR_RETURN(
        "Unexpected error while parsing value '%s' (line: %u, errno: %d).\n",
        string,
        ctx->lastParsedNodeLine,
        errno
      );

    *dst = value;
  }

  xmlFree(xmlString);

  return ret;
}

int getIfExistsInt64FromExprIgsXmlFile(
  XmlCtxPtr ctx,
  int64_t * dst,
  int64_t def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsInt64FromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

int getIfExistsLongFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  long * dst,
  long def,
  const char * pathFormat,
  va_list args
)
{
  int64_t value;
  int ret;

  ret = getIfExistsInt64FromExprVaIgsXmlFile(ctx, &value, def, pathFormat, args);
  if (ret < 0)
    return -1;

  if (value < LONG_MIN || LONG_MAX < value) {
    LIBBLU_ERROR_RETURN(
      "XML node value %" PRId64 " is out of range (line: %u).\n",
      value, ctx->lastParsedNodeLine
    );
  }
  else
    *dst = value;

  return ret;
}

int getIfExistsLongFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  long * dst,
  long def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsLongFromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

int getIfExistsIntegerFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  int * dst,
  int def,
  const char * pathFormat,
  va_list args
)
{
  int64_t value;
  int ret;

  ret = getIfExistsInt64FromExprVaIgsXmlFile(ctx, &value, def, pathFormat, args);
  if (ret < 0)
    return -1;

  if (value < INT_MIN || INT_MAX < value) {
    LIBBLU_ERROR_RETURN(
      "XML node value %" PRId64 " is out of range (line: %u).\n",
      value, ctx->lastParsedNodeLine
    );
  }
  else
    *dst = value;

  return ret;
}

int getIfExistsIntegerFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  int * dst,
  int def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsIntegerFromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

/* ### Boolean value : ##################################################### */

int getIfExistsBooleanFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  bool * dst,
  bool def,
  const char * pathFormat,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, &xmlString, NULL, pathFormat, args
  );
  if (ret < 0)
    return -1;

  string = (char *) xmlString;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    if (!xmlStrcmp(xmlString, (xmlChar *) "true"))
      *dst = true;
    else if (!xmlStrcmp(xmlString, (xmlChar *) "false"))
      *dst = false;
    else {
      LIBBLU_ERROR_RETURN(
        "Unable to parse XML node value '%s' from path '%s' as a boolean "
        "value (expect 'true' or 'false', line: %u).\n",
        string,
        pathFormat,
        ctx->lastParsedNodeLine
      );
    }
  }

  xmlFree(string);

  return 0;
}

int getIfExistsBooleanFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  bool * dst,
  bool def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsBooleanFromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

/* ### Floating point : #################################################### */

int getIfExistsDoubleFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  double * dst,
  double def,
  const char * pathFormat,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, &xmlString, NULL, pathFormat, args
  );
  if (ret < 0)
    return -1;

  string = (char *) xmlString;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    double value;
    char * end;

    errno = 0; /* Clear errno */
    value = strtod(string, &end);

    if (EINVAL == errno || '\0' != *end)
      LIBBLU_ERROR_RETURN(
        "Expect XML node value '%s' to be a valid floating point number "
        "(line: %u).\n",
        string,
        ctx->lastParsedNodeLine
      );

    if (ERANGE == errno)
      LIBBLU_ERROR_RETURN(
        "XML node value '%s' is out of range "
        "(line: %u).\n",
        string,
        ctx->lastParsedNodeLine
      );

    *dst = value;
  }

  xmlFree(xmlString);

  return ret;
}

int getIfExistsDoubleFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  double * dst,
  double def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsDoubleFromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

int getIfExistsFloatFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  float * dst,
  float def,
  const char * pathFormat,
  va_list args
)
{
  double value;
  int ret;

  ret = getIfExistsDoubleFromExprVaIgsXmlFile(ctx, &value, def, pathFormat, args);
  if (ret < 0)
    return -1;

  *dst = value;

  return ret;
}

int getIfExistsFloatFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  float * dst,
  float def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsFloatFromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

/* ### Unsigned integers : ################################################# */

int getIfExistsUint64FromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  uint64_t * dst,
  uint64_t def,
  const char * pathFormat,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(ctx, &xmlString, NULL, pathFormat, args);
  if (ret < 0)
    return -1;

  string = (char *) xmlString;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    unsigned long long value;
    char * end;

    value = strtoull(string, &end, 0);

    if (NULL == end || '\0' != *end)
      LIBBLU_ERROR_RETURN(
        "Unable to parse XML node value '%s' from path '%s' as an integer "
        "(line: %u).\n",
        string,
        pathFormat,
        ctx->lastParsedNodeLine
      );

    *dst = value;
  }

  xmlFree(xmlString);

  return ret;
}

int getIfExistsUint64FromExprIgsXmlFile(
  XmlCtxPtr ctx,
  uint64_t * dst,
  uint64_t def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsUint64FromExprVaIgsXmlFile(ctx, dst, def, pathFormat, args);
  va_end(args);

  return ret;
}

int getIfExistsUnsignedFromExprVaIgsXmlFile(
  XmlCtxPtr ctx,
  unsigned * dst,
  unsigned def,
  const char * pathFormat,
  va_list args
)
{
  uint64_t value;
  int ret;

  ret = getIfExistsUint64FromExprVaIgsXmlFile(ctx, &value, def, pathFormat, args);
  if (ret < 0)
    return -1;

  if (UINT_MAX < value)
    LIBBLU_ERROR_RETURN(
      "Node value %" PRIu64 " from path '%s' is too large.\n",
      value, pathFormat
    );

  *dst = value;

  return ret;
}

int getIfExistsUnsignedFromExprIgsXmlFile(
  XmlCtxPtr ctx,
  unsigned * dst,
  unsigned def,
  const char * pathFormat,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, pathFormat);
  ret = getIfExistsUnsignedFromExprVaIgsXmlFile(
    ctx, dst, def, pathFormat, args
  );
  va_end(args);

  return ret;
}

/* ######################################################################### */

void printNbObjectsFromExprErrorIgsXmlFile(
  int ret,
  const char * pathFormat,
  ...
)
{
  va_list args;
  lbc * format;

  if (NULL == (format = lbc_locale_convto(pathFormat)))
    LIBBLU_ERROR_VRETURN("Memory allocation error.\n");

  if (ret < 0)
    LIBBLU_DEBUG_COM("Unknown error happen when reading path '");
  else
    LIBBLU_DEBUG_COM("Unable to read path '");

  va_start(args, pathFormat);
  LIBBLU_ECHO_VA(LIBBLU_DEBUG_GLB, format, args);
  va_end(args);

  LIBBLU_DEBUG_COM_NO_HEADER("'.\n");

  free(format);
}