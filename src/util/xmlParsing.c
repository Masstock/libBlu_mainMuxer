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

#define LBC_VARIADIC
#include "xmlParsing.h"
#include "errorCodesVa.h"

/* ### Error returns : ##################################################### */

#define LIBBLU_XML_PREFIX "XML: "

#define LIBBLU_XML_ERROR_RETURN(format, ...)                                  \
  LIBBLU_ERROR_RETURN(LIBBLU_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_XML_ERROR_NRETURN(format, ...)                                 \
  LIBBLU_ERROR_NRETURN(LIBBLU_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_XML_ERROR_FRETURN(format, ...)                                 \
  LIBBLU_ERROR_FRETURN(LIBBLU_XML_PREFIX format, ##__VA_ARGS__)

/* ### Types : ############################################################# */

#define XML_MAX_SAVED_PATHS  16

struct _XmlCtx {
  xmlDocPtr libxml_ctx;

  /* Used when call setRootPathFromPathObjectXmlCtx(): */
  xmlNodePtr saved_nodes_stack[XML_MAX_SAVED_PATHS];
  unsigned saved_nodes_stack_height;

  unsigned last_parsed_node_line;
};

static lbc * _xmlCharStringToLbc(
  const xmlChar *xml_string
)
{
  return lbc_strdup(xml_string);
}

/* ### Context initialization/destruction : ################################ */

XmlCtxPtr createXmlContext(
  void
)
{
  return calloc(1, sizeof(struct _XmlCtx));
}

void destroyXmlContext(
  XmlCtxPtr ctx
)
{
  if (NULL == ctx)
    return;

  unsigned stack_height = ctx->saved_nodes_stack_height;
  while (0 < stack_height--) {
    xmlNodePtr node = ctx->saved_nodes_stack[stack_height];
    xmlFreeNode(xmlDocSetRootElement(ctx->libxml_ctx, node));
  }

  free(ctx);
}

int loadXmlCtx(
  XmlCtxPtr ctx,
  const lbc *filepath
)
{
  assert(NULL != ctx);

  LIBXML_TEST_VERSION

  if (NULL != ctx->libxml_ctx)
    xmlFreeDoc(ctx->libxml_ctx);

  if (NULL == (ctx->libxml_ctx = xmlReadFile((char *) filepath, NULL, 0)))
    LIBBLU_XML_ERROR_RETURN("Failed to parse %s.\n", filepath);

  return 0;
}

void releaseXmlCtx(
  XmlCtxPtr ctx
)
{
  if (NULL != ctx && NULL != ctx->libxml_ctx) {
    xmlFreeDoc(ctx->libxml_ctx);
    xmlCleanupParser();
    ctx->libxml_ctx = NULL;
  }
}

/* ### Context status : #################################################### */

unsigned lastParsedNodeLineXmlCtx(
  const XmlCtxPtr ctx
)
{
  return ctx->last_parsed_node_line;
}

/* ### XML Node : ########################################################## */

const lbc *getNameXmlNode(
  const XmlNodePtr node
)
{
  return node->name;
}

/* ### XML XPath : ######################################################### */

unsigned getNbNodesXmlXPathObject(
  const XmlXPathObjectPtr xpath_obj
)
{
  if (NULL == xpath_obj || NULL == xpath_obj->nodesetval)
    return 0;
  int nb_nodes = xpath_obj->nodesetval->nodeNr;
  return (nb_nodes < 0) ? 0u : (unsigned) nb_nodes;
}

XmlNodePtr getNodeXmlXPathObject(
  const XmlXPathObjectPtr xpath_obj,
  unsigned node_idx
)
{
  if (getNbNodesXmlXPathObject(xpath_obj) <= node_idx)
    LIBBLU_XML_ERROR_NRETURN(
      "Node %u is out of range of XPath object nodes.\n",
      node_idx
    );

  return xpath_obj->nodesetval->nodeTab[node_idx];
}

XmlXPathObjectPtr getPathObjectFromPathObjXmlCtx(
  const XmlXPathObjectPtr path_obj,
  const lbc *path
)
{
  xmlNodePtr node = getNodeXmlXPathObject(path_obj, 0);
  if (NULL == node)
    return NULL;

  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr  xpath_obj = NULL;
  if (NULL == (xpath_ctx = xmlXPathNewContext(node->doc)))
    goto free_return;
  if (NULL == (xpath_obj = xmlXPathEvalExpression(path, xpath_ctx)))
    goto free_return;

  if (xmlXPathNodeSetIsEmpty(xpath_obj->nodesetval))
    goto free_return;

  xmlXPathFreeContext(xpath_ctx);
  return xpath_obj;

free_return:
  destroyXmlXPathObject(xpath_obj);
  xmlXPathFreeContext(xpath_ctx);
  return NULL;
}

XmlXPathObjectPtr getPathObjectXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path
)
{
  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr xpath_obj  = NULL;
  if (NULL == (xpath_ctx = xmlXPathNewContext(ctx->libxml_ctx)))
    return NULL;
  if (NULL == (xpath_obj = xmlXPathEvalExpression(path, xpath_ctx)))
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
  destroyXmlXPathObject(xpath_obj);
  xmlXPathFreeContext(xpath_ctx);
  return NULL;
}

bool existsPathObjectXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path
)
{
  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr xpath_obj  = NULL;
  if (NULL == (xpath_ctx = xmlXPathNewContext(ctx->libxml_ctx)))
    return NULL;
  if (NULL == (xpath_obj = xmlXPathEvalExpression(path, xpath_ctx)))
    goto free_return;

  bool exists = (0 < getNbNodesXmlXPathObject(xpath_obj));

  destroyXmlXPathObject(xpath_obj);
  xmlXPathFreeContext(xpath_ctx);
  return exists;

free_return:
  destroyXmlXPathObject(xpath_obj);
  xmlXPathFreeContext(xpath_ctx);
  return NULL;
}

XmlXPathObjectPtr getPathObjectFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  va_list args
)
{
  lbc *path = NULL;
  if (lbc_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  xmlXPathObjectPtr xpath_obj = getPathObjectXmlCtx(ctx, path);
  free(path);

  return xpath_obj;
}

XmlXPathObjectPtr getPathObjectFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
)
{
  va_list args;
  va_start(args, path_format);

  xmlXPathObjectPtr xpath_obj = getPathObjectFromExprVaXmlCtx(ctx, path_format, args);

  va_end(args);
  return xpath_obj;
}

