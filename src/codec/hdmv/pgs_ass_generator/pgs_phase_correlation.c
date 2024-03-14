#include <stdio.h>
#include <stdlib.h>

#include <fftw3.h>
#include <math.h>

#include "pgs_phase_correlation.h"

static inline double _squared_norm(
  fftw_complex x
)
{
  return x[0] * x[0] + x[1] * x[1];
}

static inline double _norm(
  fftw_complex x
)
{
  return sqrt(_squared_norm(x));
}

static int _initPgsPCContext(
  PgsPCContext *ctx
)
{
  fftw_complex *img1 = fftw_alloc_complex(PGS_PC_MAX_SIZE *PGS_PC_MAX_SIZE);
  fftw_complex *img2 = fftw_alloc_complex(PGS_PC_MAX_SIZE *PGS_PC_MAX_SIZE);
  fftw_complex *out  = fftw_alloc_complex(PGS_PC_MAX_SIZE *PGS_PC_MAX_SIZE);
  if (NULL == img1 || NULL == img2 || NULL == out) {
    fftw_free(img1);
    fftw_free(img2);
    fftw_free(out);
    LIBBLU_HDMV_PGS_ASS_ERROR_RETURN("Memory allocation error.\n");
  }

  ctx->img1 = img1;
  ctx->img2 = img2;
  ctx->out  = out;
  return 0;
}

void cleanPgsPCContext(
  PgsPCContext *ctx
)
{
  fftw_free(ctx->img1);
  fftw_free(ctx->img2);
  fftw_free(ctx->out);
  for (unsigned i = 0; i < PGS_PC_NB_PLANS; i++) {
    PgsPhaseCorrelationDimPlans *plans = &ctx->plans[i];
    if (NULL != plans->img1_ffft_plan) {
      fftw_destroy_plan(plans->img1_ffft_plan);
      fftw_destroy_plan(plans->img2_ffft_plan);
      fftw_destroy_plan(plans->out_bfft_plan);
    }
  }
  fftw_cleanup();
}

static PgsPhaseCorrelationDimPlans * _getPlansPgsPCContext(
  PgsPCContext *ctx
)
{
  unsigned j = ctx->cur_w_log2_min7;
  unsigned i = ctx->cur_h_log2_min7;
  return &ctx->plans[j*PGS_PC_NB_DIM+i];
}

// #define LB_FFTW_MODE  FFTW_MEASURE
#define LB_FFTW_MODE  FFTW_ESTIMATE

static int _checkPlansPgsPCContext(
  PgsPCContext *ctx
)
{
  PgsPhaseCorrelationDimPlans *plans = _getPlansPgsPCContext(ctx);

  if (NULL == plans->img1_ffft_plan) {
    /* Create FFTW plans */
    unsigned w = PGS_PC_MIN_SIZE << ctx->cur_w_log2_min7;
    unsigned h = PGS_PC_MIN_SIZE << ctx->cur_h_log2_min7;
    fftw_plan img1_plan = fftw_plan_dft_2d(h, w, ctx->img1, ctx->img1, FFTW_FORWARD, LB_FFTW_MODE);
    fftw_plan img2_plan = fftw_plan_dft_2d(h, w, ctx->img2, ctx->img2, FFTW_FORWARD, LB_FFTW_MODE);
    fftw_plan out_plan  = fftw_plan_dft_2d(h, w, ctx->out,  ctx->out, FFTW_BACKWARD, LB_FFTW_MODE);
    if (NULL == img1_plan || NULL == img2_plan || NULL == out_plan)
      LIBBLU_HDMV_PGS_ASS_ERROR_RETURN("Unable to create plan\n");
    plans->img1_ffft_plan = img1_plan;
    plans->img2_ffft_plan = img2_plan;
    plans->out_bfft_plan  = out_plan;
  }

  return 0;
}

static double _getGrayFromRgba(
  uint32_t c
)
{
  /* Apply BT.601 luma matrix */
  double luma = 0.299 * getR_RGBA(c) + 0.587 * getG_RGBA(c) + 0.0114 * getA_RGBA(c);
  return luma * (getA_RGBA(c) / 255.); // Apply transparency
}

