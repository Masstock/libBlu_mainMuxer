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

/** \~english
 * \brief Test if filepath finishes with #IGS_COMPILER_FILE_EXT string.
 *
 * \param filepath Input filepath to test.
 * \return true Filepath ends with #IGS_COMPILER_FILE_EXT string.
 * \return false Filepath does not end with #IGS_COMPILER_FILE_EXT string.
 */
static bool isIgsCompilerFile_ext(
  const lbc * filepath
)
{
  const lbc * filepathExt;
  size_t filepathSize;

  filepathSize = lbc_strlen(filepath);
  if (filepathSize < IGS_COMPILER_FILE_EXT_SIZE)
    return false;
  filepathExt = &filepath[filepathSize - IGS_COMPILER_FILE_EXT_SIZE];

  return lbc_equal(filepathExt, IGS_COMPILER_FILE_EXT);
}

/** \~english
 * \brief Test if file starts with #IGS_COMPILER_FILE_MAGIC magic string.
 *
 * \param filepath Input filepath to test.
 * \return true The file starts with #IGS_COMPILER_FILE_MAGIC magic.
 * \return false The file does not start with #IGS_COMPILER_FILE_MAGIC magic.
 */
static bool isIgsCompilerFile_magic(
  const lbc * filepath
)
{
  FILE * fd;
  char magic[IGS_COMPILER_FILE_MAGIC_SIZE];
  bool ret;

  ret = false;
  if (NULL == (fd = lbc_fopen(filepath, "rb")))
    goto free_return;

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
  const lbc * filepath
)
{
  assert(NULL != filepath);

  /* Check by filename extension (and by file header if unsuccessful) */
  return
    isIgsCompilerFile_ext(filepath)
    || isIgsCompilerFile_magic(filepath)
  ;
}

int analyzeIgs(
  LibbluESParsingSettings * settings
)
{
  HdmvTimecodes timecodes;

  lbc * infilepathDup;
  bool igsCompilerFileMode;

  initHdmvTimecodes(&timecodes);

  igsCompilerFileMode = isIgsCompilerFile(settings->esFilepath);
  if (igsCompilerFileMode) {
#if !defined(DISABLE_IGS_COMPILER)
    int ret;

    LIBBLU_HDMV_IGS_DEBUG("Processing input file as Igs Compiler file.\n");

    if (processIgsCompiler(settings->esFilepath, &timecodes, settings->options.conf_hdl) < 0)
      return -1;

    ret = lbc_asprintf(
      &infilepathDup,
      "%" PRI_LBCS "%s",
      settings->esFilepath,
      HDMV_IGS_COMPL_OUTPUT_EXT
    );
    if (ret < 0)
      LIBBLU_HDMV_IGS_ERROR_RETURN(
        "Unable to set Igs Compiler output filename, %s (errno: %d).\n",
        strerror(errno),
        errno
      );

#else
    LIBBLU_ERROR("Igs Compiler is not available in this program build !\n");
    return -1;
#endif
  }
  else {
    if (NULL == (infilepathDup = lbc_strdup(settings->esFilepath)))
      LIBBLU_HDMV_IGS_ERROR_RETURN("Memory allocation error.\n");
  }

  HdmvContext ctx;
  if (initHdmvContext(&ctx, settings, infilepathDup, HDMV_STREAM_TYPE_IGS, igsCompilerFileMode) < 0)
    goto free_return;

  if (igsCompilerFileMode) {
    /**
     * Add original XML file to script to target changes.
     * It is not used in script but will be check-summed. If the XML file is
     * updated, the script will be regenerated.
     */
    if (addOriginalFileHdmvContext(&ctx, settings->esFilepath) < 0)
      goto free_return;
    /* Send timecode values to the HDMV context */
    if (addTimecodesHdmvContext(&ctx, timecodes) < 0)
      goto free_return;
  }

  while (!isEofHdmvContext(&ctx)) {
    /* Progress bar : */
    printFileParsingProgressionBar(inputHdmvContext(&ctx));

    if (parseHdmvSegment(&ctx) < 0)
      goto free_return;
  }

  /* Process remaining segments: */
  if (completeCurDSHdmvContext(&ctx) < 0)
    return -1;

  lbc_printf(" === Parsing finished with success. ===              \n");

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: HDMV/IGS Menu format.\n");
  lbc_printf("Number of Display Sets: %u.\n", ctx.nb_DS);
  lbc_printf("Number of Epochs: %u.\n", ctx.nb_epochs);
  lbc_printf("Total number of segments per type:\n");
  printContentHdmvContext(ctx);
  lbc_printf("=======================================================================================\n");

  if (closeHdmvContext(&ctx) < 0)
    goto free_return;
  cleanHdmvContext(ctx);
  cleanHdmvTimecodes(timecodes);
  free(infilepathDup);
  return 0;

free_return:
  cleanHdmvContext(ctx);
  cleanHdmvTimecodes(timecodes);
  free(infilepathDup);

  return -1;
}