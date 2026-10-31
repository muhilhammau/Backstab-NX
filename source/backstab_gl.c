/* backstab_gl.c -- the EGL context the engine draws into (the game imports no
 * eglCreateContext: Java's GLSurfaceView made it). One window surface, one
 * GLES 2 context, current on the calling thread. The game swaps itself. MIT. */
#include <EGL/egl.h>
#include <switch.h>

#include "util.h"

EGLDisplay b_eglGetDisplay(EGLNativeDisplayType d);
EGLBoolean b_eglInitialize(EGLDisplay d, EGLint *maj, EGLint *min);
EGLBoolean b_eglChooseConfig(EGLDisplay d, const EGLint *attrs, EGLConfig *cfgs, EGLint cap, EGLint *num);
EGLContext b_eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, const EGLint *attrs);
EGLSurface b_eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *attrs);
EGLBoolean b_eglMakeCurrent(EGLDisplay d, EGLSurface dr, EGLSurface rd, EGLContext c);
EGLBoolean b_eglSwapInterval(EGLDisplay d, EGLint i);
EGLint b_eglGetError(void);

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLConfig g_cfg;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;

int backstab_egl_ensure(void) {
  if (g_ctx == EGL_NO_CONTEXT) {
    g_dpy = b_eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj = 0, min = 0, n = 0;
    if (!b_eglInitialize(g_dpy, &maj, &min)) {
      debugPrintf("[gl] eglInitialize failed 0x%x\n", b_eglGetError());
      return 0;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint attrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                      EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
    if (!b_eglChooseConfig(g_dpy, attrs, &g_cfg, 1, &n) || n < 1) {
      debugPrintf("[gl] no EGL config (0x%x)\n", b_eglGetError());
      return 0;
    }
    g_surf = b_eglCreateWindowSurface(g_dpy, g_cfg, 0, NULL);
    static const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    g_ctx = b_eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctx_attrs);
    if (g_surf == EGL_NO_SURFACE || g_ctx == EGL_NO_CONTEXT) {
      debugPrintf("[gl] surface %p / context %p not made (0x%x)\n", g_surf, g_ctx, b_eglGetError());
      g_ctx = EGL_NO_CONTEXT;
      return 0;
    }
    b_eglSwapInterval(g_dpy, 1);
    debugPrintf("[gl] EGL %d.%d: window surface, GLES 2 context\n", (int)maj, (int)min);
  }
  if (eglGetCurrentContext() != g_ctx)
    return b_eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx) == EGL_TRUE;
  return 1;
}
