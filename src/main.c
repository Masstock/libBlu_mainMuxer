#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "util.h"
#include "muxingSettings.h"
#include "ini/iniHandler.h"
#include "input/meta/metaFiles.h"
#include "mainMuxer.h"

#if defined(ARCH_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#endif

#if !defined(DISABLE_INI)

/* From errorCodes.h */
extern int readSettingsExplodeLevels(
  IniFileContext handle
);

#endif

#include <locale.h>

static void printHelp(
  void
)
{
#define P(str) lbc_printf(lbc_str(str "\n"))

  P(" libBlu mainMuxer (temp. name) is an experimental MPEG-2 Transport     ");
  P(" Stream muxer used to multiplex video, audio and other kind of         ");
  P(" elementary stream into the BDAV MPEG-2 Transport Stream format        ");
  P(" (.m2ts) suitable for Blu-ray Disc authoring.                          ");
  P("                                                                       ");
  P(" The main target of the project is to study the ITU-T H.222 and related");
  P(" standards, and aim the best achivable compliance to the Blu-ray Disc  ");
  P(" Audio Visual standards without access to them.                        ");
  P("                                                                       ");
  P(" THIS PROGRAM AND ITS RELATED DOCUMENTATION ARE PROVIDED \"AS IT\",    ");
  P(" WITHOUT ANY WARRANTY OF ANY KIND.                                     ");
  P("                                                                       ");
  P(" PLEASE NOTE:                                                          ");
  P("  This program is currently in an early stage of development and may   ");
  P("  not be safe to use for purposes other than testing. Please rather use");
  P("  a more advanced project such as tsMuxeR.                             ");
  P("                                                                       ");
  P(" Usage:                                                                ");
  P("     mainMuxer [options] -i <infile> [-o <outfile>]                    ");
  P("                                                                       ");
  P(" Options:                                                              ");
  P("  -c <configfile>          Use supplied INI file as configuration file.");
  P("  --config <configfile>                                                ");
  P("                                                                       ");
  P("  -d[=values]              Enable verbose debugging output. If no value");
  P("  --debug[=values]         is used, all debugging messages are enabled.");
  P("                           See list of available debugging options in  ");
  P("                           dedicated section.                          ");
  P("                                                                       ");
  P("  -e --only-esms           Only performs input files scripts (ESMS     ");
  P("                           files) generation without muxing.           ");
  P("                                                                       ");
  P("  -f --force-esms          Force regeneration of ESMS script files.    ");
  P("                           This option can also be used in META file.  ");
  P("                                                                       ");
  P("  -h --help                Display this information.                   ");
  P("                                                                       ");
  P("  -i <infile>              Set the input mux instructions file (META). ");
  P("  --input <infile>                                                     ");
  P("                                                                       ");
  P("  -o <outfile>             Set the output file.                        ");
  P("  --output <outfile>                                                   ");
  P("                                                                       ");
  P("  --printdebug             Display every debugging option with a short ");
  P("                           description.                                ");
  P("                                                                       ");
  P("  --log <logfile>          Set a log file that will be used to save    ");
  P("                           enabled debug messages instead of using     ");
  P("                           standard error output. The other messages   ");
  P("                           (info, error) are also recorded.            ");
  P("                                                                       ");
  P(" If no output filename is specified, \"out.m2ts\" default filename     ");
  P(" is used.                                                              ");
  P("                                                                       ");
  P(" = Key features : ==================================================== ");
  P("  - H.264 Hypothetical Reference Decoder verifier. The parser also     ");
  P("    checks mostly of the ITU-T Rec. H.264 conformance requirements.    ");
  P("  - DTS PBR smoothing, the DTS XLL extension (com. name: DTS-HDMA, with");
  P("    DCA Core) frames size distribution based on average bit-rate. Might");
  P("    require some adjustments on the way the program use the statistics ");
  P("    file provided by the DTS encoder. Smoothing of already muxed files ");
  P("    (smoothed or not) could be added without too much efforts.         ");
  P("  - Muxer decisions based on a T-STD (Transport stream System Target   ");
  P("    Decoder) model to avoid buffer overflows and signal buffer         ");
  P("    underflows. This feature is very experimental since no comparative ");
  P("    tests are currenlty possible.                                      ");
  P("  - Generation of menus from XML, PNG pictures are used to produce     ");
  P("    buttons visuals after quantification and palettization.            ");
  P("                                                                       ");
  P(" = Embedded libraries notice : ======================================= ");
  P("  The following libraries are embed or linked statically.              ");
  P("  - libcwalk, path library for C/C++ under MIT licence.                ");
  P("     (https://github.com/likle/cwalk)                                  ");
  P("    See lib/libcwalk-LICENCE                                           ");

#if defined(ARCH_WIN32)
  P("  - libiconv, GNU Unicode conversion library under LGPL licence.       ");
  P("     (https://www.gnu.org/software/libiconv/)                          ");
  P("    See lib/libcwalk-LICENCE                                           ");
#endif

  P("                                                                       ");
  P(" = Included shared libraries notice : ================================ ");
  P("  This program may be supplied with following libraries:               ");
  P("  - libpng, PNG codec library under specific licence.                  ");
  P("    See libpng-LICENCE if supplied                                     ");
  P("  - libxml2, XML parsing library under MIT licence.                    ");
  P("     (https://gitlab.gnome.org/GNOME/libxml2)                          ");
  P("    See libxml2-LICENCE if supplied                                    ");
  P("    Required for IGS menus generation from XML.                        ");
  P("                                                                       ");
  P(" = Process : ========================================================= ");
  P("                                                                       ");
  P("  libBlu takes in input a META text-file (view syntax in 'META input   ");
  P("  file' section below) describing elementary streams (libBlu can't     ");
  P("  handle encapsulated streams such as MP4, M2TS...) to mux together.   ");
  P("  During this process, libBlu will parse, test input files to be       ");
  P("  fully compliant with Blu-ray Disc specs (libBlu is designed for BD   ");
  P("  and so won't currently tolerate non-compliant files. Due to the      ");
  P("  experimental state, some non compliant/broken files can be marked as ");
  P("  acceptable and vice-versa) and build an frame cutting/modifier       ");
  P("  script called ESMS (Elementary Stream Modification Script, .ess).    ");
  P("  These scripts contains stream frames offsets, timings, modification  ");
  P("  commands. If a compatible script as been already generated (or is    ");
  P("  supplied by option), this process is skipped. Script files uses      ");
  P("  following filename syntax: '<input stream filename>.ess'.            ");
  P("  When script process is finished, mux process just read thoses and    ");
  P("  apply instructions.                                                  ");
  P("                                                                       ");
  P(" = Currently supported codecs : ====================================== ");
  P("                                                                       ");
  P("  Video:                                                               ");
  P("   - V_H262         : H.262 MPEG-2 (.m2v)                             ");
  P("   - V_MPEG4/ISO/AVC : H.264/AVC (.264, .h264, .avc...)                ");
  P("                                                                       ");
  P("  Audio:                                                               ");
  P("   - A_LPCM          : RIFF WAVE PCM (.wav)                            ");
  P("   - A_AC3           : AC-3, E-AC-3, TrueHD (.ac3, .ac3plus, .thd...)  ");
  P("   - A_DTS           : DCA, DTS-HDHR, DTS-HDMA, DTS-Express            ");
  P("                       (.dca, .dtshd...)                               ");
  P("  HDMV:                                                                ");
  P("   - M_HDMV/IGS      : Blu-ray Interactive Graphic Stream menus (.ies, ");
  P("                       .mnu, .xml see chapter 'IGS compiler')          ");
  P("   - M_HDMV/PGS      : Blu-ray Presentation Graphic Stream graphical   ");
  P("                       subtitles (.pgs)                                ");
  P("                                                                       ");
  P(" = Configuration file : ============================================== ");
  P("                                                                       ");
  P(" In near future, program settings will be adjustable using a .ini      ");
  P(" format file. Currently this might only be used to set used DLL names. ");
  P("                                                                       ");
  P(" If no explicit configuration filepath is supplied, by default the     ");
  P(" program will try to open \"settings.ini\".                            ");
  P("                                                                       ");
  P(" = META input file : ================================================= ");
  P("                                                                       ");
  P(" libBlu originally use a similar META file syntax as tsMuxeR, but some ");
  P(" options may differ (and libBlu supports less options).                ");
  P(" META files must be encoded in UTF-8.                                  ");
  P("                                                                       ");
  P(" First line need 'MUXOPT' word and may contain global options, which   ");
  P(" defines output stream structure and options applied on every track.   ");
  P(" List of options given further.                                        ");
  P("                                                                       ");
  P("  MUXOPT [global options]                                              ");
  P("                                                                       ");
  P(" Following lines describes each track with the following syntax:       ");
  P("                                                                       ");
  P("  <codec identifier>, <stream filename> [track options...]             ");
  P("                                                                       ");
  P(" with:                                                                 ");
  P("  'codec identifier'  Elementary stream codec name in supported codecs ");
  P("                      list (e.g. A_LPCM)                               ");
  P("  'stream filename'   Elementary stream absolute or relative complete  ");
  P("                      access path (e.g. video.avc or C:/video.avc for  ");
  P("                      an H.264 video file), file extension does not    ");
  P("                      matter, program will try to parse file according ");
  P("                      to codec identifier.                             ");
  P("                      All filenames must be written using quotation    ");
  P("                      marks (\"'\"/'\"') if filename contains blank    ");
  P("                      spaces.                                          ");
  P("  'track options'     Track specific options (list given further).     ");
  P("                                                                       ");
  P(" NOTE:                                                                 ");
  P("  Options starts with double dashes and use equal symbol separator with");
  P("  the option value (if required/supplied).                             ");
  P("  Inline comments can be written using # symbol (e.g. '# Blablabla').  ");
  P("                                                                       ");
  P(" Example of META file:                                                 ");
  P("                                                                       ");
  P("  MUXOPT --start-time=60000000 --force-esms                            ");
  P("  V_MPEG4/ISO/AVC, track01.avc --level=40 --fps=30                     ");
  P("  A_DTS, audio.dts --core                                              ");
  P("                                                                       ");
  P(" This will mux H.264 track01.avc file (with modification of fps and    ");
  P(" level) with DTS audio.dts file (only with DCA core) at 60000000 clock ");
  P(" start time. Scripts generation is forced.                             ");
  P("                                                                       ");
  P(" = META Options : ==================================================== ");
  P("                                                                       ");
  P(" This section is splitted in sub-parts according to option scope.      ");
  P(" Globals options can be placed on the first line (following MUXOPT).   ");
  P(" Track specific options (as their name suggest) can only be used on    ");
  P(" tracks lines.                                                         ");
  P("                                                                       ");
  P(" Global options :                                                      ");
  P("  NOTE: Options with (*) symbol can also be used as track specific     ");
  P("   options.                                                            ");
  P("                                                                       ");
  P("  --cbr               Produce Constant Bit-Rate transport stream rather");
  P("                      than Variable Bit-Rate (VBR) using NULL padding  ");
  P("                      transport packets.                               ");
  P("                                                                       ");
  P("  --no-extra-header   Suppress addition of BDAV 4 bytes TP_extra_header");
  P("                      on MPEG-2 transport stream packets.              ");
  P("                                                                       ");
  P("  --start-time=<value>                                                 ");
  P("                      (In 90kHz clock ticks) define starting PTS clock ");
  P("                      timestamp (range: 90000 - 1620000000000).        ");
  P("                      Default: 54000000 (600s.)                        ");
  P("                                                                       ");
  P("  --mux-rate=<value>  (In bits per sec.) define maximum multiplex rate ");
  P("                      (range: 500000 - 120000000).                     ");
  P("                      Default: 48000000 (48Mbps)                       ");
  P("                                                                       ");
  P("  --force-esms        Force regeneration of an input stream script file");
  P("                      regardless of a compatible existent one (which   ");
  P("                      will be erased if present) (*).                  ");
  P("                                                                       ");
  P("  --dvd-media         Indicate to compatible input elementary parsers  ");
  P("                      that output .m2ts file is planned to be burned   ");
  P("                      supplied on DVD media (and so may apply higher   ");
  P("                      constraints and may warn if for exemple bitrate  ");
  P("                      could be too high for those good-old red lasers).");
  P("                       (*)                                             ");
  P("                                                                       ");
  P("  --disable-tstd      Disable use of MPEG T-STD buffer verifier during ");
  P("                      mux. The verifier ensure accurate generated      ");
  P("                      stream buffering compliance according to MPEG-TS ");
  P("                      and BDAV specific specs but at an important speed");
  P("                      cost.                                            ");
  P("                                                                       ");
  P(" Track specific options :                                              ");
  P("                                                                       ");
  P("  --esms=<filename>   Allows to specify a specific script file to use. ");
  P("                      (Compatibility is always checked, if script is   ");
  P("                      invalid, a new one will be generated).           ");
  P("                                                                       ");
  P("  --secondary         Indicate that stream must be muxed as a secondary");
  P("                      track. This enable secondary tracks specific     ");
  P("                      checks, enables DTS-Express tracks, apply        ");
  P("                      appropriate MPEG-2 TS descriptors and PIDs...    ");
  P("                                                                       ");
  P("   Dolby Audio specific parameters:                                    ");
  P("                                                                       ");
  P("  --core              Extracts and mux only retro-compatible AC-3 audio");
  P("                      core.                                            ");
  P("                                                                       ");
  P("   DTS Audio specific parameters:                                      ");
  P("                                                                       ");
  P("  --core              Extracts and mux only retro-compatible DCA core. ");
  P("                       NOTE: Not available with DTS-Express.           ");
  P("                                                                       ");
  P("  --pbr=<filename>    Indicate DTSPBR file to use if PBR smoothing     ");
  P("                      process is required by input file (.dtshd).      ");
  P("                                                                       ");
  P("   H.264 Video specific parameters:                                    ");
  P("                                                                       ");
  P("  --fps=<value>       Change in SPS VUI input frame-rate to specified  ");
  P("                      one.                                             ");
  P("                      (Values: 23.976, 24, 29.970, 25, 59.940, 50).    ");
  P("                                                                       ");
  P("  --ar=<value>        Change in SPS VUI input pixel aspect-ratio to    ");
  P("                      specified one. Value is in x:x format with x an  ");
  P("                      int. If choosen aspect ratio is not a standard   ");
  P("                      one, Extended_SAR structure is used.             ");
  P("                      (i.e. --ar=3:4 for 1.33).                        ");
  P("                                                                       ");
  P("  --level=<value>     Change input stream indicated level.             ");
  P("                      Be careful, if a lower level value is chosen,    ");
  P("                      bitstream may not respect requirements and       ");
  P("                      playback may be affected. Values are Rec.        ");
  P("                      ITU-T H.264 Annex A ones, in x.x format (or      ");
  P("                      without dot separation).                         ");
  P("                      (i.e. --level=4.0 or --level=40 for 4.0).        ");
  P("                                                                       ");
  P("  --remove-sei        Strip off all SEI NAL Units from input stream.   ");
  P("                                                                       ");
  P("  --disable-fixes     Disable all potential fixes (broken SPS VUI      ");
  P("                      fixes, duplicated NALUs removal...).             ");
  P("                                                                       ");
  P("  --disable-hrd-verif Disable Rec. ITU-T H.264 integrated HRD Verifier.");
  P("                                                                       ");
  // P("  --echo-hrd-cpb      Enable display of Rec. ITU-T H.264 HRD CPB       ");
  // P("                      buffering model on terminal. This option is      ");
  // P("                      overwritten by --disable-hrd-verifier.           ");
  // P("                                                                       ");
  // P("  --echo-hrd-dpb      Enable display of Rec. ITU-T H.264 HRD DPB       ");
  // P("                      buffering model on terminal. This option is      ");
  // P("                      overwritten by --disable-hrd-verifier.           ");
  // P("                                                                       ");
  // TODO
  P(" = Debugging messages : ============================================== ");
  P("                                                                       ");
  P(" Enabling debugging mode allows to show various informations about     ");
  P(" program activities. Including bitstreams parsed data, muxer decisions ");
  P(" based on T-STD Buffering Verifier etc.                                ");
  P("                                                                       ");
  P("                                                                       ");
  P(" Debugging messages are categorized (per module, type of operation...) ");
  P(" and can be triggered using the --debug/-d command line option. Without");
  P(" further argument, the command enables every message category.         ");
  P(" Otherwise, a selected list of categories can be enabled using the     ");
  P(" following syntax:                                                     ");
  P("                                                                       ");
  P("  --debug=\"category_one, category_two...\"                            ");
  P("                                                                       ");
  P(" where category_one, category_two and so long are one of the following ");
  P(" list:                                                                 ");
  printDebuggingMessageCategoriesList(3);
  P("                                                                       ");
  P(" Categories can also be enabled by topic range:                        ");
  P("                                                                       ");
  P(" See --printdebug to print list of categories and ranges of categories ");
  P(" with descriptions.                                                    ");
  P("                                                                       ");
  P(" Example:                                                              ");
  P("  ./mainMuxer input.meta output.m2ts --debug=\"h264_parsing_nalu, t_std_operations\"");
  P("                                                                       ");
  P(" = IGS compiler : ==================================================== ");
  P("                                                                       ");
  P(" It is possible to generate IGS (Blu-ray menus) from a input           ");
  P(" descriptive XML file (and .png images files for graphics).            ");
  P(" Definitive structure of XML is pending.                               ");
  P("                                                                       ");
  P(" = Known issues : ==================================================== ");
  P("                                                                       ");
  P(" - Due to non access to official Blu-ray Disc specification books some ");
  P("   constraints may be absent or wrongly guessed.                       ");
  P("                                                                       ");
  P(" - T-STD buffer verifier may not be implemented correctly due to       ");
  P("   assumptions. A lot of values are also missing.                      ");
  P("                                                                       ");
  P(" - AC-3 and MPEG-2 parser must be rewritten more properly, since these ");
  P("   has been written at the project start (when I start learning C).    ");
  P("                                                                       ");
  P(" - A lot of bugs and problems.                                         ");
  P("                                                                       ");
  P(" Any help is great !                                                   ");
  P("                                                                       ");
  P(" = Short term Roadmap : ============================================== ");
  P("                                                                       ");
  P(" - SSA/ASS to Blu-ray disc bitmap subtitles (targetting support of     ");
  P("   animations which will require a huge work on conversion/buffer      ");
  P("   management). This will use the libass library.                      ");
  P("                                                                       ");
  P(" - Menu VM commands compiler from a human-readable language.           ");
  P("                                                                       ");
  P(" - Fix of various bugs, implementation errors...                       ");
  P("                                                                       ");
  P(" - Documentation of program, ESMS fileformat...                        ");
  P("                                                                       ");
  P(" = Longer term Roadmap : ============================================= ");
  P("                                                                       ");
  P(" - Blu-ray UHD 4K support (HEVC, HDR, higher bitrates support...).     ");
  P("                                                                       ");
  P(" - Non-BD applications.                                                ");
  P("                                                                       ");
  P(" - VC-1 format support (almost out of scope).                          ");
  P("                                                                       ");
  P(" = Out of scope : ==================================================== ");
  P("                                                                       ");
  P(" - Blu-ray 3D support.                                                 ");
  P("                                                                       ");
  P(" ===================================================================== ");
#undef P

  lbc_printf(lbc_str("\n"));
}

