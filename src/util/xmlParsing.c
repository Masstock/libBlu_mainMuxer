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

int loadIgsXmlFile(
  XmlCtx * ctx,
  const lbc * filepath
)
{
  assert(NULL != ctx);

  LIBXML_TEST_VERSION

  if (NULL != ctx->libxml_ctx)
    xmlFreeDoc(ctx->libxml_ctx);

  char * filepath_converted = lbc_convfrom(filepath);
  if (NULL == filepath_converted)
    return -1;

  if (NULL == (ctx->libxml_ctx = xmlReadFile(filepath_converted, NULL, 0)))
    LIBBLU_ERROR_FRETURN("Failed to parse %s.\n", filepath);

  free(filepath_converted);
  return 0;

free_return:
  free(filepath_converted);
  return -1;
}

void releaseIgsXmlFile(
  XmlCtx * ctx
)
{
  if (NULL != ctx && NULL != ctx->libxml_ctx) {
    xmlFreeDoc(ctx->libxml_ctx);
    xmlCleanupParser();
    ctx->libxml_ctx = NULL;
  }
}

xmlXPathObjectPtr getPathObjectFromPathObjIgsXmlFile(
  const xmlXPathObjectPtr path_obj,
  const char * path
)
{
  xmlNodePtr node = XML_PATH_NODE(path_obj, 0);
  if (NULL == node)
    return NULL;

  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr xpath_obj  = NULL;
  if (NULL == (xpath_ctx = xmlXPathNewContext(node->doc)))
    goto free_return;
  if (NULL == (xpath_obj = xmlXPathEvalExpression((xmlChar *) path, xpath_ctx)))
    goto free_return;

  if (xmlXPathNodeSetIsEmpty(xpath_obj->nodesetval))
    goto free_return;

  xmlXPathFreeContext(xpath_ctx);
  return xpath_obj;

free_return:
  xmlXPathFreeObject(xpath_obj);
  xmlXPathFreeContext(xpath_ctx);
  return NULL;
}

xmlXPathObjectPtr getPathObjectIgsXmlFile(
  XmlCtx * ctx,
  const char * path
)
{
  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr xpath_obj  = NULL;
  if (NULL == (xpath_ctx = xmlXPathNewContext(ctx->libxml_ctx)))
    return NULL;
  if (NULL == (xpath_obj = xmlXPathEvalExpression((xmlChar *) path, xpath_ctx)))
    goto free_return;

  if (!xmlXPathNodeSetIsEmpty(xpath_obj->nodesetval)) {
    xmlNodePtr node = xpath_obj->nodesetval->nodeTab[0];

    if (node->type == XML_ATTRIBUTE_NODE) {
      /* Getting argument parent (XML_ELEMENT_NODE) line */
      ctx->last_parsed_node_line = node->parent->line + 1;
    }
    else
      ctx->last_parsed_node_line = node->line;
  }

  xmlXPathFreeContext(xpath_ctx);
  return xpath_obj;

free_return:
  xmlXPathFreeObject(xpath_obj);
  xmlXPathFreeContext(xpath_ctx);
  return NULL;
}

xmlXPathObjectPtr getPathObjectFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  va_list args
)
{
  char * path = NULL;
  if (lb_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  xmlXPathObjectPtr xpath_obj = getPathObjectIgsXmlFile(ctx, path);
  free(path);

  return xpath_obj;
}

xmlXPathObjectPtr getPathObjectFromExprIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  ...
)
{
  va_list args;
  va_start(args, path_format);
  xmlXPathObjectPtr xpath_obj = getPathObjectFromExprVaIgsXmlFile(ctx, path_format, args);
  va_end(args);

  return xpath_obj;
}

int setRootPathFromPathObjectIgsXmlFile(
  XmlCtx * ctx,
  xmlXPathObjectPtr xpath_obj,
  int idx
)
{
  xmlNodePtr node;

  if (XML_MAX_SAVED_PATHS <= ctx->saved_nodes_stack_height)
    LIBBLU_ERROR_RETURN(
      "Reached XML path nodes saving limit, "
      "broken input file.\n"
    );

  if (NULL == xpath_obj)
    LIBBLU_ERROR_RETURN(
      "Unable to set XML root, "
      "setRootPathFromPathObjectIgsXmlFile() receive "
      "a NULL path object.\n"
    );

  if (NULL == (node = XML_PATH_NODE(xpath_obj, idx)))
    LIBBLU_ERROR_RETURN(
      "Unable to set XML root, "
      "given path object does not contain any node.\n"
    );

  /* Removing node from nodes tree, it will be freed when restoring tree */
  xpath_obj->nodesetval->nodeTab[idx] = NULL;

  ctx->saved_nodes_stack[ctx->saved_nodes_stack_height++] =
    xmlDocSetRootElement(node->doc, node)
  ;

  return 0;
}

