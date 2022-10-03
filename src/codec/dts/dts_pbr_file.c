#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>

#include "dts_pbr_file.h"

typedef struct {
  unsigned timestamp;
  unsigned value;
} PbrFileHandlerEntry;

typedef struct {
  FILE * file;

  float frameRate;

  PbrFileHandlerEntry * entries;
  unsigned nbAllocatedEntries;
  unsigned nbUsedEntries;
  unsigned lastAccessedEntry;
} PbrFileHandler;

static PbrFileHandler pbrHandle = {
  .file = NULL,
  .entries = NULL
};

int initPbrFileHandler(
  const lbc * pbrFilename
)
{
  /* TODO: Implement PBR smoothing. */

  assert(NULL != pbrFilename);

  LIBBLU_DTS_DEBUG_PBRFILE(
    "Opening DTS PBR Statistics file '%" PRI_LBCS "'.\n",
    pbrFilename
  );

  if (NULL == (pbrHandle.file = lbc_fopen(pbrFilename, "r")))
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to open specified PBR database file '%" PRI_LBCS "', "
      "%s (errno: %d).\n",
      pbrFilename,
      strerror(errno),
      errno
    );

  return parsePbrFileHandler();
}

void releasePbrFileHandler(
  void
)
{
  if (NULL == pbrHandle.file)
    return; /* PBR Handler wasn't used, nothing to do. */

  fclose(pbrHandle.file);
  free(pbrHandle.entries);
  pbrHandle.file = NULL; /* Reset to NULL */
}

bool isInitPbrFileHandler(
  void
)
{
  return NULL != pbrHandle.file;
}

int parsePbrFileHandler(
  void
)
{
  unsigned newLength;
  PbrFileHandlerEntry * newArray;

  int lineNumber, nbParsedParam;

  char frameRateStr[DTS_PBR_FILE_FR_STR_SIZE+1];
  char timecodeLineStr[DTS_PBR_FILE_LINE_STR_SIZE+1];
  char * retPtr;

  /* Set defaults: */
  pbrHandle.entries = NULL;
  pbrHandle.nbAllocatedEntries = pbrHandle.nbUsedEntries = 0;
  pbrHandle.lastAccessedEntry = 0;
  lineNumber = 0;

  LIBBLU_DTS_DEBUG_PBRFILE("Parsing DTS PBR Statistics file.\n");

  /* Read frame-rate */
  retPtr = fgets(frameRateStr, DTS_PBR_FILE_FR_STR_SIZE, pbrHandle.file);
  if (NULL == retPtr)
    LIBBLU_DTS_PBR_ERROR_FRETURN("Expect a frame-rate value.\n");

  pbrHandle.frameRate = strtof(frameRateStr, &retPtr);
  if (pbrHandle.frameRate < 1.0 || frameRateStr == retPtr || errno == ERANGE)
    LIBBLU_DTS_PBR_ERROR_FRETURN("Expect a valid frame-rate value.\n");

  lineNumber++;

  LIBBLU_DTS_DEBUG_PBRFILE(" Frame-rate: %f.\n", pbrHandle.frameRate);

  LIBBLU_DTS_DEBUG_PBRFILE(" Entries:\n");
  for (;;) {
    unsigned h, m, s, ms;
    unsigned size;

    retPtr = fgets(
      timecodeLineStr,
      DTS_PBR_FILE_DEFAULT_NB_ENTRIES,
      pbrHandle.file
    );
    if (NULL == retPtr)
      break;

    nbParsedParam = sscanf(
      timecodeLineStr,
      "%02u:%02u:%02u:%02u,%u\n",
      &h, &m, &s, &ms, &size
    );

    if (EOF == nbParsedParam)
      break;
    if (5 != nbParsedParam)
      LIBBLU_DTS_PBR_ERROR_FRETURN(
        "Expect a line in format: \"XX:XX:XX:XX,XXXX\".\n"
      );

    if (pbrHandle.nbAllocatedEntries <= pbrHandle.nbUsedEntries) {
      newLength = GROW_ALLOCATION(
        pbrHandle.nbAllocatedEntries,
        DTS_PBR_FILE_DEFAULT_NB_ENTRIES
      );
      if (lb_mul_overflow(newLength, sizeof(PbrFileHandlerEntry)))
        LIBBLU_DTS_PBR_ERROR_RETURN(
          "Too many entries, overflow.\n"
        );

      newArray = (PbrFileHandlerEntry *) realloc(
        pbrHandle.entries,
        newLength * sizeof(PbrFileHandlerEntry)
      );
      if (NULL == newArray)
        LIBBLU_DTS_PBR_ERROR_RETURN(
          "Memory allocation error.\n"
        );

      pbrHandle.entries = newArray;
      pbrHandle.nbAllocatedEntries = newLength;
    }

    pbrHandle.entries[pbrHandle.nbUsedEntries++] = (PbrFileHandlerEntry) {
      .timestamp = h * 3600000 + m * 60000 + s * 1000 + ms,
      .value = size
    };

    if (1 == pbrHandle.nbUsedEntries) {
      if (0 != pbrHandle.entries[pbrHandle.nbUsedEntries-1].timestamp)
        LIBBLU_DTS_PBR_ERROR_FRETURN(
          "PBR first entry timestamp must be zero.\n"
        );
    }

    if (2 <= pbrHandle.nbUsedEntries) {
      if (
        pbrHandle.entries[pbrHandle.nbUsedEntries-1].timestamp
        <= pbrHandle.entries[pbrHandle.nbUsedEntries-2].timestamp
      )
        LIBBLU_DTS_PBR_ERROR_FRETURN(
          "PBR entry timestamp is earlier than the previous one.\n"
        );
    }

    LIBBLU_DTS_DEBUG_PBRFILE(
      "  - %02u:%02u:%02u:%02u,%02u\n", h, m, s, ms, size
    );
    lineNumber++;
  }
  LIBBLU_DTS_DEBUG_PBRFILE(" (%u entries parsed).\n", pbrHandle.nbUsedEntries);
  LIBBLU_DTS_DEBUG_PBRFILE("Done (End of DTS PBR Statistics file parsing).\n");

  return 0;

free_return:
  LIBBLU_DTS_ERROR_RETURN(
    "Malformed input .dtspbr statistics file (line %d).\n",
    lineNumber
  );
}

int getMaxSizePbrFileHandler(
  unsigned timestamp,
  size_t * maxSize
)
{
  unsigned i;

  assert(NULL != maxSize);

  for (i = pbrHandle.lastAccessedEntry; i < pbrHandle.nbUsedEntries; i++) {
    if (timestamp < pbrHandle.entries[i].timestamp) {
      *maxSize = pbrHandle.entries[i-1].value;
      return 0;
    }
  }

  *maxSize = pbrHandle.entries[i-1].value;
  return 0;
}

int getAvgSizePbrFileHandler(
  size_t * avgSize
)
{
  double avg;
  unsigned i;

  avg = 0;
  for (i = 0; i < pbrHandle.nbUsedEntries; i++) {
    avg += (((double) pbrHandle.entries[i].value) - avg) / (i+1);
  }

  *avgSize = (size_t) ABS(ceil(avg));
  return 0;
}