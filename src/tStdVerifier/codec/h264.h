/** \~english
 * \file h264.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 codec BDAV T-STD buffering modelization module.
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__CODEC__H264_H__
#define __LIBLU_MUXER__BUFFERING_MODEL__CODEC__H264_H__

#include "../bufferingModel.h"
#include "../../elementaryStream.h"
#include "../../codec/h264/h264_util.h"

/** \~english
 * \brief h.264 Transport Buffer Size ('TBS') in bytes.
 *
 * This value is set to 512 bytes.
 */
#define BDAV_STD_H264_TBS 512

int createH264BufferingChainBdavStd(
  BufModelNode * root,
  LibbluES * stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
);

#endif