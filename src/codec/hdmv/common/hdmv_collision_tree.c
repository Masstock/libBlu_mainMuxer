#include <stdio.h>
#include <stdlib.h>

#include "hdmv_collision_tree.h"

int insertHdmvCollisionTree(
  HdmvCollisionTreeNodePtr * tree,
  HdmvRectangle rect,
  unsigned user_value,
  HdmvRectangle * problematic_rect_ret,
  unsigned * problematic_user_value_ret
)
{
  HdmvCollisionTreeNodePtr node = *tree;

  if (NULL == node) {
    HdmvCollisionTreeNodePtr leaf = calloc(1, sizeof(HdmvCollisionTreeNode));
    if (NULL == leaf)
      LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
    leaf->rect       = rect;
    leaf->user_value = user_value;
    *tree = leaf;
    return 0;
  }

  if (NULL == node->left) {
    assert(NULL == node->right);

    if (areCollidingRectangle(node->rect, rect)) {
      if (NULL != problematic_rect_ret)
        *problematic_rect_ret       = node->rect;
      if (NULL != problematic_user_value_ret)
        *problematic_user_value_ret = node->user_value;
      return 1; // Colliding, abort insertion
    }

    HdmvCollisionTreeNodePtr inode = calloc(1, sizeof(HdmvCollisionTreeNode));
    if (NULL == inode)
      LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
    inode->left = node;
    inode->rect = mergeRectangle(node->rect, rect);

    *tree = inode;
    return insertHdmvCollisionTree(
      &inode->right, rect, user_value,
      problematic_rect_ret, problematic_user_value_ret
    );
  }

  assert(NULL != node->right);
  HdmvRectangle left_rect  = node->left->rect;
  HdmvRectangle right_rect = node->right->rect;
  node->rect = mergeRectangle(node->rect, rect);

  if (areaRectangle(mergeRectangle(left_rect, rect)) < areaRectangle(mergeRectangle(right_rect, rect)))
    return insertHdmvCollisionTree(
      &node->left, rect, user_value,
      problematic_rect_ret, problematic_user_value_ret
    );
  return insertHdmvCollisionTree(
    &node->right, rect, user_value,
    problematic_rect_ret, problematic_user_value_ret
  );
}