static void printDebugOptions(
  void
)
{
#define P(str) lbc_printf(lbc_str(str "\n"))
  P(" = Debugging messages categories : =================================== ");
  P(" List of debugging messages categories:                                ");
  printDebuggingMessageCategoriesListWithDesc(3);
  P("                                                                       ");
  P(" List of ranges:                                                       ");
  printDebuggingMessageRangesListWithDesc(3);
  P(" ===================================================================== ");
#undef P

  lbc_printf(lbc_str("\n"));
}

#if defined(ARCH_WIN32)
/* ### Windows-API specific ################################################ */

static int argc_wchar;
static LPWSTR *argv_wchar;
static lbc *optarg_buf;

static void _unloadWin32CommandLineArguments(
  void
)
{
  LocalFree(argv_wchar);
  free(optarg_buf);
}

static const lbc * _getoptarg(
  void
)
{
  if (0 == argc_wchar) {
    /* Load Windows API Unicode command line arguments */
    argv_wchar = CommandLineToArgvW(
      GetCommandLineW(),
      &argc_wchar
    );
  }

  const LPWSTR woptarg = argv_wchar[optind-1];

  /* Convert string from Windows UTF-16 to UTF-8 */
  lbc *converted_string = winWideCharToUtf8(woptarg);
  if (NULL == converted_string)
    return NULL;

  /* Update optarg buffer */
  free(optarg_buf);
  optarg_buf = converted_string;

  return optarg_buf; // Return the buffer
}

