/* backstab_java.c -- the game's Java, as the engine sees it. The unhandled-call
 * log (debug.log) is the to-do list. MIT. */
#include <stddef.h>
#include "jni.h"

#define INST "com/gameloft/android/ANMP/GloftSDHM/installer/GameInstaller"

/* GameInstaller.getSDFolder(): the data folder string the engine itself embeds
 * (dcr_path maps it to <root>/external/files). */
static jvalue h_sd_folder(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("/sdcard/Android/data/com.gameloft.android.ANMP.GloftSDHM/files"));
}

const JMethodDef jni_method_defs[] = {
    {INST, "startGame", "()V", jni_h_void},
    {INST, "getSDFolder", "()Ljava/lang/String;", h_sd_folder},
    {NULL, NULL, NULL, NULL},
};
const JFieldDef jni_field_defs[] = {
    {NULL, NULL, NULL, 0, NULL},
};
const char *const jni_class_supers[][2] = {
    {NULL, NULL},
};
const char *const jni_missing_classes[] = {NULL};
