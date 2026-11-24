#!/usr/bin/env python3
"""One-command OTA: serve the right image, publish cmd/ota, watch the result.

  python3 tools/ota/push.py --broker 10.0.0.2 --user ha --password secret \
      --device-id 768722 --firmware-dir firmware/flash

No dependencies beyond the Python 3 standard library (tiny built-in MQTT
3.1.1 client).  The script reads the retained `ota/status` to learn which bank
is running, serves the *other* image (`petfeeder_a.bin` / `petfeeder_b.bin`)
over HTTP from this machine, publishes `cmd/ota` with the SHA-512, then prints
progress until the device reboots into the new bank (or reports an error).
"""
import argparse
import hashlib
import json
import os
import socket
import struct
import sys
import threading
import time
import functools
import http.server


# ---------------------------------------------------------------- MQTT ----
def _enc_str(s):
    b = s.encode()
    return struct.pack(">H", len(b)) + b


def _enc_len(n):
    out = bytearray()
    while True:
        d = n % 128
        n //= 128
        out.append(d | (0x80 if n else 0))
        if not n:
            return bytes(out)


def build_connect(client_id, user, password, keepalive=30):
    flags = 0x02
    payload = _enc_str(client_id)
    if user:
        flags |= 0x80
        payload += _enc_str(user)
        if password is not None:
            flags |= 0x40
            payload += _enc_str(password)
    var = _enc_str("MQTT") + bytes([4, flags]) + struct.pack(">H", keepalive)
    body = var + payload
    return b"\x10" + _enc_len(len(body)) + body


def build_subscribe(pid, topic):
    body = struct.pack(">H", pid) + _enc_str(topic) + b"\x00"
    return b"\x82" + _enc_len(len(body)) + body


def build_publish(topic, payload, retain=False):
    body = _enc_str(topic) + payload
    return bytes([0x30 | (1 if retain else 0)]) + _enc_len(len(body)) + body


class Mqtt:
    def __init__(self, host, port, user, password):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.sendall(build_connect("petfeeder-ota-%d" % os.getpid(), user, password))
        t, data = self.read_packet()
        if t != 2 or len(data) < 2 or data[1] != 0:
            raise SystemExit("MQTT connect refused (code %s)" % (data[1] if len(data) > 1 else "?"))
        self.sock.settimeout(1.0)

    def _recv_exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("broker closed connection")
            buf += chunk
        return buf

    def read_packet(self):
        first = self._recv_exact(1)[0]
        mult, length = 1, 0
        while True:
            b = self._recv_exact(1)[0]
            length += (b & 0x7F) * mult
            mult *= 128
            if not b & 0x80:
                break
        return first >> 4, self._recv_exact(length) if length else b""

    def subscribe(self, topic):
        self.sock.sendall(build_subscribe(1, topic))

    def publish(self, topic, payload, retain=False):
        self.sock.sendall(build_publish(topic, payload, retain))

    def poll(self):
        """Return (topic, payload) for one PUBLISH, or None on timeout."""
        try:
            t, data = self.read_packet()
        except socket.timeout:
            self.sock.sendall(b"\xc0\x00")  # keep-alive ping
            return None
        if t != 3:
            return None
        tlen = struct.unpack(">H", data[:2])[0]
        return data[2:2 + tlen].decode(), data[2 + tlen:]


# ---------------------------------------------------------------- tool ----
def lan_ip(broker):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((broker, 1))
        return s.getsockname()[0]
    finally:
        s.close()


def serve(directory, port):
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=directory)
    handler.log_message = lambda *a, **k: None
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", port), handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def other_image(bank):
    return "petfeeder_b.bin" if bank == "A" else "petfeeder_a.bin"


def run(args):
    mq = Mqtt(args.broker, args.port, args.user, args.password)
    status_topic = "petfeeder/%s/ota/status" % args.device_id
    mq.subscribe(status_topic)

    bank = None
    deadline = time.time() + 15
    while bank is None and time.time() < deadline:
        msg = mq.poll()
        if msg and msg[0] == status_topic:
            bank = json.loads(msg[1]).get("bank")
    if bank not in ("A", "B"):
        raise SystemExit("no ota/status from device %s (is it online?)" % args.device_id)

    image = args.image or other_image(bank)
    path = os.path.join(args.firmware_dir, image)
    if not os.path.isfile(path):
        raise SystemExit("missing %s" % path)
    with open(path, "rb") as f:
        sha512 = hashlib.sha512(f.read()).hexdigest()

    serve(args.firmware_dir, args.http_port)
    url = "http://%s:%d/%s" % (args.host_ip or lan_ip(args.broker), args.http_port, image)
    cmd = json.dumps({"url": url, "sha512": sha512})
    print("device on bank %s -> pushing %s" % (bank, image))
    print("url    %s" % url)
    print("sha512 %s" % sha512)
    mq.publish("petfeeder/%s/cmd/ota" % args.device_id, cmd.encode())

    last = None
    end = time.time() + args.timeout
    while time.time() < end:
        msg = mq.poll()
        if not msg or msg[0] != status_topic:
            continue
        st = json.loads(msg[1])
        line = "%s %s%% %s bank=%s" % (st.get("state"), st.get("pct"), st.get("error", ""), st.get("bank"))
        if line != last:
            print(line)
            last = line
        if st.get("state") == "error":
            raise SystemExit(1)
        if st.get("bank") not in (None, bank) and st.get("state") in ("idle", "success"):
            print("OK: now running bank %s" % st["bank"])
            return
    raise SystemExit("timeout waiting for the device")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--broker", required=True)
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--user")
    ap.add_argument("--password")
    ap.add_argument("--device-id", required=True)
    ap.add_argument("--firmware-dir", default="firmware/flash")
    ap.add_argument("--image", help="override: petfeeder_a.bin or petfeeder_b.bin")
    ap.add_argument("--http-port", type=int, default=8000)
    ap.add_argument("--host-ip", help="this machine's LAN IP (auto-detected by default)")
    ap.add_argument("--timeout", type=int, default=300)
    run(ap.parse_args())


if __name__ == "__main__":
    sys.exit(main())