bool existsPathObjectFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  va_list args
)
{
  lbc *path = NULL;
  if (lbc_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  bool exists = existsPathObjectXmlCtx(ctx, path);
  free(path);

  return exists;
}

bool existsPathObjectFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
)
{
  va_list args;
  va_start(args, path_format);

  bool exists = existsPathObjectFromExprVaXmlCtx(ctx, path_format, args);

  va_end(args);
  return exists;
}

int setRootPathFromPathObjectXmlCtx(
  XmlCtxPtr ctx,
  XmlXPathObjectPtr xpath_obj,
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
      "setRootPathFromPathObjectXmlCtx() receive "
      "a NULL path object.\n"
    );

  if (NULL == (node = getNodeXmlXPathObject(xpath_obj, idx)))
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

void destroyXmlXPathObject(
  XmlXPathObjectPtr xpath_obj
)
{
  xmlXPathFreeObject(xpath_obj);
}

/* ### Root definition : ################################################### */

int restoreLastRootXmlCtx(
  XmlCtxPtr ctx
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

lbc *getStringXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path,
  int idx
)
{
  assert(NULL != ctx);
  assert(NULL != path);
  assert(0 <= idx);

  xmlXPathObjectPtr xpath_obj = getPathObjectXmlCtx(ctx, path);
  if (NULL == xpath_obj)
    LIBBLU_ERROR_NRETURN("Error happen during XML nodes parsing.\n");

  XmlNodePtr node = getNodeXmlXPathObject(xpath_obj, idx);
  if (NULL == node)
    LIBBLU_ERROR_FRETURN(
      "Unable to fetch an XML node from path '%" PRI_LBCS "'.\n",
      path
    );

  xmlChar *xml_string = xmlNodeListGetString(ctx->libxml_ctx, node->children, 1);
  if (NULL == xml_string)
    goto free_return;

  lbc *string = _xmlCharStringToLbc(xml_string);

  destroyXmlXPathObject(xpath_obj);
  xmlFree(xml_string);
  return string;

free_return:
  destroyXmlXPathObject(xpath_obj);
  return NULL;
}

