#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "hdmv_quantizer.h"

/* ### Debug : ############################################################# */

void writeDotHexatreeNode(
  FILE *fd,
  HdmvQuantHexTreeNodePtr node
)
{
  if (NULL == node)
    return;

  if (0 < node->leaf_dist) /* Internal node */
    fprintf(
      fd, "  n%p [label=\"<hex_node> %p, %d, %" PRIu64 "px, %d\"];\n",
      node, node, node->leaf_dist, node->data.rep, node->leaf_dist
    );
  else /* Leaf */
    fprintf(
      fd, "  n%p [label=\"<hex_node> %p, #%08" PRIX32 ", %" PRIu64 "px\"];\n",
      node, node, genValueHdmvRGBAData(node->data), node->data.rep
    );

  if (0 == node->leaf_dist)
    return;

  for (unsigned i = 0; i < 16u; i++) {
    if (NULL != node->child_nodes[i]) {
      writeDotHexatreeNode(fd, node->child_nodes[i]);
      fprintf(
        fd, "  n%p:hex_node:c -> n%p:hex_node [label=\"%u\"];\n",
        node, node->child_nodes[i], i
      );
    }
  }
}

void writeDotHeader(
  FILE *fd
)
{
  fprintf(fd, "digraph hex_tree {\n");
  fprintf(fd, "  node [shape=%s, height=%s]\n", "record", ".1");
  fprintf(fd, "  edge [tailclip=%s, arrow=%s, dir=%s];\n", "true", "dot", "forward");
}

void writeDotFooter(
  FILE *fd
)
{
  fprintf(fd, "}\n");
}

void writeDot(
  FILE *fd,
  HdmvQuantHexTreeNodePtr tree
)
{
  writeDotHeader(fd);
  writeDotHexatreeNode(fd, tree);
  writeDotFooter(fd);
}

#define HDMV_QUANT_DEBUG_MAX_FILENAME_LEN 100
#define HDMV_QUANT_DEBUG_MAX_CMD_LEN 200

int saveDebugPDF(
  char *base_filepath,
  HdmvQuantHexTreeNodePtr tree
)
{
  char dot_fp[HDMV_QUANT_DEBUG_MAX_FILENAME_LEN+1];
  snprintf(dot_fp, HDMV_QUANT_DEBUG_MAX_FILENAME_LEN, "%s.dot", base_filepath);

  FILE *fd = fopen(dot_fp, "w");
  if (NULL == fd)
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Unable to open output debug file '%s', %s (errno %d).\n",
      dot_fp,
      strerror(errno),
      errno
    );

  writeDot(fd, tree);
  fclose(fd);

  char cmd[HDMV_QUANT_DEBUG_MAX_CMD_LEN+1];
  snprintf(
    cmd, HDMV_QUANT_DEBUG_MAX_CMD_LEN,
    "dot -Tpdf %s -o %s.pdf", dot_fp, base_filepath
  );

  int ret = system(cmd);
  if (0 != ret)
    LIBBLU_HDMV_QUANT_ERROR_RETURN("dot exit code %d.\n", ret);

  return 0;
}

/* ### HDMV Quantizer Hexatree node : ###################################### */

int parseToPaletteHdmvQuantHexTreeNode(
  HdmvPalette *dst,
  HdmvQuantHexTreeNodePtr tree
)
{
  assert(NULL != dst);

  if (NULL == tree)
    return 0;

  if (tree->leaf_dist == 0) {
    /* Leaf, collect color */
    return addRgbaEntryHdmvPalette(dst, genValueHdmvRGBAData(tree->data));
  }

  for (unsigned i = 0; i < 16u; i++) {
    if (parseToPaletteHdmvQuantHexTreeNode(dst, tree->child_nodes[i]) < 0)
      return -1;
  }

  return 0;
}

/* ### HDMV Quantizer Hexatree nodes Inventory : ########################### */

