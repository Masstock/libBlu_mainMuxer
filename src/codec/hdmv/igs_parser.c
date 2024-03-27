#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "igs_parser.h"

#define IGS_COMPILER_FILE_EXT  lbc_str(".xml")
#define IGS_COMPILER_FILE_EXT_SIZE  4

#define IGS_COMPILER_FILE_MAGIC  "<?xml"
#define IGS_COMPILER_FILE_MAGIC_SIZE  5

/** \~english
 * \brief Test if filepath finishes with #IGS_COMPILER_FILE_EXT string.
 *
 * \param filepath Input filepath to test.
 * \return true Filepath ends with #IGS_COMPILER_FILE_EXT string.
 * \return false Filepath does not end with #IGS_COMPILER_FILE_EXT string.
 */
static bool isIgsCompilerFile_ext(
  const lbc *filepath
)
{
  size_t fp_size = lbc_strlen(filepath);
  if (fp_size < IGS_COMPILER_FILE_EXT_SIZE)
    return false;
  const lbc *fp_ext = &filepath[fp_size - IGS_COMPILER_FILE_EXT_SIZE];

  return lbc_equal(fp_ext, IGS_COMPILER_FILE_EXT);
}

/** \~english
 * \brief Test if file starts with #IGS_COMPILER_FILE_MAGIC magic string.
 *
 * \param filepath Input filepath to test.
 * \return true The file starts with #IGS_COMPILER_FILE_MAGIC magic.
 * \return false The file does not start with #IGS_COMPILER_FILE_MAGIC magic.
 */
static bool isIgsCompilerFile_magic(
  const lbc *filepath
)
{
  bool ret = false;
  FILE *fd = lbc_fopen(filepath, "rb");
  if (NULL == fd)
    goto free_return;

  char magic[IGS_COMPILER_FILE_MAGIC_SIZE];
  if (fread(magic, IGS_COMPILER_FILE_MAGIC_SIZE, 1, fd) <= 0)
    goto free_return;

  ret = (
    0 == memcmp(
      magic,
      IGS_COMPILER_FILE_MAGIC,
      IGS_COMPILER_FILE_MAGIC_SIZE
    )
  );

free_return:
  if (NULL != fd)
    fclose(fd);

  errno = 0; /* Reset if error */
  return ret;
}

bool isIgsCompilerFile(
  const lbc *filepath
)
{
  assert(NULL != filepath);

  /* Check by filename extension (and by file header if unsuccessful) */
  return isIgsCompilerFile_ext(filepath) || isIgsCompilerFile_magic(filepath);
}

int analyzeIgs(
  LibbluESParsingSettings *settings
)
{
  HdmvTimecodes timecodes = {0};
  lbc *es_fp_dup = NULL;

  // bool compiler_mode = isIgsCompilerFile(settings->esFilepath);
  bool compiler_mode = settings->options.hdmv.xml_input;

  if (compiler_mode) {
#if !defined(DISABLE_IGS_COMPILER)
    LIBBLU_HDMV_IGS_DEBUG("Processing input file as IGS Compiler file.\n");

    if (processIgsCompiler(settings->esFilepath, &timecodes, settings->options.conf_hdl) < 0)
      return -1;

    int ret = lbc_asprintf(
      &es_fp_dup,
      lbc_str("%s" HDMV_IGS_COMPL_OUTPUT_EXT),
      settings->esFilepath
    );
    if (ret < 0)
      LIBBLU_HDMV_IGS_ERROR_RETURN(
        "Unable to set Igs Compiler output filename, %s (errno: %d).\n",
        strerror(errno),
        errno
      );
#else
    LIBBLU_ERROR_RETURN("IGS Compiler is not available in this program build !\n");
#endif
  }
  else {
    if (NULL == (es_fp_dup = lbc_strdup(settings->esFilepath)))
      LIBBLU_HDMV_IGS_ERROR_RETURN("Memory allocation error.\n");
  }

  HdmvContext *ctx = createHdmvContext(settings, es_fp_dup, HDMV_STREAM_TYPE_IGS, compiler_mode);
  if (NULL == ctx)
    goto free_return;

  if (compiler_mode) {
    /**
     * Add original XML file to script to target changes.
     * It is not used in script but will be check-summed. If the XML file is
     * updated, the script will be regenerated.
     */
    if (addOriginalFileHdmvContext(ctx, settings->esFilepath) < 0)
      goto free_return;
    /* Send timecode values to the HDMV context */
    if (addTimecodesHdmvContext(ctx, timecodes) < 0)
      goto free_return;
  }

  while (!isEofHdmvContext(ctx)) {
    /* Progress bar : */
    printFileParsingProgressionBar(inputHdmvContext(ctx));

    if (parseHdmvSegment(ctx) < 0)
      goto free_return;
  }

  /* Process remaining segments: */
  if (completeCurDSHdmvContext(ctx) < 0)
    return -1;

  lbc_printf(
    lbc_str(" === Parsing finished with success. ===              \n")
  );

  /* Display infos : */
  lbc_printf(
    lbc_str("== Stream Infos =======================================================================\n")
  );
  lbc_printf(lbc_str("Codec: HDMV/IGS Menu format.\n"));
  lbc_printf(lbc_str("Number of Display Sets: %u.\n"), ctx->nb_DS);
  lbc_printf(lbc_str("Number of Epochs: %u.\n"), ctx->nb_epochs);
  lbc_printf(lbc_str("Total number of segments per type:\n"));
  printContentHdmvContext(ctx);
  lbc_printf(
    lbc_str("=======================================================================================\n")
  );

  if (closeHdmvContext(ctx) < 0)
    goto free_return;
  destroyHdmvContext(ctx);
  cleanHdmvTimecodes(timecodes);
  free(es_fp_dup);
  return 0;

free_return:
  destroyHdmvContext(ctx);
  cleanHdmvTimecodes(timecodes);
  free(es_fp_dup);
  return -1;
}