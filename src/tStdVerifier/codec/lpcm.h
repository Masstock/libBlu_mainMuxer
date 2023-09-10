/** \~english
 * \file lpcm.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief LPCM codec BDAV T-STD buffering modelization module.
 *
 * \todo Due to lack of documentation, currently not implemented. Values are
 * arbitrary.
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__CODEC__LPCM_H__
#define __LIBLU_MUXER__BUFFERING_MODEL__CODEC__LPCM_H__

#include "../../elementaryStream.h"

/** \~english
 * \brief LPCM Transport Buffer Size ('TBS') in bytes.
 *
 * This value is set to 512 bytes.
 */
#define BDAV_STD_LPCM_TBS 512

/** \~english
 * \brief Output data rate from LPCM Transport Buffer ('RX') in case of 192 kHz
 * audio in bits per second.
 *
 * This value is set to 16.589 Mbps to correspond to 1.2 * 96000 * 24 * 6 bps,
 * BDAV maximum LPCM capabilities (96kHz, 24 bits, 8 channels).
 */
#define BDAV_STD_LPCM_RX_48_96 16588800

/** \~english
 * \brief Output data rate from LPCM Transport Buffer ('RX') in case of 192 kHz
 * audio in bits per second.
 *
 * This value is set to 33.178 Mbps to correspond to 1.2 * 192000 * 24 * 6 bps,
 * BDAV maximum LPCM capabilities (192kHz, 24 bits, 6 channels).
 */
#define BDAV_STD_LPCM_RX_192 33177600

/** \~english
 * \brief LPCM main Buffer Size ('BS') in bytes in case of 48/96 kHz audio.
 *
 *
 *
 * This value is set to 536832 bytes.
 *
 * \note According to drmpeg's post (https://forum.doom9.org/showthread.php?p=1577216#post1577216).
 */
#define BDAV_STD_LPCM_BS_48_96 536832

/** \~english
 * \brief LPCM main Buffer Size ('BS') in bytes in case of 192 kHz audio.
 *
 * BDAV_STD_LPCM_BS_48_96 * 2.
 *
 * This value is set to 1073664 bytes.
 *
 * \note According to drmpeg's post (https://forum.doom9.org/showthread.php?p=1577216#post1577216).
 */
#define BDAV_STD_LPCM_BS_192 1073664

int createLpcmBufferingChainBdavStd(
  BufModelNode * root,
  LibbluES * stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
);

#endif