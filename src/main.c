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

#include <locale.h>

static void printHelp(
  void
)
{
#define P(str) lbc_printf(str "\n")

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
#if !DISABLE_T_STD_BUFFER_VER
  P("  --disable-tstd      Disable use of MPEG T-STD buffer verifier during ");
  P("                      mux. The verifier ensure accurate generated      ");
  P("                      stream buffering compliance according to MPEG-TS ");
  P("                      and BDAV specific specs but at an important speed");
  P("                      cost.                                            ");
  P("                                                                       ");
#endif
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
  P("  --echo-hrd-cpb      Enable display of Rec. ITU-T H.264 HRD CPB       ");
  P("                      buffering model on terminal. This option is      ");
  P("                      overwritten by --disable-hrd-verifier.           ");
  P("                                                                       ");
  P("  --echo-hrd-dpb      Enable display of Rec. ITU-T H.264 HRD DPB       ");
  P("                      buffering model on terminal. This option is      ");
  P("                      overwritten by --disable-hrd-verifier.           ");
  P("                                                                       ");
  P(" = Debugging options : =============================================== ");
  P("                                                                       ");
  P(" Enabling debugging mode allows to show various informations about     ");
  P(" program activities. Including bitstreams parsed data, muxer decisions ");
  P(" based on T-STD Buffering Verifier etc.                                ");
  P("                                                                       ");
  P(" Debugging messages are categorized and can be activated by name with  ");
  P(" the '--debug' command line option. if no option is specified, every   ");
  P(" option is enabled (as if the option \"all\" is used). Otherwise,      ");
  P("syntax to enable requested options is as following.                    ");
  P("                                                                       ");
  P("  --debug=\"categoryOne, categoryTwo, categoryThree, ...\"             ");
  P("                                                                       ");
  P(" Here is the list of available categories:                             ");
  printListLibbbluStatus(3);
  P("                                                                       ");
  P(" NOTE: Use option --printdebug to see description of each category.    ");
  P("                                                                       ");
  P(" Example:                                                              ");
  P("                                                                       ");
  P("  ./mainMuxer inputMetafile.meta out.m2ts --debug=\"all\"              ");
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

  lbc_printf("\n");
}

static void printDebugOptions(
  void
)
{
#define P(str) lbc_printf(str "\n")
  P(" = Debugging categories : ============================================ ");
  P(" List of available debugging options (see -h/--help for use).          ");
  printListWithDescLibbbluStatus(3);
  P(" ===================================================================== ");
#undef P

  lbc_printf("\n");
}