int restoreLastRootIgsXmlFile(
  XmlCtx * ctx
)
{
  xmlNodePtr restoredNode;

  if (NULL == ctx || NULL == ctx->libxml_ctx)
    LIBBLU_ERROR_RETURN(
      "Unable to restore last saved XML root, receive closed context.\n"
    );

  if (!ctx->saved_nodes_stack_height)
    LIBBLU_ERROR_RETURN(
      "Unable to restore last saved XML root, "
      "empty history.\n"
    );

  restoredNode = ctx->saved_nodes_stack[--ctx->saved_nodes_stack_height];

  xmlFreeNode(xmlDocSetRootElement(ctx->libxml_ctx, restoredNode));
  return 0;
}

xmlChar * getStringIgsXmlFile(
  XmlCtx * ctx,
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

  string = xmlNodeListGetString(ctx->libxml_ctx, node->children, XML_TRUE);
  xmlXPathFreeObject(phObj);
  return string;
}

xmlChar * getStringFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  va_list args
)
{
  char * path = NULL;
  xmlChar * string;

  if (lb_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error using lb_vasprintf().\n");

  string = getStringIgsXmlFile(ctx, path, 0);
  free(path);

  return string;
}

xmlChar * getStringFromExprIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  ...
)
{
  va_list args;
  xmlChar * string;

  va_start(args, path_format);
  string = getStringFromExprVaIgsXmlFile(ctx, path_format, args);
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
  XmlCtx * ctx,
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
  XmlCtx * ctx,
  const char * path_format,
  va_list args
)
{
  char * path = NULL;
  int nbNodes;

  if (lb_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_RETURN("Memory allocation error using lb_vasprintf().\n");

  nbNodes = getNbObjectsIgsXmlFile(ctx, path);
  free(path);

  return nbNodes;
}

int getNbObjectsFromExprIgsXmlFile(
  XmlCtx * ctx,
  const char * path_format,
  ...
)
{
  va_list args;
  int nbNodes;

  va_start(args, path_format);
  nbNodes = getNbObjectsFromExprVaIgsXmlFile(ctx, path_format, args);
  va_end(args);

  return nbNodes;
}

int getIfExistsStringIgsXmlFile(
  XmlCtx * ctx,
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
  XmlCtx * ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path_format,
  va_list args
)
{
  char * path;
  int ret;

  if (lb_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_RETURN("Memory allocation error using lb_vasprintf().\n");

  ret = getIfExistsStringIgsXmlFile(ctx, string, def, path);
  free(path);

  return ret;
}

int getIfExistsStringFromExprIgsXmlFile(
  XmlCtx * ctx,
  xmlChar ** string,
  const xmlChar * def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, string, def, path_format, args
  );
  va_end(args);

  return ret;
}

/* ### Signed integers : ################################################### */

int getIfExistsInt64FromExprVaIgsXmlFile(
  XmlCtx * ctx,
  int64_t * dst,
  int64_t def,
  const char * path_format,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, &xmlString, NULL, path_format, args
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
        ctx->last_parsed_node_line
      );

    if (ERANGE == errno)
      LIBBLU_ERROR_RETURN(
        "XML node value '%s' is out of range "
        "(line: %u).\n",
        string,
        ctx->last_parsed_node_line
      );

    if (0 != errno)
      LIBBLU_ERROR_RETURN(
        "Unexpected error while parsing value '%s' (line: %u, errno: %d).\n",
        string,
        ctx->last_parsed_node_line,
        errno
      );

    *dst = value;
  }

  xmlFree(xmlString);

  return ret;
}

int getIfExistsInt64FromExprIgsXmlFile(
  XmlCtx * ctx,
  int64_t * dst,
  int64_t def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsInt64FromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsLongFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  long * dst,
  long def,
  const char * path_format,
  va_list args
)
{
  int64_t value;
  int ret;

  ret = getIfExistsInt64FromExprVaIgsXmlFile(ctx, &value, def, path_format, args);
  if (ret < 0)
    return -1;

  if (value < LONG_MIN || LONG_MAX < value) {
    LIBBLU_ERROR_RETURN(
      "XML node value %" PRId64 " is out of range (line: %u).\n",
      value, ctx->last_parsed_node_line
    );
  }
  else
    *dst = value;

  return ret;
}