#else

static void _unloadWin32CommandLineArguments(
  void
)
{
  // Do nothing
}

static const lbc * _getoptarg(
  void
)
{
  return (lbc *) optarg;
}

#endif

#define ERROR_RETURN(format, ...)  \
  __LIBBLU_ERROR_INSTR_(return EXIT_FAILURE, format, ##__VA_ARGS__)

int main(
  int argc,
  char ** argv
)
{
  /* Set locale */
  if (NULL == setlocale(LC_ALL, ""))
    ERROR_RETURN("Unable to set locale.\n");
  if (NULL == setlocale(LC_NUMERIC, "C"))
    ERROR_RETURN("Unable to set locale.\n");

#if 0
  CrcParam crc_param = (CrcParam) {
    .length = 8,
    .poly = 0x163,
    .shifted = true
  };

  crcTableGenerator(crc_param);
  exit(0);
#endif

  lbc_printf(lbc_str(LIBBLU_PROJECT_NAME " " LIBBLU_VERSION " (" PROG_ARCH ")\n\n"));

  /* Command line options parsing */
  const lbc *conf_fp   = NULL; // Configuration INI file
  const lbc *input_fp  = NULL; // Input META file
  const lbc *output_fp = NULL; // Output M2TS file

  bool script_gen_mode_only = false; // Script generation without TS output mode
  bool force_script_regen = false;   // Force scripts (re-)generation

  enum {
    END_OF_LIST,
    BD_CLIPINF_OUTPUT = 'A',
    CONFIG_FILE,
    DEBUG,
    ESMS_GENERATION_ONLY,
    FORCE_ESMS_GENERATION,
    HELP,
    INPUT,
    OUTPUT,
    PRINT_DEBUG_OPTIONS,
    LOG_FILE
  };

  static const struct option prog_cmd_opts[] = {
  //{"clip"            , optional_argument, NULL, BD_CLIPINF_OUTPUT          },
    {"conf"            , required_argument, NULL, CONFIG_FILE                },
    {"d"               , optional_argument, NULL, DEBUG                      },
    {"debug"           , optional_argument, NULL, DEBUG                      },
    {"e"               , no_argument      , NULL, ESMS_GENERATION_ONLY       },
    {"only-esms"       , no_argument      , NULL, ESMS_GENERATION_ONLY       },
    {"f"               , no_argument      , NULL, FORCE_ESMS_GENERATION      },
    {"force-esms"      , no_argument      , NULL, FORCE_ESMS_GENERATION      },
    {"h"               , no_argument      , NULL, HELP                       },
    {"help"            , no_argument      , NULL, HELP                       },
    {"i"               , required_argument, NULL, INPUT                      },
    {"input"           , required_argument, NULL, INPUT                      },
    {"o"               , required_argument, NULL, OUTPUT                     },
    {"output"          , required_argument, NULL, OUTPUT                     },
    {"printdebug"      , no_argument      , NULL, PRINT_DEBUG_OPTIONS        },
    {"log"             , required_argument, NULL, LOG_FILE                   },
    {NULL              , no_argument      , NULL, END_OF_LIST                }
  };

  opterr = 0; // Inhibit getopt error message printing

  int opt;
  while (0 <= (opt = getopt_long_only(argc, argv, "", prog_cmd_opts, NULL))) {
    switch (opt) {
    case CONFIG_FILE:
#if !defined(DISABLE_INI)
      if (NULL == optarg)
        ERROR_RETURN("Expect a configuration filename after '-c'.\n");
      conf_fp = _getoptarg();
      if (NULL == conf_fp)
        return EXIT_FAILURE;
#else
      ERROR_RETURN("Configuration file unsupported in this build.\n");
#endif
      break;

    case DEBUG:
      if (enableDebugStatusString(NULL != optarg ? optarg : "all") < 0)
        return EXIT_FAILURE;
      break;

    case ESMS_GENERATION_ONLY:
      script_gen_mode_only = true;
      break;

    case FORCE_ESMS_GENERATION:
      force_script_regen = true;
      break;

    case HELP:
      printHelp();
      return EXIT_SUCCESS;

    case INPUT:
      if (NULL == optarg)
        ERROR_RETURN("Expect a META filename after '-i'.\n");
      input_fp = _getoptarg();
      if (NULL == input_fp)
        return EXIT_FAILURE;
      break;

    case OUTPUT:
      if (NULL == optarg)
        ERROR_RETURN("Expect a output filename after '-o'.\n");
      output_fp = _getoptarg();
      if (NULL == output_fp)
        return EXIT_FAILURE;
      break;

    case PRINT_DEBUG_OPTIONS:
      printDebugOptions();
      return EXIT_SUCCESS;

    case LOG_FILE:
      if (NULL == optarg)
        ERROR_RETURN("Expect a log filename after '--log'.\n");
      const lbc *log_filepath = _getoptarg();
      if (NULL == log_filepath)
        return EXIT_FAILURE;

      if (initDebugLogFile(log_filepath) < 0)
        return EXIT_FAILURE;
      break;

    default:
      ERROR_RETURN(
        "Unknown option '%s', type -h/--help to get full list.\n",
        argv[optind-1]
      );
    }
  }

#undef ARG_VAL

  clock_t start = clock();

  /* Configuration file reading */
  IniFileContext conf_file = {0};

#if !defined(DISABLE_INI)
  if (NULL == conf_fp) {
    if (0 <= lbc_access_fp(PROG_CONF_FILENAME, "r")) {
      if (parseIniFile(&conf_file, PROG_CONF_FILENAME) < 0)
        return EXIT_FAILURE;
    }
  }
  else {
    if (parseIniFile(&conf_file, conf_fp) < 0)
      return EXIT_FAILURE;
  }

  if (readSettingsExplodeLevels(conf_file) < 0)
    return EXIT_FAILURE;
#endif

  if (NULL == input_fp) // TODO: Enable input from stdin
    ERROR_RETURN("Expect a input META filename (see -h/--help).\n");

  /* Init options according to configuration file */
  LibbluMuxingOptions mux_options = {0};
  if (initLibbluMuxingOptions(&mux_options, conf_file) < 0)
    return EXIT_FAILURE;

  /* Apply options passed by command line */
  mux_options.force_script_generation |= force_script_regen;
  if (script_gen_mode_only)
    mux_options.disable_buffering_model = true;

  /* Parse input configuration file */
  LibbluProjectSettings project_settings = {0};
  if (parseMetaFile(&project_settings, input_fp, output_fp, mux_options) < 0)
    goto free_return;

  if (!script_gen_mode_only) {
    /* Perform mux */
    if (LIBBLU_SINGLE_MUX == project_settings.type) {
      /* Single mux */
      if (mainMux(&project_settings.single_mux_settings) < 0)
        goto free_return;
    }
    else {
      /* Disc project */
      LIBBLU_TODO();
    }
  }
  else {
    /* Only generate scripts */
    if (LIBBLU_SINGLE_MUX == project_settings.type) {
      /* Single mux */

      /* Create muxing context and destroy it immediately */
      LibbluMuxingContext ctx;
      if (initLibbluMuxingContext(&ctx, &project_settings.single_mux_settings) < 0)
        goto free_return;
      cleanLibbluMuxingContext(ctx);
    }
    else {
      /* Disc project */
      LIBBLU_TODO();
    }
  }

  clock_t duration = clock() - start;
  lbc_printf(
    lbc_str("Total execution time: %ld ticks (%.2fs, %ld ticks/s).\n"),
    duration, (1.0 * duration) / CLOCKS_PER_SEC, (long) CLOCKS_PER_SEC
  );

  cleanLibbluProjectSettings(project_settings);
  cleanIniFileContext(conf_file);
  _unloadWin32CommandLineArguments();
  return EXIT_SUCCESS;

free_return:
  duration = clock() - start;
  lbc_printf(
    lbc_str("Total execution time: %ld ticks (%.2fs, %ld ticks/s).\n"),
    duration, (1.0 * duration) / CLOCKS_PER_SEC, (long) CLOCKS_PER_SEC
  );

  cleanLibbluProjectSettings(project_settings);
  cleanIniFileContext(conf_file);
  _unloadWin32CommandLineArguments();
  return EXIT_FAILURE;
}