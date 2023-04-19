#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>

#include "metaFiles.h"

static int _parseHeaderMetaFile(
  LibbluMetaFileStructPtr meta,
  LibbluMuxingSettings * dst
)
{
  LibbluMetaFileOption * optNode;

  for (optNode = meta->commonOptions; NULL != optNode; optNode = optNode->next) {
    LibbluMetaOption option;
    LibbluMetaOptionArgValue argument;

    switch (parseLibbluMetaOption(optNode, &option, &argument, -1)) {
      case -1: /* Invalid option */
        return -1;

      case LBMETA_OPT__NO_EXTRA_HEADER:
        LIBBLU_MUX_SETTINGS_SET_OPTION(dst, writeTPExtraHeaders, false);
        break;

      case LBMETA_OPT__CBR_MUX:
        LIBBLU_MUX_SETTINGS_SET_OPTION(dst, cbrMuxing, true);
        break;

      case LBMETA_OPT__FORCE_ESMS:
        LIBBLU_MUX_SETTINGS_SET_OPTION(dst, forcedScriptBuilding, true);
        break;

      case LBMETA_OPT__DISABLE_T_STD:
        LIBBLU_MUX_SETTINGS_SET_OPTION(dst, disableBufModel, true);
        break;

      case LBMETA_OPT__START_TIME:
        if (setInitPresTimeLibbluMuxingSettings(dst, argument.u64) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "expect a value between %" PRIu64 " (inclusive) "
            "and %" PRIu64 " (non-inclusive).\n",
            option.name,
            LIBBLU_MIN_INIT_PRES_TIME,
            LIBBLU_MAX_INIT_PRES_TIME
          );
        break;

      case LBMETA_OPT__MUX_RATE:
        if (setTargetMuxRateLibbluMuxingSettings(dst, argument.u64) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "expect a value between %" PRIu64 " (inclusive) "
            "and %" PRIu64 " (non-inclusive).\n",
            option.name,
            LIBBLU_MIN_MUXING_RATE,
            LIBBLU_MAX_MUXING_RATE
          );
        break;

      case LBMETA_OPT__DVD_MEDIA:
        LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, dvdMedia, true);
        break;

      case LBMETA_OPT__DISABLE_FIXES:
        LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, disableFixes, true);
        break;

      case LBMETA_OPT__EXTRACT_CORE:
        LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, extractCore, true);
        break;

      case LBMETA_OPT__SET_FPS:
        if (setFpsChangeLibbluMuxingSettings(dst, argument.str) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "only standard FPS values can be used (see help).\n",
            option.name
          );
        break;

      case LBMETA_OPT__SET_AR:
        if (setArChangeLibbluMuxingSettings(dst, argument.str) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "must be in the format 'width:height' "
            "with non-zero numeric values (or '0:0' for 'unspecified').\n",
            option.name
          );
        break;

      case LBMETA_OPT__SET_LEVEL:
        if (setLevelChangeLibbluMuxingSettings(dst, argument.str) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "wrong level format or unknown value. Value format must be xx "
            "or x.x with numeric values (e.g. '4' or '40' or '4.0'). "
            "See ITU-T Rec. H.264 for level values list.\n",
            option.name
          );
        break;

      case LBMETA_OPT__REMOVE_SEI:
        LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, discardSei, true);
        break;

      case LBMETA_OPT__DISABLE_HRD_VERIF:
        LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, disableHrdVerifier, true);
        break;

      case LBMETA_OPT__HDMV_INITIAL_TIMESTAMP:
        if (setHdmvInitialTimestampLibbluMuxingSettings(dst, argument.u64) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "value shall be between %" PRIu64 " and %" PRIu64 " "
            "(inclusive).\n",
            option.name,
            LIBBLU_MIN_HDMV_INIT_TIMESTAMP,
            LIBBLU_MAX_HDMV_INIT_TIMESTAMP
          );
        break;

      case LBMETA_OPT__HDMV_FORCE_RETIMING:
        LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, hdmv.forceRetiming, true);
        break;

      default:
        LIBBLU_ERROR_RETURN(
          "Unexpected option '%" PRI_LBCS "', "
          "cannot be used in file header.\n",
          optNode->name
        );
    }

    cleanLibbluMetaOptionArgValue(option, argument);
  }

  return 0;
}

