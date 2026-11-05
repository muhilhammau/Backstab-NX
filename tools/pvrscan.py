import struct, sys, collections
p = sys.argv[1]
d = open(p, "rb").read()
names2 = {0x10:"RGBA4444",0x11:"RGBA5551",0x12:"RGBA8888",0x13:"RGB565",0x15:"RGB888",0x18:"PVRTC2bpp(v2:24)",0x19:"PVRTC4bpp(v2:25)",0x36:"ETC1",0x20:"DXT1",0x22:"DXT3",0x24:"DXT5"}
names3 = {0:"PVRTC 2bpp RGB",1:"PVRTC 2bpp RGBA",2:"PVRTC 4bpp RGB",3:"PVRTC 4bpp RGBA",6:"ETC1",7:"DXT1",9:"DXT3",11:"DXT5"}
h2 = collections.Counter(); h3 = collections.Counter(); ex = []
i = d.find(b"PVR!")
while i != -1:
    s = i - 44
    if s >= 0:
        hs,h,w,mip,fl,ds,bpp = struct.unpack("<7I", d[s:s+28])
        if hs == 52 and 0 < w <= 8192 and 0 < h <= 8192:
            f = fl & 0xff
            h2[names2.get(f, "v2 fmt 0x%x" % f)] += 1
            if len(ex) < 6: ex.append(("v2", s, w, h, mip, names2.get(f, hex(f))))
    i = d.find(b"PVR!", i + 4)
i = d.find(b"PVR\x03")
while i != -1:
    ver, fl, pf = struct.unpack("<IIQ", d[i:i+16])
    cs, ct, h, w = struct.unpack("<4I", d[i+16:i+32])
    if ver == 0x03525650 and 0 < w <= 8192 and 0 < h <= 8192:
        k = names3.get(pf, "v3 fmt %d" % pf) if pf < 2**32 else "v3 fmt 0x%x" % pf
        h3[k] += 1
        if len(ex) < 12: ex.append(("v3", i, w, h, 0, k))
    i = d.find(b"PVR\x03", i + 4)
print("v2 headers:", dict(h2))
print("v3 headers:", dict(h3))
for e in ex: print(e)