static void _fillReal(
  const PgsPCContext *ctx,
  fftw_complex *dst,
  const HdmvBitmap *bitmap
)
{
  unsigned dst_width  = (PGS_PC_MIN_SIZE << ctx->cur_w_log2_min7);
  unsigned dst_height = (PGS_PC_MIN_SIZE << ctx->cur_h_log2_min7);
  unsigned src_width  = bitmap->width;
  unsigned src_height = bitmap->height;

  for (unsigned j = 0; j < dst_height; j++) {
    for (unsigned i = 0; i < dst_width; i++) {
      unsigned x = MIN(i, src_width - 1);
      unsigned y = MIN(j, src_height - 1);
      uint32_t rgba = bitmap->rgba[y *src_width + x];
      dst[j *dst_width + i][0] = _getGrayFromRgba(rgba);
    }
  }

  fprintf(stderr, "Done\n");
}

static int _initInputsPgsPCContext(
  PgsPCContext *ctx,
  const HdmvBitmap *bm1,
  const HdmvBitmap *bm2
)
{
  unsigned w_log2 = MAX(PGS_PC_MIN_SIZE_LOG2, lb_fast_log2_32(MAX(bm1->width, bm2->width)));
  unsigned h_log2 = MAX(PGS_PC_MIN_SIZE_LOG2, lb_fast_log2_32(MAX(bm1->height, bm2->height)));
  ctx->cur_w_log2_min7 = w_log2 - PGS_PC_MIN_SIZE_LOG2;
  ctx->cur_h_log2_min7 = h_log2 - PGS_PC_MIN_SIZE_LOG2;

  /* Check if FFT buffers are allocated (and do if not) */
  if (NULL == ctx->img1) {
    if (_initPgsPCContext(ctx) < 0)
      return -1;
  }

  /* Check or init if required FFTW plans */
  if (_checkPlansPgsPCContext(ctx) < 0)
    return -1;

  /* Fill input buffers with images */
  _fillReal(ctx, ctx->img1, bm1);
  ctx->loaded_bm1_ptr = bm1;
  _fillReal(ctx, ctx->img2, bm2);
  ctx->loaded_bm2_ptr = bm2;

  return 0;
}

static int _computeShift(
  PgsPCContext *ctx,
  int *delta_x,
  int *delta_y,
  double *peak
)
{
  PgsPhaseCorrelationDimPlans *plans = _getPlansPgsPCContext(ctx);
  fftw_complex *img1 = ctx->img1;
  fftw_complex *img2 = ctx->img2;
  fftw_complex *out = ctx->out;
  unsigned W = PGS_PC_MIN_SIZE << ctx->cur_w_log2_min7;
  unsigned H = PGS_PC_MIN_SIZE << ctx->cur_h_log2_min7;

  assert(NULL != plans);

  fprintf(stderr, "Perform 2D FFT on each image\n");
  fftw_execute(plans->img1_ffft_plan);
  fftw_execute(plans->img2_ffft_plan);

  fprintf(stderr, "Compute normalized cross power spectrum\n");
  for (size_t i = 0; i < W*H; i++) {
    out[i][0] = ( img1[i][0] * img2[i][0]) - (-img1[i][1] * img2[i][1]);
    out[i][1] = (-img1[i][1] * img2[i][0]) + ( img1[i][0] * img2[i][1]);
    double norm = _norm(out[i]);
    out[i][0] /= norm;
    out[i][1] /= norm;
  }

  fprintf(stderr, "Perform inverse 2D FFT on obtained matrix\n");
  fftw_execute(plans->out_bfft_plan);

  fprintf(stderr, "Find peak\n");
  size_t offset = 0;
  double max = 0.0;
  int x = 0, y = 0;
  for (unsigned j = 0; j < H; j++) {
    for (unsigned i = 0; i < W; i++, offset++) {
      double d = _squared_norm(out[offset]);
      if (max < d) {
        max = d;
        x = i;
        y = j;
      }
    }
  }

  if (x > ((int) W >> 1))
    x -= (int) W;
  if (y > ((int) H >> 1))
    y -= (int) H;

  *delta_x = x;
  *delta_y = y;
  *peak = max;

  return 0;
}

int computeShiftPgsPCContext(
  PgsPCContext *ctx,
  const HdmvBitmap *bm1,
  const HdmvBitmap *bm2,
  int *delta_x,
  int *delta_y,
  double *peak
)
{
  if (_initInputsPgsPCContext(ctx, bm1, bm2) < 0)
    return -1;
  return _computeShift(ctx, delta_x, delta_y, peak);
}