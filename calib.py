import struct, select, time, json
W, H, M = 480, 320, 24
def frame(px, py):
    buf = bytearray(b"\xff\xff" * (W * H))
    def put(x, y):
        if 0 <= x < W and 0 <= y < H:
            buf[(y * W + x) * 2:(y * W + x) * 2 + 2] = b"\x00\x00"
    for d in range(-12, 13):
        for t in range(-1, 2):
            put(px + d, py + t); put(px + t, py + d)
    for a in range(-5, 6):
        for b in range(-5, 6):
            put(px + a, py + b)
    return bytes(buf)
def show(px, py):
    with open("/dev/fb1", "wb") as f: f.write(frame(px, py))
pts = [(M, M), (W - M, M), (W - M, H - M), (M, H - M)]
f = open("/dev/input/event0", "rb", buffering=0)
sz = struct.calcsize("llHHi")
res = []
for px, py in pts:
    show(px, py)
    print("TARGET", px, py, flush=True)
    while select.select([f], [], [], 0)[0]: f.read(sz)
    x = y = None; samples = []; down = False; end = time.time() + 90
    while time.time() < end:
        if not select.select([f], [], [], 1)[0]: continue
        _, _, t, c, v = struct.unpack("llHHi", f.read(sz))
        if t == 3 and c == 0: x = v
        elif t == 3 and c == 1: y = v
        elif t == 1 and c == 330:
            if v: down = True; samples = []
            elif down: break
        elif t == 0 and down and x is not None and y is not None: samples.append((x, y))
    s = samples[len(samples) // 2:] or samples
    if not s: print("NO TAP"); raise SystemExit(1)
    mx = sum(a for a, b in s) / len(s); my = sum(b for a, b in s) / len(s)
    print("RAW", round(mx), round(my), flush=True); res.append((px, py, mx, my))
    time.sleep(0.6)
show(0, 0)
json.dump(res, open("/tmp/calib.json", "w"))
print(json.dumps(res))
