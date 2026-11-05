import struct, sys
d = open(sys.argv[1], "rb").read()
print("size", len(d), "ftyp", d[4:12], d[8:12])
for tag in (b"avc1", b"mp4v", b"hvc1", b"hev1", b"vp09", b"av01"):
    i = d.find(tag)
    if i > 0 and d[i-4:i-3] is not None:
        w, h = struct.unpack(">HH", d[i+28:i+32])
        print("video", tag, w, "x", h)
        j = d.find(b"avcC", i)
        if tag == b"avc1" and j > 0:
            print("avcC profile", d[j+5], "level", d[j+7])
        break
else:
    print("video codec tag not found")
i = d.find(b"mp4a")
if i > 0:
    ch = struct.unpack(">H", d[i+20:i+22])[0]
    sr = struct.unpack(">I", d[i+28:i+32])[0] >> 16
    print("audio mp4a channels", ch, "rate", sr)
else:
    print("audio mp4a not found")
print("moov at", d.find(b"moov"), "mdat at", d.find(b"mdat"))
