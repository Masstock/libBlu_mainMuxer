#include <stdio.h>
#include <stdlib.h>

#include "pgs_merging_tree.h"

MergingTreeNode *createMergingTreeNode(
  HdmvRectangle box,
  MergingTreeNode *left,
  MergingTreeNode *right
)
{
  MergingTreeNode *node = malloc(sizeof(MergingTreeNode));
  if (NULL == node)
    return NULL;

  *node = (MergingTreeNode) {
    .left = left,
    .right = right,
    .box = box
  };

  return node;
}

void destroyMergingTreeNode(
  MergingTreeNode *node
)
{
  if (NULL == node)
    return;
  destroyMergingTreeNode(node->left);
  destroyMergingTreeNode(node->right);
  free(node);
}

int insertMergingTreeNode(
  MergingTreeNode ** tree,
  HdmvRectangle box
)
{
  if (NULL == *tree) {
    MergingTreeNode *node = createMergingTreeNode(box, NULL, NULL);
    if (NULL == node)
      return -1;

    *tree = node;
    return 0;
  }

  if (NULL == (*tree)->left) {
    HdmvRectangle res_box = mergeRectangle((*tree)->box, box);
    MergingTreeNode *node = createMergingTreeNode(res_box, *tree, NULL);
    if (NULL == node)
      return -1;
    *tree = node;
    return insertMergingTreeNode(&node->right, box);
  }

  HdmvRectangle lr_merge    = (*tree)->box;
  HdmvRectangle total_merge = mergeRectangle(lr_merge, box);

  (*tree)->box = total_merge; // Update total bounding box.

  uint32_t r_cost = areaRectangle((*tree)->right->box);
  uint32_t l_cost = areaRectangle((*tree)->left->box);
  uint32_t r_merge_cost  = areaRectangle(mergeRectangle((*tree)->right->box, box)) + l_cost;
  uint32_t l_merge_cost  = areaRectangle(mergeRectangle((*tree)->left->box, box)) + r_cost;
  uint32_t lr_merge_cost = areaRectangle(lr_merge);

  if (l_merge_cost < r_merge_cost) {
    if (l_merge_cost <= lr_merge_cost) {
      /* Left + Node merge */
      return insertMergingTreeNode(&(*tree)->left, box);
    }
  }
  else {
    if (r_merge_cost <= lr_merge_cost) {
      /* Right + Node merge */
      return insertMergingTreeNode(&(*tree)->right, box);
    }
  }

  /* Left + Right merge */
  MergingTreeNode *new_l = createMergingTreeNode(lr_merge, (*tree)->left, (*tree)->right);
  if (NULL == new_l)
    return -1;
  MergingTreeNode *new_r = createMergingTreeNode(box, NULL, NULL);
  if (NULL == new_r)
    return -1;

  (*tree)->left  = new_l;
  (*tree)->right = new_r;
  return 0;
}
