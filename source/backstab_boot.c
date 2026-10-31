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
extern so_module g_mod_game;

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

/* First milestone: the library up, its constructors and JNI_OnLoad run, and
 * every Java call it makes logged (set [debug] log_java_calls = true). */
void port_run(void) {
  jni_init();
  backstab_run_constructors();
  fn_onload onload = (fn_onload)so_try_find_addr_rx(&g_mod_game, "JNI_OnLoad");
  if (onload) {
    debugPrintf("[boot] JNI_OnLoad -> 0x%lx\n", (unsigned long)onload(g_jni_vm, NULL));
  } else {
    debugPrintf("[boot] no JNI_OnLoad\n");
  }
  debugPrintf("[boot] first milestone reached: nothing runs the game yet\n");
  log_flush_ring();
}
