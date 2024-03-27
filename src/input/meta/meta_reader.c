#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>

#include "meta_reader.h"

#include "meta_parser.h"
#include "meta_data.h"

static int parseCodecNameMetaFile(
  const lbc *string,
  LibbluStreamCodingType *track_coding_type_ret,
  const char ** track_coding_type_name_ret
)
{
  unsigned i;

  static const struct {
    const lbc *keyword;
    LibbluStreamCodingType coding_type;
    const char *name;
  } codecs[] = {
#define D_(k, c, n) {.keyword = lbc_str(k), .coding_type = c, .name = n}
    D_(           "AUTO",                       -1,          "undefined"),

    D_(        "V_MPEG2",  STREAM_CODING_TYPE_H262, "H.262/MPEG-2 video"),
    D_(         "V_H262",  STREAM_CODING_TYPE_H262, "H.262/MPEG-2 video"),

    D_("V_MPEG4/ISO/AVC",   STREAM_CODING_TYPE_AVC,    "H.264/AVC video"),
    D_(         "V_H264",   STREAM_CODING_TYPE_AVC,    "H.264/AVC video"),

    D_(         "A_LPCM",  STREAM_CODING_TYPE_LPCM,   "Linear PCM audio"),
    D_(          "A_AC3",   STREAM_CODING_TYPE_AC3,         "AC-3 audio"),
    D_(          "A_DTS",   STREAM_CODING_TYPE_DTS,          "DTS audio"),

    D_(     "M_HDMV/IGS",    STREAM_CODING_TYPE_IG,           "IG menus"),
    D_(     "M_HDMV/PGS",    STREAM_CODING_TYPE_PG,       "PG subtitles")
#undef D_
  };

  for (i = 0; i < ARRAY_SIZE(codecs); i++) {
    if (lbc_equal(string, codecs[i].keyword)) {
      /* Match ! */

      if (NULL != track_coding_type_ret)
        *track_coding_type_ret = codecs[i].coding_type;

      if (NULL != track_coding_type_name_ret)
        *track_coding_type_name_ret = codecs[i].name;

      return 0;
    }
  }

  LIBBLU_ERROR_RETURN(
    "Unknown codec name '%s'.\n",
    string
  );
}

static void _applyTrackGlobalOptions(
  LibbluESSettings *dst,
  LibbluMuxingSettings *muxSettings
)
{
  memcpy(
    &dst->options,
    &muxSettings->options.es_shared_options,
    sizeof(LibbluESSettingsOptions)
  );
}

static int _parseMUXOPTSectionTrack(
  LibbluMuxingSettings *dst,
  const LibbluMetaFileTrack *track,
  const lbc *meta_filepath
)
{
  assert(NULL != track);
  assert(NULL != dst);
  assert(NULL != meta_filepath);

  LibbluESSettings *es_settings = getNewESLibbluMuxingSettings(dst);
  if (NULL == es_settings)
    LIBBLU_ERROR_RETURN(
      "Invalid META file, too many elementary streams declared.\n"
    );

  /* Codec name */
  LibbluStreamCodingType coding_type;
  const char *coding_type_name;
  if (parseCodecNameMetaFile(track->type, &coding_type, &coding_type_name) < 0)
    return -1;

  /* Filepath */
  if (setMainESFilepathLibbluESSettings(es_settings, track->argument, meta_filepath) < 0)
    LIBBLU_ERROR_RETURN(
      "Invalid META file, invalid filepath '%s'.\n",
      track->argument
    );

  if (coding_type < 0) {
    /* Guess the stream coding type */
    if ((coding_type = guessESStreamCodingType(track->argument)) < 0)
      return -1;
  }

  /* Global shared options */
  _applyTrackGlobalOptions(es_settings, dst);

  /* Options */
  const LibbluMetaFileOption *option_node = track->options;
  for (; NULL != option_node; option_node = option_node->next) {

    LibbluMetaOption option;
    LibbluMetaOptionArgValue argument;
    switch (parseLibbluMetaTrackOption(option_node, &option, &argument, coding_type)) {
    case -1:
      return -1;

    case LBMETA_OPT__FORCE_ESMS:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, force_script_generation, true);
      break;

    case LBMETA_OPT__SECONDARY:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, secondary, true);
      break;

    case LBMETA_OPT__DISABLE_FIXES:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, disable_fixes, true);
      break;

    case LBMETA_OPT__EXTRACT_CORE:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, extract_core, true);
      break;

    case LBMETA_OPT__PBR_FILE: {
      lbc *path;
      if (lb_gen_anchor_absolute_fp(&path, meta_filepath, argument.str) < 0)
        return -1;
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, pbr_filepath, path);
      break;
    }

    case LBMETA_OPT__SET_FPS:
      if (setFpsChangeLibbluESSettings(es_settings, argument.str) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "only standard FPS values can be used (see help).\n",
          option.name
        );
      break;

    case LBMETA_OPT__SET_AR:
      if (setArChangeLibbluESSettings(es_settings, argument.str) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "must be in the format 'width:height' "
          "with non-zero numeric values (or '0:0' for 'unspecified').\n",
          option.name
        );
      break;

    case LBMETA_OPT__SET_LEVEL:
      if (setLevelChangeLibbluESSettings(es_settings, argument.str) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "wrong level format or unknown value. Value format must be xx "
          "or x.x with numeric values (e.g. '4' or '40' or '4.0'). "
          "See ITU-T Rec. H.264 for level values list.\n",
          option.name
        );
      break;

    case LBMETA_OPT__REMOVE_SEI:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, discard_sei, true);
      break;

    case LBMETA_OPT__DISABLE_HRD_VERIF:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, disable_HRD_verifier, true);
      break;

    case LBMETA_OPT__HRD_CPB_STATS: {
      lbc *path;
      if (lb_gen_anchor_absolute_fp(&path, meta_filepath, argument.str) < 0)
        return -1;
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, HRD_CPB_stats_log_fp, path);
      break;
    }

    case LBMETA_OPT__HRD_DPB_STATS: {
      lbc *path;
      if (lb_gen_anchor_absolute_fp(&path, meta_filepath, argument.str) < 0)
        return -1;
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, HRD_DPB_stats_log_fp, path);
      break;
    }

    case LBMETA_OPT__HDMV_INITIAL_TS:
      if (setHdmvInitialTimestampLibbluESSettings(es_settings, argument.u64) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "value shall be between %" PRIu64 " and %" PRIu64 " "
          "(inclusive).\n",
          option.name,
          LIBBLU_MIN_HDMV_INIT_TIMESTAMP,
          LIBBLU_MAX_HDMV_INIT_TIMESTAMP
        );
      break;

    case LBMETA_OPT__HDMV_FORCE_RETIMING:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, hdmv.force_retiming, true);
      break;

    case LBMETA_OPT__HDMV_ASS_INPUT:
      LIBBLU_ES_SETTINGS_SET_OPTION(es_settings, hdmv.ass_input, true);
      break;

    case LBMETA_OPT__ESMS:
      if (setScriptFilepathLibbluESSettings(es_settings, argument.str, meta_filepath) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "unable to set '%s' as script filepath.\n",
          option.name,
          argument.str
        );
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unexpected option '%s', cannot be used on a %s track.\n",
        option_node->name,
        coding_type_name
      );
    }

    cleanLibbluMetaOptionArgValue(option, argument);
  }

  setExpectedCodingTypeLibbluESSettings(es_settings, coding_type);

  return 0;
}

