/* backstab_boot.c -- Backstab's part of the boot: setup plan, port_load, port_run. MIT. */
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "jni.h"
#include "rt_settings.h"
#include "rt_applet.h"
#include "rt_pad.h"
#include "so_util.h"
#include "util.h"

int backstab_load_module(void);
void backstab_run_constructors(void);
int backstab_egl_ensure(void);
void backstab_egl_swap(void);
extern so_module g_mod_game;

#define SD "com/gameloft/android/ANMP/GloftSDHM"
#define SDSYM "Java_com_gameloft_android_ANMP_GloftSDHM_"

static const char *const k_libs[] = {"libPirates.so"};

const RtSetupPlan port_setup_plan = {
    .libs = k_libs,
    .nlibs = sizeof k_libs / sizeof k_libs[0],
    .libs_what = "Unpacking the game's library",
    .apk_requirement = "This port needs the 32-bit ARM (armeabi-v7a) build of Backstab\n"
                       "(com.gameloft.android.ANMP.GloftSDHM): use an APK of that build.",
    .libs_p0 = 50,
    .libs_p1 = 800,
    .classes_p0 = 800,
    .classes_p1 = 950,
};

int port_load(const char *apk) {
  dcr_setup_from_apk(apk);
  if (backstab_load_module() != 0)
    fatal_error("Could not load the game library from %s.\n\n"
                "It is unpacked from the APK on launch: delete libPirates.so and\n"
                ".setup there to unpack it again.", dcr_game_root());
  return 0;
}

typedef jint (*fn_onload)(void *vm, void *reserved);

/* The state GDRMPolicy.nativeAllow leaves behind on its "allowed" path: the
 * guard variable (GOT slot 0x6ec568) holds a pointer to an int 1. Java's
 * installer used to get there; nativeStart exits without it. */
static void drm_allowed_outcome(void) {
  int ***slot = (int ***)((uintptr_t)g_mod_game.load_virtbase + 0x6ec568);
  int **guard = *slot;
  int *one = (int *)malloc(sizeof *one);
  *one = 1;
  debugPrintf("[drm] guard variable %p held %p, now %p\n", (void *)guard, (void *)*guard, (void *)one);
  *guard = one;
  {
    static const unsigned lp[] = {0x706cdc, 0x706ce0, 0x706cfc, 0x706d00};
    for (unsigned i = 0; i < 4; i++) {
      int **v = (int **)((uintptr_t)g_mod_game.load_virtbase + lp[i]);
      if (*v == NULL) {
        int *o = (int *)malloc(sizeof *o);
        *o = 1;
        *v = o;
      }
      debugPrintf("[drm] lockPointer @+0x%x -> %p\n", lp[i], (void *)*v);
    }
  }
  log_flush_ring();
}

/* A static nativeX(JNIEnv*, jclass) with no other arguments. */
static void native_call0(const char *sym, const char *cls) {
  void (*fn)(void *, void *) = (void (*)(void *, void *))so_try_find_addr_rx(&g_mod_game, sym);
  if (!fn) {
    debugPrintf("[boot] %s: MISSING\n", sym);
    return;
  }
  debugPrintf("[boot] calling %s\n", sym);
  log_flush_ring();
  fn(g_jni_env, jni_class(cls)->obj);
  debugPrintf("[boot] %s returned\n", sym);
  log_flush_ring();
}

/* Second milestone: JNI_OnLoad, the GLES 2 context, then what Java's installer
 * did before the game activity. Every Java call is logged ([debug]
 * log_java_calls). Game / GameRenderer init come next, once their argument
 * lists are read from the library. */
