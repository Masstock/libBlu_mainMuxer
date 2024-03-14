

#ifndef __LIBBLU_MUXER__CODECS__PGS_GENERATOR__PHASE_CORRELATION_H__
#define __LIBBLU_MUXER__CODECS__PGS_GENERATOR__PHASE_CORRELATION_H__

#include "../../../util.h"
#include "../common/hdmv_bitmap.h"

#include "pgs_error.h"

typedef double lb_complex[2];

typedef struct {
  void *img1_ffft_plan;  /**< img1 Forward FFT */
  void *img2_ffft_plan;  /**< img2 Forward FFT */
  void *out_bfft_plan;   /**< out Backward FFT */
} PgsPhaseCorrelationDimPlans;

/** \~english
 * \brief Number of FFTW plans per dimension.
 *
 * Plans are for dimensions 128, 256, 512, 1024, 2048 and 4096.
 */
#define PGS_PC_NB_DIM  6

#define PGS_PC_MIN_SIZE  128u
#define PGS_PC_MIN_SIZE_LOG2  7u
#define PGS_PC_MAX_SIZE  4096u

#define PGS_PC_NB_PLANS  (PGS_PC_NB_DIM*PGS_PC_NB_DIM)

typedef struct {
  lb_complex *img1;
  const void *loaded_bm1_ptr;
  lb_complex *img2;
  const void *loaded_bm2_ptr;
  lb_complex *out;

  PgsPhaseCorrelationDimPlans plans[PGS_PC_NB_PLANS];
  unsigned cur_w_log2_min7;
  unsigned cur_h_log2_min7;
} PgsPCContext;

void cleanPgsPCContext(
  PgsPCContext *ctx
);

int computeShiftPgsPCContext(
  PgsPCContext *ctx,
  const HdmvBitmap *bm1,
  const HdmvBitmap *bm2,
  int *delta_x,
  int *delta_y,
  double *peak
);

#endif