lbc *getStringFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  va_list args
)
{
  lbc *path = NULL;
  if (lbc_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_NRETURN("Memory allocation error using lbc_vasprintf().\n");

  lbc *string = getStringXmlCtx(ctx, path, 0);

  free(path);
  return string;
}

lbc *getStringFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
)
{
  va_list args;
  va_start(args, path_format);

  lbc *string = getStringFromExprVaXmlCtx(ctx, path_format, args);

  va_end(args);
  return string;
}

int getNbObjectsXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path
)
{
  xmlXPathObjectPtr xpath_obj = getPathObjectXmlCtx(ctx, path);
  if (NULL == xpath_obj)
    return -1;

  int nb_nodes = getNbNodesXmlXPathObject(xpath_obj);

  destroyXmlXPathObject(xpath_obj);
  return nb_nodes;
}

int getNbObjectsFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  va_list args
)
{
  lbc *path = NULL;
  if (lbc_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_RETURN("Memory allocation error using lbc_vasprintf().\n");

  int nb_nodes = getNbObjectsXmlCtx(ctx, path);
  free(path);

  return nb_nodes;
}

int getNbObjectsFromExprXmlCtx(
  XmlCtxPtr ctx,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int nbNodes;

  va_start(args, path_format);
  nbNodes = getNbObjectsFromExprVaXmlCtx(ctx, path_format, args);
  va_end(args);

  return nbNodes;
}

int getIfExistsStringXmlCtx(
  XmlCtxPtr ctx,
  lbc ** string_ret,
  const lbc *def,
  const lbc *path
)
{
  xmlXPathObjectPtr phObj;

  if (NULL == (phObj = getPathObjectXmlCtx(ctx, path)))
    return -1;
  if (NULL == string_ret)
    return 0;

  if (0 < getNbNodesXmlXPathObject(phObj))
    *string_ret = getStringXmlCtx(ctx, path, 0);
  else {
    /* Copy default string: */
    if (NULL == def)
      *string_ret = NULL;
    else if (NULL == (*string_ret = lbc_strdup(def)))
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  }

  destroyXmlXPathObject(phObj);
  return 0;

free_return:
  destroyXmlXPathObject(phObj);
  return -1;
}

int getIfExistsStringFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  lbc ** string,
  const lbc *def,
  const lbc *path_format,
  va_list args
)
{
  lbc *path;
  int ret;

  if (lbc_vasprintf(&path, path_format, args) < 0)
    LIBBLU_ERROR_RETURN("Memory allocation error using lbc_vasprintf().\n");

  ret = getIfExistsStringXmlCtx(ctx, string, def, path);
  free(path);

  return ret;
}

int getIfExistsStringFromExprXmlCtx(
  XmlCtxPtr ctx,
  lbc ** string,
  const lbc *def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsStringFromExprVaXmlCtx(
    ctx, string, def, path_format, args
  );
  va_end(args);

  return ret;
}

/* ### Signed integers : ################################################### */

int getIfExistsInt64FromExprVaXmlCtx(
  XmlCtxPtr ctx,
  int64_t *dst,
  int64_t def,
  const lbc *path_format,
  va_list args
)
{
  lbc *string;
  if (getIfExistsStringFromExprVaXmlCtx(ctx, &string, NULL, path_format, args) < 0)
    return -1;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    long long value;
    lbc *end;

    errno = 0; /* Clear errno */
    value = lbc_strtoll(string, &end, 0);

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

  xmlFree(string);
  return 0;
}

