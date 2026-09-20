#!/usr/bin/env python3
"""Minimal HTTP server that supports Range requests for OTA delivery.

--fail-once-at N: the first plain GET is cut after N body bytes (headers
promise the full length) so the device must resume with a Range request.
"""

import argparse
import os
import socket
import sys
from http.server import HTTPServer, SimpleHTTPRequestHandler


class RangeHTTPRequestHandler(SimpleHTTPRequestHandler):
    fail_once_at = None
    fail_once_done = False

    def do_GET(self):
        path = self.translate_path(self.path)
        if not os.path.isfile(path):
            super().do_GET()
            return

        file_size = os.path.getsize(path)
        range_header = self.headers.get("Range")

        if range_header is None:
            cut = RangeHTTPRequestHandler.fail_once_at
            if cut is not None and not RangeHTTPRequestHandler.fail_once_done:
                RangeHTTPRequestHandler.fail_once_done = True
                self.send_response(200)
                self.send_header("Content-Type", self.guess_type(path))
                self.send_header("Content-Length", str(file_size))
                self.send_header("Accept-Ranges", "bytes")
                self.send_header("Connection", "close")
                self.end_headers()
                with open(path, "rb") as f:
                    self.wfile.write(f.read(cut))
                self.wfile.flush()
                sys.stderr.write("fault: closed after %d bytes\n" % cut)
                self.connection.shutdown(socket.SHUT_RDWR)
                self.close_connection = True
                return
            super().do_GET()
            return

        try:
            range_spec = range_header.strip().removeprefix("bytes=")
            start_str, end_str = range_spec.split("-", 1)
            start = int(start_str)
            end = int(end_str) if end_str else file_size - 1
        except (ValueError, AttributeError):
            self.send_error(416, "Invalid Range")
            return

        if start >= file_size or end >= file_size or start > end:
            self.send_error(416, "Range Not Satisfiable")
            return

        length = end - start + 1
        self.send_response(206)
        ctype = self.guess_type(path)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(length))
        self.send_header("Content-Range", f"bytes {start}-{end}/{file_size}")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Connection", "close")
        self.end_headers()

        with open(path, "rb") as f:
            f.seek(start)
            self.wfile.write(f.read(length))
        self.wfile.flush()

    def log_message(self, format, *args):
        if os.environ.get("RANGE_HTTP_QUIET") == "1":
            return
        sys.stderr.write("%s - %s\n" % (self.address_string(), format % args))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, nargs="?", default=8080)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--directory", default=".")
    parser.add_argument("--fail-once-at", type=int, default=None,
                        help="cut the first plain GET after N body bytes (resume test)")
    args = parser.parse_args()

    RangeHTTPRequestHandler.fail_once_at = args.fail_once_at
    os.chdir(args.directory)
    server = HTTPServer((args.bind, args.port), RangeHTTPRequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
