#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "hdmv_pictures_quantizer.h"

/* ### HDMV Quantizer Hexatree node : ###################################### */

int parseToPaletteHdmvQuantHexTreeNode(
  HdmvQuantHexTreeNodePtr tree,
  HdmvPaletteDefinitionPtr dst
)
{
  unsigned i;

  assert(NULL != dst);

  if (NULL == tree)
    return 0;

  if (tree->leafDist == 0) {
    /* Leaf, collect color */
    return addRgbaEntryHdmvPaletteDefinition(dst, genValueHdmvRGBAData(tree->data));
  }

  for (i = 0; i < 16; i++) {
    if (NULL == tree->childNodes[i])
      continue;
    if (parseToPaletteHdmvQuantHexTreeNode(tree->childNodes[i], dst) < 0)
      return -1;
  }

  return 0;
}

/* ### HDMV Quantizer Hexatree nodes Inventory : ########################### */

HdmvQuantHexTreeNodesInventoryPtr createHdmvQuantHexTreeNodesInventory(
  void
)
{
  HdmvQuantHexTreeNodesInventoryPtr inv;

  inv = (HdmvQuantHexTreeNodesInventoryPtr) malloc(
    sizeof(HdmvQuantHexTreeNodesInventory)
  );
  if (NULL == inv)
    LIBBLU_HDMV_QUANT_ERROR_NRETURN("Memory allocation error.\n");

  *inv = (HdmvQuantHexTreeNodesInventory) {
    0
  };

  return inv;
}

static int increaseAllocationHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventoryPtr inv
)
{
  HdmvQuantHexTreeNode * newSegment;
  size_t newSegmentSize;

  if (inv->allocatedNodesSegments <= inv->usedNodesSegments) {
    /* Reallocate segments array */
    HdmvQuantHexTreeNode ** newArray;
    size_t newSize;

    newSize = GROW_ALLOCATION(
      inv->allocatedNodesSegments,
      HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_NB
    );
    if (lb_mul_overflow(newSize, sizeof(HdmvQuantHexTreeNode *)))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Nodes segments number overflow.\n");

    newArray = (HdmvQuantHexTreeNode **) realloc(
      inv->nodesSegments,
      newSize * sizeof(HdmvQuantHexTreeNode *)
    );
    if (NULL == newArray)
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");

    inv->nodesSegments = newArray;
    inv->allocatedNodesSegments = newSize;
  }

  newSegmentSize =
    HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_SIZE
    << inv->usedNodesSegments
  ;
  if (lb_mul_overflow(newSegmentSize, sizeof(HdmvQuantHexTreeNode)))
    LIBBLU_HDMV_QUANT_ERROR_RETURN("Nodes segment size overflow.\n");

  newSegment = (HdmvQuantHexTreeNode *) malloc(
    newSegmentSize * sizeof(HdmvQuantHexTreeNode)
  );
  if (NULL == newSegment)
    LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");

  inv->nodesSegments[inv->usedNodesSegments++] = newSegment;
  inv->remainingNodes = newSegmentSize;

  return 0;
}

static HdmvQuantHexTreeNodePtr getUnusedNodeHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventoryPtr inv
)
{
  HdmvQuantHexTreeNodePtr node;

  node = inv->unusedNodesList;
  if (NULL != node)
    inv->unusedNodesList = node->nextUnusedNode;

  return node;
}

HdmvQuantHexTreeNodePtr getNodeHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventoryPtr inv
)
{
  assert(NULL != inv);

  if (NULL != inv->unusedNodesList)
    return getUnusedNodeHdmvQuantHexTreeNodesInventory(inv);

  if (!inv->remainingNodes) {
    if (increaseAllocationHdmvQuantHexTreeNodesInventory(inv) < 0)
      return NULL;
  }

  return &inv->nodesSegments[inv->usedNodesSegments-1][--inv->remainingNodes];
}

/* ######################################################################### */

static unsigned getBranchHdmvQuantizationHexatree(
  int depth,
  uint32_t rgba
)
{
  unsigned idx;

  static const unsigned indexes[] = {
    7, 6, 5, 4, 3, 2, 1, 0
  };

  assert(0 <= depth && depth < MAX_DEPTH);

  idx = indexes[depth];
  idx =
    (  (rgba >> (21 + idx)) & 0x8)
    | ((rgba >> (14 + idx)) & 0x4)
    | ((rgba >> ( 7 + idx)) & 0x2)
    | ((rgba >> (     idx)) & 0x1)
  ;

  return idx;
}

/* static unsigned genSignatureHdmvQuantizationHexatree(
  uint32_t rgba
)
{
  int i;

  for (i = 0; i <= MAX_DEPTH; i++)

} */

