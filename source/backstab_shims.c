/* backstab_shims.c -- the few bionic symbols libPirates imports that the
 * runtime has no shim for. Found by gen_imports.py (MISSING list). */

int b_fseek(void *fp, long off, int whence);
long b_ftell(void *fp);

/* __dso_handle: only its address is used (passed to __cxa_atexit, which the
 * runtime ignores). */
char b___dso_handle = 0;

/* bionic ARM32: fpos_t is off_t, 32 bits. */
int b_fgetpos(void *fp, long *pos) {
  long p = b_ftell(fp);
  if (p < 0)
    return -1;
  *pos = p;
  return 0;
}

int b_fsetpos(void *fp, const long *pos) { return b_fseek(fp, *pos, 0 /* SEEK_SET */); }

/* EHABI personality routines. libPirates has no relocation to them; they are
 * imported only so the table is complete. Trap if one is ever reached. */
void b___aeabi_unwind_cpp_pr0(void) { __builtin_trap(); }
void b___aeabi_unwind_cpp_pr1(void) { __builtin_trap(); }
