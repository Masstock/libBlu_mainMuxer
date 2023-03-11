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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "../../../util.h"
#include "../../../util/xmlParsing.h"
#include "../common/hdmv_common.h"

#include "../common/hdmv_error.h"
#include "../common/hdmv_palette_def.h"
#include "../common/hdmv_pictures_common.h"
#include "../common/hdmv_pictures_indexer.h"

#define HDMV_IGS_COMPL_OUTPUT_EXT ".ies"
#define HDMV_IGS_COMPL_OUTPUT_FMT  "%" PRI_LBCS HDMV_IGS_COMPL_OUTPUT_EXT

#if 0
typedef struct {
  uint32_t * palette_entries;
  uint32_t * yuvEntries;
  unsigned capacity;
  unsigned usedEntries;

  uint8_t version;
} IgsCompilerColorPalette, *IgsCompilerColorPalettePtr;
#endif

/* ### IGS Compiler Composition : ########################################## */

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvICParameters interactiveComposition;

  /* Hash table (key: picName, data: HdmvPicturePtr) */
  HdmvPicturesIndexerPtr refPics;

  HdmvPicturePtr * objPics;
  unsigned nbAllocatedObjPics;
  unsigned nbUsedObjPics;

  HdmvPaletteDefinitionPtr * pals;
  unsigned nbAllocatedPals;
  unsigned nbUsedPals;
} IgsCompilerComposition, *IgsCompilerCompositionPtr;

/* ###### Creation / Destruction : ######################################### */

IgsCompilerCompositionPtr createIgsCompilerComposition(
  void
);

void destroyIgsCompilerComposition(
  IgsCompilerCompositionPtr compo
);

/* ###### Add Entry : ###################################################### */

static inline int addRefPictureIgsCompilerComposition(
  IgsCompilerCompositionPtr dst,
  HdmvPicturePtr pic,
  const lbc * name
)
{
  return addHdmvPicturesIndexer(dst->refPics, pic, name);
}

#define HDMV_COMPL_COMPO_DEFAULT_NB_OBJ  8

int addObjectIgsCompilerComposition(
  IgsCompilerCompositionPtr dst,
  HdmvPicturePtr pic,
  unsigned * id
);

#define HDMV_COMPL_COMPO_DEFAULT_NB_PAL  8

int addPaletteIgsCompilerComposition(
  IgsCompilerCompositionPtr dst,
  HdmvPaletteDefinitionPtr pal,
  unsigned * id
);

/* ###### Get Entry : ###################################################### */

static inline HdmvPicturePtr getRefPictureIgsCompilerComposition(
  IgsCompilerCompositionPtr compo,
  const lbc * name
)
{
  return getHdmvPicturesIndexer(compo->refPics, name);
}

/* ######################################################################### */

#define HDMV_MAX_NB_ICS_COMPOS  1   /* TODO: Support more compositions */

typedef struct {
  HdmvVDParameters commonVideoDescriptor;
  uint64_t commonUopMask;
  unsigned curCompoNumber;

  IgsCompilerCompositionPtr compositions[HDMV_MAX_NB_ICS_COMPOS];
  unsigned nbCompo;
} IgsCompilerData;

typedef struct IgsCompilerSegment {
  struct IgsCompilerSegment * next;
  HdmvSegmentParameters param;
} IgsCompilerSegment;

#endif