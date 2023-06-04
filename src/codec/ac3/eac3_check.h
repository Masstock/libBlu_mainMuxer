

#ifndef __LIBBLU_MUXER__CODECS__AC3__EAC3_CHECK_H__
#define __LIBBLU_MUXER__CODECS__AC3__EAC3_CHECK_H__

#include "ac3_core_check.h"
#include "ac3_data.h"
#include "ac3_error.h"

int checkEac3BitStreamInfoCompliance(
  const Eac3BitStreamInfoParameters * bsi
);

int checkChangeEac3BitStreamInfoCompliance(
  const Eac3BitStreamInfoParameters * old_bsi,
  const Eac3BitStreamInfoParameters * new_bsi
);

// static bool _constantEac3BitStreamInfoCheck(
//   const Eac3BitStreamInfoParameters * first,
//   const Eac3BitStreamInfoParameters * second
// )
// {
//   return CHECK(
//     EQUAL(->strmtyp)
//     EQUAL(->substreamid)
//     EQUAL(->frmsiz)
//     EQUAL(->fscod)
//     EQUAL(->fscod2)
//     EQUAL(->numblkscod)
//     EQUAL(->acmod)
//     EQUAL(->lfeon)
//     EQUAL(->bsid)
//     EQUAL(->chanmape)
//     START_COND(->chanmape, true)
//       EQUAL(->chanmap)
//     END_COND
//     EQUAL(->mixmdate)
//     START_COND(->strmtyp, 0x2)
//       EQUAL(->blkid)
//       EQUAL(->frmsizecod)
//     END_COND
//   );
// }

#endif