int main(
  int argc,
  char ** argv
)
{
  int option;
  clock_t duration, start;
  bool cont;

  LibbluMuxingSettings param;
  IniFileContextPtr confFile = NULL;

#if !defined(DISABLE_INI)
  const lbc * confFilepath = NULL;
#endif
  const lbc * inputInstructionsFilepath = NULL;
  const lbc * outputTsFilepath = NULL;

  bool esmsGenerationOnlyMode;
  bool forceRemakeScripts;

#if defined(ARCH_WIN32)
  int argc_wchar;
  LPWSTR * argv_wchar;
#endif

  static const struct option programOptions[] = {
    {"conf"            , required_argument, NULL,  'c'},
    {"d"               , optional_argument, NULL,  'd'},
    {"debug"           , optional_argument, NULL,  'd'},
    {"e"               , no_argument      , NULL,  'e'},
    {"only-esms"       , no_argument      , NULL,  'e'},
    {"f"               , no_argument      , NULL,  'f'},
    {"force-esms"      , no_argument      , NULL,  'f'},
    {"h"               , no_argument      , NULL,  'h'},
    {"help"            , no_argument      , NULL,  'h'},
    {"i"               , required_argument, NULL,  'i'},
    {"input"           , required_argument, NULL,  'i'},
    {"o"               , required_argument, NULL,  'o'},
    {"output"          , required_argument, NULL,  'o'},
    {"printdebug"      , no_argument      , NULL,  'p'},
    {NULL              , no_argument      , NULL, '\0'}
  };

  if (NULL == setlocale(LC_ALL, ""))
    LIBBLU_ERROR_RETURN("Unable to set locale.\n");
  if (NULL == setlocale(LC_NUMERIC, "C"))
    LIBBLU_ERROR_RETURN("Unable to set locale.\n");

#if defined(ARCH_WIN32)
  /* On windows, get the command line version with unicode characters
  (due to lack of UTF-8). */
  argv_wchar = CommandLineToArgvW(
    GetCommandLineW(),
    &argc_wchar
  );

  /* Initiate the iconv library used for UTF-8/Windows locale conversions */
  if (lb_init_iconv() < 0)
    return -1;
#endif

  opterr = 0;
  esmsGenerationOnlyMode = false;
  forceRemakeScripts = false;

  start = clock();

#if 0
  CrcParam crcParam = {
    DTS_EXT_SS_CRC_LENGTH,
    DTS_EXT_SS_CRC_POLY,
    0,
    0
  };

  crcTableGenerator(crcParam);
  exit(0);
#endif

  lbc_printf(PROG_INFOS " (" PROG_ARCH ")\n\n");

  /* defineDebugVerboseLevel(1); */

#if defined(ARCH_WIN32)
#  define ARG_VAL  argv_wchar[optind-1]
#else
#  define ARG_VAL  optarg
#endif

  cont = true;
  while (
    cont
    && 0 <= (
      option = getopt_long_only(argc, argv, "", programOptions, NULL)
    )
  ) {
    switch (option) {
      case 'c':
#if !defined(DISABLE_INI)
        if (NULL == optarg)
          LIBBLU_ERROR_RETURN("Expect a configuration filename after '-c'.\n");
        confFilepath = ARG_VAL;
#else
        LIBBLU_ERROR_RETURN("Configuration file unsupported in this build.\n");
#endif
        break;

      case 'd':
        if (enableDebugStatusString(NULL != optarg ? optarg : "all") < 0)
          return -1;
        break;

      case 'e':
        esmsGenerationOnlyMode = true;
        break;

      case 'f':
        forceRemakeScripts = true;
        break;

      case 'h':
        /* Help */
        printHelp();
        return 0;

      case 'i':
        /* Input */
        if (NULL == optarg)
          LIBBLU_ERROR_RETURN("Expect a META filename after '-i'.\n");
        inputInstructionsFilepath = ARG_VAL;
        break;

      case 'o':
        /* Output */
        if (NULL == optarg)
          LIBBLU_ERROR_RETURN("Expect a output filename after '-o'.\n");
        outputTsFilepath = ARG_VAL;
        break;

      case 'p':
        printDebugOptions();
        return 0;

      case -1:
        /* End of options */
        cont = false;
        break;

      default:
        LIBBLU_ERROR_RETURN(
          "Unknown option '%s', type -h/--help to get full list.\n",
          argv[optind-1]
        );
    }
  }

#undef ARG_VAL

#if !defined(DISABLE_INI)
  if (NULL == confFilepath) {
    if (0 <= lbc_access_fp(PROG_CONF_FILENAME, "r")) {
      if (parseIniFile(&confFile, PROG_CONF_FILENAME) < 0)
        return -1;
    }
  }
  else {
    if (parseIniFile(&confFile, confFilepath) < 0)
      return -1;
  }
#endif

  if (NULL == inputInstructionsFilepath)
    LIBBLU_ERROR_FRETURN(
      "Expect a input META filename (see -h/--help).\n"
    );

  if (initLibbluMuxingSettings(&param, outputTsFilepath, confFile) < 0) {
    cleanLibbluMuxingSettings(param);
    goto free_return;
  }

  LIBBLU_MUX_SETTINGS_SET_OPTION(
    &param,
    forcedScriptBuilding,
    forceRemakeScripts
  );

  if (parseMetaFile(inputInstructionsFilepath, &param) < 0)
    goto free_return;

  if (!esmsGenerationOnlyMode) {
    if (mainMux(param) < 0)
      goto free_return;
  }
  else {
    LibbluMuxingContextPtr ctx;

    param.options.disableTStdBufVerifier = true;

    /* Create muxing context and destroy it immediately */
    if (NULL == (ctx = createLibbluMuxingContext(param)))
      goto free_return;
    destroyLibbluMuxingContext(ctx);
  }

#if defined(ARCH_WIN32)
  /* On windows, release the iconv libary handle */
  if (lb_close_iconv() < 0)
    goto free_return;
#endif

  duration = clock() - start;
  lbc_printf("Total execution time: %ld ticks (%.2fs, %ld ticks/s).\n", duration, (float) duration / CLOCKS_PER_SEC, (clock_t) CLOCKS_PER_SEC);

  /* DO NOT clean muxing settings since these has been managed by mainMux(). */
  destroyIniFileContext(confFile);
  return 0;

free_return:
  destroyIniFileContext(confFile);

  duration = clock() - start;
  lbc_printf("Total execution time: %ld ticks (%.2fs, %ld ticks/s).\n", duration, (float) duration / CLOCKS_PER_SEC, (clock_t) CLOCKS_PER_SEC);

  return -1;
}