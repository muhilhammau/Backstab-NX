import subprocess, sys
t = int(sys.argv[2], 16)
out = subprocess.check_output(["arm-none-eabi-nm", "-D", "-n", sys.argv[1]], text=True)
best = None
for line in out.splitlines():
    p = line.split()
    if len(p) >= 3:
        a = int(p[0], 16)
        if a <= t: best = (a, p[2])
        else: break
print("nearest exported symbol below 0x%x: %s +0x%x" % (t, best[1], t - best[0]) if best else "none")
