#ifndef PVRTC_DECODE_H
#define PVRTC_DECODE_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t r, g, b, a; } pv_c;

static uint32_t pv_twiddle(uint32_t x, uint32_t y, uint32_t bw, uint32_t bh) {
  uint32_t mn = bw < bh ? bw : bh;
  uint32_t rest = (bh < bw) ? x : y;
  uint32_t t = 0, src = 1, dst = 1, sh = 0;
  while (src < mn) {
    if (y & src) t |= dst;
    if (x & src) t |= dst << 1;
    src <<= 1; dst <<= 2; sh++;
  }
  return t | ((rest >> sh) << (2 * sh));
}
static int pv_e5(int v) { return (v << 3) | (v >> 2); }
static int pv_e3(int v) { return (v << 5) | (v << 2) | (v >> 1); }

/* low colour (bits 1..15 of word1): opaque 554, translucent 3443 */
static pv_c pv_colA(uint32_t d) {
  pv_c c;
  if (d & 0x8000) {
    int r = (d >> 10) & 31, g = (d >> 5) & 31, b = (d >> 1) & 15;
    b = (b << 1) | (b >> 3);
    c.r = pv_e5(r); c.g = pv_e5(g); c.b = pv_e5(b); c.a = 255;
  } else {
    int a = (d >> 12) & 7, r = (d >> 8) & 15, g = (d >> 4) & 15, b = (d >> 1) & 7;
    c.a = pv_e3(a); c.r = r * 17; c.g = g * 17; c.b = pv_e3(b);
  }
  return c;
}
/* high colour (bits 16..31 of word1): opaque 555, translucent 3444 */
static pv_c pv_colB(uint32_t d) {
  pv_c c;
  if (d & 0x8000) {
    c.r = pv_e5((d >> 10) & 31); c.g = pv_e5((d >> 5) & 31); c.b = pv_e5(d & 31); c.a = 255;
  } else {
    int a = (d >> 12) & 7;
    c.a = pv_e3(a); c.r = ((d >> 8) & 15) * 17; c.g = ((d >> 4) & 15) * 17; c.b = (d & 15) * 17;
  }
  return c;
}

/* w,h: pangkat dua, >= 8. dst: w*h*4 byte RGBA. return 0 kalau sukses */
static int pvrtc4_decode(const uint8_t *src, int w, int h, uint8_t *dst) {
  int bw = w >> 2, bh = h >> 2, n = bw * bh;
  pv_c *A = malloc(sizeof(pv_c) * n), *B = malloc(sizeof(pv_c) * n);
  uint32_t *M = malloc(4 * n);
  uint8_t *F = malloc(n);
  if (!A || !B || !M || !F) { free(A); free(B); free(M); free(F); return -1; }
  for (int by = 0; by < bh; by++)
    for (int bx = 0; bx < bw; bx++) {
      uint32_t i = pv_twiddle(bx, by, bw, bh), w0, w1;
      memcpy(&w0, src + i * 8, 4);
      memcpy(&w1, src + i * 8 + 4, 4);
      int k = by * bw + bx;
      M[k] = w0; F[k] = w1 & 1;
      A[k] = pv_colA(w1 & 0xfffe);
      B[k] = pv_colB(w1 >> 16);
    }
  static const int wt_n[4] = {0, 3, 5, 8}, wt_p[4] = {0, 4, 4, 8};
  for (int py = 0; py < h; py++)
    for (int px = 0; px < w; px++) {
      int bx = px >> 2, by = py >> 2, x = px & 3, y = py & 3;
      int ox = x >= 2 ? x - 2 : 2 - x, oy = y >= 2 ? y - 2 : 2 - y;
      int nbx = x < 2 ? (bx + bw - 1) % bw : (bx + 1) % bw;
      int nby = y < 2 ? (by + bh - 1) % bh : (by + 1) % bh;
      int xs[2] = {bx, nbx}, ys[2] = {by, nby};
      int wx[2] = {4 - ox, ox}, wy[2] = {4 - oy, oy};
      int as[4] = {0, 0, 0, 0}, bs[4] = {0, 0, 0, 0};
      for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
          int k = ys[j] * bw + xs[i], wgt = wx[i] * wy[j];
          as[0] += wgt * A[k].r; as[1] += wgt * A[k].g; as[2] += wgt * A[k].b; as[3] += wgt * A[k].a;
          bs[0] += wgt * B[k].r; bs[1] += wgt * B[k].g; bs[2] += wgt * B[k].b; bs[3] += wgt * B[k].a;
        }
      int k = by * bw + bx;
      int m = (M[k] >> (2 * (y * 4 + x))) & 3;
      int wt = F[k] ? wt_p[m] : wt_n[m];
      uint8_t *o = dst + (py * w + px) * 4;
      for (int c = 0; c < 4; c++) o[c] = (as[c] * (8 - wt) + bs[c] * wt + 64) >> 7;
      if (F[k] && m == 3) o[3] = 0;
    }
  free(A); free(B); free(M); free(F);
  return 0;
}
#endif
