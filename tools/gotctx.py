import re, subprocess, sys
so = sys.argv[1]
lo = int(sys.argv[2], 16); hi = int(sys.argv[3], 16)
lits = [int(x, 16) for x in sys.argv[4:]]
out = subprocess.check_output(["arm-none-eabi-objdump", "-d", "--start-address=%#x" % lo,
                               "--stop-address=%#x" % hi, so], text=True)
L = [l for l in out.splitlines() if re.match(r"\s+[0-9a-f]+:\t", l)]
for i, l in enumerate(L):
    m = re.search(r"@ ([0-9a-f]+)\b", l)
    if m and "ldr" in l and int(m.group(1), 16) in lits:
        start = None
        for j in range(i, -1, -1):
            if re.search(r"\bpush\s+\{.*lr\}", L[j]):
                start = L[j].split(":")[0].strip(); break
        print("--- ref to literal %s (nearest preceding push{..lr} at %s)" % (m.group(1), start))
        for x in L[max(0, i - 4):i + 10]:
            print(x)
