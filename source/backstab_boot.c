/* backstab_boot.c -- Backstab's part of the boot: setup plan, port_load, port_run. MIT. */
#include <stdio.h>
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

  native_call0(SDSYM "installer_GameInstaller_initNative", SD "/installer/GameInstaller");
  native_call0(SDSYM "installer_GameInstaller_getPublicKey", SD "/installer/GameInstaller");
  drm_allowed_outcome();
  native_call0(SDSYM "installer_GameInstaller_nativeStart", SD "/installer/GameInstaller");

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
      fn(g_jni_env, jni_class(SD "/GameRenderer")->obj, 0, 1280, 720, jni_str("nx"));
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
      resize(g_jni_env, jni_class(SD "/GameRenderer")->obj, 1280, 720);
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
      native_call0(SDSYM "Game_nativeResume", SD "/Game");
      native_call0(SDSYM "GameGLSurfaceView_nativeResume", SD "/GameGLSurfaceView");
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
        int timeout = armTicksToNs(armGetSystemTick() - t0) > 45000000000ULL;
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
        render(g_jni_env, cls);
        backstab_egl_swap();
        if (log_it) {
          debugPrintf("[boot] frame %lu: swapped\n", frame);
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











