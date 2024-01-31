/** \~english
 * \file hdmv_quantizer.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief TODO
 *
 * \todo Fix algorithm
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_QUANTIZER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_QUANTIZER_H__

#include "../../../util.h"

#include "hdmv_error.h"
#include "hdmv_palette.h"
#include "hdmv_pictures_common.h"
#include "hdmv_color.h"

#define MAX_DEPTH 8
#define IGS_QUANTIZER_NODES_ALLOC_INCREMENT 1024

#define COLOR_REDUCTION_PREFERENCE 1
/**
 * 0: Merge in priority biggest color flats.
 * 1: Merge in priority smallest color flats.
 */

typedef struct {
  // TODO: Switch to Lab color space
  uint64_t r;     /**< Red color channel.                                    */
  uint64_t g;     /**< Green color channel.                                  */
  uint64_t b;     /**< Blue color channel.                                   */
  uint64_t a;     /**< Alpha transparency channel.                           */
  uint64_t rep;   /**< Number of pixels using this color.                    */
  uint32_t rgba;  /**< RGBA color value used for comparisons. This value
    is updated when merging colors.                                          */
} HdmvRGBAData;

/** \~english
 * \brief Set a #HdmvRGBAData object.
 *
 * \param dst Destination data.
 * \param rgba RGBA color value.
 * \param rep Number of pixels sharing this color value.
 */
static inline void setHdmvRGBAData(
  HdmvRGBAData * dst,
  uint32_t rgba,
  uint64_t rep
)
{
  *dst = (HdmvRGBAData) {
    .r = GET_CHANNEL(rgba, C_R) * rep,
    .g = GET_CHANNEL(rgba, C_G) * rep,
    .b = GET_CHANNEL(rgba, C_B) * rep,
    .a = GET_CHANNEL(rgba, C_A) * rep,
    .rep = rep,
    .rgba = rgba
  };
}

/** \~english
 * \brief Perform sum between two operands and save result on the first one.
 *
 * \param dst Destination operand.
 * \param src Source operand.
 *
 * \note #HdmvRGBAData.rgba value is not updated.
 */
static inline void addHdmvRGBAData(
  HdmvRGBAData * dst,
  HdmvRGBAData src
)
{
  dst->r   += src.r;
  dst->g   += src.g;
  dst->b   += src.b;
  dst->a   += src.a;
  dst->rep += src.rep;
}

/** \~english
 * \brief Generate the RGBA value.
 *
 * \param data Source RGBA data object.
 * \return uint32_t Generated RGBA value.
 */
static inline uint32_t genValueHdmvRGBAData(
  HdmvRGBAData data
)
{
  assert(0 < data.rep);

  uint32_t rgba = 0x00;
  rgba |= CHANNEL_VALUE(DIV(data.r, data.rep), C_R);
  rgba |= CHANNEL_VALUE(DIV(data.g, data.rep), C_G);
  rgba |= CHANNEL_VALUE(DIV(data.b, data.rep), C_B);
  rgba |= CHANNEL_VALUE(DIV(data.a, data.rep), C_A);

  return rgba;
}

/* ### HDMV Quantizer Hexatree node : ###################################### */

typedef union HdmvQuantHexTreeNode {
  union HdmvQuantHexTreeNode * next_unused_node; /* Only used in inventory. */

  struct {
    int leaf_dist; /* If == 0, leaf, else node */

    HdmvRGBAData data;
    union HdmvQuantHexTreeNode * child_nodes[16];
  };
} HdmvQuantHexTreeNode, *HdmvQuantHexTreeNodePtr;

static inline void initHdmvQuantHexTreeNode(
  HdmvQuantHexTreeNode * dst,
  bool isLeaf,
  uint32_t rgba,
  uint64_t rep
)
{
  *dst = (HdmvQuantHexTreeNode) {
    .leaf_dist = (!isLeaf) /* If leaf, 0 otherwise 1. */
  };
  setHdmvRGBAData(&dst->data, rgba, rep);
}

static inline void getHdmvQuantHexTreeNode(
  HdmvQuantHexTreeNodePtr node,
  int * leaf_dist,
  unsigned * rep
)
{
  assert(NULL != node);
  assert(NULL != leaf_dist);
  assert(NULL != rep);

  *leaf_dist = node->leaf_dist;
  *rep = node->data.rep;
}

int parseToPaletteHdmvQuantHexTreeNode(
  HdmvPalette * dst,
  HdmvQuantHexTreeNodePtr tree
);

/* ### HDMV Quantizer Hexatree nodes Inventory : ########################### */

#define HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_NB  4
#define HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_SIZE  8

typedef struct {
  HdmvQuantHexTreeNode ** node_segments;
  size_t nb_alloc_node_seg;
  size_t nb_used_node_seg;
  size_t nb_rem_nodes;

  HdmvQuantHexTreeNodePtr unused_nodes_list;
} HdmvQuantHexTreeNodesInventory;

static inline void cleanHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventory inv
)
{
  for (size_t i = 0; i < inv.nb_used_node_seg; i++)
    free(inv.node_segments[i]);
  free(inv.node_segments);
  // DO NOT free inv.unused_nodes_list
}

/* ######################################################################### */

int performHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr * tree_ref,
  HdmvQuantHexTreeNodesInventory * inv,
  HdmvBitmap bitmap,
  unsigned target_nb_colors,
  unsigned * out_nb_colors_ref
);

static inline int generateHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr * tree,
  HdmvQuantHexTreeNodesInventory * inv,
  HdmvBitmap bitmap,
  unsigned target_nb_colors,
  unsigned * out_nb_colors_ret
)
{
  *tree = NULL;
  return performHdmvQuantizationHexatree(
    tree, inv, bitmap, target_nb_colors, out_nb_colors_ret
  );
}


#endif