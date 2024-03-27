#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "meta_parser.h"

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
  const lbc **rp,
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

  // LIBBLU_META_PARSER_DEBUG("Parse string '%s'.\n", *rp);

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

static int _parseSectionName(
  lbc **section_name_ret,
  const lbc **rp
)
{
  _skipWhiteSpace(rp);
  if (_isEndOfLine(*rp))
    return 0; // End of line/Commentary

  lbc section_name[128];
  if (_parseString(section_name, 128, rp, lbc_str("")) < 0)
    return -1;

  if (NULL != section_name_ret) {
    if (NULL == (*section_name_ret = lbc_strdup(section_name)))
      LIBBLU_META_PARSER_ERROR_RETURN("Memory allocation error.\n");
  }

  return 1;
}

/* ### META File option : ################################################## */

LibbluMetaFileOption * _createLibbluMetaFileOption(
  const lbc *name,
  const lbc *arg,
  unsigned line
)
{
  LibbluMetaFileOption *op;

  op = (LibbluMetaFileOption *) malloc(sizeof(LibbluMetaFileOption));
  if (NULL == op)
    LIBBLU_META_PARSER_ERROR_NRETURN("Memory allocation error.\n");

  *op = (LibbluMetaFileOption) {
    .line = line
  };

  if (NULL == (op->name = lbc_strdup(name)))
    LIBBLU_META_PARSER_ERROR_FRETURN("Memory allocation error.\n");
  if (NULL != arg && '\0' != *arg) {
    if (NULL == (op->arg = lbc_strdup(arg)))
      LIBBLU_META_PARSER_ERROR_FRETURN("Memory allocation error.\n");
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
  LibbluMetaFileOption **dst,
  const lbc *line,
  unsigned line_idx
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
    unsigned opt_name_size = 0;

    if (!lbc_sscanf(rp, lbc_str(" %1023[^=\r\n ]%n"), opt_name, &opt_name_size))
      break;
    rp += opt_name_size;

    /* Option argument */
    lbc opt_arg[1024] = {'\0'};

    _skipWhiteSpace(&rp);
    if (*rp == lbc_char('=')) {
      if (_parseString(opt_arg, 1024, &rp, lbc_str("="))) {
        LIBBLU_META_PARSER_ERROR_RETURN(
          "Invalid META file, unable to parse option argument (line %u).\n",
          line_idx
        );
      }
    }

    LIBBLU_META_PARSER_DEBUG(
      "   Option: name='%s' argument='%s' (line %u).\n",
      opt_name, opt_arg, line_idx
    );

    LibbluMetaFileOption *option = _createLibbluMetaFileOption(
      opt_name, opt_arg, line_idx
    );
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

LibbluMetaFileTrack * createLibbluMetaFileTrack(
  const lbc *track_type,
  const lbc *track_argument
)
{
  LibbluMetaFileTrack *track = (LibbluMetaFileTrack *) calloc(
    1, sizeof(LibbluMetaFileTrack)
  );
  if (NULL == track)
    LIBBLU_META_PARSER_ERROR_NRETURN("Memory allocation error.\n");

  if (NULL == (track->type = lbc_strdup(track_type)))
    LIBBLU_META_PARSER_ERROR_FRETURN("Memory allocation error.\n");
  if (NULL == (track->argument = lbc_strdup(track_argument)))
    LIBBLU_META_PARSER_ERROR_FRETURN("Memory allocation error.\n");
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
  free(track->type);
  free(track->argument);
  destroyLibbluMetaFileOption(track->options);
  free(track);
}

static int _parseTrackMetaFileStructure(
  LibbluMetaFileTrack **track_ret,
  const lbc *line,
  unsigned line_idx
)
{
  const lbc *rp = line;

  assert(NULL != track_ret);

  _skipWhiteSpace(&rp);
  if (_isEndOfLine(rp))
    return 0; // End of line/commentary

  LIBBLU_META_PARSER_DEBUG("  Parsing track.\n");

  lbc track_type[64];
  unsigned track_type_size = 0;
  if (lbc_sscanf(rp, lbc_str(" %63[^, ]%n"), track_type, &track_type_size) <= 0)
    LIBBLU_META_PARSER_ERROR_RETURN("Invalid META file, missing track type.\n");
  rp += track_type_size;

  LIBBLU_META_PARSER_DEBUG(
    "   Track type: '%s' (%u) (line %u).\n",
    track_type,
    track_type_size,
    line_idx
  );

  char sep;
  unsigned sep_space = 0;
  if (lbc_sscanf(rp, lbc_str(" %c%n"), &sep, &sep_space) <= 0)
    LIBBLU_META_PARSER_ERROR_RETURN(
      "Invalid META file, missing track argument (missing separator) (line %u).\n",
      line_idx
    );
  if (sep != ',')
    LIBBLU_META_PARSER_ERROR_RETURN(
      "Invalid META file, missing track argument (wrong separator) (line %u).\n",
      line_idx
    );
  rp += sep_space;
  _skipWhiteSpace(&rp);

  lbc track_argument[4096];
  if (_parseString(track_argument, 4096, &rp, lbc_str("")) < 0)
    LIBBLU_META_PARSER_ERROR_RETURN(
      "Invalid META file, invalid track argument '%s' (line %u).\n",
      track_argument,
      line_idx
    );

  LIBBLU_META_PARSER_DEBUG(
    "   Track argument: '%s' (line %u).\n",
    track_argument,
    line_idx
  );

  LibbluMetaFileTrack *track = createLibbluMetaFileTrack(track_type, track_argument);
  if (NULL == track)
    return -1;
  track->line = line_idx;

  if (_parseOptionsMetaFileStructure(&track->options, rp, line_idx) < 0)
    return -1;

  *track_ret = track;
  return 1;
}

/* ### META File section : ################################################# */

static bool _isLibbluMetaFileSectionHeader(
  const lbc *string,
  LibbluMetaFileType *file_type_ret,
  LibbluMetaFileSectionType *section_type_ret,
  bool *is_taking_name_ret
)
{
  static const struct {
    const lbc *name;
    LibbluMetaFileType file_type;
    LibbluMetaFileSectionType section_type;
    bool is_taking_name;
  } headers[] = {
    {LBMETA_MUXOPT_HEADER,  LBMETA_MUXOPT,   LBMETA_HEADER, 0}, /* Single mux */
    {LBMETA_DISCOPT_HEADER, LBMETA_DISCOPT,  LBMETA_HEADER, 0}, /* Disc project */

    {LBMETA_CLPINFO_HEADER, LBMETA_DISCOPT, LBMETA_CLPINFO, 1}, /* Clip definitions */
  };

  for (size_t i = 0; i < ARRAY_SIZE(headers); i++) {
    if (lbc_equal(string, headers[i].name)) {
      if (NULL != file_type_ret)
        *file_type_ret = headers[i].file_type;
      if (NULL != section_type_ret)
        *section_type_ret = headers[i].section_type;
      if (NULL != is_taking_name_ret)
        *is_taking_name_ret = headers[i].is_taking_name;
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
    LIBBLU_META_PARSER_ERROR_NRETURN("Memory allocation error.\n");

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
  const lbc *line,
  unsigned line_idx
)
{
  const lbc *rp = line;

  LibbluMetaFileSection *header_section = &dst->sections[LBMETA_HEADER];
  assert(NULL == header_section->name);
  assert(NULL == header_section->common_options);
  assert(NULL == header_section->tracks);

  lbc meta_header[10];
  unsigned meta_header_size = 0;
  if (!lbc_sscanf(rp, lbc_str(" %9s%n"), meta_header, &meta_header_size))
    LIBBLU_META_PARSER_ERROR_RETURN(
      "Unable to read META file header, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  rp += meta_header_size;

  LibbluMetaFileSectionType section_type;
  bool is_taking_name;
  if (
    !_isLibbluMetaFileSectionHeader(meta_header, &dst->type, &section_type, &is_taking_name)
    || LBMETA_HEADER != section_type
  ) {
    LIBBLU_META_PARSER_ERROR_RETURN(
      "Invalid META file, expect 'MUXOPT' or 'DISCOPT' keyword (line %u).\n",
      line_idx
    );
  }

  LIBBLU_META_PARSER_DEBUG(
    " Parsing first section, '%s' header (line %u).\n",
    meta_header,
    line_idx
  );

  /* Name */
  if (is_taking_name) {
    int ret = _parseSectionName(&header_section->name, &rp);
    if (ret < 0)
      return -1;
    if (0 == ret)
      LIBBLU_META_PARSER_ERROR_RETURN(
        "Invalid META file, missing expected header section name argument.\n"
      );

    LIBBLU_META_PARSER_DEBUG(
      "  Section name: '%s' (line %u).\n",
      header_section->name,
      line_idx
    );
  }

  /* Options */
  if (_parseOptionsMetaFileStructure(&header_section->common_options, rp, line_idx) < 0)
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

  LIBBLU_META_PARSER_DEBUG("Parsing META file structure.\n");
  unsigned line_idx = 0;

  do {
    line_idx++;

    lbc line[8192];
    if (NULL == lbc_fgets(line, 8192, input_fd)) {
      if (!feof(input_fd))
        LIBBLU_META_PARSER_ERROR_FRETURN(
          "Invalid META file, unable to parse line: %s (errno: %d).\n",
          strerror(errno),
          errno
        );
      break; // Empty line before EOF
    }

    // LIBBLU_META_PARSER_DEBUG("Input line '%s'\n", line);

    if (NULL == cur_section) {
      if (_parseHeaderMetaFileStructure(meta, line, line_idx) < 0)
        goto free_return;
      cur_section = &meta->sections[LBMETA_HEADER];
      cur_section->line = line_idx;
      assert(NULL == cur_section_last_track);
      continue;
    }

    lbc heading[10];
    unsigned heading_size = 0;
    if (!lbc_sscanf(line, lbc_str(" %9s%n"), heading, &heading_size))
      LIBBLU_META_PARSER_ERROR_FRETURN(
        "Unable to read META file header, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

    LibbluMetaFileType file_type;
    LibbluMetaFileSectionType section_type;
    bool is_taking_name;
    if (_isLibbluMetaFileSectionHeader(heading, &file_type, &section_type, &is_taking_name)) {
      /* New section */
      if (file_type < meta->type)
        LIBBLU_META_PARSER_ERROR_FRETURN(
          "Invalid META file, unexpected %s section in a %s file (line %u).\n",
          LibbluMetaFileSectionTypeStr(section_type),
          LibbluMetaFileTypeStr(file_type),
          line_idx
        );

      LIBBLU_META_PARSER_DEBUG(" Parsing %s section.\n", LibbluMetaFileSectionTypeStr(section_type));
      cur_section = &meta->sections[section_type];
      cur_section->line = line_idx;
      cur_section_last_track = NULL;

      const lbc *rp = &line[heading_size]; // Skip header size

      /* Name */
      if (is_taking_name) {
        int ret = _parseSectionName(&cur_section->name, &rp);
        if (ret < 0)
          goto free_return;
        if (0 == ret)
          LIBBLU_META_PARSER_ERROR_FRETURN(
            "Invalid META file, missing expected section %s name argument (line %u).\n",
            LibbluMetaFileSectionTypeStr(section_type),
            line_idx
          );
        LIBBLU_META_PARSER_DEBUG(
          "  Section name: '%s' (line %u).\n",
          cur_section->name,
          line_idx
        );
      }

      /* Options */
      if (_parseOptionsMetaFileStructure(&cur_section->common_options, rp, line_idx) < 0)
        goto free_return;
    }
    else {
      /* Track of current section */
      LibbluMetaFileTrack *track;
      int ret = _parseTrackMetaFileStructure(&track, line, line_idx);
      if (ret < 0)
        goto free_return;

      if (0 < ret) {
        if (NULL == cur_section_last_track)
          cur_section->tracks = track;
        else
          cur_section_last_track->next = track;
        cur_section_last_track = track;
      }
    }
  } while (!feof(input_fd));

  return meta;

free_return:
  destroyLibbluMetaFileStruct(meta);
  return NULL;
}