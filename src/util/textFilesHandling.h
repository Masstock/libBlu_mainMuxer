

#ifndef __LIBBLU_MUXER__UTIL__TEXT_FILES_HANDLING_H__
#define __LIBBLU_MUXER__UTIL__TEXT_FILES_HANDLING_H__

#include "macros.h"
#include "common.h"
#include "errorCodes.h"

#define LINE_BUFSIZE  STR_BUFSIZE

typedef struct TxtFileHandler *TxtFileHandlerPtr;

TxtFileHandlerPtr openTxtFile(
  const lbc *filepath
);

void closeTxtFile(
  TxtFileHandlerPtr handler
);

const lbc * readTxtFile(
  TxtFileHandlerPtr handler
);

const lbc * lastReadedTxtFile(
  TxtFileHandlerPtr handler
);

bool isCommentaryTxtFile(
  TxtFileHandlerPtr handler
);

lbc nextCharTxtFile(
  TxtFileHandlerPtr handler
);

unsigned curLineTextFile(
  TxtFileHandlerPtr handler
);

bool isEolTxtFile(
  TxtFileHandlerPtr handler
);

bool isEofTxtFile(
  TxtFileHandlerPtr handler
);

#endif