/** \~english
 * \file dts_patcher.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams patching and modification module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__PATCHER_H__
#define __LIBBLU_MUXER__CODECS__DTS__PATCHER_H__

#include "../../esms/scriptCreation.h"

#include "dts_data.h"
#include "dts_error.h"

/** \~english
 * \brief Append on given ESMS script handle a reconstruction of the Extension
 * Substream header based on given parameters.
 *
 * \param script Destination ESMS script handle.
 * \param insert_off Script frame insertion offset in bytes.
 * \param param Ext SS parameters.
 * \return size_t Number of bytes written.
 */
uint32_t appendDcaExtSSHeader(
  EsmsHandlerPtr script,
  uint32_t insert_off,
  const DcaExtSSHeaderParameters *param
);

uint32_t appendDcaExtSSAsset(
  EsmsHandlerPtr script,
  uint32_t insert_off,
  const DcaXllFrameSFPosition *param,
  unsigned scriptSourceFileId
);

#endif