static int _analyzeMUXOPTSection(
  LibbluMuxingSettings *dst,
  const LibbluMetaFileSection *header_section,
  const lbc *meta_filepath
)
{
  assert(NULL != dst);
  assert(NULL != header_section);

  LibbluMetaFileOption *option_node = header_section->common_options;
  for (; NULL != option_node; option_node = option_node->next) {
    LibbluMetaOption option;
    LibbluMetaOptionArgValue argument;

    switch (parseLibbluMetaHeaderOption(option_node, &option, &argument)) {
    case -1: /* Invalid option */
      return -1;

    case LBMETA_OPT__NO_EXTRA_HEADER:
      dst->options.write_TP_extra_headers = false;
      break;

    case LBMETA_OPT__CBR_MUX:
      dst->options.CBR_mux = true;
      break;

    case LBMETA_OPT__FORCE_ESMS:
      dst->options.force_script_generation = true;
      break;

    case LBMETA_OPT__DISABLE_T_STD:
      dst->options.disable_buffering_model = true;
      break;

    case LBMETA_OPT__START_TIME:
      if (setInitPresTimeLibbluMuxingSettings(dst, argument.u64) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "expect a value between %" PRIu64 " (inclusive) "
          "and %" PRIu64 " (non-inclusive).\n",
          option.name,
          MUX_MIN_PRES_START_TIME,
          MUX_MAX_PRES_START_TIME
        );
      break;

    case LBMETA_OPT__MUX_RATE:
      if (setTargetMuxRateLibbluMuxingSettings(dst, argument.u64) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "expect a value between %" PRIu64 " (inclusive) "
          "and %" PRIu64 " (non-inclusive).\n",
          option.name,
          MUX_MIN_TS_RECORDING_RATE,
          MUX_MAX_TS_RECORDING_RATE
        );
      break;

    case LBMETA_OPT__DVD_MEDIA:
      dst->options.es_shared_options.dvd_media = true;
      break;

    case LBMETA_OPT__DISABLE_FIXES:
      dst->options.es_shared_options.disable_fixes = true;
      break;

    case LBMETA_OPT__EXTRACT_CORE:
      dst->options.es_shared_options.extract_core = true;
      break;

    case LBMETA_OPT__SET_FPS:
      if (setFpsChangeLibbluMuxingSettings(dst, argument.str) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "only standard FPS values can be used (see help).\n",
          option.name
        );
      break;

    case LBMETA_OPT__SET_AR:
      if (setArChangeLibbluMuxingSettings(dst, argument.str) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "must be in the format 'width:height' "
          "with non-zero numeric values (or '0:0' for 'unspecified').\n",
          option.name
        );
      break;

    case LBMETA_OPT__SET_LEVEL:
      if (setLevelChangeLibbluMuxingSettings(dst, argument.str) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "wrong level format or unknown value. Value format must be xx "
          "or x.x with numeric values (e.g. '4' or '40' or '4.0'). "
          "See ITU-T Rec. H.264 for level values list.\n",
          option.name
        );
      break;

    case LBMETA_OPT__REMOVE_SEI:
      dst->options.es_shared_options.discard_sei = true;
      break;

    case LBMETA_OPT__DISABLE_HRD_VERIF:
      dst->options.es_shared_options.disable_HRD_verifier = true;
      break;

    case LBMETA_OPT__HDMV_INITIAL_TS:
      if (setHdmvInitialTimestampLibbluMuxingSettings(dst, argument.u64) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid '%s' option value, "
          "value shall be between %" PRIu64 " and %" PRIu64 " "
          "(inclusive).\n",
          option.name,
          LIBBLU_MIN_HDMV_INIT_TIMESTAMP,
          LIBBLU_MAX_HDMV_INIT_TIMESTAMP
        );
      break;

    case LBMETA_OPT__HDMV_FORCE_RETIMING:
      dst->options.es_shared_options.hdmv.force_retiming = true;
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unexpected option '%s', cannot be used in MUXOPT section header.\n",
        option_node->name
      );
    }

    cleanLibbluMetaOptionArgValue(option, argument);
  }

  /* MUXOPT tracks, ES filepath, track specific options */
  LibbluMetaFileTrack *track = header_section->tracks;
  for (; NULL != track; track = track->next) {
    if (_parseMUXOPTSectionTrack(dst, track, meta_filepath) < 0)
      return -1;
  }

  return 0;
}


