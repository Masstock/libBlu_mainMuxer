

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__INDEXER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__INDEXER_H__

#include "hdmv_error.h"
#include "hdmv_pictures_common.h"
#include "hdmv_pictures_io.h"
#include "../../../util/hashTables.h"

/* ### Pictures indexer : ################################################## */

typedef Hashtable HdmvPicturesIndexer, *HdmvPicturesIndexerPtr;

/* ###### Creation / Destruction : ######################################### */

static inline HdmvPicturesIndexerPtr createHdmvPicturesIndexer(
  void
)
{
  Hashtable * table;

  if (NULL == (table = createHashTable()))
    LIBBLU_HDMV_PIC_ERROR_NRETURN("Memory allocation error.\n");
  return table;
}

static inline void destroyHdmvPicturesIndexer(
  HdmvPicturesIndexerPtr indexer
)
{
  destroyHashTable(indexer);
}

/* ###### Add Entry : ###################################################### */

static inline int addHdmvPicturesIndexer(
  HdmvPicturesIndexerPtr indexer,
  HdmvPicturePtr pic,
  const lbc * name
)
{
  lbc * nameDup;

  if (NULL == (nameDup = lbc_strdup(name)))
    LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");

  return putHashTable(
    indexer,
    nameDup,
    pic
  );
}

/* ###### Get Entry : ###################################################### */

static inline HdmvPicturePtr getHdmvPicturesIndexer(
  HdmvPicturesIndexerPtr indexer,
  const lbc * name
)
{
  return (HdmvPicturePtr) getHashTable(
    indexer,
    name
  );
}

#endif