static int splitLeafHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvQuantHexTreeNodePtr * tree,
  int depth
)
{
  HdmvQuantHexTreeNodePtr leaf;

  if (depth < 0 || MAX_DEPTH < depth)
    LIBBLU_HDMV_QUANT_ERROR_RETURN("Unexpected node depth %d\n", depth);

  assert(NULL != tree);
  assert((*tree)->leafDist == 0);

  leaf = *tree; /* Save leaf */
  if (NULL == (*tree = getNodeHdmvQuantHexTreeNodesInventory(inv)))
    return -1;
  initHdmvQuantHexTreeNode(*tree, false, 0, leaf->data.rep);

  (*tree)->childNodes[
    getBranchHdmvQuantizationHexatree(depth, genValueHdmvRGBAData(leaf->data))
  ] = leaf;

  return 0;
}

static int insertHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvQuantHexTreeNodePtr * tree,
  uint32_t rgba,
  int depth,
  size_t * treeSize
)
{
  int ret;

  assert(NULL != tree);
  assert(NULL != treeSize);
  assert(depth <= MAX_DEPTH + 1);

  if (NULL == *tree) {
    if (NULL == (*tree = getNodeHdmvQuantHexTreeNodesInventory(inv)))
      return -1;

    initHdmvQuantHexTreeNode(*tree, true, rgba, 1);
    (*treeSize)++;
    return 0;
  }

  if ((*tree)->leafDist == 0) {
    /* uint32_t leafRgba = genValueHdmvRGBAData((*tree)->data); */

    /* Leaf */
    if ((*tree)->data.rgba == rgba || depth >= MAX_DEPTH) {
      HdmvRGBAData data;

      setHdmvRGBAData(&data, rgba, 1);
      addHdmvRGBAData(&(*tree)->data, data);
      return 0;
    }

    /* Transform current leaf in a node. */
    if (splitLeafHdmvQuantizationHexatree(inv, tree, depth) < 0)
      return -1;
  }

  ret = insertHdmvQuantizationHexatree(
    inv,
    &(*tree)->childNodes[getBranchHdmvQuantizationHexatree(depth, rgba)],
    rgba,
    depth + 1,
    treeSize
  );
  if (ret < 0)
    return -1;

  /* Updating inter-node values: */
  (*tree)->leafDist = MAX((*tree)->leafDist, ret + 1);
  (*tree)->data.rep++;

  return ret + 1;
}

static unsigned getReducibleBranchHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr node
)
{
  unsigned i, selectedIdx, selectedIdx_rep;
  int selectedIdx_leafDist;


  assert(NULL != node);

  selectedIdx = selectedIdx_leafDist = selectedIdx_rep = 0;

  for (i = 0; i < 16; i++) {
    /* Browse each child */
    if (NULL != node->childNodes[i] && (0 < node->childNodes[i]->leafDist)) {
      /* Selection criterias are sort from
        the highest priority to the lowest. */
      bool update = false;

      if (selectedIdx_leafDist < node->childNodes[i]->leafDist) {
        /* Always set priority to deapest leaves (closest colors,
          least significant codes diff) */
        update = true;
      }

      else if (selectedIdx_leafDist == node->childNodes[i]->leafDist) {
        /* If two childs shares same leaves max distance,
          choose according to representative pixels amount. */

#if COLOR_REDUCTION_PREFERENCE == 0
        /* Priorise the node that represent the most pixels
          (reduce global error sum) */
        update = (selectedIdx_rep < node->childNodes[i]->data.rep);
#else
        /* Priorise the node that represent the fewest pixels
          (preserve details) */
        update = (selectedIdx_rep >= node->childNodes[i]->data.rep);
#endif
      }

      if (update) {
        getHdmvQuantHexTreeNode(
          node->childNodes[i],
          &selectedIdx_leafDist,
          &selectedIdx_rep
        );
        selectedIdx = i;
      }
    }
  }

  if (NULL == node->childNodes[selectedIdx])
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Reached an internal node without any child.\n"
    );

  return selectedIdx;
}

static size_t mergeBranchHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvQuantHexTreeNodePtr node
)
{
  /* Returns the number of merged nodes */
  HdmvRGBAData res;
  size_t mergedNodes;
  int i;

  assert(NULL != inv);
  assert(NULL != node);
  assert(0 < node->leafDist);

  setHdmvRGBAData(&res, 0, 0);
  mergedNodes = 0;

  for (i = 0; i < 16; i++) {
    HdmvQuantHexTreeNodePtr child;

    if (NULL == (child = node->childNodes[i]))
      continue; /* Empty child, continue to next one. */

    if (child->leafDist != 0)
      LIBBLU_HDMV_QUANT_ERROR_ZRETURN(
        "Unexpected internal-node has a 1 leaf-distance node child.\n"
      );

    addHdmvRGBAData(&res, child->data);
    mergedNodes++;

    putBackNodeHdmvQuantHexTreeNodesInventory(inv, child);
    node->childNodes[i] = NULL;
  }

  if (mergedNodes < 2)
    LIBBLU_HDMV_QUANT_ERROR_ZRETURN(
      "Branch merging involves less than two leaves (%p).\n",
      node
    );

  /* Transforms node in leaf (and create an average new color): */
  res.rgba = genValueHdmvRGBAData(res);
  node->leafDist = 0;
  node->data = res;

  return mergedNodes - 1;
}

int reduceHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvQuantHexTreeNodePtr * tree,
  size_t * treeSize
)
{
  HdmvQuantHexTreeNodePtr child;
  size_t sizeReduction;
  unsigned idx, i, nbChilds;

  assert(NULL != tree);
  assert(NULL != treeSize);

  if (NULL == (*tree) || (*tree)->leafDist == 0)
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Unexpected %s in quantization tree.\n",
      (NULL == (*tree)) ? "NULL pointer" : "leaf"
    );

  if ((*tree)->leafDist == 1) {
    /* Performing childs fusion */
    if (!(sizeReduction = mergeBranchHdmvQuantizationHexatree(inv, *tree)))
      return -1;

    if (*treeSize < sizeReduction)
      LIBBLU_HDMV_QUANT_ERROR_RETURN(
        "Unexpected tree size reduction (%zu < %zu).\n",
        *treeSize, sizeReduction
      );

    assert(sizeReduction <= *treeSize);
    *treeSize = *treeSize - sizeReduction;
    return 0;
  }

  /* Looking for a reducible node: */
  idx = getReducibleBranchHdmvQuantizationHexatree(*tree);
  if (reduceHdmvQuantizationHexatree(inv, &((*tree)->childNodes[idx]), treeSize) < 0)
    return -1;

  /* Getting new highest leaf distance: */
  (*tree)->leafDist = nbChilds = 0;
  for (i = 0; i < 16; i++) {
    if (NULL != (*tree)->childNodes[i]) {
      nbChilds++;
      if ((*tree)->leafDist < (*tree)->childNodes[i]->leafDist)
        (*tree)->leafDist = (*tree)->childNodes[i]->leafDist;
    }
  }
  (*tree)->leafDist++;

  if (1 == nbChilds/*  && (*tree)->childNodes[idx]->leafDist == 0 */) {
    /* Current node is now useless, since it only carries one leaf child. */
    child = (*tree)->childNodes[idx];
    putBackNodeHdmvQuantHexTreeNodesInventory(inv, *tree);
    *tree = child;
  }

  return 0;
}

int continueHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr * tree,
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvPicturePtr pic,
  size_t targetedNbColors,
  size_t * outputNbColors
)
{
  unsigned picWidth, picHeight;
  const uint32_t * pix, *eol, * end;
  size_t curNbColors;

  clock_t duration, start;

  assert(NULL != inv);
  assert(NULL != pic);

  LIBBLU_HDMV_QUANT_DEBUG("Starting quantization.\n");

  if (targetedNbColors < 2 || 256 < targetedNbColors)
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Can't handle color number targets out of [2-256] range.\n"
    );

  LIBBLU_HDMV_QUANT_DEBUG(" Parsing picture data.\n");
  getDimensionsHdmvPicture(pic, &picWidth, &picHeight);
  if (NULL == (pix = getRgbaHdmvPicture(pic)))
    return -1;

  if ((start = clock()) < 0)
    LIBBLU_HDMV_QUANT_DEBUG(" Warning: Unable to use clock().\n");

  curNbColors = 0;
  for (end = &pix[picWidth * picHeight]; pix != end; ) {
    for (eol = &pix[picWidth]; pix != eol; pix++) {
      if (insertHdmvQuantizationHexatree(inv, tree, *pix, 0, &curNbColors) < 0)
        goto free_return;
    }

    while (targetedNbColors < curNbColors) {
      /* Reduction needed */
      if (reduceHdmvQuantizationHexatree(inv, tree, &curNbColors) < 0)
        goto free_return;
    }
  }

  duration = clock() - start;
  LIBBLU_HDMV_QUANT_DEBUG(
    " Quantization execution time: %ld ticks (%.2fs, %ld ticks/s).\n",
    duration, (float) duration / CLOCKS_PER_SEC, (clock_t) CLOCKS_PER_SEC
  );

  if (NULL != outputNbColors)
    *outputNbColors = curNbColors;

  LIBBLU_HDMV_QUANT_DEBUG("Completed.\n");
  return 0;

free_return:
  return -1;
}
