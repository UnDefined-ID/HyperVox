#!/usr/bin/env python3
"""Tests the Steel helper without the game:  python3 tools/steel_probe.py
Starts steel/steel-provider, asks for chunk (0,0) of seed 1, decodes it with steel/block.json and prints what it finds."""
import json, os, re, socket, struct, subprocess, sys, time
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
prov, bj = os.path.join(root, "steel", "steel-provider"), os.path.join(root, "steel", "block.json")
for f in (prov, bj):
    if not os.path.exists(f): sys.exit(f"missing {f} - run ./build-steel.sh")
port = 42000 + os.getpid() % 1000
p = subprocess.Popen([prov, f"127.0.0.1:{port}"], stderr=subprocess.PIPE, text=True)
try:
    s = None
    for _ in range(300):
        if p.poll() is not None: sys.exit("helper exited early: " + p.stderr.read())
        try: s = socket.create_connection(("127.0.0.1", port), timeout=5); break
        except OSError: time.sleep(0.2)
    if not s: sys.exit("could not connect")
    s.settimeout(300); t0 = time.time()
    s.sendall(struct.pack(">QiiB", 1, 0, 0, 0))
    def rd(n):
        b = b""
        while len(b) < n:
            c = s.recv(n - len(b));  assert c, "connection closed"; b += c
        return b
    ln = struct.unpack(">I", rd(4))[0]; body = rd(ln); status = struct.unpack(">I", body[:4])[0]; data = body[4:]
    print(f"response: status={status}, {len(data)} bytes, {time.time()-t0:.1f}s for the first chunk")
    if status: sys.exit("provider error: " + data.decode(errors="replace"))
    print("first 96 bytes:", data[:96].hex(" "))
    # block states
    states = {}
    for name, b in json.load(open(bj)).items():
        for key, st in b["states"].items(): states[st["stateId"]] = (name, key)
    print("block.json:", len(states), "states; example:", next(iter(states.items())))
    def run(prefix):
        i = 0
        def var():
            nonlocal i; v = 0; sh = 0
            while True:
                b = data[i]; i += 1; v |= (b & 0x7f) << sh; sh += 7
                if not b & 0x80: return v
        def cont(entries, maxind):
            nonlocal i
            bits = data[i]; i += 1
            if bits == 0:
                v = var()
                if prefix: assert var() == 0
                return [v] * entries
            pal = None
            if bits <= maxind: pal = [var() for _ in range(var())]
            per = 64 // bits; longs = -(-entries // per)
            if prefix: assert var() == longs, "length mismatch"
            out = []
            for li in range(longs):
                L = struct.unpack(">Q", data[i:i+8])[0]; i += 8
                for k in range(per):
                    if len(out) < entries:
                        v = (L >> (k * bits)) & ((1 << bits) - 1); out.append(pal[v] if pal is not None else v)
            return out
        secs = []
        while i < len(data):
            i += 2; secs.append(cont(4096, 8)); cont(64, 3)
        return secs, i
    for prefix in (False, True):
        try:
            secs, used = run(prefix)
            ok = used == len(data)
            print(f"layout {'with' if prefix else 'without'} array-length prefix: {len(secs)} sections, {used}/{len(data)} bytes -> {'OK' if ok else 'no'}")
            if ok:
                names = {}
                for sec in secs:
                    for sid in sec: names[states.get(sid, ('?id%d' % sid,))[0]] = names.get(states.get(sid, ('?id%d' % sid,))[0], 0) + 1
                print("most common blocks:", sorted(names.items(), key=lambda kv: -kv[1])[:12])
                print("unknown state ids:", sum(1 for sec in secs for sid in sec if sid not in states))
                break
        except Exception as e:
            print(f"layout {'with' if prefix else 'without'} prefix failed: {e!r}")
finally:
    p.terminate()
