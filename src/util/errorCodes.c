#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "errorCodes.h"
#include "../ini/iniHandler.h"

static struct {
  const LibbluExplodeLevel level;
  const char * name;
  bool disabled;
} explodeLevels[] = {
#define DECLARE_LEVEL(l, n)  {.level = l, .name = n}

  DECLARE_LEVEL(LIBBLU_EXPLODE_COMPLIANCE, "COMPLIANCE"),
  DECLARE_LEVEL(LIBBLU_EXPLODE_BD_COMPLIANCE, "BDCOMPLIANCE"),
  DECLARE_LEVEL(LIBBLU_EXPLODE_STD_COMPLIANCE, "STDCOMPLIANCE"),
  DECLARE_LEVEL(LIBBLU_EXPLODE_BDAV_STD_COMPLIANCE, "BDAVSTDCOMPLIANCE"),

#undef DECLARE_LEVEL
};

int isDisabledExplodeLevel(
  LibbluExplodeLevel level
)
{
  for (size_t i = 0; i < ARRAY_SIZE(explodeLevels); i++) {
    if (explodeLevels[i].level == level)
      return explodeLevels[i].disabled;
  }
  return false;
}

int readSettingsExplodeLevels(
  IniFileContextPtr handle
)
{
  if (NULL == handle)
    return 0;

  for (size_t i = 0; i < ARRAY_SIZE(explodeLevels); i++) {
    char optionPath[1024];
    snprintf(optionPath, 1024, "DISABLEDERRORS.%s", explodeLevels[i].name);

    lbc * string = lookupIniFile(handle, optionPath);
    if (NULL != string) {
      bool value;

      if (lbc_atob(&value, string) < 0)
        LIBBLU_ERROR_RETURN(
          "Invalid boolean value setting '%s' in section "
          "[DisabledErrors] of INI file.\n",
          explodeLevels[i].name
        );
      explodeLevels[i].disabled = value;
    }
  }

  return 0;
}

typedef enum {
  SINGLE_OPTION,
  RANGE
} debugging_option_type;

