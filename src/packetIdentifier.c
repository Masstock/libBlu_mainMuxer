#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "packetIdentifier.h"

void setBDAVStdLibbluNumberOfESTypes(
  LibbluNumberOfESTypes * dst
)
{
  *dst = (LibbluNumberOfESTypes) {
    .primaryVideo = 1,
    .secondaryVideo = 8,

    .primaryAudio = 32,
    .secondaryAudio = 32,

    .pg = 32,
    .ig = 32,
    .txtSubtitles = 256
  };
}

static LibbluRegisteredPIDValuesEntryPtr createLibbluRegisteredPIDValuesEntry(
  uint16_t pid
)
{
  LibbluRegisteredPIDValuesEntryPtr entry;

  entry = (LibbluRegisteredPIDValuesEntryPtr) malloc(
    sizeof(LibbluRegisteredPIDValuesEntry)
  );
  if (NULL == entry)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  entry->pid = pid;

  return entry;
}

int insertLibbluRegisteredPIDValues(
  LibbluRegisteredPIDValues * set,
  uint16_t pid
)
{
  LibbluRegisteredPIDValuesEntryPtr last, cur;
  LibbluRegisteredPIDValuesEntryPtr new;

  last = NULL;
  for (
    cur = set->entries;
    NULL != cur && pid <= cur->pid;
    cur = cur->nextEntry
  ) {
    if (cur->pid == pid)
      return 0; /* Already exist */
    last = cur;
  }

  if (NULL == (new = createLibbluRegisteredPIDValuesEntry(pid)))
    return -1; /* Error */

  new->nextEntry = cur;

  if (NULL == last)
    set->entries = new;
  else
    last->nextEntry = new;

  return 1; /* Successfully inserted */
}
