

#ifndef __LIBBLU_MUXER__CODECS__PGS_GENERATOR__MERGING_TREE_H__
#define __LIBBLU_MUXER__CODECS__PGS_GENERATOR__MERGING_TREE_H__

#include "../../../util.h"

#include "../common/hdmv_rectangle.h"

typedef struct MTN {
  struct MTN *left;
  struct MTN *right;
  HdmvRectangle box;
} MergingTreeNode;

MergingTreeNode *createMergingTreeNode(
  HdmvRectangle box,
  MergingTreeNode *left,
  MergingTreeNode *right
);

void destroyMergingTreeNode(
  MergingTreeNode *node
);

int insertMergingTreeNode(
  MergingTreeNode ** tree,
  HdmvRectangle box
);

static inline bool isSingleZoneMergingTreeNode(
  const MergingTreeNode *tree
)
{
  if (NULL == tree || NULL == tree->left)
    return true; // Empty tree or single leaf

  assert(NULL != tree->right);
  if (areCollidingRectangle(tree->left->box, tree->right->box))
    return true; // Two first childs are colliding

  printf(" No collide.\n");
  return false;
}

#endif