static void engine_print(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt ? fmt : "", ap);
  va_end(ap);
  debugPrintf("[engine] %s\n", buf);
}
static void engine_log(const char *msg, int level) {
  debugPrintf("[engine:log%d] %s\n", level, msg ? msg : "(null)");
}
static void engine_logf1(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt ? fmt : "", ap);
  va_end(ap);
  debugPrintf("[engine:logf] %s\n", buf);
}
static void engine_logf2(int level, const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt ? fmt : "", ap);
  va_end(ap);
  debugPrintf("[engine:logf%d] %s\n", level, buf);
}
static void dump_flags(const char *tag) {
  static const struct { const char *n; unsigned off; } g[] = {
      {"inited", 0x6ed8f8}, {"app", 0x6ec0f8}, {"flag13a4", 0x6ec454}, {"counter13f8", 0x6ec4a8},
      {"resume2294", 0x6ed344}, {"device", 0x6ec19c}, {"var2018", 0x6ed0c8}};
  for (unsigned i = 0; i < sizeof g / sizeof g[0]; i++) {
    unsigned *p = *(unsigned **)((uintptr_t)g_mod_game.load_virtbase + g[i].off);
    debugPrintf("[flags] %s: %s = 0x%x (at %p)\n", tag, g[i].n, p ? *p : 0xdeadbeef, (void *)p);
  }
  log_flush_ring();
}
static void track_app(unsigned long frame) {
  static int lp = -1, li = -9999, n = 0;
  unsigned *pp = *(unsigned **)((uintptr_t)g_mod_game.load_virtbase + 0x6ec0f8);
  if (!pp || !*pp || n >= 80) return;
  uintptr_t app = *pp;
  int ph = *(volatile unsigned char *)(app + 0x58);
  int ix = *(volatile int *)(app + 0x38);
  if (ph != lp || ix != li) {
    debugPrintf("[app] frame %lu: phase(+0x58)=%d stackIndex(+0x38)=%d\n", frame, ph, ix);
    log_flush_ring();
    lp = ph; li = ix; n++;
  }
}
static void touch_poll(void) {
  static int inited = 0;
  static void (*fn)(void *, void *, int, int, int, int);
  static struct { u32 id; int x, y, on; } sl[2];
  static int logged = 0;
  if (!inited) {
    hidInitializeTouchScreen();
    fn = (void (*)(void *, void *, int, int, int, int))so_try_find_addr_rx(&g_mod_game, SDSYM "GameGLSurfaceView_nativeOnTouch");
    debugPrintf("[touch] nativeOnTouch %s\n", fn ? "found" : "MISSING");
    inited = 1;
  }
  if (!fn) return;
  HidTouchScreenState st;
  memset(&st, 0, sizeof st);
  if (!hidGetTouchScreenStates(&st, 1)) return;
  void *obj = jni_class(SD "/GameGLSurfaceView")->obj;
  int seen[2] = {0, 0};
  for (int i = 0; i < (int)st.count; i++) {
    int x = (int)(st.touches[i].x * 1.5f), y = (int)(st.touches[i].y * 1.5f);
    u32 fid = st.touches[i].finger_id;
    int k = -1;
    for (int j = 0; j < 2; j++) if (sl[j].on && sl[j].id == fid) k = j;
    int act = 2;
    if (k < 0) {
      for (int j = 0; j < 2; j++) if (!sl[j].on) { k = j; break; }
      if (k < 0) continue;
      sl[k].on = 1; sl[k].id = fid; act = 1;
    } else if (sl[k].x == x && sl[k].y == y) { seen[k] = 1; continue; }
    sl[k].x = x; sl[k].y = y; seen[k] = 1;
    if (logged < 20) { debugPrintf("[touch] act %d x %d y %d idx %d\n", act, x, y, k); logged++; }
    fn(g_jni_env, obj, act, x, y, k);
  }
  for (int j = 0; j < 2; j++)
    if (sl[j].on && !seen[j]) { fn(g_jni_env, obj, 0, sl[j].x, sl[j].y, j); sl[j].on = 0; }
}
static void key_poll(PadState *p0, PadState *p1) {
  static int inited = 0, logged = 0;
  static unsigned prev = 0;
  static void (*kd)(void *, void *, int, int);
  static void (*ku)(void *, void *, int, int);
  if (!inited) {
    kd = (void (*)(void *, void *, int, int))so_try_find_addr_rx(&g_mod_game, SDSYM "Game_nativeOnKeyDown");
    ku = (void (*)(void *, void *, int, int))so_try_find_addr_rx(&g_mod_game, SDSYM "Game_nativeOnKeyUp");
    debugPrintf("[key] nativeOnKeyDown %s, nativeOnKeyUp %s\n", kd ? "found" : "MISSING", ku ? "found" : "MISSING");
    inited = 1;
  }
  if (!kd || !ku) return;
  float s0[4] = {0, 0, 0, 0}, s1[4] = {0, 0, 0, 0};
  u64 b = rt_pad_read(p0, s0) | rt_pad_read(p1, s1);
  float sx = s0[0] != 0 ? s0[0] : s1[0], sy = s0[1] != 0 ? s0[1] : s1[1];
  static const struct { u64 btn; int key; } map[] = {
    {HidNpadButton_Up, 19}, {HidNpadButton_Down, 20}, {HidNpadButton_Left, 21}, {HidNpadButton_Right, 22},
    {HidNpadButton_A, 23}, {HidNpadButton_B, 4}, {HidNpadButton_X, 99}, {HidNpadButton_Y, 100},
    {HidNpadButton_L, 102}, {HidNpadButton_R, 103}, {HidNpadButton_Plus, 108}, {HidNpadButton_Minus, 82}};
  unsigned cur = 0;
  for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) if (b & map[i].btn) cur |= 1u << i;
  if (sy > 0.5f) cur |= 1u << 0;
  if (sy < -0.5f) cur |= 1u << 1;
  if (sx < -0.5f) cur |= 1u << 2;
  if (sx > 0.5f) cur |= 1u << 3;
  void *obj = jni_class(SD "/Game")->obj;
  for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) {
    unsigned m = 1u << i;
    if ((cur & m) == (prev & m)) continue;
    int down = (cur & m) != 0;
    if (logged < 40) { debugPrintf("[key] %s key %d\n", down ? "down" : "up", map[i].key); logged++; }
    (down ? kd : ku)(g_jni_env, obj, map[i].key, 0);
  }
  prev = cur;
}static void pad_touch(PadState *p0, PadState *p1) {
  static int inited = 0, sdown = 0, bdown = 0, lx = 0, ly = 0, logged = 0;
  static void (*fn)(void *, void *, int, int, int, int);
  if (!inited) {
    fn = (void (*)(void *, void *, int, int, int, int))so_try_find_addr_rx(&g_mod_game, SDSYM "GameGLSurfaceView_nativeOnTouch");
    inited = 1;
  }
  if (!fn) return;
  float s0[4] = {0, 0, 0, 0}, s1[4] = {0, 0, 0, 0};
  u64 b = rt_pad_read(p0, s0) | rt_pad_read(p1, s1);
  float sx = s0[0] != 0 ? s0[0] : s1[0], sy = s0[1] != 0 ? s0[1] : s1[1];
  if (b & HidNpadButton_Left) sx = -1.0f;
  if (b & HidNpadButton_Right) sx = 1.0f;
  if (b & HidNpadButton_Up) sy = 1.0f;
  if (b & HidNpadButton_Down) sy = -1.0f;
  float mg = __builtin_sqrtf(sx * sx + sy * sy); if (mg > 1.0f) { sx /= mg; sy /= mg; } int active = (sx * sx + sy * sy) > 0.16f;
  void *obj = jni_class(SD "/GameGLSurfaceView")->obj;
  const int CX = 228, CY = 770, R = 220, BX = 1725, BY = 960;
  if (active) {
    int x = CX + (int)(sx * R), y = CY - (int)(sy * R); if (x < 8) x = 8; if (y < 8) y = 8; if (x > 1912) x = 1912; if (y > 1072) y = 1072;
    if (!sdown) {
      fn(g_jni_env, obj, 1, CX, CY, 2);
      sdown = 1; lx = CX; ly = CY;
      if (logged < 6) { debugPrintf("[padtouch] stick down\n"); logged++; }
    }
    if (x != lx || y != ly) { fn(g_jni_env, obj, 2, x, y, 2); lx = x; ly = y; }
  } else if (sdown) {
    fn(g_jni_env, obj, 0, lx, ly, 2);
    sdown = 0;
    if (logged < 6) { debugPrintf("[padtouch] stick up\n"); logged++; }
  }
  { static int b2down_; int w = (b & (HidNpadButton_B | HidNpadButton_X)) != 0; if (w && !b2down_) { fn(g_jni_env, obj, 1, 1540, 757, 4); b2down_ = 1; } else if (!w && b2down_) { fn(g_jni_env, obj, 0, 1540, 757, 4); b2down_ = 0; } }
  int atk = (b & (HidNpadButton_A | HidNpadButton_R)) != 0;
  if (atk && !bdown) {
    fn(g_jni_env, obj, 1, BX, BY, 3); bdown = 1;
    if (logged < 6) { debugPrintf("[padtouch] attack down\n"); logged++; }
  } else if (!atk && bdown) {
    fn(g_jni_env, obj, 0, BX, BY, 3); bdown = 0;
  }
}static void pad_extra(PadState *p0, PadState *p1) {
  static int inited = 0, zdown = 0, camdown = 0, cx = 1400, cy = 400, logged = 0;
  static void (*fn)(void *, void *, int, int, int, int);
  if (!inited) {
    fn = (void (*)(void *, void *, int, int, int, int))so_try_find_addr_rx(&g_mod_game, SDSYM "GameGLSurfaceView_nativeOnTouch");
    inited = 1;
  }
  if (!fn) return;
  float s0[4] = {0, 0, 0, 0}, s1[4] = {0, 0, 0, 0};
  u64 b = rt_pad_read(p0, s0) | rt_pad_read(p1, s1);
  float rx = s0[2] != 0 ? s0[2] : s1[2], ry = s0[3] != 0 ? s0[3] : s1[3];
  void *obj = jni_class(SD "/GameGLSurfaceView")->obj;
  int z = (b & (HidNpadButton_ZL | HidNpadButton_ZR)) != 0;
  if (z && !zdown) {
    fn(g_jni_env, obj, 1, 1440, 960, 5); zdown = 1;
    if (logged < 6) { debugPrintf("[padtouch] aim/shoot down\n"); logged++; }
  } else if (!z && zdown) {
    fn(g_jni_env, obj, 0, 1440, 960, 5); zdown = 0;
  }
  float mg = __builtin_sqrtf(rx * rx + ry * ry);
  if (mg > 0.2f) {
    if (!camdown) {
      cx = 1400; cy = 400;
      fn(g_jni_env, obj, 1, cx, cy, 6); camdown = 1;
      if (logged < 6) { debugPrintf("[padtouch] camera down\n"); logged++; }
    }
    cx -= (int)(rx * 22.0f); cy += (int)(ry * 22.0f);
    int out = (cx < 1000 || cx > 1850 || cy < 150 || cy > 650);
    if (cx < 1000) cx = 1000; if (cx > 1850) cx = 1850;
    if (cy < 150) cy = 150; if (cy > 650) cy = 650;
    if (out) { fn(g_jni_env, obj, 0, cx, cy, 6); camdown = 0; }
    else fn(g_jni_env, obj, 2, cx, cy, 6);
  } else if (camdown) {
    fn(g_jni_env, obj, 0, cx, cy, 6); camdown = 0;
  }
}

