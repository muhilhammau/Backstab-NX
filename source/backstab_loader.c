/* backstab_loader.c -- loading libPirates.so. MIT. */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "dcr_path.h"
#include "error.h"
#include "so_util.h"
#include "util.h"

so_module g_mod_game;

int backstab_load_module(void) {
  char path[512];
  snprintf(path, sizeof path, "%s/libPirates.so", dcr_game_root());
  int rc = so_load(&g_mod_game, path, NULL, PORT_SO_REGION_BYTES);
  if (rc < 0) {
    const char *why = rc == -1 ? "cannot open it, or it is not a 32-bit ARM ELF"
                    : rc == -2 ? "out of memory"
                    : rc == -3 ? "larger than PORT_SO_REGION_BYTES"
                    : rc == -4 ? "too many program headers" : "?";
    debugPrintf("[boot] so_load(%s) failed rc=%d: %s\n", path, rc, why);
    return -1;
  }
  so_relocate(&g_mod_game);
  int missing = so_resolve(&g_mod_game, NULL, 0, 1);
  debugPrintf("[boot] %s %u KB staged %p -> %p (%d unresolved imports)\n", g_mod_game.base_name,
              (unsigned)(g_mod_game.load_size >> 10), g_mod_game.load_base, g_mod_game.load_virtbase, missing);
  if (so_fix_kuser_helpers(&g_mod_game) < 0)
    debugPrintf("[boot] kuser fix: module already sealed\n");
  so_finalize(&g_mod_game);
  so_flush_caches(&g_mod_game);
  return 0;
}

void backstab_run_constructors(void) {
  u64 t0 = armGetSystemTick();
  so_execute_init_array(&g_mod_game);
  debugPrintf("[boot] %s constructors done in %llu ms\n", g_mod_game.base_name,
              (unsigned long long)(armTicksToNs(armGetSystemTick() - t0) / 1000000ull));
}
