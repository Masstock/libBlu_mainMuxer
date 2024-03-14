#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "textFilesHandling.h"

struct TxtFileHandler {
  struct {
    FILE *fd;
    lbc *es_filepath;
  } inputFile;

  lbc lineBuffer[LINE_BUFSIZE+1];  /**< Line buffer.                         */
  lbc *rp;                        /**< Line buffer reading pointer.         */
  unsigned lineNumber;             /**< Current line number.                 */

  lbc *token;
  size_t tokenAllocatedSize;

  bool eof;
};

TxtFileHandlerPtr openTxtFile(
  const lbc *filepath
)
{
  TxtFileHandlerPtr handler;

  handler = (TxtFileHandlerPtr) malloc(sizeof(struct TxtFileHandler));
  if (NULL == handler)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  handler->inputFile.fd = NULL;
  handler->inputFile.es_filepath = NULL;
  handler->lineBuffer[0] = lbc_char('\0');
  handler->rp = handler->lineBuffer;
  handler->lineNumber = 0;
  handler->token = lbc_char('\0');
  handler->tokenAllocatedSize = 0;

  handler->inputFile.fd = lbc_fopen(filepath, "r");

  handler->eof = feof(handler->inputFile.fd);

  if (NULL == handler->inputFile.fd)
    LIBBLU_ERROR_FRETURN(
      "Unable to open file '%" PRI_LBCS "', %s (errno: %d).\n",
      filepath,
      strerror(errno),
      errno
    );

  if (NULL == (handler->inputFile.es_filepath = lbc_strdup(filepath)))
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  return handler;

free_return:
  closeTxtFile(handler);
  return NULL;
}

void closeTxtFile(
  TxtFileHandlerPtr handler
)
{
  if (NULL == handler)
    return;

  if (NULL != handler->inputFile.fd) {
    if (fclose(handler->inputFile.fd) < 0)
      LIBBLU_ERROR(
        "Unable to close file '%" PRI_LBCS "', %s (errno: %d).\n",
        handler->inputFile.es_filepath,
        strerror(errno),
        errno
      );
  }

  free(handler->inputFile.es_filepath);
  free(handler->token);
  free(handler);
}

static int fillBufferTxtFile(
  TxtFileHandlerPtr handler
)
{
  lbc *car;

  if (handler->eof)
    LIBBLU_ERROR_RETURN(
      "Error while reading file '%" PRI_LBCS "', prematurate end-of-file.\n",
      handler->inputFile.es_filepath
    );

  if (NULL == lbc_fgets(handler->lineBuffer, LINE_BUFSIZE, handler->inputFile.fd)) {
    if (ferror(handler->inputFile.fd))
      return -1;

    handler->eof = true;
    return 0;
  }

  /* Suppress cntrl characters */
#if 1
  for (car = handler->lineBuffer; lbc_char('\0') != *car; car++) {
    switch (*car) {
    case lbc_char('\n'):
      handler->lineNumber++;
      break;

    case lbc_char('\r'):
    case lbc_char('\t'):
      *car = ' ';
    }
  }
  *car = lbc_char('\0');
#endif

  handler->rp = handler->lineBuffer; /* Reset the reading pointer */
  handler->eof = feof(handler->inputFile.fd); /* Set EOF if reached */

  return 1;
}

static int consumeGarbage(
  TxtFileHandlerPtr handler
)
{
  int ret;

  while (true) {
    if (
      lbc_isblank((lbc_int) *handler->rp)
      || *handler->rp == lbc_char(',')
      || *handler->rp == lbc_char('=')
    )
      handler->rp++;
    else if (*handler->rp == lbc_char('\0')) {
      if (handler->eof)
        break;

      if ((ret = fillBufferTxtFile(handler)) < 0)
        LIBBLU_ERROR_RETURN(
          "Error while reading file '%" PRI_LBCS "', %s (errno: %d).\n",
          handler->inputFile.es_filepath,
          strerror(errno),
          errno
        );

      if (!ret)
        break;
    }
    else
      break;
  }

  return 0;
}

