#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H
#define PORT_TITLE    "Backstab"
#define PORT_NAME     "backstab_nx"
#define PORT_PACKAGE  "com.gameloft.android.ANMP.GloftSDHM"
#define PORT_BANNER   "backstab_nx: Backstab (Gameloft, armeabi-v7a)"
#define PORT_ABI_DIR  "lib/armeabi-v7a/"
#define PORT_SO_REGION_BYTES (32u * 1024 * 1024)
#define PORT_APK_DESC "Backstab (com.gameloft.android.ANMP.GloftSDHM, armeabi-v7a)"
#define RT_OPENSLES 0
#define PORT_APK_ROLES \
  {.what = "the game", .name = "game.apk", \
   .need = (const char *const[]){"lib/armeabi-v7a/libPirates.so", NULL}, \
   .flags = RT_APK_ADOPT}
#define RT_PAD_MAX_PLAYERS 1
#define RT_PATH_TRACE_EXTRA ".gla"
#endif
