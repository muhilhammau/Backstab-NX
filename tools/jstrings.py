#!/usr/bin/env python3
"""jstrings.py <lib.so> <objdump-bin> [sym-substring ...]
For each exported function whose name contains one of the substrings, list the
strings it refers to (pc-relative literal + 'add rN, pc, rN') and the JNIEnv
slots it calls (ldr pc, [rX, #off] -> index off/4). Library must be ARM mode."""
import os, re, struct, subprocess, sys

HEAD = int(os.environ.get("HEAD", "0"))

JNI = {6: "FindClass", 21: "NewGlobalRef", 22: "DeleteGlobalRef", 31: "GetObjectClass",
       33: "GetMethodID", 34: "CallObjectMethod", 61: "CallVoidMethod", 94: "GetFieldID",
       113: "GetStaticMethodID", 114: "CallStaticObjectMethod", 117: "CallStaticBooleanMethod",
       129: "CallStaticIntMethod", 141: "CallStaticVoidMethod", 144: "GetStaticFieldID",
       167: "NewStringUTF", 169: "GetStringUTFChars", 171: "GetArrayLength",
       176: "NewByteArray", 184: "GetByteArrayElements", 200: "GetByteArrayRegion",
       208: "SetByteArrayRegion",
       215: "RegisterNatives"}

def analyze(lines, rd_u32, rd_str):
    lit, out = {}, []
    for ln in lines:
        m = re.match(r"\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(.*)$", ln)
        if not m:
            continue
        addr, ins = int(m.group(1), 16), m.group(2)
        m1 = re.match(r"ldr\s+(r\d+|sl|fp|ip),\s*\[pc, #\d+\]\s+@ ([0-9a-f]+)\b", ins)
        if m1:
            lit[m1.group(1)] = rd_u32(int(m1.group(2), 16))
            continue
        m2 = re.match(r"add\s+(r\d+|sl|fp|ip),\s*pc,\s*(r\d+|sl|fp|ip)\b", ins)
        if m2 and m2.group(2) in lit:
            target = (lit[m2.group(2)] + addr + 8) & 0xffffffff
            s = rd_str(target)
            if s is not None:
                out.append("    str  %x: %r" % (target, s))
            continue
        m3 = re.match(r"ldr\s+pc,\s*\[(\w+), #(\d+)\]", ins)
        if m3 and int(m3.group(2)) % 4 == 0:
            i = int(m3.group(2)) // 4
            out.append("    JNI  #%d %s" % (i, JNI.get(i, "?")))
    return out

class Elf:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        phoff, = struct.unpack_from("<I", self.d, 28)
        phentsize, phnum = struct.unpack_from("<HH", self.d, 42)
        self.segs = []
        for i in range(phnum):
            t, off, va, pa, fsz, msz, fl, al = struct.unpack_from("<8I", self.d, phoff + i * phentsize)
            if t == 1:
                self.segs.append((va, off, fsz))
    def off(self, va):
        for v, o, f in self.segs:
            if v <= va < v + f:
                return o + va - v
        return None
    def u32(self, va):
        o = self.off(va)
        return struct.unpack_from("<I", self.d, o)[0] if o is not None else 0
    def cstr(self, va):
        o = self.off(va)
        if o is None:
            return None
        e = self.d.find(b"\0", o)
        b = self.d[o:e]
        if 2 <= len(b) <= 120 and all(32 <= c < 127 for c in b):
            return b.decode()
        return None

if __name__ == "__main__":
    so, objdump = sys.argv[1], sys.argv[2]
    pats = sys.argv[3:] or ["Java_"]
    elf = Elf(so)
    nm = subprocess.check_output([objdump.replace("objdump", "nm"), "-D", "-n", "--defined-only", so], text=True)
    syms = [(int(l.split()[0], 16), l.split()[2]) for l in nm.splitlines() if len(l.split()) == 3]
    for i, (a, n) in enumerate(syms):
        if not any(p in n for p in pats):
            continue
        end = min(syms[i + 1][0] if i + 1 < len(syms) else a + 0x800, a + 0x1400)
        dis = subprocess.check_output([objdump, "-d", "--start-address=%#x" % a,
                                       "--stop-address=%#x" % end, so], text=True).splitlines()
        print("== %s @%x (+%#x)" % (n, a, end - a))
        if HEAD:
            ins = [l for l in dis if re.match(r"\s*[0-9a-f]+:\s+[0-9a-f]{8}\s", l)]
            print("  -- first %d instructions" % HEAD)
            print("\n".join(x.replace("\t", " ") for x in ins[:HEAD]))
            print("  -- strings and JNI calls")
        print("\n".join(analyze(dis, elf.u32, elf.cstr)))
