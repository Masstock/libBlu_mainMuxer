

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COLLISION_TREE_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COLLISION_TREE_H__

#include "hdmv_rectangle.h"
#include "hdmv_error.h"

typedef struct Node {
  struct Node *left, * right;
  HdmvRectangle rect;
  unsigned user_value;
} HdmvCollisionTreeNode, *HdmvCollisionTreeNodePtr;

static inline void destroyHdmvCollisionTree(
  HdmvCollisionTreeNodePtr tree
)
{
  if (NULL == tree)
    return;
  destroyHdmvCollisionTree(tree->left);
  destroyHdmvCollisionTree(tree->right);
  free(tree);
}

int insertHdmvCollisionTree(
  HdmvCollisionTreeNodePtr *tree,
  HdmvRectangle rect,
  unsigned user_value,
  HdmvRectangle *problematic_rect_ret,
  unsigned *problematic_user_value_ret
);

#endif