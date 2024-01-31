

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_INDEXER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_INDEXER_H__

#include "hdmv_error.h"
#include "hdmv_bitmap.h"
#include "../../../util/hashTables.h"

/* ### Pictures indexer : ################################################## */

typedef Hashtable HdmvPicturesIndexer;

/* ###### Creation / Destruction : ######################################### */

static inline void initHdmvPicturesIndexer(
  HdmvPicturesIndexer * dst
)
{
  initHashTable(dst);
}

static inline void cleanHdmvPicturesIndexer(
  HdmvPicturesIndexer indexer
)
{
  cleanHashTable(indexer);
}

/* ###### Add Entry : ###################################################### */

static inline int addHdmvPicturesIndexer(
  HdmvPicturesIndexer * indexer,
  HdmvBitmap * pic,
  const lbc * name
)
{
  lbc * name_dup = lbc_strdup(name);
  if (NULL == name_dup)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");

  return putHashTable(
    indexer,
    name_dup,
    pic
  );
}

/* ###### Get Entry : ###################################################### */

static inline HdmvBitmap * getHdmvPicturesIndexer(
  HdmvPicturesIndexer * indexer,
  const lbc * name
)
{
  return (HdmvBitmap *) getHashTable(
    indexer,
    name
  );
}

#endif