static int consumeDelimiters(
  TxtFileHandlerPtr handler,
  const lbc *delimiters
)
{

  do {
    bool inPrefix;

    if (lbc_char('\0') == *handler->rp) {
      int ret;

      if ((ret = fillBufferTxtFile(handler)) < 0)
        LIBBLU_ERROR_RETURN(
          "Error while reading file '%" PRI_LBCS "', %s (errno: %d).\n",
          handler->inputFile.es_filepath,
          strerror(errno),
          errno
        );

      if (!ret)
        LIBBLU_ERROR_RETURN(
          "Error while reading file '%" PRI_LBCS "', prematurate end-of-file.\n",
          handler->inputFile.es_filepath
        );
    }

    do  {
      size_t i;

      inPrefix = false;
      for (i = 0; delimiters[i] != lbc_char('\0') && !inPrefix; i++) {
        if (*handler->rp == delimiters[i])
          inPrefix = true;
      }
    } while (inPrefix && *(handler->rp++) != lbc_char('\0'));
  } while (lbc_char('\0') == *handler->rp);

  return 0;
}

static int consumeToken(
  TxtFileHandlerPtr handler,
  const lbc *delimiters
)
{
  size_t tokenSize;

  /* Prefix */
  if (consumeDelimiters(handler, delimiters) < 0)
    return -1;

  /* Token */
  tokenSize = lbc_strcspn(handler->rp, delimiters);
  if (handler->tokenAllocatedSize <= tokenSize) {
    size_t newSize;
    lbc *newToken;

    newSize = tokenSize + 1;
    if (lb_mul_overflow_size_t(newSize, sizeof(lbc)))
      LIBBLU_ERROR_RETURN("String token size overflow.\n");

    newToken = (lbc *) realloc(handler->token, newSize *sizeof(lbc));
    if (NULL == newToken)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    handler->token = newToken;
    handler->tokenAllocatedSize = newSize;
  }

  memcpy(handler->token, handler->rp, tokenSize *sizeof(lbc));
  handler->token[tokenSize] = lbc_char('\0');

  handler->rp += tokenSize;

  return 0;
}

static int getNextToken(
  TxtFileHandlerPtr handler
)
{
  const lbc *delimiters;
  lbc expectedFinalChar;

  static const lbc *normalStateDelimiters = lbc_str(" \'\",=\n");
  static const lbc *singleQuotesStateDelimiters = lbc_str("\'\n");
  static const lbc *doubleQuotesStateDelimiters = lbc_str("\"\n");

  if (consumeGarbage(handler) < 0)
    return -1;

  switch (*handler->rp) {
  case lbc_char('\''):
    delimiters = singleQuotesStateDelimiters;
    expectedFinalChar = lbc_char('\'');
    break;

  case lbc_char('\"'):
    delimiters = doubleQuotesStateDelimiters;
    expectedFinalChar = lbc_char('\"');
    break;

  default:
    delimiters = normalStateDelimiters;
    expectedFinalChar = lbc_char('\0');
    break;
  }

  if (consumeToken(handler, delimiters) < 0)
    return -1;

  if (expectedFinalChar != lbc_char('\0')) {
    if (*handler->rp != expectedFinalChar)
      LIBBLU_ERROR_RETURN(
        "Broken string, expect a valid delimiter (lineNumber %u).\n",
        handler->lineNumber
      );
    handler->rp++;
  }

  if (*handler->token == lbc_char('#')) {
    while (*handler->rp != '\n')
      handler->rp++;
  }
  else {
    while (lbc_isblank((lbc_int) *handler->rp))
      handler->rp++;
  }

  return 0;
}

const lbc *readTxtFile(
  TxtFileHandlerPtr handler
)
{
  if (getNextToken(handler) < 0)
    return NULL;
  return handler->token;
}

const lbc *lastReadedTxtFile(
  TxtFileHandlerPtr handler
)
{
  return handler->token;
}

bool isCommentaryTxtFile(
  TxtFileHandlerPtr handler
)
{
  return (*handler->token == lbc_char('#'));
}

lbc nextCharTxtFile(
  TxtFileHandlerPtr handler
)
{
  return *handler->rp;
}

unsigned curLineTextFile(
  TxtFileHandlerPtr handler
)
{
  return handler->lineNumber;
}

bool isEolTxtFile(
  TxtFileHandlerPtr handler
)
{
  if (consumeGarbage(handler) < 0)
    return true;
  return nextCharTxtFile(handler) == lbc_char('\n') || isEofTxtFile(handler);
}

bool isEofTxtFile(
  TxtFileHandlerPtr handler
)
{
  if (consumeGarbage(handler) < 0)
    return true;
  return handler->eof && lbc_char('\0') == *handler->rp;
}