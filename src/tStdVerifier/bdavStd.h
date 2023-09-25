/** \~english
 * \file bdavStd.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief BDAV-STD buffering modelization module.
 */

#ifndef __LIBBLU_MUXER__T_STD_VERIFIER__BDAV_STD_H__
#define __LIBBLU_MUXER__T_STD_VERIFIER__BDAV_STD_H__

#include "../util.h"
#include "../stream.h"
#include "bufferingModel.h"
#include "tStdVerifierError.h"

#include "systemStreams.h"
#include "codec/ac3.h"
#include "codec/dts.h"
#include "codec/h264.h"
#include "codec/hdmv.h"
#include "codec/lpcm.h"

int pidSwitchFilterDecisionFunBdavStd(
  BufModelFilterPtr filter,
  unsigned * idx,
  void * streamPtr
);

int createBdavStd(
  BufModelNode * rootNode
);

static inline bool isManagedSystemBdavStd(
  uint16_t sys_stream_pid,
  PatParameters pat
)
{
  if (sys_stream_pid <= 0x000F)
    return true;

  for (uint8_t i = 0; i < pat.nb_programs; i++) {
    if (pat.programs[i].program_number == 0x0)
      continue; /* network_pid */
    if (pat.programs[i].network_program_map_PID == sys_stream_pid)
      return true;
  }

  return false;
}

int addSystemToBdavStd(
  BufModelBuffersListPtr bufList,
  BufModelNode rootNode,
  uint64_t initialTimestamp,
  uint64_t transportRate
);

int addESToBdavStd(
  BufModelNode rootNode,
  LibbluES * es,
  uint16_t pid,
  uint64_t initialTimestamp
);

// int addESPesPacketToBdavStd(
//   LibbluStreamPtr stream,
//   size_t headerLength,
//   size_t payloadLength,
//   uint64_t referentialStc
// );

int addESTsFrameToBdavStd(
  BufModelBuffersListPtr dst,
  size_t headerLength,
  size_t payloadLength
);

int addSystemFramesToBdavStd(
  BufModelBuffersListPtr dst,
  size_t headerLength,
  size_t payloadLength
);

#endif