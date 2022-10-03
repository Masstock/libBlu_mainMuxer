/** \~english
 * \file dts.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS codec BDAV T-STD buffering modelization module.
 *
 * REFERENCE:
 *  - DTS Specifications - Document 9302J85300 (H262 Structures).
 *  - ETSI TS 102 114 V1.4.1 (2012-09) - DTS Coherent Acoustics.
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__CODEC__DTS_H__
#define __LIBLU_MUXER__BUFFERING_MODEL__CODEC__DTS_H__

#include "../bufferingModel.h"
#include "../../elementaryStream.h"

/** \~english
 * \brief DTS Transport Buffer Size ('TBS') in bytes.
 *
 * This value is set to 512 bytes.
 */
#define BDAV_STD_DTS_TBS 512

/** \~english
 * \brief Output data rate from DTS Transport Buffer ('RX') in bits per
 * second for DTS core streams (DCA, DTS Coherent Acoustics).
 *
 * This value is set to 2 Mb/s.
 */
#define BDAV_STD_DTS_RX_DCA 2000000

/** \~english
 * \brief Output data rate from DTS Transport Buffer ('RX') in bits per
 * second for DTS-HD Lossless streams (DTS-HD XLL or commercial DTS-HDMA).
 *
 * This value is set to 32 Mb/s
 */
#define BDAV_STD_DTS_RX_DTS_HDMA 32000000

/** \~english
 * \brief Output data rate from DTS Transport Buffer ('RX') in bits per
 * second for non-lossless DTS-HD streams (commercial DTS-HDHR).
 *
 * This value is set to 8 Mb/s
 */
#define BDAV_STD_DTS_RX_DTS_HDHR 8000000

/** \~english
 * \brief DTS main Buffer Size ('BS') in bytes in case of DTS core streams
 * (DCA, DTS Coherent Acoustics).
 *
 * This value is set to 9088 bytes.
 */
#define BDAV_STD_DTS_BS_DCA 9088

/** \~english
 * \brief DTS main Buffer Size ('BS') in bytes in case of DTS-HD Lossless
 * streams (DTS-HD XLL or commercial DTS-HDMA).
 *
 * This value is set to 66432 bytes.
 */
#define BDAV_STD_DTS_BS_HDMA 66432

/** \~english
 * \brief DTS main Buffer Size ('BS') in bytes in case of non-lossless
 * DTS-HD streams (commercial DTS-HDHR).
 *
 * This value is set to 17814 bytes.
 */
#define BDAV_STD_DTS_BS_HDHR 17814

int createDtsBufferingChainBdavStd(
  BufModelNode * root,
  LibbluESPtr stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
);

#endif