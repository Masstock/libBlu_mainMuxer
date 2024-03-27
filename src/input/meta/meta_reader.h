/** \~english
 * \file meta_reader.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Input META description files parsing module.
 */

#ifndef __LIBBLU_MUXER__INPUT__META__READER_H__
#define __LIBBLU_MUXER__INPUT__META__READER_H__

#include "../../projectSettings.h"

int readMetaFile(
  LibbluProjectSettings *dst,
  const lbc *meta_filepath,
  const lbc *output_filepath,
  const LibbluMuxingOptions mux_options
);

#endif