#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "metaReader.h"

static void _skipWhiteSpace(
  const lbc ** rp
)
{
  while (lbc_isspace((lbc_int) **rp))
    (*rp)++;
}

static bool _isEndOfLine(
  const lbc *rp
)
{
  return lbc_iscntrl((lbc_int) *rp) || *rp == '#';
}

static int _parseString(
  lbc *string,
  unsigned string_buf_size,
  const lbc ** rp,
  const lbc *prefix
)
{
  assert(NULL != string);
  assert(0 < string_buf_size);
  assert(NULL != rp);
  assert(NULL != prefix);

  unsigned string_size = 0;

  lbc format_double_quote[32];
  lbc_sprintf(format_double_quote, lbc_str("%s \"%%%u[^\"\r\n]\"%%n"), prefix, string_buf_size-1);

  lbc format_single_quote[32];
  lbc_sprintf(format_single_quote, lbc_str("%s \'%%%u[^\'\r\n]\'%%n"), prefix, string_buf_size-1);

  lbc format[32];
  lbc_sprintf(format,              lbc_str("%s %%%u[^\r\n ]%%n"),      prefix, string_buf_size-1);

  LIBBLU_DEBUG_COM("Parse string '%s'.\n", *rp);

  if (
    !lbc_sscanf(*rp,    format_double_quote, string, &string_size)
    && !lbc_sscanf(*rp, format_single_quote, string, &string_size)
    && !lbc_sscanf(*rp, format,              string, &string_size)
  ) {
    return -1;
  }

  *rp += string_size;

  return 0;
}

/* ### META File option : ################################################## */

