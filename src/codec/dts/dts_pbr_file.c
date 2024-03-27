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
  uint64_t timestamp;
  unsigned frame_size;
} PbrFileHandlerEntry;

static struct {
  FILE *file;
  DtshdTCFrameRate TC_Frame_Rate;
  bool dropped_frame_rate;

  PbrFileHandlerEntry *entries;
  unsigned nb_alloc_entries;
  unsigned nb_entries;
  unsigned last_accessed_entry;
} dtspbr_handle;

static DtshdTCFrameRate _getTCFrameRate(
  char *frame_rate,
  bool dropped
)
{
  static const struct {
    DtshdTCFrameRate code;
    char *frame_rate;
    bool dropped;
  } values[] = {
    {     DTSHD_TCFR_23_976, "23.976", false},
    {         DTSHD_TCFR_24,     "24", false},
    {         DTSHD_TCFR_25,     "25", false},
    {DTSHD_TCFR_29_970_DROP, "29.970",  true},
    {     DTSHD_TCFR_29_970, "29.970", false},
    {    DTSHD_TCFR_30_DROP,     "30",  true},
    {         DTSHD_TCFR_30,     "30", false}
  };

  for (unsigned i = 0; i < ARRAY_SIZE(values); i++) {
    if (
      lb_str_equal(frame_rate, values[i].frame_rate)
      && dropped == values[i].dropped
    ) {
      return values[i].code; // Match
    }
  }

  return DTSHD_TCFR_RESERVED;
}

static int _parseFrameRate(
  void
)
{
  char buf[20];

  char *ret = fgets(buf, sizeof(buf), dtspbr_handle.file);
  if (NULL == ret)
    LIBBLU_DTS_PBR_ERROR_RETURN("Expect a frame-rate value.\n");

  char frame_rate_str[20];
  char drop_str[5];
  int nb_param = sscanf(buf, "%19s %4s", frame_rate_str, drop_str);

  if (nb_param <= 0)
    LIBBLU_DTS_PBR_ERROR_RETURN("Expect a frame-rate value.\n");

  bool dropped = false;
  if (1 < nb_param) {
    if (!lb_str_equal(drop_str, "Drop"))
      LIBBLU_DTS_PBR_ERROR_RETURN(
        "Invalid frame-rate specifier, expect 'Drop' or nothing.\n"
      );
    dropped = true;
  }

  DtshdTCFrameRate TC_Frame_Rate = _getTCFrameRate(frame_rate_str, dropped);
  if (DTSHD_TCFR_RESERVED == TC_Frame_Rate)
    LIBBLU_DTS_PBR_ERROR_RETURN(
      "Unknown timecode frame-rate value '%s'.\n",
      frame_rate_str
    );

  dtspbr_handle.TC_Frame_Rate = TC_Frame_Rate;
  return 0;
}

static uint64_t _applyFrameDropCompensation(
  uint64_t nb_frames,
  float frame_rate
)
{
  return nb_frames + (uint64_t) ceilf((nb_frames / frame_rate) * (18.f / 10.f));
}

static uint64_t _TCtoTimestamp(
  uint64_t seconds,
  uint64_t rem_frames
)
{

  static const unsigned frame_rates[][2] = {
    {24000, 1001},
    {24, 1},
    {25, 1},
    {30000, 1001},
    {30000, 1001},
    {30, 1},
    {30, 1}
  };

  unsigned frame_rate_idx = dtspbr_handle.TC_Frame_Rate - 1;

  if (likely(!dtspbr_handle.dropped_frame_rate)) {
    return (
      SUB_CLOCK_90KHZ *seconds
      + DIV_ROUND_UP(
        SUB_CLOCK_90KHZ *rem_frames *frame_rates[frame_rate_idx][1],
        frame_rates[frame_rate_idx][0]
      )
    );
  }

  /* Case of 'Drop' frame-rates */
  // TODO: Simplify this piece
  uint64_t nb_frames = rem_frames + (
    seconds *frame_rates[frame_rate_idx][0]
    / frame_rates[frame_rate_idx][1]
  );

  if (DTSHD_TCFR_29_970_DROP == dtspbr_handle.TC_Frame_Rate)
    nb_frames = _applyFrameDropCompensation(nb_frames, 30000.f / 1001.f);
  else // DTSHD_TCFR_30_DROP == dtspbr_handle.TC_Frame_Rate
    nb_frames = _applyFrameDropCompensation(nb_frames, 30.f);

  return DIV_ROUND_UP(
    SUB_CLOCK_90KHZ *nb_frames *frame_rates[frame_rate_idx][1],
    frame_rates[frame_rate_idx][0]
  );
}

