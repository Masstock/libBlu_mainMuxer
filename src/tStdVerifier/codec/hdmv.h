/** \~english
 * \file ac3.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV Menus & Subtitles (IGS, PGS) codecs BDAV T-STD buffering
 * modelization module.
 *
 * \xrefitem references "References" "References list"
 *  [1] Patent US 8,139,915 B2
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__CODEC__HDMV_H__
#define __LIBLU_MUXER__BUFFERING_MODEL__CODEC__HDMV_H__

#include "../bufferingModel.h"
#include "../../elementaryStream.h"

/** \~english
 * \brief HDMV Transport Buffer Size ('TBS') in bytes.
 *
 * This value is set to 512 bytes.
 */
#define BDAV_STD_HDMV_TBS 512

/** \~english
 * \brief Output data rate from HDMV Transport Buffer ('Rx') in bits per
 * second.
 *
 * This value is set to 16 Mb/s.
 */
#define BDAV_STD_HDMV_RX 16000000

/** \~english
 * \brief HDMV Coded Data Buffer (CDB) size in bytes.
 *
 * This value is set to 1 MiB.
 */
#define BDAV_STD_HDMV_CDBS 1048576

int createHdmvBufferingChainBdavStd(
  BufModelNode * root,
  LibbluES * stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
);

#endif