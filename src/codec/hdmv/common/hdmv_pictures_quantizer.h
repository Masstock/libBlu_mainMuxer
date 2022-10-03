

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__QUANTIZER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__QUANTIZER_H__

#include "../../../util.h"

#include "hdmv_error.h"
#include "hdmv_palette_def.h"
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
  uint32_t rgba;

  assert(0 < data.rep);

  rgba  = CHANNEL_VALUE(data.r / data.rep, C_R);
  rgba |= CHANNEL_VALUE(data.g / data.rep, C_G);
  rgba |= CHANNEL_VALUE(data.b / data.rep, C_B);
  rgba |= CHANNEL_VALUE(data.a / data.rep, C_A);

  return rgba;
}

/* ### HDMV Quantizer Hexatree node : ###################################### */

typedef union HdmvQuantHexTreeNode {
  union HdmvQuantHexTreeNode * nextUnusedNode; /* Only used in inventory. */

  struct {
    int leafDist; /* If == 0, leaf, else node */

    HdmvRGBAData data;
    union HdmvQuantHexTreeNode * childNodes[16];
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
    .leafDist = (!isLeaf) /* If leaf, 0 otherwise 1. */
  };
  setHdmvRGBAData(&dst->data, rgba, rep);
}

static inline void getHdmvQuantHexTreeNode(
  HdmvQuantHexTreeNodePtr node,
  int * leafDist,
  unsigned * rep
)
{
  assert(NULL != node);
  assert(NULL != leafDist);
  assert(NULL != rep);

  *leafDist = node->leafDist;
  *rep = node->data.rep;
}

int parseToPaletteHdmvQuantHexTreeNode(
  HdmvQuantHexTreeNodePtr tree,
  HdmvPaletteDefinitionPtr dst
);

/* ### HDMV Quantizer Hexatree nodes Inventory : ########################### */

#define HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_NB  4
#define HDMV_QUANT_HEXTREE_DEFAULT_NODES_SEG_SIZE  8

typedef struct {
  HdmvQuantHexTreeNode ** nodesSegments;
  size_t allocatedNodesSegments;
  size_t usedNodesSegments;
  size_t remainingNodes;

  HdmvQuantHexTreeNodePtr unusedNodesList;
} HdmvQuantHexTreeNodesInventory, *HdmvQuantHexTreeNodesInventoryPtr;

HdmvQuantHexTreeNodesInventoryPtr createHdmvQuantHexTreeNodesInventory(
  void
);

static inline void destroyHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventoryPtr inv
)
{
  size_t i;

  if (NULL == inv)
    return;

  for (i = 0; i < inv->usedNodesSegments; i++)
    free(inv->nodesSegments[i]);
  free(inv->nodesSegments);
  /* DO NOT free inv->unusedNodesList */
  free(inv);
}

HdmvQuantHexTreeNodePtr getNodeHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventoryPtr inv
);

static inline void putBackNodeHdmvQuantHexTreeNodesInventory(
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvQuantHexTreeNodePtr node
)
{
  assert(NULL != inv);

  if (NULL == node)
    return;

  node->nextUnusedNode = inv->unusedNodesList;
  inv->unusedNodesList = node;
}

/* ######################################################################### */

int continueHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr * tree,
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvPicturePtr pic,
  size_t targetedNbColors,
  size_t * outputNbColors
);

static inline int generateHdmvQuantizationHexatree(
  HdmvQuantHexTreeNodePtr * tree,
  HdmvQuantHexTreeNodesInventoryPtr inv,
  HdmvPicturePtr pic,
  size_t targetedNbColors,
  size_t * outputNbColors
)
{
  *tree = NULL;
  return continueHdmvQuantizationHexatree(
    tree, inv, pic, targetedNbColors, outputNbColors
  );
}


#endif