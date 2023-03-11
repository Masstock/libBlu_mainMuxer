

#ifndef __LIBBLU_MUXER__INPUT__META__READER_H__
#define __LIBBLU_MUXER__INPUT__META__READER_H__

#include "../../util.h"

/* ### META File option : ################################################## */

typedef struct LibbluMetaFileOption {
  struct LibbluMetaFileOption * next;

  lbc * name;
  lbc * arg;
} LibbluMetaFileOption;

LibbluMetaFileOption * createLibbluMetaFileOption(
  const lbc * name,
  const lbc * arg
);

void destroyLibbluMetaFileOption(
  LibbluMetaFileOption * option
);

/* ### META File track : ################################################### */

typedef struct LibbluMetaFileTrack {
  struct LibbluMetaFileTrack * next;

  lbc * codec;
  lbc * filepath;

  LibbluMetaFileOption * options;
} LibbluMetaFileTrack;

LibbluMetaFileTrack * createLibbluMetaFileTrack(
  const lbc * codec,
  const lbc * filepath
);

void destroyLibbluMetaFileTrack(
  LibbluMetaFileTrack * track
);

/* ### META File structure : ############################################### */

#define LBMETA_FILE_HEADER  lbc_str("MUXOPT")
#define LBMETA_FILE_HEADER_SIZE  6

typedef struct {
  LibbluMetaFileOption * commonOptions;
  LibbluMetaFileTrack * tracks;
} LibbluMetaFileStruct, *LibbluMetaFileStructPtr;

LibbluMetaFileStructPtr createLibbluMetaFileStruct(
  void
);

void destroyLibbluMetaFileStruct(
  LibbluMetaFileStructPtr meta
);

LibbluMetaFileStructPtr parseMetaFileStructure(
  FILE * input
);

#endif