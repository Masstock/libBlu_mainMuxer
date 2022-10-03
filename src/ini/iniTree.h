
#ifndef __LIBBLU_MUXER__INI__INI_TREE_H__
#define __LIBBLU_MUXER__INI__INI_TREE_H__

#include "../util/common.h"

/* ### INI File node : ##################################################### */

typedef enum {
  INI_ENTRY,
  INI_SECTION
} IniFileNodeType;

typedef struct IniFileNode {
  struct IniFileNode * sibling;

  char * name;
  size_t nameSize;

  IniFileNodeType type;
  union {
    lbc * entryValue;
    struct IniFileNode * sectionChild;
  };
} IniFileNode, *IniFileNodePtr;

IniFileNodePtr createEntryIniFileNode(
  const char * name,
  const char * value
);

IniFileNodePtr createSectionIniFileNode(
  const char * name
);

static inline void destroyIniFileNode(
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
);

void attachChildIniFileNode(
  IniFileNodePtr dst,
  IniFileNodePtr child
);

lbc * lookupIniFileNode(
  const IniFileNodePtr node,
  const char * name
);

void printIniFileNode(
  IniFileNodePtr node
);

#endif