import bisect, re, subprocess, sys
so = sys.argv[1]
offs = [int(x, 16) for x in sys.argv[2:]]
def sh(*a):
    return subprocess.check_output(a, text=True, errors="replace")
syms = []
for line in sh("arm-none-eabi-nm", "-n", "--defined-only", so).splitlines():
    p = line.split(None, 2)
    if len(p) == 3 and p[1] in ("T", "t", "W", "w"):
        syms.append((int(p[0], 16), p[2]))
addrs = [a for a, _ in syms]
dis = sh("arm-none-eabi-objdump", "-d", so)
for off in offs:
    print("== literal GOT offset 0x%x" % off)
    pat = re.compile(r"^\s*([0-9a-f]+):\s+%08x\s" % off, re.M)
    for m in pat.finditer(dis):
        a = int(m.group(1), 16)
        i = bisect.bisect_right(addrs, a) - 1
        where = "%s+0x%x" % (syms[i][1], a - syms[i][0]) if i >= 0 else "?"
        print("  literal at %x in %s" % (a, where))
