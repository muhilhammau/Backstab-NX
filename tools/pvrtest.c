#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pvrtc_decode.h"
int main(int argc, char **argv) {
  if (argc < 6) { puts("usage: gla offset w h out.bmp"); return 1; }
  FILE *f = fopen(argv[1], "rb");
  long off = atol(argv[2]); int w = atoi(argv[3]), h = atoi(argv[4]);
  size_t sz = (size_t)w * h / 2;
  uint8_t *src = malloc(sz), *px = malloc((size_t)w * h * 4);
  fseek(f, off + 52, SEEK_SET);
  if (fread(src, 1, sz, f) != sz) { puts("short read"); return 1; }
  if (pvrtc4_decode(src, w, h, px)) { puts("decode fail"); return 1; }
  FILE *o = fopen(argv[5], "wb");
  uint8_t hd[54] = {0}; uint32_t fs = 54 + w * h * 4, off2 = 54, hs = 40;
  int32_t ww = w, hh = -h;
  hd[0] = 'B'; hd[1] = 'M';
  memcpy(hd + 2, &fs, 4); memcpy(hd + 10, &off2, 4); memcpy(hd + 14, &hs, 4);
  memcpy(hd + 18, &ww, 4); memcpy(hd + 22, &hh, 4); hd[26] = 1; hd[28] = 32;
  fwrite(hd, 1, 54, o);
  for (long i = 0; i < (long)w * h; i++) {
    uint8_t p[4] = {px[i*4+2], px[i*4+1], px[i*4+0], 255};
    fwrite(p, 1, 4, o);
  }
  fclose(o);
  puts("ok");
  return 0;
}
