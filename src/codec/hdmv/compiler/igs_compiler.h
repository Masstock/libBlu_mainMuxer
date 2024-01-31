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

#include "../common/hdmv_bitmap_list.h"
#include "../common/hdmv_bitmap.h"
#include "../common/hdmv_object.h"
#include "../common/hdmv_paletized_bitmap.h"
#include "../common/hdmv_palette_gen.h"

int processIgsCompiler(
  const lbc * xml_filepath,
  HdmvTimecodes * timecodes,
  IniFileContext conf_hdl
);

#endif