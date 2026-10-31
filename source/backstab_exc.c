/* gangstar_exc.c -- emulate the Linux kuser helpers that the game's inline
 * atomics call by address (0xffff0fa0 barrier, 0xffff0fc0 cmpxchg). MIT. */
#include <stdint.h>
#include "exc_handler.h"

#define KUSER_BARRIER 0xffff0fa0u
#define KUSER_CMPXCHG 0xffff0fc0u

int g_kuser_emu;

int port_exception_hook(uint32_t type, ThreadExceptionInfo32 *info, ThreadExceptionFrame32 *frame) {
  (void)type;
  uint32_t pc = (uint32_t)info->pc;
  if (pc != KUSER_BARRIER && pc != KUSER_CMPXCHG)
    return 0;
  uint32_t *lr = rt_exc_reg(info, frame, 14);
  if (!lr)
    return 0;
  if (pc == KUSER_CMPXCHG) {
    /* r0 = old, r1 = new, r2 = ptr; result in r0 (0 = swapped), C flag set on success */
    uint32_t *r0 = rt_exc_reg(info, frame, 0), *r1 = rt_exc_reg(info, frame, 1), *r2 = rt_exc_reg(info, frame, 2);
    uint32_t expect = *r0;
    int ok = __atomic_compare_exchange_n((uint32_t *)(uintptr_t)*r2, &expect, *r1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    *r0 = ok ? 0 : 1;
    if (ok) info->pstate |= 0x20000000u; else info->pstate &= ~0x20000000u;
  } else {
    __asm__ volatile("dmb ish" ::: "memory");
  }
  info->pc = *lr & ~1u;                       /* return to the caller */
  if (*lr & 1u) info->pstate |= 0x20u; else info->pstate &= ~0x20u;  /* Thumb bit */
  g_kuser_emu++;
  return 1;
}