static const struct {
  debugging_option_type type;
  char * desc;

  union {
    struct {
      LibbluStatus opt;
      char ** names;
    } single;
    struct {
      char * name;
      LibbluStatus * values;
    } range;
  };
} debugging_options[] = {
#define DECLARE_OPTION(o, d, ...)  \
  {.type = SINGLE_OPTION, .desc = d, .single = {.opt = o, .names = (char *[]) {__VA_ARGS__, NULL}}}
#define DECLARE_RANGE(d, n, ...)  \
  {.type = RANGE, .desc = d, .range = {.name = n, .values = (LibbluStatus[]) {__VA_ARGS__, 0}}}

  DECLARE_OPTION(
    LIBBLU_DEBUG_GLB,
    "Enable all available categories, as '-d' option",
    "all"
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_SCRIPTS,
    "Muxer scripts management",
    "esms_operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_SCRIPTS_WRITING,
    "Muxer script files fields writing",
    "esms_writing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_SCRIPTS_WRITING_OPERATIONS,
    "Muxer scripts creation operations",
    "esms_writing_operations",
    "esms_writing_op"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_SCRIPTS_READING,
    "Muxer script files fields parsing",
    "esms_parsing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_SCRIPTS_READING_OPERATIONS,
    "Muxer scripts reading operations",
    "esms_parsing_operations",
    "esms_parsing_op"
  ),
  DECLARE_RANGE(
    "Muxer scripts management, creation and parsing",
    "esms",
    LIBBLU_DEBUG_SCRIPTS,
    LIBBLU_DEBUG_SCRIPTS_WRITING,
    LIBBLU_DEBUG_SCRIPTS_WRITING_OPERATIONS,
    LIBBLU_DEBUG_SCRIPTS_READING,
    LIBBLU_DEBUG_SCRIPTS_READING_OPERATIONS
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_MUXER_DECISION,
    "Muxer packets decision",
    "muxer_decisions"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_PES_BUILDING,
    "PES packets building information",
    "pes_building"
  ),

  /* Muxer T-STD Buffer Verifier : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_TEST,
    "T-STD Buffer Verifier tests",
    "t_std_test"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_DECLARATIONS,
    "T-STD Buffer Verifier packets data declaration",
    "t_std_decl"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_T_STD_VERIF_OPERATIONS,
    "T-STD Buffer Verifier operations",
    "t_std_operations"
  ),

  /* LCPM Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_LPCM_PARSING,
    "LPCM audio content",
    "lcpm_parsing"
  ),

  /* AC-3 Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_AC3_PARSING,
    "AC-3 audio content parsing",
    "ac3_parsing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_EAC3_PARSING,
    "Enhanced AC-3 audio content parsing",
    "eac3_parsing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_MLP_PARSING_HDR,
    "MLP audio content headers parsing",
    "mlp_hdr_parsing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_MLP_PARSING_MS,
    "MLP audio content major sync parsing",
    "mlp_ms_parsing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_MLP_PARSING_SS,
    "MLP audio content substreams parsing",
    "mlp_ss_parsing"
  ),
  DECLARE_RANGE(
    "MLP audio content parsing",
    "mlp_parsing",
    LIBBLU_DEBUG_MLP_PARSING_HDR,
    LIBBLU_DEBUG_MLP_PARSING_MS,
    LIBBLU_DEBUG_MLP_PARSING_SS
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_AC3_OPERATIONS,
    "AC-3 audio family parsers operations",
    "ac3_operations"
  ),
  DECLARE_RANGE(
    "AC-3 audio family parsing and operations",
    "ac3",
    LIBBLU_DEBUG_AC3_PARSING,
    LIBBLU_DEBUG_EAC3_PARSING,
    LIBBLU_DEBUG_MLP_PARSING_HDR,
    LIBBLU_DEBUG_MLP_PARSING_MS,
    LIBBLU_DEBUG_MLP_PARSING_SS,
    LIBBLU_DEBUG_AC3_OPERATIONS
  ),

  /* DTS Audio : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_DTSHD,
    "DTS audio DTSHD file headers content",
    "dts_parsing_dtshd"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_PBRFILE,
    "DTS audio dtspbr statistics file content",
    "dts_parsing_pbrfile"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_CORE,
    "DTS audio Core Substream content",
    "dts_parsing_core"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_EXTSS,
    "DTS audio Extension Substreams content",
    "dts_parsing_extSS"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PARSING_XLL,
    "DTS audio XLL extension content",
    "dts_parsing_xll"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PATCHER_OPERATIONS,
    "DTS audio patcher operations",
    "dts_patcher"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PATCHER_WRITING,
    "DTS audio patcher fields writing",
    "dts_patcher"
  ),
  DECLARE_RANGE(
    "DTS audio patcher",
    "dts_patcher",
    LIBBLU_DEBUG_DTS_PATCHER_OPERATIONS,
    LIBBLU_DEBUG_DTS_PATCHER_WRITING
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_PBR,
    "DTS audio PBR smoothing process informations",
    "dts_pbr"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_DTS_OPERATIONS,
    "DTS audio parser operations",
    "dts_operations"
  ),
  DECLARE_RANGE(
    "DTS audio parsing and operations",
    "dts",
    LIBBLU_DEBUG_DTS_PARSING_DTSHD,
    LIBBLU_DEBUG_DTS_PARSING_PBRFILE,
    LIBBLU_DEBUG_DTS_PARSING_CORE,
    LIBBLU_DEBUG_DTS_PARSING_EXTSS,
    LIBBLU_DEBUG_DTS_PARSING_XLL,
    LIBBLU_DEBUG_DTS_PATCHER_OPERATIONS,
    LIBBLU_DEBUG_DTS_PATCHER_WRITING,
    LIBBLU_DEBUG_DTS_PBR,
    LIBBLU_DEBUG_DTS_OPERATIONS
  ),

  /* H.262 Video : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_H262_PARSING,
    "H.262 video content",
    "h262_parsing"
  ),

  /* H.264 Video : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_NAL,
    "H.264 video NAL Units headers content",
    "h264_parsing_nalu"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_AUD,
    "H.264 video Access Unit Delimiter and other structural NALUs content",
    "h264_parsing_aud"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SPS,
    "H.264 video Sequence Parameter Set NALUs content",
    "h264_parsing_sps"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_VUI,
    "H.264 video SPS Video Usability Information content",
    "h264_parsing_vui"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_PPS,
    "H.264 video Picture Parameter Set NALUs content",
    "h264_parsing_pps"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SEI,
    "H.264 video Supplemental Enhancement Information messages NALUs content",
    "h264_parsing_sei"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_PARSING_SLICE,
    "H.264 video Slice headers NALUs content",
    "h264_parsing_slice"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_OPERATIONS,
    "H.264 video parser operations",
    "h264_operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_AU_PROCESSING,
    "H.264 video Access Units detection and stitching",
    "h264_au_processing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_AU_TIMINGS,
    "H.264 video Access Units timings",
    "h264_au_timings"
  ),
  DECLARE_RANGE(
    "H.264 video parsing and operations",
    "h264",
    LIBBLU_DEBUG_H264_PARSING_NAL,
    LIBBLU_DEBUG_H264_PARSING_AUD,
    LIBBLU_DEBUG_H264_PARSING_SPS,
    LIBBLU_DEBUG_H264_PARSING_VUI,
    LIBBLU_DEBUG_H264_PARSING_PPS,
    LIBBLU_DEBUG_H264_PARSING_SEI,
    LIBBLU_DEBUG_H264_PARSING_SLICE,
    LIBBLU_DEBUG_H264_OPERATIONS,
    LIBBLU_DEBUG_H264_AU_PROCESSING,
    LIBBLU_DEBUG_H264_AU_TIMINGS
  ),

  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_HRD,
    "H.264 HRD informations",
    "h264_hrd"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_HRD_CPB,
    "H.264 HRD Coded Picture Buffer status",
    "h264_hrd_cpb"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_H264_HRD_DPB,
    "H.264 HRD Decoded Picture Buffer status",
    "h264_hrd_dpb"
  ),

  /* HDMV Common : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_COMMON,
    "HDMV bitstreams common operations",
    "hdmv_common"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PARSER,
    "HDMV bitstreams common parsing informations",
    "hdmv_parser"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_CHECKS,
    "HDMV bitstreams compliance checks",
    "hdmv_checks"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_TS_COMPUTE,
    "HDMV bitstreams timestamps computation",
    "hdmv_ts_compute"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_SEG_BUILDER,
    "HDMV bitstreams segments builder",
    "hdmv_seg_builder"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_QUANTIZER,
    "HDMV pictures color quantization using Octree (in fact. an Hexatree)",
    "hdmv_quantizer"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PAL,
    "HDMV palettes contruction",
    "hdmv_pal"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_PICTURES,
    "HDMV pictures handling",
    "hdmv_pic"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_LIBPNG,
    "HDMV pictures LibPng IO operations",
    "hdmv_libpng"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_HDMV_TC,
    "HDMV streams generation timecodes management",
    "hdmv_timecode"
  ),

  /* HDMV IGS Parser : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_PGS_PARSER,
    "HDMV PGS (subtitles) content",
    "pgs_parser"
  ),

  /* HDMV IGS Parser : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_PARSER,
    "HDMV IGS (menus) content",
    "igs_parser"
  ),

  DECLARE_RANGE(
    "HDMV PG/IG parsing and processing",
    "hdmv",
    LIBBLU_DEBUG_HDMV_COMMON,
    LIBBLU_DEBUG_HDMV_PARSER,
    LIBBLU_DEBUG_HDMV_CHECKS,
    LIBBLU_DEBUG_HDMV_TS_COMPUTE,
    LIBBLU_DEBUG_HDMV_SEG_BUILDER,
    LIBBLU_DEBUG_HDMV_QUANTIZER,
    LIBBLU_DEBUG_HDMV_PAL,
    LIBBLU_DEBUG_HDMV_PICTURES,
    LIBBLU_DEBUG_HDMV_LIBPNG,
    LIBBLU_DEBUG_HDMV_TC,
    LIBBLU_DEBUG_PGS_PARSER,
    LIBBLU_DEBUG_IGS_PARSER
  ),

  /* HDMV IGS Compiler : */
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS,
    "HDMV IGS (menus) compiler XML file operations",
    "igs_compl_xml_operations"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_XML_PARSING,
    "HDMV IGS (menus) compiler XML file content",
    "igs_compl_xml_parsing"
  ),
  DECLARE_OPTION(
    LIBBLU_DEBUG_IGS_COMPL_OPERATIONS,
    "HDMV IGS (menus) compiler operations",
    "igs_compl_operations"
  )
