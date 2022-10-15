/** \~english
 * \file igs_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV menus (Interactive Graphic Stream) parsing module.
 *
 * \todo Add references.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__IGS_PARSER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__IGS_PARSER_H__

#include "../common/esParsingSettings.h"
#include "../../util.h"
#include "../../esms/scriptCreation.h"

#include "common/hdmv_data.h"
#include "common/hdmv_common.h"
#if !defined(DISABLE_IGS_COMPILER)
#  include "compiler/igs_compiler.h"
#endif

#define IGS_COMPILER_FILE_EXT  lbc_str(".xml")
#define IGS_COMPILER_FILE_EXT_SIZE  4

#define IGS_COMPILER_FILE_MAGIC  "<?xml"
#define IGS_COMPILER_FILE_MAGIC_SIZE  5

#define IGS_DEBUG_FORCE_RETIME false

bool isIgsCompilerFile(const lbc * filePath);

uint64_t computeIgsOdsDecodeDuration(
  HdmvODataParameters param
);

uint64_t computeIgsOdsTransferDuration(
  HdmvODataParameters param,
  uint64_t decodeDuration
);

int computeIgsEpochInitializeDuration(
  uint64_t * res,
  HdmvVDParameters videoDesc,
  HdmvSequencePtr odsSequences
);

int processIgsEpochTimingValues(
  HdmvSegmentsContextPtr ctx
);

int processCompleteIgsEpoch(
  HdmvSegmentsContextPtr ctx
);

int parseIgsMnuHeader(
  BitstreamReaderPtr igsInput,
  HdmvSegmentsContextPtr ctx
);

bool nextUint8IsIgsSegmentType(
  BitstreamReaderPtr igsInput
);

int parseIgsSegment(
  BitstreamReaderPtr igsInput,
  HdmvSegmentsContextPtr ctx
);

int analyzeIgs(
  LibbluESParsingSettings * settings
);

#endif