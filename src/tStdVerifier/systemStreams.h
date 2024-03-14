/** \~english
 * \file systemStreams.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief BDAV T-STD system streams buffering model creation module.
 *
 * \todo Check systems streams management by T-STD.
 */

#ifndef __LIBLU_MUXER__BUFFERING_MODEL__SYSTEM_STREAMS_H__
#define __LIBLU_MUXER__BUFFERING_MODEL__SYSTEM_STREAMS_H__

#include "bufferingModel.h"
#include "../packetIdentifier.h"
#include "../util.h"

#define BDAV_STD_SYSTEM_STREAMS_NB_PIDS 3

/** \~english
 * \brief
 *
 * \todo Patch to remove static supported PIDs list, use standard defined
 * conditions (according to PAT parameters). Rec. ITU-T H.222 03/2017 p18
 */
#define BDAV_STD_SYSTEM_STREAMS_PIDS {PAT_PID, PMT_PID, SIT_PID}

/** \~english
 * \brief System Transport Buffer Size ('TBS') in bytes.
 *
 * This value is set to 512 bytes.
 */
#define BDAV_STD_SYSTEM_TBS 512

/** \~english
 * \brief Output data rate from System Transport Buffer ('RX') in bits per
 * second.
 *
 * This value is set to 1Mbps.
 */
#define BDAV_STD_SYSTEM_RX 1000000

/** \~english
 * \brief System main Buffer Size ('BS') in bytes.
 *
 * This value is set to 1536 bytes.
 */
#define BDAV_STD_SYSTEM_BS 1536

/** \~english
 * \brief Output data rate from System main Buffer ('R') in bits per second.
 *
 * This value is set to 80Kbps.
 */
#define BDAV_STD_SYSTEM_R_MIN 80000

int createSystemBufferingChainBdavStd(
  BufModelBuffersListPtr bufList,
  BufModelNode *root,
  uint64_t initialTimestamp,
  uint64_t transportRate
);

#endif