#undef DECLARE_OPTION
#undef DECLARE_RANGE
};

static void _printDebugOptions(
  void
)
{
  LIBBLU_ERROR("List of all available debugging options: ");
  const char * sep = "";
  for (size_t i = 0; i < ARRAY_SIZE(debugging_options); i++) {
    if (SINGLE_OPTION == debugging_options[i].type) {
      LIBBLU_ERROR_NO_HEADER("%s'%s'", sep, debugging_options[i].single.names[0]);
      sep = ", ";
    }
  }
  LIBBLU_ERROR_NO_HEADER(".\n");
}

static char enabled_status[LIBBLU_NB_STATUS];
static bool enabled_debug;

static int _checkApplyDebugOption(
  const char * req_opt_name,
  size_t i
)
{

  if (SINGLE_OPTION == debugging_options[i].type) {
    char * const * opt_name = debugging_options[i].single.names;
    for (; NULL != *opt_name; opt_name++) {
      if (0 == strcmp(req_opt_name, *opt_name)) {
        LibbluStatus status = debugging_options[i].single.opt;

        enabled_debug = true;
        enabled_status[status] = 1;

        if (status == LIBBLU_DEBUG_GLB)
          memset(enabled_status, 1, ARRAY_SIZE(enabled_status));
        return 1;
      }
    }
  }
  else { // RANGE == debugging_options[i].type
    if (0 == strcmp(req_opt_name, debugging_options[i].range.name)) {
      for (size_t j = 0; 0 < debugging_options[i].range.values[j]; j++)
        enabled_status[debugging_options[i].range.values[j]] = 1;
      return 1;
    }
  }

  return 0;
}

