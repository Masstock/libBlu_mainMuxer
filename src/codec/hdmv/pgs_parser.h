/** \~english
 * \file pgs_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV subtitles (Presentation Graphic Stream) parsing module.
 *
 * \xrefitem references "References" "References list"
 *  [1] Patent US 2009/0185789 A1.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__PGS_PARSER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__PGS_PARSER_H__

#include "../common/esParsingSettings.h"
#include "../../util.h"
#include "../../esms/scriptCreation.h"

#include "common/hdmv_data.h"
#include "common/hdmv_context.h"
#include "common/hdmv_parser.h"

bool nextUint8IsPgsSegmentType(
  BitstreamReaderPtr pgsInput
);

int analyzePgs(
  LibbluESParsingSettings * settings
);

#endif