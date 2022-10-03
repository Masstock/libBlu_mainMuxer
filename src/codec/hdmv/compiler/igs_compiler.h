/** \~english
 * \file igs_compiler.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV Compiler main header module.
 */

/** \~english
 * \dir compiler
 *
 * \brief HDMV Interactive Graphic Stream compilation and generation modules.
 */

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__H__

#include "../../../libs/cwalk/include/cwalk.h"
#include "../../../ini/iniData.h"

#include "igs_compiler_context_data.h"
#include "igs_compiler_data.h"
#include "igs_segmentsBuilding.h"
#include "igs_xmlParser.h"

#include "../common/hdmv_pictures_common.h"
#include "../common/hdmv_pictures_list.h"
#include "../common/hdmv_palette_gen.h"

IgsCompilerContextPtr createIgsCompilerContext(
  const lbc * xmlFilename,
  IniFileContextPtr conf
);

void resetOriginalDirIgsCompilerContext(
  IgsCompilerContextPtr ctx
);

void destroyIgsCompilerContext(
  IgsCompilerContextPtr ctx
);

int getFileWorkingDirectory(
  lbc ** dirPath,
  lbc ** xmlFilename,
  const lbc * xmlFilepath
);

int updateIgsCompilerWorkingDirectory(
  IgsCompilerContextPtr ctx,
  const lbc * xmlFilename
);

int buildIgsCompilerComposition(
  IgsCompilerCompositionPtr compo,
  HdmvPictureColorDitheringMethod ditherMeth,
  HdmvPaletteColorMatrix colorMatrix
);

int processIgsCompiler(
  const lbc * xmlPath,
  IniFileContextPtr conf
);

#endif