/* backstab_pvrtc.c -- announce PVRTC and decode it to RGBA8 for Mesa. MIT. */
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl_layer.h"
#include "util.h"
#include "pvrtc_decode.h"

#define PV_RGB4  0x8C00
#define PV_RGB2  0x8C01
#define PV_RGBA4 0x8C02
#define PV_RGBA2 0x8C03

static void (*real_cti)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
static const GLubyte *(*real_gs)(GLenum);

static const GLubyte *w_gs(GLenum n) {
  static char buf[16384];
  const GLubyte *s = real_gs(n);
  if (n == GL_EXTENSIONS && s && !strstr((const char *)s, "GL_IMG_texture_compression_pvrtc")) {
    snprintf(buf, sizeof buf, "%s GL_IMG_texture_compression_pvrtc", (const char *)s);
    return (const GLubyte *)buf;
  }
  return s;
}

static void w_cti(GLenum target, GLint level, GLenum fmt, GLsizei w, GLsizei h,
                  GLint border, GLsizei size, const void *data) {
  if (fmt < PV_RGB4 || fmt > PV_RGBA2) {
    real_cti(target, level, fmt, w, h, border, size, data);
    return;
  }
  static int logged;
  if (!logged) { logged = 1; debugPrintf("[pvrtc] first upload fmt=0x%x %dx%d size=%d\n", fmt, w, h, size); }
  if (!data) {
    glTexImage2D(target, level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    return;
  }
  int pw = w < 8 ? 8 : w, ph = h < 8 ? 8 : h;
  uint8_t *full = malloc((size_t)pw * ph * 4);
  if (!full) return;
  if (fmt == PV_RGB4 || fmt == PV_RGBA4) {
    if (pvrtc4_decode((const uint8_t *)data, pw, ph, full)) { free(full); return; }
  } else {
    debugPrintf("[pvrtc] 2bpp not implemented (fmt 0x%x)\n", fmt);
    for (size_t i = 0; i < (size_t)pw * ph; i++) {
      full[i*4] = 255; full[i*4+1] = 0; full[i*4+2] = 255; full[i*4+3] = 255;
    }
  }
  if (pw != w || ph != h) {
    for (int y = 0; y < h; y++) memmove(full + (size_t)y * w * 4, full + (size_t)y * pw * 4, (size_t)w * 4);
  }
  glTexImage2D(target, level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, full);
  free(full);
}

uintptr_t pvrtc_wrap(const char *name, uintptr_t real) {
  if (real && !strcmp(name, "glCompressedTexImage2D")) {
    real_cti = (void (*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *))real;
    return (uintptr_t)w_cti;
  }
  if (real && !strcmp(name, "glGetString")) {
    real_gs = (const GLubyte *(*)(GLenum))real;
    return (uintptr_t)w_gs;
  }
  return 0;
}