int enableDebugStatusString(
  const char * string
)
{
  do {
    // Skip empty prefix
    while (isspace(*string) || ispunct(*string))
      string++;

    char req_opt_name[50];
    int ret;
    if (EOF == (ret = sscanf(string, "%49[A-Za-z0-9_]s", req_opt_name)))
      return -1;
    if (!ret)
      LIBBLU_ERROR_RETURN(
        "Invalid character in debugging modes string '%s'.\n",
        string
      );

    bool valid = false;
    for (size_t i = 0; !valid && i < ARRAY_SIZE(debugging_options); i++) {
      if (_checkApplyDebugOption(req_opt_name, i))
        valid = true;
    }

    if (!valid) {
      LIBBLU_ERROR("Unknown debugging mode '%s'.\n", string);
      _printDebugOptions();
      return -1;
    }

    string += strlen(req_opt_name);
  } while (*string != '\0');

  return 0;
}

bool isDebugEnabledLibbbluStatus(
  void
)
{
  return enabled_debug;
}

bool isEnabledLibbbluStatus(
  LibbluStatus status
)
{
  return enabled_status[status];
}

static FILE * debugging_fd = NULL;

int initDebugLogFile(
  const lbc * log_filepath
)
{
  FILE * fd = lbc_fopen(log_filepath, "w");
  if (NULL == fd)
    LIBBLU_ERROR_RETURN(
      "Unable to create output log file '%" PRI_LBCS "', %s (errno: %d).\n",
      log_filepath,
      strerror(errno),
      errno
    );

  LIBBLU_DEBUG_COM("Set output log file to '%" PRI_LBCS "'.\n", log_filepath);
  debugging_fd = fd;
  return 0;
}

int closeDebugLogFile(
  void
)
{
  if (NULL != debugging_fd && fclose(debugging_fd) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to close output log file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  return 0;
}

void echoMessageFdVa(
  FILE * fd,
  LibbluStatus status,
  const lbc * format,
  va_list args
)
{
  assert(NULL != fd);
  assert(NULL != format);
  assert(status < ARRAY_SIZE(enabled_status));

  if (status < LIBBLU_DEBUG_GLB || enabled_status[status])
    lbc_vfprintf(fd, format, args);
}

void echoMessageVa(
  LibbluStatus status,
  const lbc * format,
  va_list args
)
{
  // Print everything to log file if enabled:
  if (NULL != debugging_fd)
    echoMessageFdVa(debugging_fd, status, format, args);
  // Print debug to stderr if log is not enabled:
  if (LIBBLU_DEBUG_GLB <= status && NULL == debugging_fd)
    echoMessageFdVa(stderr, status, format, args);
  // Print warning and errors to stderr:
  if (LIBBLU_WARNING <= status && status < LIBBLU_DEBUG_GLB)
    echoMessageFdVa(stderr, status, format, args);
  // Print info to stdout:
  if (status <= LIBBLU_INFO)
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

void printDebuggingMessageCategoriesList(
  unsigned indent
)
{
  for (size_t i = 0; i < ARRAY_SIZE(debugging_options); i++) {
    if (SINGLE_OPTION != debugging_options[i].type)
      continue;

    lbc_printf("%-*c- %s", indent, ' ', debugging_options[i].single.names[0]);

    char * prefix = " (";
    char * suffix = "";
    char * const * name = &debugging_options[i].single.names[1];
    for (; NULL != *name; name++) {
      lbc_printf("%s%s", prefix, *name);
      prefix = ", ";
      suffix = ")";
    }

    lbc_printf("%s;\n", suffix);
  }
}

void printDebuggingMessageCategoriesListWithDesc(
  unsigned indent
)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(debugging_options); i++) {
    if (SINGLE_OPTION != debugging_options[i].type)
      continue;

    lbc_printf("%-*c- '%s'", indent, ' ', debugging_options[i].single.names[0]);

    char * prefix = " (aliase(s): ";
    char * suffix = "";
    char * const * name = &debugging_options[i].single.names[1];
    for (; NULL != *name; name++) {
      lbc_printf("%s'%s'", prefix, *name);
      prefix = ", ";
      suffix = ")";
    }
    lbc_printf("%s:\n", suffix);

    lbc_printf(
      "%-*c   %s.\n",
      indent, ' ',
      debugging_options[i].desc
    );
  }
}

void printDebuggingMessageRangesListWithDesc(
  unsigned indent
)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(debugging_options); i++) {
    if (RANGE != debugging_options[i].type)
      continue;

    lbc_printf("%-*c- '%s':\n", indent, ' ', debugging_options[i].range.name);
    lbc_printf("%-*c   %s.\n", indent, ' ', debugging_options[i].desc);
  }
}