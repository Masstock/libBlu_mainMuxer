

#ifndef __LIBBLU_MUXER__INPUT__META__READER_H__
#define __LIBBLU_MUXER__INPUT__META__READER_H__

#include "../../util.h"

typedef struct LibbluMetaFileOption {
  struct LibbluMetaFileOption * next;

  lbc * name;
  lbc * arg;
} LibbluMetaFileOption;

LibbluMetaFileOption * createLibbluMetaFileOption(
  const lbc * name,
  const lbc * arg
);

static inline void destroyLibbluMetaFileOption(
  LibbluMetaFileOption * option
)
{
  if (NULL == option)
    return;

  destroyLibbluMetaFileOption(option->next);
  free(option->name);
  free(option->arg);
  free(option);
}

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

static inline void destroyLibbluMetaFileTrack(
  LibbluMetaFileTrack * track
)
{
  if (NULL == track)
    return;

  destroyLibbluMetaFileTrack(track->next);
  free(track->codec);
  free(track->filepath);
  destroyLibbluMetaFileOption(track->options);
  free(track);
}

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

static inline void destroyLibbluMetaFileStruct(
  LibbluMetaFileStructPtr meta
)
{
  if (NULL == meta)
    return;

  destroyLibbluMetaFileOption(meta->commonOptions);
  destroyLibbluMetaFileTrack(meta->tracks);
  free(meta);
}

LibbluMetaFileStructPtr parseMetaFileStructure(
  FILE * input
);

#endif