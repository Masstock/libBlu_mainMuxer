/** \~english
 * \file metaFiles.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Input META description files parsing module.
 */

#ifndef __LIBBLU_MUXER__INPUT__META_FILES_H__
#define __LIBBLU_MUXER__INPUT__META_FILES_H__

#include "metaReader.h"
#include "metaFilesData.h"
#include "../../streamCodingType.h"
#include "../../codecsUtilities.h"
#include "../../muxingSettings.h"
#include "../../stream.h"

int parseMetaFile(
  const lbc * filepath,
  LibbluMuxingSettings * dst
);

#endif