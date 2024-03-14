/** \~english
 * \file hdmv_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IG/PG segments parsing.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PARSER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PARSER_H__

#include "hdmv_context.h"

int parseHdmvSegment(
  HdmvContext *ctx
);

#endif