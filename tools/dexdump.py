import struct, sys
d = open(sys.argv[1], "rb").read()
flt = sys.argv[2] if len(sys.argv) > 2 else "com/gameloft"
u32 = lambda o: struct.unpack_from("<I", d, o)[0]
u16 = lambda o: struct.unpack_from("<H", d, o)[0]
def uleb(o):
    r = s = 0
    while True:
        b = d[o]; o += 1
        r |= (b & 0x7f) << s; s += 7
        if not b & 0x80:
            return r, o
soff = u32(60); toff = u32(68); poff = u32(76); moff = u32(92)
csz, coff = u32(96), u32(100)
def string(i):
    o = u32(soff + 4 * i)
    _, o = uleb(o)
    return d[o:d.index(b"\0", o)].decode("utf-8", "replace")
def tname(i):
    return string(u32(toff + 4 * i))
def sig(pi):
    o = poff + 12 * pi
    po = u32(o + 8)
    ps = [tname(u16(po + 4 + 2 * k)) for k in range(u32(po))] if po else []
    return "(" + "".join(ps) + ")" + tname(u32(o + 4))
def meth(i):
    o = moff + 8 * i
    return string(u32(o + 4)), sig(u16(o + 2))
for c in range(csz):
    o = coff + 32 * c
    cname = tname(u32(o))
    if flt not in cname:
        continue
    sup = u32(o + 8)
    print("%s extends %s" % (cname, tname(sup) if sup != 0xFFFFFFFF else "-"))
    cd = u32(o + 24)
    if not cd:
        continue
    p = cd
    nsf, p = uleb(p); nif, p = uleb(p); ndm, p = uleb(p); nvm, p = uleb(p)
    for _ in range(nsf + nif):
        _, p = uleb(p); _, p = uleb(p)
    for n in (ndm, nvm):
        mid = 0
        for _ in range(n):
            dif, p = uleb(p); acc, p = uleb(p); _, p = uleb(p)
            mid += dif
            name, sg = meth(mid)
            fl = ("static " if acc & 8 else "") + ("native " if acc & 0x100 else "")
            print("    %s%s%s" % (fl, name, sg))
