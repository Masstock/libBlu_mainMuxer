
#ifndef __LIBBLU_MUXER__INI__INI_TREE_H__
#define __LIBBLU_MUXER__INI__INI_TREE_H__

#include "../util/common.h"

/* ### INI File node : ##################################################### */

typedef enum {
  INI_ENTRY,
  INI_SECTION
} IniFileNodeType;

typedef struct IniFileNode {
  struct IniFileNode *sibling;

  char *name;
  size_t nameSize;

  IniFileNodeType type;
  union {
    lbc *entryValue;
    struct IniFileNode *sectionChild;
  };
} IniFileNode, *IniFileNodePtr;

IniFileNodePtr createEntryIniFileNode(
  const char *name,
  const char *value
);

IniFileNodePtr createSectionIniFileNode(
  const char *name
);

void destroyIniFileNode(
  IniFileNodePtr node
);

void attachSiblingIniFileNode(
  IniFileNodePtr dst,
  IniFileNodePtr sibling
);

void attachChildIniFileNode(
  IniFileNodePtr dst,
  IniFileNodePtr child
);

lbc *lookupIniFileNode(
  const IniFileNodePtr node,
  const char *name
);

void printIniFileNode(
  IniFileNodePtr node
);

#endif