

#ifndef __LIBBLU_MUXER__CODECS__AC3__CORE_CHECK_H__
#define __LIBBLU_MUXER__CODECS__AC3__CORE_CHECK_H__

#include "ac3_error.h"
#include "ac3_data.h"


int checkAc3SyncInfoCompliance(
  const Ac3SyncInfoParameters * param
);

int checkAc3AddbsiCompliance(
  const Ac3Addbsi * param
);

int checkAc3BitStreamInfoCompliance(
  const Ac3BitStreamInfoParameters * param
);

int checkAc3AlternateBitStreamInfoCompliance(
  const Ac3BitStreamInfoParameters * param
);

#endif
