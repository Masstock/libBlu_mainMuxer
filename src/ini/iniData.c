#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "iniData.h"

IniFileContextPtr createIniFileContext(
  void
)
{
  IniFileContextPtr ctx;

  if (NULL == (ctx = (IniFileContextPtr) malloc(sizeof(IniFileContext))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  *ctx = (IniFileContext) {
    0
  };

  return ctx;
}

int loadSourceIniFileContext(
  IniFileContextPtr dst,
  FILE * inputFile
)
{
  char ** sourceCode;
  size_t allocatedLines;
  size_t usedLines;

  char * lineBuf = NULL;
  size_t allocatedLineBuf = 0;

  fpos_t originalOffset;

  assert(NULL == dst->src);

  if (fgetpos(inputFile, &originalOffset) < 0) {
    perror("Unable to parse input file");
    return -1;
  }

  sourceCode = NULL;
  allocatedLines = usedLines = 0;

  do {
    size_t usedLineBuf, lineLength;

    if (allocatedLines <= usedLines) {
      char ** newArray;
      size_t newSize;

      newSize = GROW_ALLOCATION(
        INI_DEFAULT_NB_SOURCE_CODE_LINES,
        allocatedLines
      );

      if (lb_mul_overflow(newSize, sizeof(char *)))
        LIBBLU_ERROR_FRETURN("Too many lines in input INI file, overflow.\n")

      newArray = (char **) realloc(sourceCode, newSize * sizeof(char *));
      if (NULL == newArray)
        LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

      sourceCode = newArray;
      allocatedLines = newSize;
    }

    usedLineBuf = 0;
    do {
      char * ret;

      if (allocatedLineBuf <= usedLineBuf) {
        char * newArray;
        size_t newSize;

        newSize = GROW_ALLOCATION(
          INI_DEFAULT_LEN_SOURCE_CODE_LINE,
          allocatedLineBuf
        );

        if (newSize <= usedLineBuf)
          LIBBLU_ERROR_FRETURN("Too many characters in INI file, overflow.\n");

        if (NULL == (newArray = (char *) realloc(lineBuf, newSize)))
          LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

        lineBuf = newArray;
        allocatedLineBuf = newSize;
      }

      if ((int) (allocatedLineBuf - usedLineBuf) <= 0)
        LIBBLU_ERROR_FRETURN("INI file characters amount value overflow.\n");

      ret = fgets(lineBuf + usedLineBuf, allocatedLineBuf - usedLineBuf, inputFile);
      if (NULL == ret) {
        if (ferror(inputFile))
          LIBBLU_ERROR_FRETURN(
            "Unable to read input INI file, %s (errno: %d).\n",
            strerror(errno),
            errno
          );

        strcpy(lineBuf, "\n");
      }

      lineLength = strlen(lineBuf + usedLineBuf);

      if (usedLineBuf + lineLength < usedLineBuf)
        LIBBLU_ERROR_FRETURN("Too many characters in source code.\n");
      usedLineBuf += lineLength;

    } while (allocatedLineBuf <= usedLineBuf);

    lineLength = strlen(lineBuf);
    sourceCode[usedLines] = (char *) malloc((lineLength + 1) * sizeof(char));
    if (NULL == sourceCode[usedLines])
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");
    strcpy(sourceCode[usedLines], lineBuf);

    usedLines++;
  } while (!feof(inputFile));

  free(lineBuf);

  dst->src = sourceCode;
  dst->srcNbLines = usedLines;

  if (fsetpos(inputFile, &originalOffset) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to rewind INI file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;

free_return:
  free(sourceCode);
  free(lineBuf);
  return -1;
}