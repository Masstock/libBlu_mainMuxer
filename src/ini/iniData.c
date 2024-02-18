#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "iniData.h"

int loadSourceIniFileContext(
  IniFileContext * dst,
  FILE * input_fd
)
{
  // char ** sourceCode;
  // size_t allocatedLines;
  // size_t usedLines;

  // char * lineBuf = NULL;
  // size_t allocatedLineBuf = 0;

  // fpos_t originalOffset;

  assert(NULL == dst->src);

  fpos_t orig_off;
  if (fgetpos(input_fd, &orig_off) < 0) {
    perror("Unable to parse input file");
    return -1;
  }

  char ** ini_lines = NULL;
  size_t allocated_ini_lines = 0, used_ini_lines = 0;

  char * line_buf = NULL;
  size_t allocated_line_buf = 0;

  do {
    if (allocated_ini_lines <= used_ini_lines) {
      size_t new_size = GROW_ALLOCATION(
        INI_DEFAULT_NB_SOURCE_CODE_LINES,
        allocated_ini_lines
      );

      if (lb_mul_overflow_size_t(new_size, sizeof(char *)))
        LIBBLU_ERROR_FRETURN("Too many lines in input INI file, overflow.\n")

      char ** new_array = realloc(ini_lines, new_size * sizeof(char *));
      if (NULL == new_array)
        LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

      ini_lines = new_array;
      allocated_ini_lines = new_size;
    }

    size_t used_line_buf = 0;
    do {
      if (allocated_line_buf <= used_line_buf) {
        size_t new_size = GROW_ALLOCATION(
          INI_DEFAULT_LEN_SOURCE_CODE_LINE,
          allocated_line_buf
        );

        if (new_size <= used_line_buf)
          LIBBLU_ERROR_FRETURN("Too many characters in INI file, overflow.\n");

        char * new_array = realloc(line_buf, new_size);
        if (NULL == new_array)
          LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

        line_buf = new_array;
        allocated_line_buf = new_size;
      }

      if (INT_MAX < allocated_line_buf - used_line_buf)
        LIBBLU_ERROR_FRETURN("INI file characters amount value overflow.\n");
      int reading_size = allocated_line_buf - used_line_buf;

      if (NULL == fgets(&line_buf[used_line_buf], reading_size, input_fd)) {
        if (ferror(input_fd))
          LIBBLU_ERROR_FRETURN(
            "Unable to read input INI file, %s (errno: %d).\n",
            strerror(errno),
            errno
          );
      }

      size_t added_len = strlen(&line_buf[used_line_buf]);

      if (used_line_buf + added_len < used_line_buf)
        LIBBLU_ERROR_FRETURN("Too many characters in source code.\n");
      used_line_buf += added_len;

    } while (allocated_line_buf <= used_line_buf);

    char * line = lb_str_dup(line_buf);
    if (NULL == line)
      LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

    ini_lines[used_ini_lines] = line;
    used_ini_lines++;
  } while (!feof(input_fd));

  free(line_buf);

  dst->src = ini_lines;
  dst->src_nb_lines = used_ini_lines;

  if (fsetpos(input_fd, &orig_off) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to rewind INI file, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;

free_return:
  free(ini_lines);
  free(line_buf);
  return -1;
}