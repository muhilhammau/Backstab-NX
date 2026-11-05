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

#include "rt_audout.h"
#define AT "android/media/AudioTrack"
#define RB_FRAMES 16384
static int16_t g_rb[RB_FRAMES * 2];
static volatile unsigned g_rb_w, g_rb_r;
static int g_at_rate = 44100, g_at_ch = 2, g_at_started = 0;
static unsigned g_at_acc;

static void at_fill(int16_t *out, int frames, void *ud) {
  (void)ud;
  for (int i = 0; i < frames; i++) {
    unsigned r = __atomic_load_n(&g_rb_r, __ATOMIC_ACQUIRE), w = __atomic_load_n(&g_rb_w, __ATOMIC_ACQUIRE);
    if (r == w) { out[i * 2] = 0; out[i * 2 + 1] = 0; continue; }
    out[i * 2] = g_rb[(r % RB_FRAMES) * 2];
    out[i * 2 + 1] = g_rb[(r % RB_FRAMES) * 2 + 1];
    __atomic_store_n(&g_rb_r, r + 1, __ATOMIC_RELEASE);
  }
}

H(h_at_minbuf) { return jv_i(4096); }
H(h_at_init) {
  g_at_rate = a[1].i > 0 ? a[1].i : 44100;
  g_at_ch = (a[2].i == 2 || a[2].i == 4) ? 1 : 2;
  debugPrintf("[audio] AudioTrack init: stream %d rate %d chcfg %d fmt %d buf %d mode %d\n",
              a[0].i, a[1].i, a[2].i, a[3].i, a[4].i, a[5].i);
  if (!g_at_started) {
    g_at_started = 1;
    rt_audout_pump_start(at_fill, NULL, 0x2A, -2);
  }
  return jv_none();
}
H(h_at_write) {
  JObj *arr = (JObj *)a[0].l;
  int off = a[1].i, size = a[2].i;
  { static int n; if (n < 6 || (n % 300) == 0) debugPrintf("[audio] write #%d arr=%p kind=%d off=%d size=%d pump=%lu\n", n, (void *)arr, arr ? (int)arr->kind : -1, off, size, rt_audout_pump_blocks()); n++; }
  if (!arr || arr->kind != JK_ARRAY || size <= 0) return jv_i(size > 0 ? size : 0);
  const int16_t *src = (const int16_t *)((const char *)arr->a.data + off);
  { static int e; if (e < 3) { const unsigned char *bp = (const unsigned char *)arr->a.data; int pkall = 0; const int16_t *sp = (const int16_t *)bp; for (int q = 0; q < arr->a.len / 2; q++) { int v = sp[q] < 0 ? -sp[q] : sp[q]; if (v > pkall) pkall = v; } debugPrintf("[audio] array elem %d len %d off %d size %d peakall %d first8 %02x %02x %02x %02x %02x %02x %02x %02x\n", (int)arr->a.elem, (int)arr->a.len, off, size, pkall, bp[0], bp[1], bp[2], bp[3], bp[4], bp[5], bp[6], bp[7]); e++; } }
  int nsamp = size / 2, step = g_at_ch;
  { static int m, k; { int pk = 0; for (int q = 0; q < nsamp; q++) { int v = src[q] < 0 ? -src[q] : src[q]; if (v > pk) pk = v; } if (m < 3 || (pk > 0 && k < 10)) { debugPrintf("[audio] write peak %d nsamp %d (#%d)\n", pk, nsamp, m); if (m >= 3) k++; } m++; } }
  for (int s = 0; s + step <= nsamp; s += step) {
    int16_t l = src[s], r = step == 2 ? src[s + 1] : src[s];
    g_at_acc += 48000;
    while (g_at_acc >= (unsigned)g_at_rate) {
      g_at_acc -= g_at_rate;
      int spin = 0;
      for (;;) {
        unsigned w = __atomic_load_n(&g_rb_w, __ATOMIC_RELAXED), rd = __atomic_load_n(&g_rb_r, __ATOMIC_ACQUIRE);
        if (w - rd < RB_FRAMES) {
          g_rb[(w % RB_FRAMES) * 2] = l;
          g_rb[(w % RB_FRAMES) * 2 + 1] = r;
          __atomic_store_n(&g_rb_w, w + 1, __ATOMIC_RELEASE);
          break;
        }
        if (++spin > 2000) return jv_i(size);
        svcSleepThread(2000000ll);
      }
    }
  }
  return jv_i(size);
}
#include <string.h>
int b_open(const char *path, int lflags, ...);
void *b_fopen(const char *path, const char *mode);
static int io_hot(const char *p) {
  if (!p) return 0;
  return strstr(p, "sound") || strstr(p, "Sound") || strstr(p, ".xml") || strstr(p, ".zip") ||
         strstr(p, ".mpc") || strstr(p, ".ogg") || strstr(p, ".wav");
}
int my_open(const char *path, int lflags, int mode) {
  static int fails, hot;
  char fixbuf[300]; if (path && !strcmp(path, "sounds.zip")) { strcpy(fixbuf, "/sdcard/Android/data/com.gameloft.android.ANMP.GloftSDHM/files/sounds.zip"); path = fixbuf; } int r = b_open(path, lflags, mode);
  int h = io_hot(path) && hot < 200;
  if (h || (r < 0 && fails < 40)) {
    if (r < 0) fails++;
    if (h) hot++;
    debugPrintf("[open] %s flags 0x%x -> %d\n", path ? path : "(null)", lflags, r);
  }
  return r;
}
void *my_fopen(const char *path, const char *mode) {
  static int fails, hot;
  char fixbuf[300]; if (path && !strcmp(path, "sounds.zip")) { strcpy(fixbuf, "/sdcard/Android/data/com.gameloft.android.ANMP.GloftSDHM/files/sounds.zip"); path = fixbuf; } void *r = b_fopen(path, mode);
  int h = io_hot(path) && hot < 200;
  if (h || (!r && fails < 40)) {
    if (!r) fails++;
    if (h) hot++;
    debugPrintf("[fopen2] %s %s -> %p\n", path ? path : "(null)", mode ? mode : "", r);
  }
  return r;
}
H(h_texfmt) { return jv_i(1); } /* 1 = .pvr (data game PVRTC) */
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
    {GAME, "GetTextureFormat", NULL, h_texfmt},
    {AT, "getMinBufferSize", NULL, h_at_minbuf},
    {AT, "<init>", NULL, h_at_init},
    {AT, "write", NULL, h_at_write},
    {AT, "play", NULL, jni_h_void},
    {AT, "pause", NULL, jni_h_void},
    {AT, "stop", NULL, jni_h_void},
    {AT, "release", NULL, jni_h_void},
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