static int parseCodecNameMetaFile(
  const lbc * string,
  LibbluStreamCodingType * trackCodingType,
  const char ** trackCodingTypeName
)
{
  unsigned i;

  static const struct {
    const lbc * keyword;
    LibbluStreamCodingType codingType;
    const char * name;
  } codecs[] = {
#define D_(k, c, n) {.keyword = lbc_str(k), .codingType = c, .name = n}
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

      if (NULL != trackCodingType)
        *trackCodingType = codecs[i].codingType;

      if (NULL != trackCodingTypeName)
        *trackCodingTypeName = codecs[i].name;

      return 0;
    }
  }

  LIBBLU_ERROR_RETURN(
    "Unknown codec name '%" PRI_LBCS "'.\n",
    string
  );
}

static void _applyTrackGlobalOptions(
  LibbluESSettings * dst,
  LibbluMuxingSettings * muxSettings
)
{
  memcpy(
    &dst->options,
    &muxSettings->options.globalSharedOptions,
    sizeof(LibbluESSettingsOptions)
  );
}

static int parseTrackMetaFile(
  const LibbluMetaFileTrack * track,
  LibbluMuxingSettings * dst,
  const lbc * metaFilepath
)
{
  LibbluESSettings * elemStream;

  LibbluStreamCodingType trackCodingType;
  const char * trackCodingTypeName;

  const LibbluMetaFileOption * optNode;

  if (NULL == (elemStream = getNewESLibbluMuxingSettings(dst)))
    LIBBLU_ERROR_RETURN(
      "Invalid META file, too many elementary streams declared.\n"
    );

  /* Codec name */
  if (parseCodecNameMetaFile(track->codec, &trackCodingType, &trackCodingTypeName) < 0)
    return -1;

  /* Filepath */
  if (setMainESFilepathLibbluESSettings(elemStream, track->filepath, metaFilepath) < 0)
    LIBBLU_ERROR_RETURN(
      "Invalid META file, invalid filepath '%s'.\n",
      track->filepath
    );

  if (trackCodingType < 0) {
    /* Guess the stream coding type */
    if ((trackCodingType = guessESStreamCodingType(track->filepath)) < 0)
      return -1;
  }

  /* Global shared options */
  _applyTrackGlobalOptions(elemStream, dst);

  /* Options */
  for (optNode = track->options; NULL != optNode; optNode = optNode->next) {
    LibbluMetaOption option;
    LibbluMetaOptionArgValue argument;

    switch (parseLibbluMetaOption(optNode, &option, &argument, trackCodingType)) {
      case -1:
        return -1;

      case LBMETA_OPT__FORCE_ESMS:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, forcedScriptBuilding, true);
        break;

      case LBMETA_OPT__SECONDARY:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, secondaryStream, true);
        break;

      case LBMETA_OPT__DISABLE_FIXES:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, disableFixes, true);
        break;

      case LBMETA_OPT__EXTRACT_CORE:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, extractCore, true);
        break;

      case LBMETA_OPT__PBR_FILE: {
        lbc * path;
        if (lb_gen_anchor_absolute_fp(&path, metaFilepath, argument.str) < 0)
          return -1;
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, pbrFilepath, path);
        break;
      }

      case LBMETA_OPT__SET_FPS:
        if (setFpsChangeLibbluESSettings(elemStream, argument.str) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "only standard FPS values can be used (see help).\n",
            option.name
          );
        break;

      case LBMETA_OPT__SET_AR:
        if (setArChangeLibbluESSettings(elemStream, argument.str) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "must be in the format 'width:height' "
            "with non-zero numeric values (or '0:0' for 'unspecified').\n",
            option.name
          );
        break;

      case LBMETA_OPT__SET_LEVEL:
        if (setLevelChangeLibbluESSettings(elemStream, argument.str) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "wrong level format or unknown value. Value format must be xx "
            "or x.x with numeric values (e.g. '4' or '40' or '4.0'). "
            "See ITU-T Rec. H.264 for level values list.\n",
            option.name
          );
        break;

      case LBMETA_OPT__REMOVE_SEI:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, discardSei, true);
        break;

      case LBMETA_OPT__DISABLE_HRD_VERIF:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, disableHrdVerifier, true);
        break;

      case LBMETA_OPT__HRD_CPB_STATS: {
        lbc * path;
        if (lb_gen_anchor_absolute_fp(&path, metaFilepath, argument.str) < 0)
          return -1;
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, hrdCpbStatsFilepath, path);
        break;
      }

      case LBMETA_OPT__HRD_DPB_STATS: {
        lbc * path;
        if (lb_gen_anchor_absolute_fp(&path, metaFilepath, argument.str) < 0)
          return -1;
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, hrdDpbStatsFilepath, path);
        break;
      }

      case LBMETA_OPT__HDMV_INITIAL_TIMESTAMP:
        if (setHdmvInitialTimestampLibbluESSettings(elemStream, argument.u64) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "value shall be between %" PRIu64 " and %" PRIu64 " "
            "(inclusive).\n",
            option.name,
            LIBBLU_MIN_HDMV_INIT_TIMESTAMP,
            LIBBLU_MAX_HDMV_INIT_TIMESTAMP
          );
        break;

      case LBMETA_OPT__HDMV_FORCE_RETIMING:
        LIBBLU_ES_SETTINGS_SET_OPTION(elemStream, hdmv.forceRetiming, true);
        break;

      case LBMETA_OPT__ESMS:
        if (setScriptFilepathLibbluESSettings(elemStream, argument.str, metaFilepath) < 0)
          LIBBLU_ERROR_RETURN(
            "Invalid '%" PRI_LBCS "' option value, "
            "unable to set '%" PRI_LBCS "' as script filepath.\n",
            option.name,
            argument.str
          );
        break;

      default:
        LIBBLU_ERROR_RETURN(
          "Unexpected option '%" PRI_LBCS "', cannot be used on a %s track.\n",
          optNode->name,
          trackCodingTypeName
        );
    }
  }

  setExpectedCodingTypeLibbluESSettings(elemStream, trackCodingType);

  return 0;
}

int parseMetaFile(
  const lbc * filepath,
  LibbluMuxingSettings * dst
)
{
  FILE * input;
  LibbluMetaFileStructPtr meta;

  LibbluMetaFileTrack * track;

#if defined(ARCH_WIN32)
  input = lbc_fopen(filepath, "r, ccs=UTF-8");
#else
  input = lbc_fopen(filepath, "r");
#endif
  if (NULL == input)
    LIBBLU_ERROR_RETURN(
      "Unable to open file '%" PRI_LBCS "', %s (errno: %d).\n",
      filepath,
      strerror(errno),
      errno
    );

  /* Parse the structure of the META file */
  if (NULL == (meta = parseMetaFileStructure(input)))
    goto free_return;

  /* META header, main mux and common options */
  if (_parseHeaderMetaFile(meta, dst) < 0)
    goto free_return;

  /* META tracks, ES filepath, track specific options */
  for (track = meta->tracks; NULL != track; track = track->next) {
    if (parseTrackMetaFile(track, dst, filepath) < 0)
      goto free_return;
  }

  destroyLibbluMetaFileStruct(meta);
  fclose(input);
  return 0;

free_return:
  destroyLibbluMetaFileStruct(meta);
  fclose(input);
  return -1;
}