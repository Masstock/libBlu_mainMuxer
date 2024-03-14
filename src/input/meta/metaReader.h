

#ifndef __LIBBLU_MUXER__INPUT__META__READER_H__
#define __LIBBLU_MUXER__INPUT__META__READER_H__

#include "../../util.h"

/* ### META File header : ################################################## */

#define LBMETA_MUXOPT_HEADER  lbc_str("MUXOPT")
#define LBMETA_DISCOPT_HEADER  lbc_str("DISCOPT")

typedef enum {
  LBMETA_MUXOPT,
  LBMETA_DISCOPT
} LibbluMetaFileType;

inline static const char * LibbluMetaFileTypeStr(
  LibbluMetaFileType file_type
)
{
  static const char *strings[] = {
    "MUXOPT",
    "DISCOPT"
  };

  if (ARRAY_SIZE(strings) <= file_type)
    return "unknown";
  return strings[file_type];
}

/* ### META File option : ################################################## */

typedef struct LibbluMetaFileOption {
  struct LibbluMetaFileOption *next;

  lbc *name;
  lbc *arg;
} LibbluMetaFileOption;

LibbluMetaFileOption * createLibbluMetaFileOption(
  const lbc *name,
  const lbc *arg
);

void destroyLibbluMetaFileOption(
  LibbluMetaFileOption *option
);

/* ### META File track : ################################################### */

typedef struct LibbluMetaFileTrack {
  struct LibbluMetaFileTrack *next;

  lbc *codec;
  lbc *filepath;

  LibbluMetaFileOption *options;
} LibbluMetaFileTrack;

LibbluMetaFileTrack * createLibbluMetaFileTrack(
  const lbc *codec,
  const lbc *filepath
);

void destroyLibbluMetaFileTrack(
  LibbluMetaFileTrack *track
);

/* ### META File section : ################################################# */

typedef enum {
  LBMETA_HEADER
} LibbluMetaFileSectionType;

#define LBMETA_NB_SECTIONS  1

inline static const char * LibbluMetaFileSectionTypeStr(
  LibbluMetaFileSectionType section_type
)
{
  static const char *strings[] = {
    "header"
  };

  if (ARRAY_SIZE(strings) <= section_type)
    return "unknown";
  return strings[section_type];
}

typedef struct {
  LibbluMetaFileOption *common_options;
  LibbluMetaFileTrack *tracks;
} LibbluMetaFileSection;

/* ### META File structure : ############################################### */

typedef struct {
  LibbluMetaFileType type;

  LibbluMetaFileSection sections[LBMETA_NB_SECTIONS];
} LibbluMetaFileStruct, *LibbluMetaFileStructPtr;

LibbluMetaFileStructPtr createLibbluMetaFileStruct(
  void
);

void destroyLibbluMetaFileStruct(
  LibbluMetaFileStructPtr meta
);

LibbluMetaFileStructPtr parseMetaFileStructure(
  FILE *input
);

#endif