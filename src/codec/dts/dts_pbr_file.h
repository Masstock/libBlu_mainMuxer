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
#include "dts_data.h"

#define DTS_PBR_FILE_FR_STR_SIZE 10

#define DTS_PBR_FILE_DEFAULT_NB_ENTRIES 512

int initPbrFileHandler(
  const lbc *dtspbr_filename,
  const DtshdFileHandler *dtshd
);

void releasePbrFileHandler(
  void
);

bool isInitPbrFileHandler(
  void
);

int getMaxSizePbrFileHandler(
  unsigned timestamp,
  unsigned *frame_size
);

int getAvgSizePbrFileHandler(
  uint32_t *avg_size
);

#endif