static int _increaseAllocInv(
  HdmvQuantHexTreeNodesInventory *inv
)
{
  if (inv->nb_alloc_node_seg <= inv->nb_used_node_seg) {
    /* Reallocate segments array */
    size_t new_size = GROW_ALLOCATION(
      inv->nb_alloc_node_seg,
      HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_NB
    );
    if (lb_mul_overflow_size_t(new_size, sizeof(HdmvQuantHexTreeNode *)))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Nodes segments number overflow.\n");

    HdmvQuantHexTreeNode ** new_arr = (HdmvQuantHexTreeNode **) realloc(
      inv->node_segments,
      new_size *sizeof(HdmvQuantHexTreeNode *)
    );
    if (NULL == new_arr)
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");

    inv->node_segments = new_arr;
    inv->nb_alloc_node_seg = new_size;
  }

  size_t new_seg_size =
    HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_SIZE
    << inv->nb_used_node_seg
  ;
  if (lb_mul_overflow_size_t(new_seg_size, sizeof(HdmvQuantHexTreeNode)))
    LIBBLU_HDMV_QUANT_ERROR_RETURN("Nodes segment size overflow.\n");

  HdmvQuantHexTreeNode *new_seg = (HdmvQuantHexTreeNode *) malloc(
    new_seg_size *sizeof(HdmvQuantHexTreeNode)
  );
  if (NULL == new_seg)
    LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");

  inv->node_segments[inv->nb_used_node_seg++] = new_seg;
  inv->nb_rem_nodes = new_seg_size;

  return 0;
}

static HdmvQuantHexTreeNodePtr _getUnusedNodeInv(
  HdmvQuantHexTreeNodesInventory *inv
)
{
  HdmvQuantHexTreeNodePtr node = inv->unused_nodes_list;
  if (NULL != node)
    inv->unused_nodes_list = node->next_unused_node;
  return node;
}

static HdmvQuantHexTreeNodePtr _getNodeInv(
  HdmvQuantHexTreeNodesInventory *inv
)
{
  assert(NULL != inv);

  if (NULL != inv->unused_nodes_list)
    return _getUnusedNodeInv(inv);

  if (!inv->nb_rem_nodes) {
    if (_increaseAllocInv(inv) < 0)
      return NULL;
  }

  return &inv->node_segments[inv->nb_used_node_seg-1][--inv->nb_rem_nodes];
}

static void _putBackNodeInv(
  HdmvQuantHexTreeNodesInventory *inv,
  HdmvQuantHexTreeNodePtr node
)
{
  assert(NULL != inv);

  if (NULL == node)
    return;

  node->next_unused_node = inv->unused_nodes_list;
  inv->unused_nodes_list = node;
}

/* ######################################################################### */

static unsigned _getBranch(
  int depth,
  uint32_t rgba
)
{
  assert(0 <= depth && depth < MAX_DEPTH);

  unsigned idx = 7u - depth;

  return (
    (  (rgba >> (21u + idx)) & 0x8)
    | ((rgba >> (14u + idx)) & 0x4)
    | ((rgba >> ( 7u + idx)) & 0x2)
    | ((rgba >> (      idx)) & 0x1)
  );
}

static int splitLeafHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventory *inv,
  HdmvQuantHexTreeNodePtr *tree,
  int depth
)
{
  if (depth < 0 || MAX_DEPTH < depth)
    LIBBLU_HDMV_QUANT_ERROR_RETURN("Unexpected node depth %d\n", depth);

  assert(NULL != tree);

  HdmvQuantHexTreeNodePtr leaf = *tree; /* Save leaf */
  assert(leaf->leaf_dist == 0);

  if (NULL == (*tree = _getNodeInv(inv)))
    return -1;
  initHdmvQuantHexTreeNode(*tree, false, 0, leaf->data.rep);

  uint32_t rgba = genValueHdmvRGBAData(leaf->data);

  (*tree)->child_nodes[_getBranch(depth, rgba)] = leaf;

  return 0;
}

static int insertHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventory *inv,
  HdmvQuantHexTreeNodePtr *tree_ref,
  uint32_t rgba,
  int depth,
  unsigned *tree_size_ref
)
{
  int ret;

  assert(NULL != tree_ref);
  assert(NULL != tree_size_ref);
  assert(depth <= MAX_DEPTH);

  if (NULL == *tree_ref) {
    if (NULL == (*tree_ref = _getNodeInv(inv)))
      return -1;

    initHdmvQuantHexTreeNode(*tree_ref, true, rgba, 1);
    (*tree_size_ref)++;
    return 0;
  }

  if ((*tree_ref)->leaf_dist == 0) {
    /* Leaf */
    if ((*tree_ref)->data.rgba == rgba || depth >= MAX_DEPTH) {
      // assert(depth != MAX_DEPTH || (*tree_ref)->data.rgba == rgba);
      HdmvRGBAData data;

      setHdmvRGBAData(&data, rgba, 1);
      addHdmvRGBAData(&(*tree_ref)->data, data);
      return 0;
    }

    /* Transform current leaf in a node. */
    if (splitLeafHdmvQuantizationHexatree(inv, tree_ref, depth) < 0)
      return -1;

  }

  ret = insertHdmvQuantizationHexatree(
    inv,
    &((*tree_ref)->child_nodes[_getBranch(depth, rgba)]),
    rgba,
    depth + 1,
    tree_size_ref
  );
  if (ret < 0)
    return -1;

  /* Updating inter-node values: */
  (*tree_ref)->leaf_dist = MAX((*tree_ref)->leaf_dist, ret + 1);
  (*tree_ref)->data.rep++;

  return ret + 1;
}

