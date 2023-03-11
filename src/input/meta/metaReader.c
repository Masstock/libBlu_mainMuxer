#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "metaReader.h"

static size_t _parseString(
  const lbc * expr,
  lbc * dst
)
{
  int ret;
  size_t prefixSize;

  switch (*expr) {
    case lbc_char('"'):
      ret = lbc_sscanf(expr, "\"%" PRI_LBCP "[^\"]", dst);
      prefixSize = 2;
      break;

    case lbc_char('\''):
      ret = lbc_sscanf(expr, "'%" PRI_LBCP "[^']", dst);
      prefixSize = 2;
      break;

    default:
      ret = lbc_sscanf(expr, "%" PRI_LBCS, dst);
      prefixSize = 0;
  }

  if (!ret)
    return 0;
  return prefixSize + lbc_strlen(dst);
}

/* ### META File option : ################################################## */

LibbluMetaFileOption * createLibbluMetaFileOption(
  const lbc * name,
  const lbc * arg
)
{
  LibbluMetaFileOption * op;

  op = (LibbluMetaFileOption *) malloc(sizeof(LibbluMetaFileOption));
  if (NULL == op)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  *op = (LibbluMetaFileOption) {
    0
  };

  if (NULL == (op->name = lbc_strdup(name)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  if (NULL != arg) {
    if (NULL == (op->arg = lbc_strdup(arg)))
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  }

  return op;

free_return:
  destroyLibbluMetaFileOption(op);
  return NULL;
}

void destroyLibbluMetaFileOption(
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

static int _parseOptionsMetaFileStructure(
  const lbc * line,
  LibbluMetaFileOption ** dst
)
{
  const lbc * rp;

  LibbluMetaFileOption * prevOption, * option;

  assert(NULL == *dst);

  prevOption = NULL;
  for (rp = line; *rp != '\0'; ) {
    lbc name[STR_BUFSIZE];
    lbc arg[STR_BUFSIZE];
    bool argPres;

    while (lbc_isspace((lbc_int) *rp))
      rp++;
    if (lbc_iscntrl((lbc_int) *rp) || *rp == '#')
      break; /* End of line */

    /* Option name */
    if (!lbc_sscanf(rp, " %1024" PRI_LBCP "[^\n\r= ]", name))
      break;
    rp += lbc_strlen(name);

    /* Option argument */
    argPres = false;
    while (lbc_isspace((lbc_int) *rp))
      rp++;
    if (*rp == lbc_char('=')) {
      size_t size;

      rp++;
      while (lbc_isspace((lbc_int) *rp))
        rp++;
      if (0 == (size = _parseString(rp, arg)))
        LIBBLU_ERROR_RETURN("Invalid META file, missing filepath.\n");
      rp += size;
      argPres = true;
    }

    if (NULL == (option = createLibbluMetaFileOption(name, (argPres) ? arg : NULL)))
      return -1;

    if (NULL == *dst)
      *dst = option;
    if (NULL != prevOption)
      prevOption->next = option;
    prevOption = option;

    /* if (argPres)
      printf(">> '%" PRI_LBCS "', '%" PRI_LBCS "'\n", name, arg);
    else
      printf(">> '%" PRI_LBCS "'\n", name); */
  }

  return 0;
}

/* ### META File track : ################################################### */

LibbluMetaFileTrack * createLibbluMetaFileTrack(
  const lbc * codec,
  const lbc * filepath
)
{
  LibbluMetaFileTrack * track;

  track = (LibbluMetaFileTrack *) malloc(sizeof(LibbluMetaFileTrack));
  if (NULL == track)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  track->next = NULL;
  track->codec = NULL;
  track->filepath = NULL;
  track->options = NULL;

  if (NULL == (track->codec = lbc_strdup(codec)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  if (NULL == (track->filepath = lbc_strdup(filepath)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  return track;

free_return:
  destroyLibbluMetaFileTrack(track);
  return NULL;
}

void destroyLibbluMetaFileTrack(
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

static int _parseTrackMetaFileStructure(
  FILE * input,
  LibbluMetaFileStructPtr dst
)
{
  lbc * rp, buf[STR_BUFSIZE];

  lbc codec[STR_BUFSIZE];
  lbc filepath[STR_BUFSIZE];
  size_t size;

  LibbluMetaFileTrack * prevTrack, * track;

  if (NULL == lbc_fgets(buf, STR_BUFSIZE, input)) {
    if (feof(input))
      return 0;
    LIBBLU_ERROR_RETURN(
      "Invalid META file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  }

  rp = buf;
  while (lbc_isspace((lbc_int) *rp))
    rp++;
  if (lbc_iscntrl((lbc_int) *rp) || *rp == lbc_char('#'))
    return 0;

  if (!lbc_sscanf(rp, " %" PRI_LBCP "[^,]", codec))
    LIBBLU_ERROR_RETURN("Invalid META file, missing codec name.\n");
  rp += lbc_strlen(codec) + 1;

  while (lbc_isspace((lbc_int) *rp) || *rp == lbc_char(','))
    rp++;
  if (*rp == lbc_char('#'))
    LIBBLU_ERROR_RETURN("Invalid META file, missing filepath.\n");

  if (0 == (size = _parseString(rp, filepath)))
    LIBBLU_ERROR_RETURN("Invalid META file, missing filepath.\n");
  rp += size;

  /* printf("> '%" PRI_LBCS "' '%" PRI_LBCS "'\n", codec, filepath); */

  if (NULL == (track = createLibbluMetaFileTrack(codec, filepath)))
    return -1;

  for (
    prevTrack = dst->tracks;
    NULL != prevTrack && NULL != prevTrack->next;
    prevTrack = prevTrack->next
  )
    ;

  if (NULL == dst->tracks)
    dst->tracks = track;
  else
    prevTrack->next = track;

  if (_parseOptionsMetaFileStructure(rp, &track->options) < 0)
    return -1;

  return 0;
}

/* ### META File structure : ############################################### */

LibbluMetaFileStructPtr createLibbluMetaFileStruct(
  void
)
{
  LibbluMetaFileStructPtr meta;

  meta = (LibbluMetaFileStructPtr) malloc(sizeof(LibbluMetaFileStruct));
  if (NULL == meta)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  meta->commonOptions = NULL;
  meta->tracks = NULL;

  return meta;
}

void destroyLibbluMetaFileStruct(
  LibbluMetaFileStructPtr meta
)
{
  if (NULL == meta)
    return;

  destroyLibbluMetaFileOption(meta->commonOptions);
  destroyLibbluMetaFileTrack(meta->tracks);
  free(meta);
}

static int _parseHeaderMetaFileStructure(
  FILE * input,
  LibbluMetaFileStructPtr dst
)
{
  lbc buf[STR_BUFSIZE];

  assert(NULL == dst->commonOptions);

  if (NULL == lbc_fgets(buf, LBMETA_FILE_HEADER_SIZE+1, input))
    LIBBLU_ERROR_RETURN(
      "Invalid META file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  /* printf("> %" PRI_LBCS "\n", buf); */
  if (!lbc_equal(buf, LBMETA_FILE_HEADER))
    LIBBLU_ERROR_RETURN("Invalid META file, missing 'MUXOPT' keyword.\n");

  if (NULL == lbc_fgets(buf, STR_BUFSIZE, input)) {
    if (feof(input))
      return 0;
    LIBBLU_ERROR_RETURN(
      "Invalid META file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  }

  if (_parseOptionsMetaFileStructure(buf, &dst->commonOptions) < 0)
    return -1;

  return 0;
}

LibbluMetaFileStructPtr parseMetaFileStructure(
  FILE * input
)
{
  LibbluMetaFileStructPtr meta;

  if (NULL == (meta = createLibbluMetaFileStruct()))
    return NULL;

  if (_parseHeaderMetaFileStructure(input, meta) < 0)
    goto free_return;

  while (!feof(input)) {
    if (_parseTrackMetaFileStructure(input, meta) < 0)
      goto free_return;
  }

  return meta;

free_return:
  destroyLibbluMetaFileStruct(meta);
  return NULL;
}