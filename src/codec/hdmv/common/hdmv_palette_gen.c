#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "hdmv_palette_gen.h"

int buildPaletteListHdmvPalette(
  HdmvPalette *dst,
  HdmvBitmapList *list,
  unsigned target_nb_colors,
  HdmvQuantHexTreeNodesInventory *inv_ref
)
{
  HdmvQuantHexTreeNodesInventory local_inv = {0};
  bool use_local_inv = false;

  if (NULL == inv_ref) {
    inv_ref = &local_inv;
    use_local_inv = true;
  }

  HdmvQuantHexTreeNodePtr tree = NULL;

  unsigned i = 0;
  HdmvBitmap bitmap;
  unsigned cur_nb_colors = 0u;
  while (iterateHdmvBitmapList(list, &bitmap, &i)) {
    if (performHdmvQuantizationHexatree(&tree, inv_ref, bitmap, target_nb_colors, &cur_nb_colors) < 0)
      goto free_return;
  }

  if (parseToPaletteHdmvQuantHexTreeNode(dst, tree) < 0)
    goto free_return;

  if (use_local_inv)
    cleanHdmvQuantHexTreeNodesInventory(local_inv);
  return 0;

free_return:
  if (use_local_inv)
    cleanHdmvQuantHexTreeNodesInventory(local_inv);
  return -1;
}