static int _parsePbrFile(
  const DtshdFileHandler *dtshd
)
{
  LIBBLU_DTS_DEBUG_PBRFILE("Parsing DTS PBR Statistics file.\n");
  unsigned line_number = 0;

  /* Parse frame-rate */
  if (_parseFrameRate() < 0)
    goto free_return;
  line_number++;

  LIBBLU_DTS_DEBUG_PBRFILE(
    " Timecode Frame-rate: %s.\n",
    DtshdTCFrameRateStr(dtspbr_handle.TC_Frame_Rate)
  );

  unsigned samples_per_frame = dtshd->AUPR_HDR.Samples_Per_Frame_At_Max_Fs;
  unsigned sample_rate = dtshd->AUPR_HDR.Max_Sample_Rate_Hz;

  LIBBLU_DTS_DEBUG_PBRFILE(
    " Entries:\n"
  );
  for (;;) {
    char buf[30];

    char *ret = fgets(buf, sizeof(buf), dtspbr_handle.file);
    if (NULL == ret)
      break; // EOF

    unsigned h, m, s, frames, br_kbps;
    int nb_parsed_param = sscanf(
      buf, "%02u:%02u:%02u:%02u,%u\n",
      &h, &m, &s, &frames, &br_kbps
    );

    if (EOF == nb_parsed_param)
      break; // EOF (empty line)

    if (5 != nb_parsed_param)
      LIBBLU_DTS_PBR_ERROR_FRETURN(
        "Expect a line in format: \"XX:XX:XX:XX,XXXX\".\n"
      );

    if (dtspbr_handle.nb_alloc_entries <= dtspbr_handle.nb_entries) {
      unsigned new_size = GROW_ALLOCATION(
        dtspbr_handle.nb_alloc_entries,
        DTS_PBR_FILE_DEFAULT_NB_ENTRIES
      );
      if (lb_mul_overflow_size_t(new_size, sizeof(PbrFileHandlerEntry)))
        LIBBLU_DTS_PBR_ERROR_RETURN("Too many entries, overflow.\n");

      PbrFileHandlerEntry *new_arr = (PbrFileHandlerEntry *) realloc(
        dtspbr_handle.entries,
        new_size *sizeof(PbrFileHandlerEntry)
      );
      if (NULL == new_arr)
        LIBBLU_DTS_PBR_ERROR_RETURN("Memory allocation error.\n");

      dtspbr_handle.entries = new_arr;
      dtspbr_handle.nb_alloc_entries = new_size;
    }

    uint64_t timestamp  = _TCtoTimestamp(h * 3600u + m * 60u + s, frames);
    unsigned frame_size = DIV_ROUND_UP(
      br_kbps *samples_per_frame * 125u,
      sample_rate
    );

    dtspbr_handle.entries[dtspbr_handle.nb_entries++] = (PbrFileHandlerEntry) {
      .timestamp  = timestamp,
      .frame_size = frame_size
    };

    if (1 == dtspbr_handle.nb_entries) {
      if (0 != dtspbr_handle.entries[0].timestamp)
        LIBBLU_DTS_PBR_ERROR_FRETURN("PBR first entry timestamp must be zero.\n");
    }

    if (2 <= dtspbr_handle.nb_entries) {
      unsigned last_entry = dtspbr_handle.nb_entries - 1;

      if (
        dtspbr_handle.entries[last_entry].timestamp
        <= dtspbr_handle.entries[last_entry-1].timestamp
      ) {
        LIBBLU_DTS_PBR_ERROR_FRETURN(
          "PBR entry timestamp is earlier than the previous one.\n"
        );
      }
    }

    LIBBLU_DTS_DEBUG_PBRFILE(
      "  - %02u:%02u:%02u:%02u (%" PRIu64 "): "
      "%u kbps, %u bytes.\n",
      h, m, s, frames, timestamp,
      br_kbps, frame_size
    );
    line_number++;
  }

  LIBBLU_DTS_DEBUG_PBRFILE(
    " (%u entries parsed).\n",
    dtspbr_handle.nb_entries
  );
  LIBBLU_DTS_DEBUG_PBRFILE(
    "Done (End of DTS PBR Statistics file parsing).\n"
  );

  return 0;

free_return:
  LIBBLU_DTS_ERROR_RETURN(
    "Malformed input .dtspbr statistics file (line %u).\n",
    line_number
  );
}

int initPbrFileHandler(
  const lbc *dtspbr_filename,
  const DtshdFileHandler *dtshd
)
{
  assert(NULL != dtspbr_filename);

  LIBBLU_DTS_DEBUG_PBRFILE(
    "Opening DTS PBR Statistics file '%s'.\n",
    dtspbr_filename
  );

  if (NULL == (dtspbr_handle.file = lbc_fopen(dtspbr_filename, "r")))
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to open specified PBR database file '%s', "
      "%s (errno: %d).\n",
      dtspbr_filename,
      strerror(errno),
      errno
    );

  return _parsePbrFile(dtshd);
}

void releasePbrFileHandler(
  void
)
{
  if (NULL == dtspbr_handle.file)
    return; // PBR Handler wasn't used, nothing to do.

  fclose(dtspbr_handle.file);
  free(dtspbr_handle.entries);
  memset(&dtspbr_handle, 0, sizeof(dtspbr_handle)); // Reset
}

bool isInitPbrFileHandler(
  void
)
{
  return NULL != dtspbr_handle.file;
}

int getMaxSizePbrFileHandler(
  unsigned timestamp,
  unsigned *frame_size
)
{
  assert(NULL != frame_size);

  unsigned i = dtspbr_handle.last_accessed_entry;
  for (; i < dtspbr_handle.nb_entries; i++) {
    if (timestamp <= dtspbr_handle.entries[i].timestamp) {
      *frame_size = dtspbr_handle.entries[i].frame_size;
      return 0;
    }
  }

  *frame_size = dtspbr_handle.entries[i-1].frame_size;
  return 0;
}

int getAvgSizePbrFileHandler(
  uint32_t *avg_size
)
{
  double avg = 0.;
  for (unsigned i = 0; i < dtspbr_handle.nb_entries; i++) {
    avg += (((double) dtspbr_handle.entries[i].frame_size) - avg) / (i + 1.);
  }

  if (avg < 0 || UINT32_MAX < (uint64_t) ceil(avg))
    LIBBLU_DTS_PBR_ERROR_RETURN("Out of range avg frame size (%f).\n", avg);

  *avg_size = ceil(avg);
  return 0;
}