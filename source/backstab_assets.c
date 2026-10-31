/* backstab_assets.c -- AAssetManager over the APK (whole-entry reads). MIT. */
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz/miniz.h>
#include "dcr_path.h"
#include "util.h"

typedef struct { uint8_t *data; size_t size, pos; } BAsset;
static mz_zip_archive g_zip;
static int g_zip_ok = -1;
static Mutex g_mx;

static int zip_open(void) {
  if (g_zip_ok < 0) {
    memset(&g_zip, 0, sizeof g_zip);
    g_zip_ok = mz_zip_reader_init_file(&g_zip, dcr_apk_path(), 0) ? 1 : 0;
    if (!g_zip_ok) debugPrintf("[assets] cannot open %s\n", dcr_apk_path());
  }
  return g_zip_ok;
}

void *b_AAssetManager_open(void *mgr, const char *name, int mode) {
  (void)mgr; (void)mode;
  char path[512];
  snprintf(path, sizeof path, "assets/%s", name);
  mutexLock(&g_mx);
  void *out = NULL;
  if (zip_open()) {
    size_t sz = 0;
    void *buf = mz_zip_reader_extract_file_to_heap(&g_zip, path, &sz, 0);
    if (buf) {
      BAsset *a = calloc(1, sizeof *a);
      a->data = buf; a->size = sz;
      out = a;
    }
  }
  mutexUnlock(&g_mx);
  if (!out) debugPrintf("[assets] open %s: not found\n", path);
  return out;
}

int b_AAsset_read(void *asset, void *buf, size_t n) {
  BAsset *a = asset;
  size_t left = a->size - a->pos;
  if (n > left) n = left;
  memcpy(buf, a->data + a->pos, n);
  a->pos += n;
  return (int)n;
}

long b_AAsset_seek(void *asset, long off, int whence) {
  BAsset *a = asset;
  long base = whence == 1 ? (long)a->pos : whence == 2 ? (long)a->size : 0;
  long p = base + off;
  if (p < 0 || (size_t)p > a->size) return -1;
  a->pos = (size_t)p;
  return p;
}

void b_AAsset_close(void *asset) {
  BAsset *a = asset;
  if (!a) return;
  mz_free(a->data);
  free(a);
}
