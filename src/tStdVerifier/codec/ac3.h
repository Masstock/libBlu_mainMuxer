/** \~english
 * \file ac3.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief AC-3 (Dolby Digital) codec BDAV T-STD buffering modelization module.
 *
 * REFERENCE:
 *  - ETSI TS 102 366 V1.4.1 (2017-09) - Digital Audio Compression.
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__CODEC__AC3_H__
#define __LIBLU_MUXER__BUFFERING_MODEL__CODEC__AC3_H__

#include "../bufferingModel.h"
#include "../../elementaryStream.h"

/** \~english
 * \brief DTS Transport Buffer Size ('TBS') in bytes.
 *
 * This value is set to 512 bytes.
 */
#define BDAV_STD_AC3_TBS 512

/** \~english
 * \brief Output data rate from AC-3 Transport Buffer ('Rx') in bits per
 * second.
 *
 * Nothing is defined, using "other audio" H.222 specified Rx_n.
 *
 * This value is set to 2 Mb/s.
 */
#define BDAV_STD_AC3_RX 2000000

/** \~english
 * \brief AC-3 main Buffer Size ('BS') in bytes.
 *
 * BS_mux = 736 bytes
 * BS_dec = (640000 bps * (1536 Hz / 48000 Hz)) = 20480 bits = 2560 bytes.
 * BS_oh = BS_pad = 64 bytes
 * => BS_n = 736 + 2560 + 64 = 3360 bytes.
 *
 * BS_dec + BS_oh = 2624 bytes <= 2848 bytes (H.222 condition check).
 *
 * This value is set to 3360 bytes.
 */
#define BDAV_STD_AC3_BS 3360

int createAc3BufferingChainBdavStd(
  BufModelNode * root,
  LibbluESPtr stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
);

#endif