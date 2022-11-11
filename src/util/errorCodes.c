#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "errorCodes.h"

static const struct {
  LibbluStatus opt;
  char * name;
  char * desc;
} debuggingOptions[] = {
#define DECLARE_OPTION(o, n, d)  {.opt = o, .name = n, .desc = d}

  DECLARE_OPTION(
    LIBBLU_DEBUG_GLB,
    "all",
    "Enable all available categories, as '-d' option"
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_MUXER_DECISION,
    "muxer_decisions",
    "Muxer packets decision"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_PES_BUILDING,
    "pes_building",
    "PES packets building information"
  ),

  /* Muxer T-STD Buffer Verifier : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_TEST,
    "t_std_test",
    "T-STD Buffer Verifier tests"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_DECLARATIONS,
    "t_std_decl",
    "T-STD Buffer Verifier packets data declaration"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_OPERATIONS,
    "t_std_operations",
    "T-STD Buffer Verifier operations"
  ),

  /* LCPM Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_LPCM_PARSING,
    "lcpm_parsing",
    "LPCM audio content"
  ),

  /* AC-3 Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_AC3_PARSING,
    "ac3_parsing",
    "AC-3 audio content"
  ),

  /* DTS Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_DTSHD,
    "dts_parsing_dtshd",
    "DTS audio DTSHD file headers content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_PBRFILE,
    "dts_parsing_pbrfile",
    "DTS audio dtspbr statistics file content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_CORE,
    "dts_parsing_core",
    "DTS audio Core Substream content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_EXTSS,
    "dts_parsing_extSS",
    "DTS audio Extension Substreams content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_XLL,
    "dts_parsing_xll",
    "DTS audio XLL extension content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PBR,
    "dts_pbr",
    "DTS audio PBR smoothing process informations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_OPERATIONS,
    "dts_operations",
    "DTS audio parser operations"
  ),

  /* H.264 Video : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_NAL,
    "h264_parsing_nalu",
    "H.264 video NAL Units headers content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_AUD,
    "h264_parsing_aud",
    "H.264 video Access Unit Delimiter and other structural NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SPS,
    "h264_parsing_sps",
    "H.264 video Sequence Parameter Set NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_VUI,
    "h264_parsing_vui",
    "H.264 video SPS Video Usability Information content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_PPS,
    "h264_parsing_pps",
    "H.264 video Picture Parameter Set NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SEI,
    "h264_parsing_sei",
    "H.264 video Supplemental Enhancement Information messages NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SLICE,
    "h264_parsing_slice",
    "H.264 video Slice headers NALUs content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_OPERATIONS,
    "h264_operations",
    "H.264 video parser operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_AU_PROCESSING,
    "h264_access_units_processing",
    "H.264 video Access Units detection and stitching"
  ),

  /* HDMV Common : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_COMMON,
    "hdmv_common",
    "HDMV bitstreams common operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_SEG_BUILDER,
    "hdmv_seg_builder",
    "HDMV bitstreams segments builder"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_QUANTIZER,
    "hdmv_quantizer",
    "HDMV pictures color quantization using Octree (in fact. an Hexatree)"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PAL,
    "hdmv_pal",
    "HDMV palettes contruction"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PICTURES,
    "hdmv_pic",
    "HDMV pictures handling"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_LIBPNG,
    "hdmv_libpng",
    "HDMV pictures LibPng IO operations"
  ),

  /* HDMV IGS Parser : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_PARSER,
    "igs_parser",
    "HDMV IGS (menus) content"
  ),

  /* HDMV IGS Compiler : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS,
    "igs_compl_xml_operations",
    "HDMV IGS (menus) compiler XML file operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_XML_PARSING,
    "igs_compl_xml_parsing",
    "HDMV IGS (menus) compiler XML file content"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_OPERATIONS,
    "igs_compl_operations",
    "HDMV IGS (menus) compiler operations"
  )
#undef DECLARE_OPTION
};

#if 0
static const char * debuggingLevelsNames[] = {
  "all",

  "muxer_decisions",
  "pes_building",

  "t_std_test",
  "t_std_decl",
  "t_std_operations",

  "dts_parsing_dtshd",
  "dts_parsing_pbrfile",
  "dts_parsing_core",
  "dts_parsing_extSS",
  "dts_parsing_xll",
  "dts_pbr",
  "dts_operations",

  "h264_parsing_nalu",
  "h264_parsing_aud",
  "h264_parsing_sps",
  "h264_parsing_vui",
  "h264_parsing_pps",
  "h264_parsing_sei",
  "h264_parsing_slice",
  "h264_operations",
  "h264_access_units_processing"
};

static const char * debuggingLevelsDescs[] = {
  "Enable all available categories, as '-d' option",

  "Muxer packets decision",
  "PES packets building information",

  "T-STD Buffer Verifier tests",
  "T-STD Buffer Verifier packets data declaration",
  "T-STD Buffer Verifier operations",

  "DTS audio DTSHD file headers content",
  "DTS audio dtspbr statistics file content",
  "DTS audio Core Substream content",
  "DTS audio Extension Substream content",
  "DTS audio XLL extension content",
  "DTS audio PBR smoothing process informations",
  "DTS audio parser operations",

  "H.264 video NAL Units headers content",
  "H.264 video Access Unit Delimiter and other structural NALUs content",
  "H.264 video Sequence Parameter Set NALUs content",
  "H.264 video SPS Video Usability Information content",
  "H.264 video Picture Parameter Set NALUs content",
  "H.264 video Supplemental Enhancement Information messages NALUs content",
  "H.264 video Slice headers NALUs content",
  "H.264 video parser operations",
  "H.264 video Access Units detection and stitching"
};

static const LibbluStatus debuggingLevels[] = {
  LIBBLU_DEBUG_GLB,

  LIBBLU_DEBUG_MUXER_DECISION,
  LIBBLU_DEBUG_PES_BUILDING,

  LIBBLU_DEBUG_T_STD_VERIF_TEST,
  LIBBLU_DEBUG_T_STD_VERIF_DECLARATIONS,
  LIBBLU_DEBUG_T_STD_VERIF_OPERATIONS,

  LIBBLU_DEBUG_DTS_PARSING_DTSHD,
  LIBBLU_DEBUG_DTS_PARSING_PBRFILE,
  LIBBLU_DEBUG_DTS_PARSING_CORE,
  LIBBLU_DEBUG_DTS_PARSING_EXTSS,
  LIBBLU_DEBUG_DTS_PARSING_XLL,
  LIBBLU_DEBUG_DTS_PBR,
  LIBBLU_DEBUG_DTS_OPERATIONS,

  LIBBLU_DEBUG_H264_PARSING_NAL,
  LIBBLU_DEBUG_H264_PARSING_AUD,
  LIBBLU_DEBUG_H264_PARSING_SPS,
  LIBBLU_DEBUG_H264_PARSING_VUI,
  LIBBLU_DEBUG_H264_PARSING_PPS,
  LIBBLU_DEBUG_H264_PARSING_SEI,
  LIBBLU_DEBUG_H264_PARSING_SLICE,
  LIBBLU_DEBUG_H264_OPERATIONS,
  LIBBLU_DEBUG_H264_AU_PROCESSING
};
#endif

static char enabledStatus[LIBBLU_NB_STATUS] = {
  0
};

int enableDebugStatusString(
  const char * string
)
{
  bool arrayMode = false;

  if (*string == '\"') {
    arrayMode = true;
    string++;
  }

  do {
    LibbluStatus status;
    char modeName[50];
    size_t i;

    sscanf(string, " %49[A-Za-z0-9_]s", modeName);
    string += strlen(modeName);

    status = LIBBLU_FATAL_ERROR;
    for (i = 0; i < ARRAY_SIZE(debuggingOptions); i++) {
      if (0 == strcmp(modeName, debuggingOptions[i].name)) {
        status = debuggingOptions[i].opt;
        break;
      }
    }

    if (status <= LIBBLU_FATAL_ERROR)
      LIBBLU_ERROR_RETURN(
        "Unknown debugging mode '%s'.\n",
        modeName
      );

    enabledStatus[status] = 1;
    if (status == LIBBLU_DEBUG_GLB) {
      memset(
        enabledStatus,
        1,
        ARRAY_SIZE(enabledStatus) * sizeof(uint8_t)
      );
    }

    while (arrayMode && (ispunct(*string) || isspace(*string)))
      string++;
  } while (arrayMode && (*string != '\"' && *string != '\0'));

  return 0;
}

bool isEnabledLibbbluStatus(
  LibbluStatus status
)
{
  return enabledStatus[status];
}

void echoMessageFdVa(
  FILE * fd,
  LibbluStatus status,
  const lbc * format,
  va_list args
)
{
  if (status < LIBBLU_DEBUG_GLB || enabledStatus[status])
    lbc_vfprintf(fd, format, args);
}

void echoMessageVa(
  LibbluStatus status,
  const lbc * format,
  va_list args
)
{
  if (LIBBLU_FATAL_ERROR <= status)
    echoMessageFdVa(stderr, status, format, args);
  else
    echoMessageFdVa(stdout, status, format, args);
}

void echoMessageFd(
  FILE * fd,
  LibbluStatus status,
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);
  echoMessageFdVa(fd, status, format, args);
  va_end(args);
}

void echoMessage(
  LibbluStatus status,
  const lbc * format,
  ...
)
{
  va_list args;

  va_start(args, format);
  echoMessageVa(status, format, args);
  va_end(args);
}

void printListLibbbluStatus(
  unsigned indent
)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(debuggingOptions); i++) {
    lbc_printf(
      "%-*c- %s;\n",
      indent, ' ',
      debuggingOptions[i].name
    );
  }
}

void printListWithDescLibbbluStatus(
  unsigned indent
)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(debuggingOptions); i++) {
    lbc_printf(
      "%-*c- %s;\n",
      indent, ' ',
      debuggingOptions[i].name
    );
    lbc_printf(
      "%-*c   %s.\n",
      indent, ' ',
      debuggingOptions[i].desc
    );
  }
}