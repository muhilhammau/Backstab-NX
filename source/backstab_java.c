/* backstab_java.c -- the game's Java, as the engine sees it. The unhandled-call
 * log (debug.log) is the to-do list. MIT. */
#include <stddef.h>
#include "jni.h"
#include "util.h"

#define INST "com/gameloft/android/ANMP/GloftSDHM/installer/GameInstaller"
#define GAME "com/gameloft/android/ANMP/GloftSDHM/Game"

static jvalue h_sd_folder(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("/sdcard/Android/data/com.gameloft.android.ANMP.GloftSDHM/files"));
}

static jvalue h_print_debug(JObj *self, const jvalue *a, const JMethod *m) {
  jvalue r = {0};
  debugPrintf("[game] %s\n", jni_utf(a[0].l));
  return r;
}

static jvalue h_tex_format(JObj *self, const jvalue *a, const JMethod *m) {
  jvalue r = {0};
  r.i = 1;
  return r;
}

static jvalue h_cpu_freq(JObj *self, const jvalue *a, const JMethod *m) {
  jvalue r = {0};
  r.f = 1020.0f;
  return r;
}

static jvalue h_manufacturer(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("Nintendo"));
}
static jvalue h_model(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("Switch"));
}
static jvalue h_cpu_name(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("ARMv7 Processor rev 10 (v7l)"));
}
static jvalue h_gpu_name(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("NVIDIA Tegra X1"));
}

const JMethodDef jni_method_defs[] = {
    {INST, "startGame", "()V", jni_h_void},
    {INST, "getSDFolder", "()Ljava/lang/String;", h_sd_folder},
    {GAME, "PrintDebug", "(Ljava/lang/String;)V", h_print_debug},
    {GAME, "GetTextureFormat", "()I", h_tex_format},
    {GAME, "GetPhoneManufacturer", "()Ljava/lang/String;", h_manufacturer},
    {GAME, "GetPhoneModel", "()Ljava/lang/String;", h_model},
    {GAME, "GetPhoneCPUName", "()Ljava/lang/String;", h_cpu_name},
    {GAME, "GetPhoneCPUFreq", "()F", h_cpu_freq},
    {GAME, "GetPhoneGPUName", "()Ljava/lang/String;", h_gpu_name},
    {NULL, NULL, NULL, NULL},
};
const JFieldDef jni_field_defs[] = {
    {NULL, NULL, NULL, 0, NULL},
};
const char *const jni_class_supers[][2] = {
    {NULL, NULL},
};
const char *const jni_missing_classes[] = {NULL};
