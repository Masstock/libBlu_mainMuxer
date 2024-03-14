#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "iniTree.h"

/* ### INI File node : ##################################################### */

static IniFileNodePtr createIniFileNode(
  IniFileNodeType type,
  const char *name
)
{
  char *name_copy = NULL;
  if (NULL != name) {
    if (NULL == (name_copy = lb_str_dup_to_upper(name)))
      LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  }

  IniFileNodePtr node = (IniFileNodePtr) malloc(sizeof(IniFileNode));
  if (NULL == node)
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  *node = (IniFileNode) {
    .type = type,
    .name = name_copy,
    .nameSize = ((NULL != name_copy) ? strlen(name_copy) : 0)
  };

  return node;

free_return:
  free(name_copy);
  return NULL;
}

IniFileNodePtr createEntryIniFileNode(
  const char *name,
  const char *value
)
{
  lbc *value_copy = lbc_strdup((lbc *) value);
  if (NULL == value_copy)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  IniFileNodePtr node = createIniFileNode(INI_ENTRY, name);
  if (NULL == node)
    goto free_return;

  node->entryValue = value_copy;
  return node;

free_return:
  free(value_copy);
  return NULL;
}

IniFileNodePtr createSectionIniFileNode(
  const char *name
)
{
  IniFileNodePtr node;

  if (NULL == (node = createIniFileNode(INI_SECTION, name)))
    return NULL;
  return node;
}

void destroyIniFileNode(
  IniFileNodePtr node
)
{
  if (NULL == node)
    return;

  destroyIniFileNode(node->sibling);
  free(node->name);

  switch (node->type) {
  case INI_ENTRY:
    free(node->entryValue);
    break;

  case INI_SECTION:
    destroyIniFileNode(node->sectionChild);
    break;
  }

  free(node);
}

void attachSiblingIniFileNode(
  IniFileNodePtr dst,
  IniFileNodePtr sibling
)
{
  assert(NULL != dst);

  if (NULL != dst->sibling)
    attachSiblingIniFileNode(dst->sibling, sibling);
  else
    dst->sibling = sibling;
}

void attachChildIniFileNode(
  IniFileNodePtr dst,
  IniFileNodePtr child
)
{
  assert(NULL != dst);
  assert(dst->type == INI_SECTION);

  if (NULL != dst->sectionChild)
    attachSiblingIniFileNode(dst->sectionChild, child);
  else
    dst->sectionChild = child;
}

static bool matchIniFileNode(
  const IniFileNodePtr node,
  const char *expr,
  size_t *namePartSize
)
{
  const char *cp;
  size_t nameSize;

  nameSize = 0;
  for (cp = expr; '\0' != *cp && '.' != *cp; cp++)
    nameSize++;

  if (nameSize != node->nameSize)
    return false;

  if (0 != memcmp(expr, node->name, nameSize))
    return false;

  *namePartSize = nameSize;
  return true;
}

lbc *lookupIniFileNode(
  const IniFileNodePtr node,
  const char *expr
)
{
  if (NULL == node)
    return NULL;

  if (NULL == node->name)
    return lookupIniFileNode(node->sectionChild, expr);

  size_t name_size;
  if (matchIniFileNode(node, expr, &name_size)) {
    switch (node->type) {
    case INI_ENTRY:
      if (expr[name_size] != '\0')
        break;
      return node->entryValue;

    case INI_SECTION:
      if (expr[name_size] != '.')
        break;
      return lookupIniFileNode(node->sectionChild, expr + name_size + 1);
    }
  }

  return lookupIniFileNode(node->sibling, expr);
}

static void sub_printIniFileNode(
  IniFileNodePtr node,
  unsigned indent
)
{
  if (NULL == node)
    return;

  switch (node->type) {
  case INI_ENTRY:
    printf(
      "%*c'%s' = '%" PRI_LBCS "'\n", indent, '\0',
      node->name, node->entryValue
    );
    break;

  case INI_SECTION:
    printf("%*c[%s]\n", indent, '\0', node->name);
    sub_printIniFileNode(node->sectionChild, indent + 4);
    break;
  }

  sub_printIniFileNode(node->sibling, indent);
}

void printIniFileNode(
  IniFileNodePtr node
)
{
  sub_printIniFileNode(node, 0);
}