int getIfExistsLongFromExprIgsXmlFile(
  XmlCtx * ctx,
  long * dst,
  long def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsLongFromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsIntegerFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  int * dst,
  int def,
  const char * path_format,
  va_list args
)
{
  int64_t value;
  int ret = getIfExistsInt64FromExprVaIgsXmlFile(
    ctx, &value, def, path_format, args
  );
  if (ret < 0)
    return -1;

  if (value < INT_MIN || INT_MAX < value) {
    LIBBLU_ERROR_RETURN(
      "XML node value %" PRId64 " is out of range (line: %u).\n",
      value, ctx->last_parsed_node_line
    );
  }
  else
    *dst = value;

  return ret;
}

int getIfExistsIntegerFromExprIgsXmlFile(
  XmlCtx * ctx,
  int * dst,
  int def,
  const char * path_format,
  ...
)
{
  va_list args;

  va_start(args, path_format);
  int ret = getIfExistsIntegerFromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

/* ### Boolean value : ##################################################### */

int getIfExistsBooleanFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  bool * dst,
  bool def,
  const char * path_format,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, &xmlString, NULL, path_format, args
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
        path_format,
        ctx->last_parsed_node_line
      );
    }
  }

  xmlFree(string);

  return 0;
}

int getIfExistsBooleanFromExprIgsXmlFile(
  XmlCtx * ctx,
  bool * dst,
  bool def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsBooleanFromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

/* ### Floating point : #################################################### */

int getIfExistsDoubleFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  double * dst,
  double def,
  const char * path_format,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(
    ctx, &xmlString, NULL, path_format, args
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
        ctx->last_parsed_node_line
      );

    if (ERANGE == errno)
      LIBBLU_ERROR_RETURN(
        "XML node value '%s' is out of range "
        "(line: %u).\n",
        string,
        ctx->last_parsed_node_line
      );

    *dst = value;
  }

  xmlFree(xmlString);

  return ret;
}

int getIfExistsDoubleFromExprIgsXmlFile(
  XmlCtx * ctx,
  double * dst,
  double def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsDoubleFromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsFloatFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  float * dst,
  float def,
  const char * path_format,
  va_list args
)
{
  double value;
  int ret;

  ret = getIfExistsDoubleFromExprVaIgsXmlFile(ctx, &value, def, path_format, args);
  if (ret < 0)
    return -1;

  *dst = value;

  return ret;
}

int getIfExistsFloatFromExprIgsXmlFile(
  XmlCtx * ctx,
  float * dst,
  float def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsFloatFromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

/* ### Unsigned integers : ################################################# */

int getIfExistsUint64FromExprVaIgsXmlFile(
  XmlCtx * ctx,
  uint64_t * dst,
  uint64_t def,
  const char * path_format,
  va_list args
)
{
  xmlChar * xmlString;
  char * string;
  int ret;

  ret = getIfExistsStringFromExprVaIgsXmlFile(ctx, &xmlString, NULL, path_format, args);
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
        path_format,
        ctx->last_parsed_node_line
      );

    *dst = value;
  }

  xmlFree(xmlString);

  return ret;
}

int getIfExistsUint64FromExprIgsXmlFile(
  XmlCtx * ctx,
  uint64_t * dst,
  uint64_t def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsUint64FromExprVaIgsXmlFile(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsUnsignedFromExprVaIgsXmlFile(
  XmlCtx * ctx,
  unsigned * dst,
  unsigned def,
  const char * path_format,
  va_list args
)
{
  uint64_t value;
  int ret;

  ret = getIfExistsUint64FromExprVaIgsXmlFile(ctx, &value, def, path_format, args);
  if (ret < 0)
    return -1;

  if (UINT_MAX < value)
    LIBBLU_ERROR_RETURN(
      "Node value %" PRIu64 " from path '%s' is too large.\n",
      value, path_format
    );

  *dst = value;

  return ret;
}

int getIfExistsUnsignedFromExprIgsXmlFile(
  XmlCtx * ctx,
  unsigned * dst,
  unsigned def,
  const char * path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsUnsignedFromExprVaIgsXmlFile(
    ctx, dst, def, path_format, args
  );
  va_end(args);

  return ret;
}

/* ######################################################################### */
