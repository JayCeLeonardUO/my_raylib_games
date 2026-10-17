#!/usr/bin/env python3
"""
lua_console.py -- drive the entity_demo Lua state from a terminal.

Interactive REPL. Launches the game automatically if it is not already running:
    ./lua_console.py
    ./lua_console.py --no-launch     # only attach to an already running game

One-shot:
    ./lua_console.py -e 'trait_add(0, "pickup")'
    ./lua_console.py -e 'entity_count()'

From Python:
    from lua_console import GameEngine
    game = GameEngine()
    game.launch()                       # builds if needed, starts the game, waits for the port
    print(game.run_lua('spawn("pickup1", 1, 0, 1)'))
    print(game.run_lua('entity_count()'))
    game.quit()

Protocol: one Lua chunk per line over TCP to 127.0.0.1:7777. Newlines inside a chunk are
sent as 0x1E. The game replies with output lines followed by a line containing 0x04.
"""

import argparse
import os
import socket
import subprocess
import sys
import time

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 7777
RECORD_SEP = "\x1e"
END_OF_RESPONSE = "\x04"

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
TARGET = "entity_demo"
BINARY = os.path.join(PROJECT_ROOT, "build", TARGET)


class GameEngine:
    """Connection to a running entity_demo, with optional launching of the process."""

    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT):
        self.host = host
        self.port = port
        self.sock = None
        self.proc = None
        self._recv_buf = b""

    # ---- lifecycle ----

    def is_listening(self):
        try:
            with socket.create_connection((self.host, self.port), timeout=0.2):
                return True
        except OSError:
            return False

    def build(self):
        subprocess.check_call(
            ["cmake", "-B", "build", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"], cwd=PROJECT_ROOT
        )
        subprocess.check_call(["cmake", "--build", "build", "--target", TARGET], cwd=PROJECT_ROOT)

    def launch(self, build_if_missing=True, timeout=30.0, log_path=None):
        """Start the game (from the project root so assets/ resolves) and wait for its port."""
        if self.is_listening():
            return self
        if not os.path.exists(BINARY):
            if not build_if_missing:
                raise FileNotFoundError(BINARY)
            self.build()
        env = dict(os.environ, ENTITY_DEMO_LUA_PORT=str(self.port))
        log = open(log_path, "ab") if log_path else subprocess.DEVNULL
        self.proc = subprocess.Popen([BINARY], cwd=PROJECT_ROOT, env=env, stdout=log, stderr=log)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(f"{TARGET} exited with code {self.proc.returncode} before listening")
            if self.is_listening():
                return self
            time.sleep(0.1)
        raise TimeoutError(f"{TARGET} did not open port {self.port} within {timeout}s")

    def connect(self):
        if self.sock is None:
            self.sock = socket.create_connection((self.host, self.port), timeout=10)
        return self

    def _socket(self):
        self.connect()
        assert self.sock is not None
        return self.sock

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            finally:
                self.sock = None

    def quit(self, kill_after=3.0):
        """Close the connection and stop the process this object launched, if any."""
        self.close()
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=kill_after)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    # ---- the one function that matters ----

    def run_lua(self, code):
        """Send a Lua chunk and return its output as a string (print lines, return values, or error)."""
        sock = self._socket()
        wire = code.replace("\r\n", "\n").replace("\n", RECORD_SEP)
        sock.sendall((wire + "\n").encode("utf-8"))
        lines = []
        while True:
            line = self._read_line()
            if line is None:
                raise ConnectionError("game closed the connection")
            if line == END_OF_RESPONSE:
                break
            lines.append(line)
        return "\n".join(lines)

    def _read_line(self):
        while b"\n" not in self._recv_buf:
            chunk = self._socket().recv(4096)
            if not chunk:
                return None
            self._recv_buf += chunk
        line, _, self._recv_buf = self._recv_buf.partition(b"\n")
        return line.decode("utf-8", errors="replace")

    def __enter__(self):
        return self.connect()

    def __exit__(self, *_exc):
        self.close()


# ---- CLI ----


def repl(game):
    try:
        import readline  # noqa: F401  (line editing + history when available)
    except ImportError:
        pass
    print(f"connected to {game.host}:{game.port} -- Lua REPL. Ctrl-D to exit. "
          "End a line with '\\' to continue on the next line.")
    pending = []
    while True:
        try:
            prompt = "...  " if pending else "lua> "
            line = input(prompt)
        except EOFError:
            print()
            return
        except KeyboardInterrupt:
            print()
            pending = []
            continue
        if line.endswith("\\"):
            pending.append(line[:-1])
            continue
        pending.append(line)
        code = "\n".join(pending).strip()
        pending = []
        if not code:
            continue
        try:
            out = game.run_lua(code)
        except (ConnectionError, OSError) as e:
            print(f"[connection lost: {e}]")
            return
        if out:
            print(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Remote Lua console for entity_demo")
    ap.add_argument("-e", "--exec", metavar="CODE", action="append",
                    help="run this Lua and exit (repeatable, runs in order)")
    ap.add_argument("--launch", action="store_true",
                    help="(default) start the game if it is not already listening")
    ap.add_argument("--no-launch", action="store_true",
                    help="only connect; fail if the game is not already running")
    ap.add_argument("--port", type=int, default=int(os.environ.get("ENTITY_DEMO_LUA_PORT", DEFAULT_PORT)))
    ap.add_argument("--host", default=DEFAULT_HOST)
    args = ap.parse_args(argv)

    game = GameEngine(args.host, args.port)
    if game.is_listening():
        pass
    elif args.no_launch:
        print(f"nothing listening on {args.host}:{args.port} and --no-launch given.", file=sys.stderr)
        return 1
    else:
        print(f"no game on {args.host}:{args.port}, launching {TARGET}...", file=sys.stderr)
        game.launch()

    try:
        if args.exec:
            for code in args.exec:
                out = game.run_lua(code)
                if out:
                    print(out)
        else:
            repl(game)
    finally:
        game.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
