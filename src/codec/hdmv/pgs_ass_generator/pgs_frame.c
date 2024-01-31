#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "pgs_frame.h"

PgsFrame * createPgsFrame(
  void
)
{
  return calloc(1, sizeof(PgsFrame));
}

PgsFrameSequence * createPgsFrameSequence(
  void
)
{
  return calloc(1, sizeof(PgsFrameSequence));
}