/* backstab_glslog.c -- logs Mesa shader errors; if a shader fails, retries with
 * texture2D() on samplerCube uniforms rewritten to textureCube(). MIT. */
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gl_layer.h"
#include "util.h"

#define MAXSH 512
static void (*real_compile)(GLuint);
static void (*real_source)(GLuint, GLsizei, const GLchar *const *, const GLint *);
static char *g_orig[MAXSH];
static char *g_fix[MAXSH];
static int g_logged;

static int is_id(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

static char *fix_cube(const char *src, int *nfix) {
  char names[16][48];
  int nn = 0;
  const char *p;
  *nfix = 0;
  for (p = src; (p = strstr(p, "samplerCube")) != NULL; p += 11) {
    const char *q = p + 11;
    if (p > src && is_id(p[-1])) continue;
    if (is_id(*q)) continue;
    while (*q == ' ' || *q == '\t') q++;
    int n = 0;
    while (is_id(q[n]) && n < 47) n++;
    if (n && nn < 16) { memcpy(names[nn], q, n); names[nn][n] = 0; nn++; }
  }
  size_t len = strlen(src);
  char *out = malloc(len + 2 * (len / 9) + 1);
  if (!out) return NULL;
  if (!nn) { memcpy(out, src, len + 1); return out; }
  char *o = out;
  p = src;
  while (*p) {
    int hit = 0;
    if (!strncmp(p, "texture2D", 9) && (p == src || !is_id(p[-1]))) {
      const char *q = p + 9;
      while (*q == ' ' || *q == '\t') q++;
      if (*q == '(') {
        q++;
        while (*q == ' ' || *q == '\t') q++;
        int n = 0;
        while (is_id(q[n])) n++;
        for (int i = 0; i < nn && !hit; i++)
          if ((int)strlen(names[i]) == n && !strncmp(names[i], q, n)) hit = 1;
      }
    }
    if (hit) { memcpy(o, "textureCube", 11); o += 11; p += 9; (*nfix)++; }
    else *o++ = *p++;
  }
  *o = 0;
  return out;
}

static void w_source(GLuint sh, GLsizei count, const GLchar *const *str,
                     const GLint *len) {
  size_t total = 0, o = 0;
  for (GLsizei i = 0; i < count; i++)
    if (str[i]) total += (len && len[i] >= 0) ? (size_t)len[i] : strlen(str[i]);
  char *cat = malloc(total + 1);
  if (!cat) { real_source(sh, count, str, len); return; }
  for (GLsizei i = 0; i < count; i++) {
    if (!str[i]) continue;
    size_t n = (len && len[i] >= 0) ? (size_t)len[i] : strlen(str[i]);
    memcpy(cat + o, str[i], n);
    o += n;
  }
  cat[o] = 0;
  int nfix = 0;
  char *fixed = fix_cube(cat, &nfix);
  if (fixed && !nfix) { free(fixed); fixed = NULL; }
  const GLchar *one = cat;
  real_source(sh, 1, &one, NULL);
  if (sh < MAXSH) {
    free(g_orig[sh]); free(g_fix[sh]);
    g_orig[sh] = cat; g_fix[sh] = fixed;
  } else { free(cat); free(fixed); }
}

static int try_compile(GLuint sh, char *log, int cap) {
  GLint ok = 0;
  GLsizei n = 0;
  real_compile(sh);
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (ok) return 1;
  glGetShaderInfoLog(sh, cap - 1, &n, log);
  log[n < 0 ? 0 : n] = 0;
  return 0;
}

static void w_compile(GLuint sh) {
  char l1[1024], l2[1024];
  l1[0] = l2[0] = 0;
  if (try_compile(sh, l1, sizeof l1)) return;
  if (sh < MAXSH && g_fix[sh]) {
    const GLchar *one = g_fix[sh];
    real_source(sh, 1, &one, NULL);
    if (try_compile(sh, l2, sizeof l2)) {
      debugPrintf("[glsl] shader %u: original failed, samplerCube fix OK\n", sh);
      return;
    }
    one = g_orig[sh];
    real_source(sh, 1, &one, NULL);
    real_compile(sh);
  }
  if (g_logged++ < 12) {
    debugPrintf("[glsl] shader %u failed, original:\n%s\n", sh, l1);
    if (l2[0]) debugPrintf("[glsl] shader %u failed, with fix:\n%s\n", sh, l2);
  }
  log_flush_ring();
}

uintptr_t pvrtc_wrap(const char *name, uintptr_t real);
uintptr_t port_gl_wrap(const char *name, uintptr_t real) {
  if (real && !strcmp(name, "glCompileShader")) {
    real_compile = (void (*)(GLuint))real;
    return (uintptr_t)w_compile;
  }
  if (real && !strcmp(name, "glShaderSource")) {
    real_source = (void (*)(GLuint, GLsizei, const GLchar *const *, const GLint *))real;
    return (uintptr_t)w_source;
  }
  return pvrtc_wrap(name, real);
}
