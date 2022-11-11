/** \~english
 * \file scriptParsing.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Elementary Stream Modification Script PES cutting parsing module.
 */

#ifndef __LIBBLU_MUXER__ESMS__SCRIPT_PARSING_H__
#define __LIBBLU_MUXER__ESMS__SCRIPT_PARSING_H__

#include "../elementaryStreamProperties.h"
#include "../streamCodingType.h"
#include "scriptData.h"
#include "scriptDebug.h"

/* ### ESMS ES Properties section : ######################################## */

static inline int seekESPropertiesEsms(
  const lbc * scriptFilename,
  BitstreamReaderPtr scriptHandle
)
{
  return seekDirectoryOffset(
    scriptHandle,
    scriptFilename,
    ESMS_DIRECTORY_ID_ES_PROP
  );
}

int parseESPropertiesHeaderEsms(
  BitstreamReaderPtr script,
  LibbluESProperties * dst,
  uint64_t * refPts,
  uint64_t * endPts
);

int parseESPropertiesSourceFilesEsms(
  BitstreamReaderPtr script,
  EsmsESSourceFiles * dst
);

/* ### ESMS ES Format Properties section : ################################# */

static inline bool isConcernedESFmtPropertiesEsms(
  const LibbluESProperties prop
)
{
  return prop.type == ES_VIDEO || prop.type == ES_AUDIO;
}

static inline int seekESFmtPropertiesEsms(
  const lbc * scriptFilename,
  BitstreamReaderPtr scriptHandle
)
{
  return seekDirectoryOffset(
    scriptHandle,
    scriptFilename,
    ESMS_DIRECTORY_ID_ES_FMT_PROP
  );
}

int parseESFmtPropertiesEsms(
  BitstreamReaderPtr script,
  LibbluESProperties * dst,
  LibbluESFmtSpecProp * fmtSpecDst
);

/* ### ESMS ES Data Blocks Definition section : ############################ */

static inline int isPresentESDataBlocksDefinitionEsms(
  const lbc * scriptFilename
)
{
  return isPresentDirectory(
    scriptFilename,
    ESMS_DIRECTORY_ID_ES_DATA_BLK_DEF
  );
}

static inline int seekESDataBlocksDefinitionEsms(
  const lbc * scriptFilename,
  BitstreamReaderPtr scriptHandle
)
{
  return seekDirectoryOffset(
    scriptHandle,
    scriptFilename,
    ESMS_DIRECTORY_ID_ES_DATA_BLK_DEF
  );
}

int parseESDataBlocksDefinitionEsms(
  BitstreamReaderPtr script,
  EsmsDataBlocks * dst
);

/* ### ESMS ES PES Cutting section : ####################################### */

static inline int seekESPesCuttingEsms(
  const lbc * scriptFilename,
  BitstreamReaderPtr scriptHandle
)
{
  return seekDirectoryOffset(
    scriptHandle,
    scriptFilename,
    ESMS_DIRECTORY_ID_ES_PES_CUTTING
  );
}

/* ###### ESMS PES Cutting commands data parsing structure : ############### */

typedef struct {
  uint8_t * data;
  size_t dataUsedSize;
  size_t dataAllocatedSize;
} EsmsCommandParsingData;

static inline void initEsmsCommandParsingData(
  EsmsCommandParsingData * dst
)
{
  *dst = (EsmsCommandParsingData) {0};
}

static inline void cleanEsmsCommandParsingData(
  EsmsCommandParsingData data
)
{
  free(data.data);
}

/* ######################################################################### */

int seekESPesCuttingEsms(
  const lbc * scriptFilename,
  BitstreamReaderPtr scriptHandle
);

static inline bool isEndReachedESPesCuttingEsms(
  BitstreamReaderPtr script
)
{
  /* [v8 endMarker] */
  return (0xFF == nextUint8(script));
}

EsmsPesPacketNodePtr parseFrameNodeESPesCuttingEsms(
  BitstreamReaderPtr script,
  LibbluStreamCodingType codingType,
  EsmsCommandParsingData * data
);

#endif