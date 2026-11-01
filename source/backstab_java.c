/* backstab_java.c -- the Java side of Backstab, answered in C.
 *
 * Game_nativeInit looks up statics on Game, GLResLoader_nativeInit five
 * resource getters on GLResLoader (names read from the ARM code, see
 * tools/jstrings.py). Entries match by name; where the return type is not
 * settled the handler reads what the engine asked for (JMethod.ret).
 * Anything not in the tables is logged once ([debug] log_java_calls): that log
 * is the to-do list. GetTextureFormat is left unanswered on purpose: its
 * answer picks the texture compression, so the log should show what it wants. MIT. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "dcr_time.h"
#include "jni.h"
#include "rt_applet.h"
#include "util.h"

#define SD   "com/gameloft/android/ANMP/GloftSDHM"
#define GAME SD "/Game"
#define RES  SD "/GLResLoader"

void *backstab_apk_read(const char *entry, size_t *size);
void backstab_apk_free(void *p);

#define H(fn) static jvalue fn(JObj *self, const jvalue *a, const JMethod *m)

/* Exit / sendAppToBackground: the engine expects the process to end. */
H(h_exit) {
  rt_request_exit();
  return jv_none();
}

/* A getter whose return type has not been seen: a string when the engine asked
 * for one, else 0. */
static jvalue str_or_zero(const JMethod *m, const char *s) {
  return m->ret == 'L' ? jv_l(jni_str(s)) : jv_none();
}
H(h_lang)  { return str_or_zero(m, "en"); }
H(h_maker) { return str_or_zero(m, "NVIDIA"); }
H(h_model) { return str_or_zero(m, "SHIELD Android TV"); }
H(h_cpu)   { return str_or_zero(m, "ARM Cortex-A57"); }
H(h_gpu)   { return str_or_zero(m, "NVIDIA Tegra X1"); }
H(h_os)    { return str_or_zero(m, "6.0.1"); }

H(h_time) { return jv_j((jlong)(dcr_monotonic_ns() / 1000000ull)); }
H(h_cpufreq) { return m->ret == 'F' ? jv_f(1020.0f) : jv_i(1020); }

/* ------------------------------------------------------------ GLResLoader */
static void *res_load(const char *name, size_t *sz) {
  static const char *const where[] = {"assets/", "res/raw/", ""};
  char path[512];
  for (unsigned i = 0; i < 3; i++) {
    snprintf(path, sizeof path, "%s%s", where[i], name);
    void *p = backstab_apk_read(path, sz);
    if (p)
      return p;
  }
  debugPrintf("[res] not in the APK: %s\n", name);
  *sz = 0;
  return NULL;
}

static jvalue res_bytes(const char *name, size_t off, size_t len, int whole) {
  size_t sz = 0;
  void *buf = res_load(name, &sz);
  if (!buf)
    return jv_l(NULL);
  if (whole) {
    off = 0;
    len = sz;
  }
  if (off > sz)
    off = sz;
  if (len > sz - off)
    len = sz - off;
  JObj *arr = jni_array('B', (jsize)len);
  if (len)
    memcpy(arr->a.data, (char *)buf + off, len);
  backstab_apk_free(buf);
  return jv_l(arr);
}

H(h_res_full) { return res_bytes(jni_utf(a[0].l), 0, 0, 1); }
H(h_res_bytes) {
  return res_bytes(jni_utf(a[0].l), a[1].i < 0 ? 0 : (size_t)a[1].i, a[2].i < 0 ? 0 : (size_t)a[2].i, 0);
}
H(h_res_len) {
  size_t sz = 0;
  void *buf = res_load(jni_utf(a[0].l), &sz);
  if (buf)
    backstab_apk_free(buf);
  return jv_i((jint)sz);
}
H(h_sound_raw) {
  debugPrintf("[res] getSoundRaw(%d): no id table yet\n", (int)a[0].i);
  return jv_l(NULL);
}
H(h_sound_len) {
  debugPrintf("[res] getResourceLengthSoundRaw(%d): no id table yet\n", (int)a[0].i);
  return jv_i(0);
}

const JMethodDef jni_method_defs[] = {
    {GAME, "Exit", NULL, h_exit},
    {GAME, "sendAppToBackground", NULL, h_exit},
    {GAME, "openBrowser", NULL, jni_h_void},
    {GAME, "isWifiEnabled", NULL, jni_h_false},
    {GAME, "Pause", NULL, jni_h_void},
    {GAME, "GetPhoneLanguage", NULL, h_lang},
    {GAME, "launchGLLive", NULL, jni_h_void},
    {GAME, "getManufacture", NULL, h_maker},
    {GAME, "notifyTrophy", NULL, jni_h_void},
    {GAME, "launchIGP", NULL, jni_h_void},
    {GAME, "GetCurrentTime", NULL, h_time},
    {GAME, "PrintDebug", NULL, jni_h_void},
    {GAME, "GetPhoneManufacturer", NULL, h_maker},
    {GAME, "GetPhoneModel", NULL, h_model},
    {GAME, "GetPhoneCPUName", NULL, h_cpu},
    {GAME, "GetPhoneCPUFreq", NULL, h_cpufreq},
    {GAME, "GetPhoneGPUName", NULL, h_gpu},
    {GAME, "GC", NULL, jni_h_void},
    {GAME, "GetOSVersion", NULL, h_os},
    {GAME, "GameTracking", NULL, jni_h_void},
    {RES, "getResourceFull", NULL, h_res_full},
    {RES, "getResourceBytes", NULL, h_res_bytes},
    {RES, "getResourceLength", NULL, h_res_len},
    {RES, "getSoundRaw", NULL, h_sound_raw},
    {RES, "getResourceLengthSoundRaw", NULL, h_sound_len},
    {NULL, NULL, NULL, NULL},
};

const JFieldDef jni_field_defs[] = {
    {NULL, NULL, NULL, 0, NULL},
};

const char *const jni_class_supers[][2] = {
    {GAME, "android/app/Activity"},
    {NULL, NULL},
};

const char *const jni_missing_classes[] = {NULL};
