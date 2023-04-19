#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "ac3_data.h"

unsigned getNbChannels6ChPresentationAssignment(
  uint8_t u6ch_presentation_channel_assignment
)
{
  static const unsigned nb_channels[] = {
    2, // L/R
    1, // C
    1, // LFE
    2, // Ls/Rs
    2  // Tfl/Tfr
  };

  unsigned totalNbChannels = 0;
  for (unsigned i = 0; i < 5; i++) {
    if (u6ch_presentation_channel_assignment & (1 << i)) {
      totalNbChannels += nb_channels[i];
    }
  }
  return totalNbChannels;
}

unsigned getNbChannels8ChPresentationAssignment(
  uint8_t u8ch_presentation_channel_assignment
)
{
  // TODO: Consider reserved bits when bit 11 of flags == 1.
  static const unsigned nb_channels[] = {
    2, // L/R
    1, // C
    1, // LFE
    2, // Ls/Rs
    2, // Tfl/Tfr
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1
  };

  unsigned totalNbChannels = 0;
  for (unsigned i = 0; i < 13; i++) {
    if (u8ch_presentation_channel_assignment & (1 << i)) {
      totalNbChannels += nb_channels[i];
    }
  }
  return totalNbChannels;
}