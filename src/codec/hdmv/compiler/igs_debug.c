#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* #include <libbmp.h> */

#include "igs_debug.h"

#if ENABLE_IGS_DEBUG
void printPalette(HdmvPaletteDefinitionPtr pal)
{
  unsigned i;
  assert(NULL != pal);

  lbc_printf(
    "Palette (indexes: %zu):\n",
    pal->nbUsedEntries
  );

  for (i = 0; i < pal->nbUsedEntries; i++)
    lbc_printf(" - 0x%08" PRIX32 "\n", pal->entries[i].rgba);
}

void writeHexatreeNode(FILE * f, HdmvQuantHexTreeNodePtr node)
{
  unsigned i;

  if (NULL == node)
    return;

  if (0 < node->leafDist) /* Internal node */
    fprintf(f, "  n%p [label=\"<hexNode> %p, %d, %" PRIu64 "px\"];\n", node, node, node->leafDist, node->data.rep);
  else /* Leaf */
    fprintf(
      f, "  n%p [label=\"<hexNode> %p, #%08" PRIX32 ", %" PRIu64 "px\"];\n",
      node, node, genValueHdmvRGBAData(node->data), node->data.rep
    );

  for (i = 0; i < ((0 < node->leafDist) ? 16 : 0); i++) {
    if (NULL != node->childNodes[i]) {
      writeHexatreeNode(f, node->childNodes[i]);
      fprintf(f, "  n%p:hexNode:c -> n%p:hexNode;\n", node, node->childNodes[i]);
    }
  }
}

void ecrireDebut(FILE * f)
{
  fprintf(f, "digraph arbre {\n");
  fprintf(f, "  node [shape=%s, height=%s]\n", "record", ".1");
  fprintf(f, "  edge [tailclip=%s, arrow=%s, dir=%s];\n", "true", "dot", "forward");
}

void ecrireFin(FILE * f)
{
  fprintf(f, "}\n");
}

void dessineCode(FILE * f, HdmvQuantHexTreeNodePtr tree)
{
  if (NULL == tree)
    return;

  ecrireDebut(f);
  writeHexatreeNode(f, tree);
  ecrireFin(f);
}

int creePDFCode(char *dot, char *pdf, HdmvQuantHexTreeNodePtr tree)
{
  int ret;

  FILE * out;
  char cmd[IGS_DEBUG_MAX_CMD_LEN+1];

  if (NULL == tree) {
    fprintf(stderr, "Erreur, l'arbre donné en paramètre vaut 'NULL'.\n");
    return -1;
  }

  if (NULL == (out = fopen(dot, "w"))) {
    fprintf(stderr, "Erreur, impossible de créer le fichier de sortie.\n");
    return -1;
  }

  dessineCode(out, tree);
  fclose(out);

  ret = snprintf(cmd, IGS_DEBUG_MAX_CMD_LEN, "dot -Tpdf %s -o %s", dot, pdf);
  if (0 < ret)
    ret = system(cmd);

  return ret;
}


int creePNGCode(char *dot, char *png, HdmvQuantHexTreeNodePtr tree)
{
  int ret;

  FILE * out;
  char cmd[IGS_DEBUG_MAX_CMD_LEN+1];

  if (NULL == tree) {
    fprintf(stderr, "Erreur, l'arbre donné en paramètre vaut 'NULL'.\n");
    return -1;
  }

  if (NULL == (out = fopen(dot, "w"))) {
    fprintf(stderr, "Erreur, impossible de créer le fichier de sortie.\n");
    return -1;
  }

  dessineCode(out, tree);
  fclose(out);

  ret = snprintf(cmd, IGS_DEBUG_MAX_CMD_LEN, "dot -Tpng %s -o %s", dot, png);
  if (0 < ret)
    ret = system(cmd);

  return ret;
}
#endif

#if 0
#include <libbmp.h>

int writeTestBmp(IgsCompilerInputPicturePtr pic, IgsCompilerColorPalettePtr pal)
{
  bmp_img img;
  uint32_t color;
  uint8_t * drawingPnt;
  size_t y, x;
  uint8_t r, g, b, a;

  bmp_img_init_df(&img, pic->infos.width, pic->infos.height);

  drawingPnt = pic->indexedData;
  for (y = 0; y < pic->infos.height; y++) {
    for (x = 0; x < pic->infos.width; x++) {
      color = pal->palette_entries[*(drawingPnt++)];

      r = (color >> 24) & 0xFF;
      g = (color >> 16) & 0xFF;
      b = (color >>  8) & 0xFF;
      /* a = (color      ) & 0xFF; */

      bmp_pixel_init(&img.img_pixels[y][x], r, g, b);
    }
  }

  bmp_img_write(&img, "test.bmp");
  bmp_img_free(&img);
  return 0;
}
#endif