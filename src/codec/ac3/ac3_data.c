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

  unsigned nb_ch = 0;
  for (unsigned i = 0; i < 5; i++) {
    nb_ch += nb_channels[i] * ((u6ch_presentation_channel_assignment >> i) & 0x1);
  }
  return nb_ch;
}

unsigned getNbChannels8ChPresentationAssignment(
  uint8_t u8ch_presentation_channel_assignment,
  bool alternate_meaning
)
{
  static const unsigned nb_channels[] = {
    2, // L/R
    1, // C
    1, // LFE
    2, // Ls/Rs
    2, // Tfl/Tfr
    2, // Lsc/Rsc
    2, // Lb/Rb
    1, // Cb
    1, // Tc
    2, // Lsd/Rsd
    2, // Lw/Rw
    1, // Tfc
    1  // LFE2
  };

  unsigned nb_ch = 0;
  for (unsigned i = 0; i < ((alternate_meaning) ? 5 : 13); i++) {
    nb_ch += nb_channels[i] * ((u8ch_presentation_channel_assignment >> i) & 0x1);
  }
  return nb_ch;
}