int getIfExistsInt64FromExprXmlCtx(
  XmlCtxPtr ctx,
  int64_t *dst,
  int64_t def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsInt64FromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsLongFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  long *dst,
  long def,
  const lbc *path_format,
  va_list args
)
{
  int64_t value;
  int ret;

  ret = getIfExistsInt64FromExprVaXmlCtx(ctx, &value, def, path_format, args);
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

int getIfExistsLongFromExprXmlCtx(
  XmlCtxPtr ctx,
  long *dst,
  long def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsLongFromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsIntegerFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  int *dst,
  int def,
  const lbc *path_format,
  va_list args
)
{
  int64_t value;
  int ret = getIfExistsInt64FromExprVaXmlCtx(
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

int getIfExistsIntegerFromExprXmlCtx(
  XmlCtxPtr ctx,
  int *dst,
  int def,
  const lbc *path_format,
  ...
)
{
  va_list args;

  va_start(args, path_format);
  int ret = getIfExistsIntegerFromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

/* ### Boolean value : ##################################################### */

int getIfExistsBooleanFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  bool *dst,
  bool def,
  const lbc *path_format,
  va_list args
)
{
  lbc *string;
  if (getIfExistsStringFromExprVaXmlCtx(ctx, &string, NULL, path_format, args) < 0)
    return -1;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    if (lbc_atob(dst, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Unable to parse XML node value '%s' from path '%s' as a boolean "
        "value (expect 'true' or 'false', line: %u).\n",
        string,
        path_format,
        ctx->last_parsed_node_line
      );
  }

  xmlFree(string);
  return 0;
}

int getIfExistsBooleanFromExprXmlCtx(
  XmlCtxPtr ctx,
  bool *dst,
  bool def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsBooleanFromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

/* ### Floating point : #################################################### */

int getIfExistsDoubleFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  double *dst,
  double def,
  const lbc *path_format,
  va_list args
)
{
  lbc *string;
  if (getIfExistsStringFromExprVaXmlCtx(ctx, &string, NULL, path_format, args) < 0)
    return -1;

  if (NULL == string)
    *dst = def; // Does not exists
  else {
    errno = 0; // Clear errno

    lbc *end;
    double value = lbc_strtod(string, &end);

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

  xmlFree(string);
  return 0;
}

int getIfExistsDoubleFromExprXmlCtx(
  XmlCtxPtr ctx,
  double *dst,
  double def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsDoubleFromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsFloatFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  float *dst,
  float def,
  const lbc *path_format,
  va_list args
)
{
  double value;
  int ret;

  ret = getIfExistsDoubleFromExprVaXmlCtx(ctx, &value, def, path_format, args);
  if (ret < 0)
    return -1;

  *dst = value;

  return ret;
}

int getIfExistsFloatFromExprXmlCtx(
  XmlCtxPtr ctx,
  float *dst,
  float def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsFloatFromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

/* ### Unsigned integers : ################################################# */

int getIfExistsUint64FromExprVaXmlCtx(
  XmlCtxPtr ctx,
  uint64_t *dst,
  uint64_t def,
  const lbc *path_format,
  va_list args
)
{
  lbc *string;
  if (getIfExistsStringFromExprVaXmlCtx(ctx, &string, NULL, path_format, args) < 0)
    return -1;

  if (NULL == string)
    *dst = def; /* Does not exists */
  else {
    unsigned long long value;
    lbc *end;

    value = lbc_strtoull(string, &end, 0);

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

  xmlFree(string);
  return 0;
}

int getIfExistsUint64FromExprXmlCtx(
  XmlCtxPtr ctx,
  uint64_t *dst,
  uint64_t def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsUint64FromExprVaXmlCtx(ctx, dst, def, path_format, args);
  va_end(args);

  return ret;
}

int getIfExistsUnsignedFromExprVaXmlCtx(
  XmlCtxPtr ctx,
  unsigned *dst,
  unsigned def,
  const lbc *path_format,
  va_list args
)
{
  uint64_t value;
  int ret;

  ret = getIfExistsUint64FromExprVaXmlCtx(ctx, &value, def, path_format, args);
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

int getIfExistsUnsignedFromExprXmlCtx(
  XmlCtxPtr ctx,
  unsigned *dst,
  unsigned def,
  const lbc *path_format,
  ...
)
{
  va_list args;
  int ret;

  va_start(args, path_format);
  ret = getIfExistsUnsignedFromExprVaXmlCtx(
    ctx, dst, def, path_format, args
  );
  va_end(args);

  return ret;
}

/* ######################################################################### */