static unsigned getReducibleBranchHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr node
)
{
  assert(NULL != node);

  unsigned sel_idx = 0, sel_idx_rep = 0;
  int sel_idx_leaf_dist = 0;

  for (unsigned i = 0; i < 16u; i++) {
    /* Browse each child */
    if (NULL != node->child_nodes[i] && (0 < node->child_nodes[i]->leaf_dist)) {
      /* Selection criterias are sort from
        the highest priority to the lowest. */
      bool update = false;

      if (sel_idx_leaf_dist < node->child_nodes[i]->leaf_dist) {
        /* Always set priority to deapest leaves (closest colors,
          least significant codes diff) */
        update = true;
      }

      else if (sel_idx_leaf_dist == node->child_nodes[i]->leaf_dist) {
        /* If two childs shares same leaves max distance,
          choose according to representative pixels amount. */

#if COLOR_REDUCTION_PREFERENCE == 0
        /* Priorise the node that represent the most pixels
          (reduce global error sum) */
        update = (sel_idx_rep < node->child_nodes[i]->data.rep);
#else
        /* Priorise the node that represent the fewest pixels
          (preserve details) */
        update = (sel_idx_rep >= node->child_nodes[i]->data.rep);
#endif
      }

      if (update) {
        getHdmvQuantHexTreeNode(
          node->child_nodes[i],
          &sel_idx_leaf_dist,
          &sel_idx_rep
        );
        sel_idx = i;
      }
    }
  }

  if (NULL == node->child_nodes[sel_idx])
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Reached an internal node without any child.\n"
    );

  return sel_idx;
}

static unsigned mergeBranchHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventory *inv,
  HdmvQuantHexTreeNodePtr node
)
{
  /* Returns the number of merged nodes */
  assert(NULL != inv);
  assert(NULL != node);
  assert(0 < node->leaf_dist);

  HdmvRGBAData res;
  setHdmvRGBAData(&res, 0, 0);
  unsigned nb_merged_nodes = 0u;

  // fprintf(stderr, "Collect nodes:\n");

  for (unsigned i = 0; i < 16u; i++) {
    HdmvQuantHexTreeNodePtr child = node->child_nodes[i];
    if (NULL == child)
      continue; /* Empty child, continue to next one. */

    if (child->leaf_dist != 0)
      LIBBLU_HDMV_QUANT_ERROR_ZRETURN(
        "Unexpected internal-node has a 1 leaf-distance node child.\n"
      );

    // fprintf(
    //   stderr, " %" PRIu64 ") 0x%08" PRIX32
    //   " 0x%02" PRIX64 " 0x%02" PRIX64 " 0x%02" PRIX64 " 0x%02" PRIX64 "\n",
    //   child->data.rep,
    //   child->data.rgba,
    //   child->data.r,
    //   child->data.g,
    //   child->data.b,
    //   child->data.a
    // );

    addHdmvRGBAData(&res, child->data);
    nb_merged_nodes++;

    _putBackNodeInv(inv, child);
    node->child_nodes[i] = NULL;
  }

  // if (nb_merged_nodes < 2)
  //   LIBBLU_HDMV_QUANT_ERROR_ZRETURN(
  //     "Branch merging involves less than two leaves (%p, %u).\n",
  //     node, moment
  //   );

  /* Transforms node in leaf (and create an average new color): */
  res.rgba = genValueHdmvRGBAData(res);

  node->leaf_dist = 0;
  node->data = res;

  return nb_merged_nodes - 1;
}

int reduceHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodesInventory *inv,
  HdmvQuantHexTreeNodePtr *tree,
  unsigned *tree_size_ref
)
{
  assert(NULL != tree);
  assert(NULL != tree_size_ref);

  if (NULL == (*tree) || (*tree)->leaf_dist == 0)
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Unexpected %s in quantization tree.\n",
      (NULL == (*tree)) ? "NULL pointer" : "leaf"
    );

  if ((*tree)->leaf_dist == 1) {
    /* Performing childs fusion */
    unsigned size_red = mergeBranchHdmvQuantizationHexatree(inv, *tree);
    if (0 == size_red)
      return 0;

    if (*tree_size_ref < size_red)
      LIBBLU_HDMV_QUANT_ERROR_RETURN(
        "Unexpected tree size reduction (%zu < %zu).\n",
        *tree_size_ref, size_red
      );

    assert(size_red <= *tree_size_ref);
    *tree_size_ref = *tree_size_ref - size_red;
    return 0;
  }

  /* Looking for a reducible node: */
  unsigned idx = getReducibleBranchHdmvQuantizationHexatree(*tree);
  if (reduceHdmvQuantizationHexatree(inv, &((*tree)->child_nodes[idx]), tree_size_ref) < 0)
    return -1;

  /* Getting new highest leaf distance: */
  unsigned nb_childs = 0u;
  (*tree)->leaf_dist = 0;
  for (unsigned i = 0; i < 16u; i++) {
    if (NULL != (*tree)->child_nodes[i]) {
      (*tree)->leaf_dist = MAX((*tree)->leaf_dist, (*tree)->child_nodes[i]->leaf_dist);
      nb_childs++;
    }
  }
  (*tree)->leaf_dist++;

  if (1 == nb_childs /* && (*tree)->child_nodes[idx]->leafDist == 0 */) {
    /* Current node is now useless, since it only carries one leaf child. */
    HdmvQuantHexTreeNodePtr child = (*tree)->child_nodes[idx];
    _putBackNodeInv(inv, *tree);
    *tree = child;
  }

  return 0;
}

int performHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr *tree_ref,
  HdmvQuantHexTreeNodesInventory *inv,
  HdmvBitmap bitmap,
  unsigned target_nb_colors,
  unsigned *out_nb_colors_ref
)
{
  assert(NULL != inv);

  LIBBLU_HDMV_QUANT_DEBUG("Starting quantization.\n");

  if (target_nb_colors < 2 || 255 < target_nb_colors)
    LIBBLU_HDMV_QUANT_ERROR_RETURN(
      "Can't handle color number targets out of [2-255] range.\n"
    );

  LIBBLU_HDMV_QUANT_DEBUG(" Parsing picture data.\n");
  clock_t start = clock();
  if (start < 0)
    LIBBLU_HDMV_QUANT_DEBUG(" Warning: Unable to use clock().\n");

  const uint32_t *pix   = bitmap.rgba;
  const uint32_t *end   = &pix[bitmap.width *bitmap.height];
  unsigned cur_nb_colors = (NULL != out_nb_colors_ref) ? *out_nb_colors_ref : 0;
  size_t stride          = bitmap.width;

  while (pix != end) {
    for (const uint32_t *eol = &pix[stride]; pix != eol; pix++) {
      if (insertHdmvQuantizationHexatree(inv, tree_ref, *pix, 0, &cur_nb_colors) < 0)
        goto free_return;
    }

    while (target_nb_colors < cur_nb_colors) {
      /* Reduction needed */
      if (reduceHdmvQuantizationHexatree(inv, tree_ref, &cur_nb_colors) < 0)
        goto free_return;
    }
  }

  if (0 <= start) {
    clock_t duration = clock() - start;
    LIBBLU_HDMV_QUANT_DEBUG(
      " Quantization execution time: %ld ticks (%.2fs, %ld ticks/s).\n",
      duration, (float) duration / CLOCKS_PER_SEC, (clock_t) CLOCKS_PER_SEC
    );
  }

  LIBBLU_HDMV_QUANT_DEBUG(
    " Output number of colors / target: %u / %u.\n",
    cur_nb_colors,
    target_nb_colors
  );

  if (NULL != out_nb_colors_ref)
    *out_nb_colors_ref = cur_nb_colors;

  LIBBLU_HDMV_QUANT_DEBUG("Completed.\n");
  return 0;

free_return:
  return -1;
}
