/** \~english
 * \file igs_compiler_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IGS Compiler data structures module.
 */

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__DATA_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__DATA_H__

#include "igs_debug.h"

#include "../../../util.h"
#include "../../../util/xmlParsing.h"
#include "../common/hdmv_common.h"

#include "../common/hdmv_bitmap_indexer.h"
#include "../common/hdmv_error.h"
#include "../common/hdmv_object.h"
#include "../common/hdmv_palette.h"
#include "../common/hdmv_pictures_common.h"

#define HDMV_IGS_COMPL_OUTPUT_EXT ".ies"
#define HDMV_IGS_COMPL_OUTPUT_FMT  "%s" HDMV_IGS_COMPL_OUTPUT_EXT

#if 0
typedef struct {
  uint32_t *palette_entries;
  uint32_t *yuvEntries;
  unsigned capacity;
  unsigned usedEntries;

  uint8_t version;
} IgsCompilerColorPalette, *IgsCompilerColorPalettePtr;
#endif

/* ### IGS Compiler Composition : ########################################## */

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvICParameters interactive_composition;

  /* Hash table (key: lbc *reference picture name, data: HdmvBitmap) */
  HdmvPicturesIndexer ref_pics_indexer;
  HdmvBitmap *ref_pics_bitmaps;
  unsigned nb_allocated_ref_pics_bitmaps;
  unsigned nb_used_ref_pics_bitmaps;

  HdmvBitmap *object_bitmaps;
  unsigned nb_allocated_object_bitmaps;
  unsigned nb_used_object_bitmaps;

  HdmvPalette *palettes;
  unsigned nb_allocated_palettes;
  unsigned nb_used_palettes;

  HdmvObject *objects;
  unsigned nb_objects;
} IgsCompilerComposition;

/* ###### Creation / Destruction : ######################################### */

static inline void cleanIgsCompilerComposition(
  IgsCompilerComposition compo
)
{
  cleanHdmvICParameters(compo.interactive_composition);
  cleanHdmvPicturesIndexer(compo.ref_pics_indexer);
  for (unsigned i = 0; i < compo.nb_used_object_bitmaps; i++)
    cleanHdmvBitmap(compo.object_bitmaps[i]);
  free(compo.object_bitmaps);
  free(compo.palettes);
  for (unsigned i = 0; i < compo.nb_objects; i++)
    cleanHdmvObject(compo.objects[i]);
  free(compo.objects);
}

/* ###### Add Entry : ###################################################### */

int addRefPictureIgsCompilerComposition(
  IgsCompilerComposition *dst,
  HdmvBitmap bitmap,
  const lbc *name
);

int addObjectIgsCompilerComposition(
  IgsCompilerComposition *dst,
  HdmvBitmap pic,
  unsigned *idx_ret
);

int addPaletteIgsCompilerComposition(
  IgsCompilerComposition *dst,
  HdmvPalette *pal,
  uint8_t *palette_id_ret
);

/* ###### Get Entry : ###################################################### */

static inline const HdmvBitmap * getRefPictureIgsCompilerComposition(
  IgsCompilerComposition *compo,
  const lbc *name
)
{
  return getHdmvPicturesIndexer(
    &compo->ref_pics_indexer, name
  );
}

/* ######################################################################### */

#define HDMV_MAX_NB_ICS_COMPOS  1   /* TODO: Support more compositions */

typedef struct {
  HdmvVDParameters common_video_desc;
  uint64_t common_uop_mask;

  IgsCompilerComposition compositions[HDMV_MAX_NB_ICS_COMPOS];
  unsigned nb_compositions;
} IgsCompilerData;

typedef struct IgsCompilerSegment {
  struct IgsCompilerSegment *next;
  HdmvSegmentParameters param;
} IgsCompilerSegment;

#endif