static int _analyzeCLPINFOSection(
  LibbluDiscProjectSettings *dst,
  const LibbluMetaFileSection *clpinfo_section,
  const lbc *meta_filepath
)
{
  LibbluMetaFileOption *option_node = clpinfo_section->common_options;
  for (; NULL != option_node; option_node = option_node->next) {
    LibbluMetaOption option;
    LibbluMetaOptionArgValue argument;

    switch (parseLibbluMetaHeaderOption(option_node, &option, &argument)) {
    case -1: /* Invalid option */
      return -1;

    default:
      LIBBLU_ERROR_RETURN(
        "Unexpected option '%s', cannot be used as Clip descriptor option.\n",
        option_node->name
      );
    }

    cleanLibbluMetaOptionArgValue(option, argument);
  }

  /* Clip tracks, ES filepath, track specific options */
  LibbluMetaFileTrack *track = clpinfo_section->tracks;
  for (; NULL != track; track = track->next) {
    // if (_parseMUXOPTSectionTrack(dst, track, meta_filepath) < 0)
    //   return -1;
    (void) dst;
    (void) meta_filepath;
  }

  return 0;
}


int readMetaFile(
  LibbluProjectSettings *dst,
  const lbc *meta_filepath,
  const lbc *output_filepath,
  const LibbluMuxingOptions mux_options
)
{
  FILE *input = lbc_fopen(meta_filepath, "r");
  if (NULL == input)
    LIBBLU_ERROR_RETURN(
      "Unable to open file '%s', %s (errno: %d).\n",
      meta_filepath,
      strerror(errno),
      errno
    );

  /* Parse the structure of the META file */
  LibbluMetaFileStructPtr meta = parseMetaFileStructure(input);
  if (NULL == meta)
    goto free_return;

  if (LBMETA_MUXOPT == meta->type) {
    /* Single Mux */
    dst->type = LIBBLU_SINGLE_MUX;

    /* Default settings filename */
    if (initLibbluMuxingSettings(&dst->single_mux_settings, output_filepath, mux_options) < 0)
      return -1;

    /* MUXOPT header, main mux and common options */
    const LibbluMetaFileSection *header_section = &meta->sections[LBMETA_HEADER];
    if (_analyzeMUXOPTSection(&dst->single_mux_settings, header_section, meta_filepath) < 0)
      goto free_return;
  }
  else {
    assert(LBMETA_DISCOPT == meta->type);
    /* Disc project */
    dst->type = LIBBLU_DISC_PROJECT;
    dst->disc_project_settings.shared_mux_options = mux_options;

    /* DISCOPT header, main mux and common options */
    // const LibbluMetaFileSection *header_section = &meta->sections[LBMETA_HEADER];
    // if (_analyzeDISCOPTSection(&dst->disc_project_settings, header_section, meta_filepath) < 0)
    //   goto free_return;

    /* CLPINFO header, clip definitions */
    const LibbluMetaFileSection *clpinfo_section = &meta->sections[LBMETA_CLPINFO];
    if (_analyzeCLPINFOSection(&dst->disc_project_settings, clpinfo_section, meta_filepath) < 0)
      goto free_return;

    LIBBLU_TODO();
  }

  destroyLibbluMetaFileStruct(meta);
  fclose(input);
  return 0;

free_return:
  destroyLibbluMetaFileStruct(meta);
  fclose(input);
  return -1;
}