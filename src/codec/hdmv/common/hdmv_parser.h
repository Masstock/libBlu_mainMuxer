/** \~english
 * \file hdmv_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IG/PG segments parsing.
 */

/** \~english
 * \dir hdmv
 *
 * \brief HDMV menus and subtitles (IG, PG) bitstreams handling modules.
 *
 * \xrefitem references "References" "References list"
 *  [1] Patent US 8,849,102 B2\n // IG semantics
 *  [2] Patent US 8,638,861 B2\n // PG semantics
 *  [3] Patent US 7,634,739 B2\n // IG timings
 *  [4] Patent US 7,680,394 B2\n // PG timings
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PARSER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PARSER_H__

#include "hdmv_context.h"

int parseHdmvSegment(
  HdmvContextPtr ctx
);

#endif