LibbluMetaFileOption *createLibbluMetaFileOption(
  const lbc *name,
  const lbc *arg
)
{
  LibbluMetaFileOption *op;

  op = (LibbluMetaFileOption *) malloc(sizeof(LibbluMetaFileOption));
  if (NULL == op)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  *op = (LibbluMetaFileOption) {
    0
  };

  if (NULL == (op->name = lbc_strdup(name)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  if (NULL != arg && '\0' != *arg) {
    if (NULL == (op->arg = lbc_strdup(arg)))
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
  }

  return op;

free_return:
  destroyLibbluMetaFileOption(op);
  return NULL;
}

void destroyLibbluMetaFileOption(
  LibbluMetaFileOption *option
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
  const lbc *line,
  LibbluMetaFileOption ** dst
)
{
  assert(NULL != line);

  LibbluMetaFileOption *prev_option = NULL;

  for (const lbc *rp = line; *rp != '\0';) {
    _skipWhiteSpace(&rp);

    if (_isEndOfLine(rp))
      break; // End of line/Commentary

    /* Option name */
    lbc opt_name[STR_BUFSIZE];
    unsigned opt_arg_size = 0;

    if (!lbc_sscanf(rp, lbc_str(" %1023[^=\r\n ]%n"), opt_name, &opt_arg_size))
      break;
    rp += opt_arg_size;

    /* Option argument */
    lbc opt_arg[1024] = {'\0'};

    _skipWhiteSpace(&rp);
    if (*rp == lbc_char('=')) {
      if (_parseString(opt_arg, 1024, &rp, lbc_str("="))) {
        LIBBLU_ERROR_RETURN(
          "Invalid META file, unable to parse option argument.\n"
        );
      }
    }

    LibbluMetaFileOption *option = createLibbluMetaFileOption(opt_name, opt_arg);
    if (NULL == option)
      return -1;

    if (NULL == prev_option)
      *dst = option;
    else
      prev_option->next = option;
    prev_option = option;
  }

  return 0;
}

/* ### META File track : ################################################### */

LibbluMetaFileTrack *createLibbluMetaFileTrack(
  const lbc *codec,
  const lbc *filepath
)
{
  LibbluMetaFileTrack *track = (LibbluMetaFileTrack *) calloc(
    1, sizeof(LibbluMetaFileTrack)
  );
  if (NULL == track)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

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
  LibbluMetaFileTrack *track
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

static LibbluMetaFileTrack * _parseTrackMetaFileStructure(
  const lbc *line
)
{
  const lbc *rp = line;

  LIBBLU_DEBUG_COM("Parsing track line '%s'.\n", line);

  _skipWhiteSpace(&rp);
  if (_isEndOfLine(rp))
    return 0;

  lbc track_type[64];
  unsigned track_type_size = 0;
  if (lbc_sscanf(rp, lbc_str(" %63[^, ]%n"), track_type, &track_type_size) <= 0)
    LIBBLU_ERROR_NRETURN("Invalid META file, missing track type.\n");
  rp += track_type_size;

  LIBBLU_DEBUG_COM(" Track type: '%s' (%u).\n", track_type, track_type_size);

  char sep;
  unsigned sep_space = 0;
  if (lbc_sscanf(rp, lbc_str(" %c%n"), &sep, &sep_space) <= 0)
    LIBBLU_ERROR_NRETURN("Invalid META file, missing track argument (missing separator).\n");
  if (sep != ',')
    LIBBLU_ERROR_NRETURN("Invalid META file, missing track argument (wrong separator).\n");
  rp += sep_space;
  _skipWhiteSpace(&rp);

  lbc track_argument[4096];
  if (_parseString(track_argument, 4096, &rp, lbc_str("")) < 0)
    LIBBLU_ERROR_NRETURN("Invalid META file, invalid track argument '%s'.\n", track_argument);

  LIBBLU_DEBUG_COM(" Track argument: '%s'.\n", track_argument);

  LibbluMetaFileTrack *track = createLibbluMetaFileTrack(track_type, track_argument);
  if (NULL == track)
    return NULL;

  if (_parseOptionsMetaFileStructure(rp, &track->options) < 0)
    return NULL;
  return track;
}

/* ### META File section : ################################################# */

static bool _isLibbluMetaFileSectionHeader(
  const lbc *string,
  LibbluMetaFileType *file_type_ret,
  LibbluMetaFileSectionType *section_type_ret
)
{
  static const struct {
    const lbc *name;
    LibbluMetaFileType file_type;
    LibbluMetaFileSectionType section_type;
  } headers[] = {
    {LBMETA_MUXOPT_HEADER,  LBMETA_MUXOPT,  LBMETA_HEADER}, /* Single mux */
    {LBMETA_DISCOPT_HEADER, LBMETA_DISCOPT, LBMETA_HEADER}, /* Disc project */
  };

  for (size_t i = 0; i < ARRAY_SIZE(headers); i++) {
    if (lbc_equal(string, headers[i].name)) {
      if (NULL != file_type_ret)
        *file_type_ret = headers[i].file_type;
      if (NULL != section_type_ret)
        *section_type_ret = headers[i].section_type;
      return true;
    }
  }

  return false;
}

/* ### META File structure : ############################################### */

LibbluMetaFileStructPtr createLibbluMetaFileStruct(
  void
)
{
  LibbluMetaFileStructPtr meta = (LibbluMetaFileStructPtr) calloc(
    1, sizeof(LibbluMetaFileStruct)
  );
  if (NULL == meta)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  return meta;
}

void destroyLibbluMetaFileStruct(
  LibbluMetaFileStructPtr meta
)
{
  if (NULL == meta)
    return;

  for (size_t i = 0; i < LBMETA_NB_SECTIONS; i++) {
    destroyLibbluMetaFileOption(meta->sections[i].common_options);
    destroyLibbluMetaFileTrack(meta->sections[i].tracks);
  }
  free(meta);
}

static int _parseHeaderMetaFileStructure(
  LibbluMetaFileStructPtr dst,
  const lbc *line
)
{
  LibbluMetaFileSection *header_section = &dst->sections[LBMETA_HEADER];
  const lbc *rp = line;

  lbc meta_header[10];
  unsigned meta_header_size = 0;
  if (!lbc_sscanf(rp, lbc_str(" %9s%n"), meta_header, &meta_header_size))
    LIBBLU_ERROR_RETURN(
      "Unable to read META file header, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  rp += meta_header_size;

  LibbluMetaFileSectionType section_type;
  if (
    !_isLibbluMetaFileSectionHeader(meta_header, &dst->type, &section_type)
    || LBMETA_HEADER != section_type
  ) {
    LIBBLU_ERROR_RETURN("Invalid META file, expect 'MUXOPT' or 'DISCOPT' keyword.\n");
  }

  assert(NULL == header_section->common_options);
  assert(NULL == header_section->tracks);

  if (_parseOptionsMetaFileStructure(rp, &header_section->common_options) < 0)
    return -1;
  return 0;
}

LibbluMetaFileStructPtr parseMetaFileStructure(
  FILE *input_fd
)
{
  LibbluMetaFileStructPtr meta = createLibbluMetaFileStruct();
  if (NULL == meta)
    return NULL;

  LibbluMetaFileSection *cur_section = NULL;
  LibbluMetaFileTrack *cur_section_last_track = NULL;

  do {
    lbc line[8192];
    if (NULL == lbc_fgets(line, 8192, input_fd)) {
      if (!feof(input_fd))
        LIBBLU_ERROR_FRETURN(
          "Invalid META file, unable to parse line: %s (errno: %d).\n",
          strerror(errno),
          errno
        );
      break; // Empty line before EOF
    }

    if (NULL == cur_section) {
      /* Header, first section */
      if (_parseHeaderMetaFileStructure(meta, line) < 0)
        goto free_return;
      cur_section = &meta->sections[LBMETA_HEADER];
      assert(NULL == cur_section_last_track);
      continue;
    }

    lbc header[10];
    unsigned header_size = 0;
    if (!lbc_sscanf(line, lbc_str(" %9s"), header, &header_size))
      LIBBLU_ERROR_FRETURN(
        "Unable to read META file header, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

    LibbluMetaFileType file_type;
    LibbluMetaFileSectionType section_type;
    if (_isLibbluMetaFileSectionHeader(header, &file_type, &section_type)) {
      /* New section */
      if (file_type < meta->type)
        LIBBLU_ERROR_FRETURN(
          "Invalid META file, unexpected %s section in a %s file.\n",
          LibbluMetaFileSectionTypeStr(section_type),
          LibbluMetaFileTypeStr(file_type)
        );

      LIBBLU_TODO();

      cur_section = &meta->sections[LBMETA_HEADER];
      cur_section_last_track = NULL;
    }
    else {
      /* Track of current section */
      LibbluMetaFileTrack *track = _parseTrackMetaFileStructure(line);
      if (NULL == track)
        goto free_return;

      if (NULL == cur_section_last_track)
        cur_section->tracks = track;
      else
        cur_section_last_track->next = track;
      cur_section_last_track = track;
    }
  } while (!feof(input_fd));

  return meta;

free_return:
  destroyLibbluMetaFileStruct(meta);
  return NULL;
}