#define CAM_INVERT_Y 1
#define CAM_SENS 0.35f
#define LS_X 242
#define LS_Y 850
#define LS_R 154
#define RS_X 1324
#define RS_Y 474
#define RS_R 84

static void pad_poll(PadState *p0, PadState *p1) {
  static void (*fn)(void *, void *, int, int, int, int);
  static int inited = 0, lon = 0, ron = 0, lx0 = -1, ly0 = -1, rx0 = -1, ry0 = -1, logged = 0;
  static u64 prev = 0;
  static const struct { u64 btn; int x, y, id; } map[] = {
      {HidNpadButton_B, 1540, 762, 21},  /* lompat */
      {HidNpadButton_X, 1760, 630, 22},  /* meriam */
      {HidNpadButton_Y, 1724, 972, 23},  /* pedang */
      {HidNpadButton_ZL, 1440, 970, 24}, /* bidik */
      {HidNpadButton_L, 1440, 970, 24},
      {HidNpadButton_A, 1725, 960, 25},  /* serang */
      {HidNpadButton_R, 1725, 960, 25},
  };
  if (!inited) {
    fn = (void (*)(void *, void *, int, int, int, int))so_try_find_addr_rx(&g_mod_game, SDSYM "GameGLSurfaceView_nativeOnTouch");
    inited = 1;
  }
  if (!fn) return;
  void *obj = jni_class(SD "/GameGLSurfaceView")->obj;
  u64 held = padGetButtons(p0) | padGetButtons(p1);
  u64 down = held & ~prev, up = ~held & prev;
  for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) {
    if (down & map[i].btn) {
      if (logged < 30) { debugPrintf("[pad] down id %d\n", map[i].id); logged++; }
      fn(g_jni_env, obj, 1, map[i].x, map[i].y, map[i].id);
    }
    if (up & map[i].btn) fn(g_jni_env, obj, 0, map[i].x, map[i].y, map[i].id);
  }
  prev = held;

  HidAnalogStickState a = padGetStickPos(p0, 0), b = padGetStickPos(p1, 0);
  if (abs(b.x) + abs(b.y) > abs(a.x) + abs(a.y)) a = b;
  float lx = a.x / 32767.0f, ly = a.y / 32767.0f;
  float lm = sqrtf(lx * lx + ly * ly);
  if (lm > 0.2f) {
    if (lm > 1.0f) { lx /= lm; ly /= lm; }
    int tx = LS_X + (int)(lx * LS_R), ty = LS_Y - (int)(ly * LS_R);
    if (!lon) { fn(g_jni_env, obj, 1, LS_X, LS_Y, 11); lon = 1; }
    if (tx != lx0 || ty != ly0) { fn(g_jni_env, obj, 2, tx, ty, 11); lx0 = tx; ly0 = ty; }
  } else if (lon) {
    fn(g_jni_env, obj, 0, lx0, ly0, 11); lon = 0; lx0 = ly0 = -1;
  }

  HidAnalogStickState c = padGetStickPos(p0, 1), d = padGetStickPos(p1, 1);
  if (abs(d.x) + abs(d.y) > abs(c.x) + abs(c.y)) c = d;
  float rx = c.x / 32767.0f, ry = c.y / 32767.0f;
  float rm = sqrtf(rx * rx + ry * ry);
  if (ron) { fn(g_jni_env, obj, 0, rx0, ry0, 12); ron = 0; }
  if (rm > 0.2f) {
    if (rm > 1.0f) { rx /= rm; ry /= rm; }
    float sy = CAM_INVERT_Y ? ry : -ry;
    rx0 = RS_X + (int)(rx * CAM_SENS * RS_R);
    ry0 = RS_Y + (int)(sy * CAM_SENS * RS_R);
    fn(g_jni_env, obj, 1, RS_X, RS_Y, 12);
    fn(g_jni_env, obj, 2, rx0, ry0, 12);
    ron = 1;
  }
}
void port_run(void) {
  jni_init();
  backstab_run_constructors();
  {
    void *pp = (void *)so_try_find_addr_rx(&g_mod_game, "_ZN6glitch2os7Printer5printEPKcz");
    if (pp) {
      hook_arm((uintptr_t)pp, (uintptr_t)&engine_print);
      debugPrintf("[boot] Printer::print hooked at %p\n", pp);
    }
  }
  {
    static const struct { const char *sym; void *fn; } hs[] = {
      {"_ZN6glitch2os7Printer3logEPKcNS_10ELOG_LEVELE", (void *)&engine_log},
      {"_ZN6glitch2os7Printer4logfEPKcz", (void *)&engine_logf1},
      {"_ZN6glitch2os7Printer4logfENS_10ELOG_LEVELEPKcz", (void *)&engine_logf2},
    };
    for (unsigned i = 0; i < 3; i++) {
      void *p = (void *)so_try_find_addr_rx(&g_mod_game, hs[i].sym);
      if (p) {
        hook_arm((uintptr_t)p, (uintptr_t)hs[i].fn);
        debugPrintf("[boot] hooked %s\n", hs[i].sym);
      }
    }
  }
  fn_onload onload = (fn_onload)so_try_find_addr_rx(&g_mod_game, "JNI_OnLoad");
  if (onload) {
    debugPrintf("[boot] JNI_OnLoad -> 0x%lx\n", (unsigned long)onload(g_jni_vm, NULL));
  } else {
    debugPrintf("[boot] no JNI_OnLoad\n");
  }
  log_flush_ring();

  if (!backstab_egl_ensure())
    fatal_error("No GLES 2 context could be made.\nSee debug.log.");
  debugPrintf("[boot] GLES 2 context ready\n");
  log_flush_ring();

  // skipped, the installer is Java's DRM flow:   native_call0(SDSYM "installer_GameInstaller_initNative", SD "/installer/GameInstaller");
  // skipped, the installer is Java's DRM flow:   native_call0(SDSYM "installer_GameInstaller_getPublicKey", SD "/installer/GameInstaller");
  drm_allowed_outcome();
  // skipped, the installer is Java's DRM flow:   native_call0(SDSYM "installer_GameInstaller_nativeStart", SD "/installer/GameInstaller");

  native_call0(SDSYM "GLResLoader_nativeInit", SD "/GLResLoader");

  {
    /* GameRenderer_nativeInit(env, thiz, a, width, height, jstring):
       stores width/height, calls appInit(a, width, height, strdup(str)) which
       builds the engine's Application singleton. a is unconfirmed (0, as in
       the Gangstar reference); the string is kept short: appInit strcpy's it
       into a 32-byte buffer. */
    void (*fn)(void *, void *, int, int, int, void *) = (void (*)(void *, void *, int, int, int, void *))
        so_try_find_addr_rx(&g_mod_game, SDSYM "GameRenderer_nativeInit");
    if (!fn) {
      debugPrintf("[boot] GameRenderer_nativeInit: MISSING\n");
    } else {
      debugPrintf("[boot] calling GameRenderer_nativeInit\n");
      log_flush_ring();
      fn(g_jni_env, jni_class(SD "/GameRenderer")->obj, 0, 1920, 1080, jni_str("1.0"));
      debugPrintf("[boot] GameRenderer_nativeInit returned\n");
      log_flush_ring();
    }
  }

  native_call0(SDSYM "Game_nativeInit", SD "/Game");

  {
    void (*resize)(void *, void *, int, int) = (void (*)(void *, void *, int, int))
        so_try_find_addr_rx(&g_mod_game, SDSYM "GameRenderer_nativeResize");
    if (resize) {
      debugPrintf("[boot] calling GameRenderer_nativeResize(1280,720)\n");
      log_flush_ring();
      resize(g_jni_env, jni_class(SD "/GameRenderer")->obj, 1920, 1080);
      debugPrintf("[boot] GameRenderer_nativeResize returned\n");
    } else {
      debugPrintf("[boot] GameRenderer_nativeResize: MISSING\n");
    }
    log_flush_ring();
  }

  {
    void (*render)(void *, void *) = (void (*)(void *, void *))
        so_try_find_addr_rx(&g_mod_game, SDSYM "GameRenderer_nativeRender");
    if (!render) {
      debugPrintf("[boot] GameRenderer_nativeRender: MISSING\n");
    } else {
      dump_flags("before Resume");
      native_call0(SDSYM "Game_nativeResume", SD "/Game");
      native_call0(SDSYM "GameGLSurfaceView_nativeResume", SD "/GameGLSurfaceView");
      {
        void (*fc)(void *, void *, int) = (void (*)(void *, void *, int))
            so_try_find_addr_rx(&g_mod_game, SDSYM "Game_nativeHandleWindowFocusChanged");
        if (fc) {
          debugPrintf("[boot] calling nativeHandleWindowFocusChanged(1)\n");
          log_flush_ring();
          fc(g_jni_env, jni_class(SD "/Game")->obj, 1);
          debugPrintf("[boot] nativeHandleWindowFocusChanged returned\n");
          log_flush_ring();
        } else debugPrintf("[boot] nativeHandleWindowFocusChanged: MISSING\n");
      }
      dump_flags("after Resume");
      PadState pads[2];
      rt_pad_setup(1, 1);
      rt_pad_slot(&pads[0], 0);
      rt_pad_slot(&pads[1], RT_PAD_HANDHELD);

      void *cls = jni_class(SD "/GameRenderer")->obj;
      unsigned long frame = 0;
      u64 t0 = armGetSystemTick();
      debugPrintf("[boot] entering main loop (exit: + and - together, or 45 s)\n");
      log_flush_ring();

      while (!rt_exit_requested() && appletMainLoop()) {
        rt_applet_poll();
        if (!rt_focused()) {
          svcSleepThread(50000000ll);
          continue;
        }
        padUpdate(&pads[0]);
        padUpdate(&pads[1]);
        u64 held = padGetButtons(&pads[0]) | padGetButtons(&pads[1]);
        int combo = (held & HidNpadButton_Plus) && (held & HidNpadButton_Minus);
        int timeout = 0; (void)t0;
        if (combo || timeout) {
          debugPrintf("[boot] leaving the loop at frame %lu (%s)\n", frame, combo ? "exit combo" : "45 s limit");
          rt_request_exit();
          break;
        }

        int log_it = (frame < 5) || (frame % 100000 == 0);
        if (log_it) {
          debugPrintf("[boot] frame %lu: calling nativeRender\n", frame);
          log_flush_ring();
        }
        /* key_poll(&pads[0], &pads[1]); */
        /* pad_touch(&pads[0], &pads[1]); */
        /* pad_extra(&pads[0], &pads[1]); */
        touch_poll();
        pad_poll(&pads[0], &pads[1]);
        render(g_jni_env, cls);
        track_app(frame);
        backstab_egl_swap();
        if (log_it) {
          debugPrintf("[boot] frame %lu: swapped\n", frame);
          if (frame == 0) dump_flags("after frame 0");
          log_flush_ring();
        }
        frame++;
      }
      rt_applet_stop();
      debugPrintf("[boot] main loop ended after %lu frames\n", frame);
    }
  }

  debugPrintf("[boot] fifth milestone: main loop ran and exited cleanly\n");
  log_flush_ring();}





























