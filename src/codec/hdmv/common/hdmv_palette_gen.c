#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "hdmv_palette_gen.h"

extern int creePDFCode(char *dot, char *pdf, HdmvQuantHexTreeNodePtr tree);

int buildPaletteListHdmvPalette(
  HdmvPaletteDefinitionPtr dst,
  const HdmvPicturesListPtr list,
  size_t targetedNbColors,
  HdmvQuantHexTreeNodesInventoryPtr inv
)
{
  HdmvPicturePtr pic;
  unsigned i;
  bool localInv;

  HdmvQuantHexTreeNodePtr tree;

  assert(NULL != list);

  if (NULL == inv) {
    if (NULL == (inv = createHdmvQuantHexTreeNodesInventory()))
      return -1;
    localInv = true;
  }
  else
    localInv = false;

  assert(NULL != inv);

  tree = NULL, i = 0;
  while (NULL != (pic = iterateHdmvPicturesList(list, &i))) {
    if (continueHdmvQuantizationHexatree(&tree, inv, pic, targetedNbColors, NULL) < 0)
      goto free_return;
  }

  if (parseToPaletteHdmvQuantHexTreeNode(tree, dst) < 0)
    goto free_return;

  if (localInv)
    destroyHdmvQuantHexTreeNodesInventory(inv);
  return 0;

free_return:
  if (localInv)
    destroyHdmvQuantHexTreeNodesInventory(inv);
  return -1;
}