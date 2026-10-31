/* backstab_boot.c -- Backstab's part of the boot: setup plan, port_load, port_run. MIT. */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "jni.h"
#include "rt_settings.h"
#include "so_util.h"
#include "util.h"

int backstab_load_module(void);
void backstab_run_constructors(void);
int backstab_egl_ensure(void);
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
void port_run(void) {
  jni_init();
  backstab_run_constructors();
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
  native_call0(SDSYM "installer_GameInstaller_nativeStart", SD "/installer/GameInstaller");

  debugPrintf("[boot] second milestone reached: Game/GameRenderer init come next\n");
  log_flush_ring();
}
