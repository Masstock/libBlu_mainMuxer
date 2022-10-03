/** \~english
 * \file dts_pbr_file.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio PBR statistics file (.dtspbr) handling.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__PBR_FILE_H__
#define __LIBBLU_MUXER__CODECS__DTS__PBR_FILE_H__

#include "../../util.h"

#include "dts_error.h"

#define DTS_PBR_FILE_FR_STR_SIZE 10

/** \~english
 * \brief Expected max size of a timecode line.
 *
 * Expected format: XX:XX:XX:XX,XXXXXXXXX
 */
#define DTS_PBR_FILE_LINE_STR_SIZE 21

#define DTS_PBR_FILE_DEFAULT_NB_ENTRIES 512

int initPbrFileHandler(
  const lbc * pbrFilename
);

void releasePbrFileHandler(
  void
);

bool isInitPbrFileHandler(
  void
);

int parsePbrFileHandler(
  void
);

int getMaxSizePbrFileHandler(
  unsigned timestamp,
  size_t * maxSize
);

int getAvgSizePbrFileHandler(
  size_t * avgSize
);

#endif