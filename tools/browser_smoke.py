#!/usr/bin/env python3
"""Headless browser smoke test for Emscripten game builds.

Serves a WASM build directory over localhost, loads each named game in
headless Chromium (via Playwright), and asserts the game's
requestAnimationFrame loop reaches a running frame.

The engine sets ``window.__umbraFrameCount`` on every completed frame
(see ``Game::mainLoop`` in Engine/Core/SDLApp.cpp), so reaching the
threshold proves SDL init + window/canvas creation + update + render +
present all succeeded, and that the loop is continuously running rather
than doing a single one-shot init.

Usage:
    python3 tools/browser_smoke.py <build-dir> <game-name> [<game-name> ...]
    python3 tools/browser_smoke.py <build-dir> --targets-file cmake/smoke_targets.txt

Exit code 0 if every game reached a running frame, 1 otherwise.

--targets-file reads target names from a file (one per line, '#' comment
lines skipped) and smokes the ``*_game`` entries. The CI jobs pass
cmake/smoke_targets.txt - the same canonical list that drives the native
smoke ctest entries - so the browser game list can never drift from the
native one. The file also carries ``*_example`` names, which the WASM build
covers separately via its representative example subset; this pass is
scoped to the shipped games.
"""

import argparse
import os
import sys
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

FRAMES_REQUIRED = 3  # prove the loop keeps running, not a one-shot init
PAGE_TIMEOUT_MS = 20000

# Emscripten's SDL2 port looks for an existing <canvas id="canvas"> (see
# dependencies/sdl/src/video/emscripten/SDL_emscriptenvideo.c - the single
# canonical vendored SDL tree), so the shell
# must provide one - AND must bind it as Module.canvas before the game
# script runs. Without the binding, EGL context creation
# (_eglCreateContext -> Browser.getCanvas() -> Module["canvas"]) finds no
# canvas and the game dies before its first frame. The default Emscripten
# shell binds Module.canvas upfront; this shell mirrors that. The async
# script tag also mirrors Emscripten's default shell.
HTML_SHELL = """<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>{name}</title>
<style>body {{ margin: 0; background: #000; }}</style>
</head>
<body>
<canvas id="canvas"></canvas>
<script>
  var Module = {{ canvas: document.getElementById('canvas') }};
</script>
<script async src="{name}.js"></script>
</body>
</html>
"""


class _Handler(SimpleHTTPRequestHandler):
    # Pin the MIME types Emscripten cares about: instantiateStreaming rejects
    # .wasm unless it is served as application/wasm, and the .data preload is
    # fetched over XHR. Don't rely on the host's /etc/mime.types.
    extensions_map = {
        **SimpleHTTPRequestHandler.extensions_map,
        ".js": "text/javascript",
        ".wasm": "application/wasm",
        ".data": "application/octet-stream",
        ".mem": "application/octet-stream",
    }

    def log_message(self, *args):  # keep the smoke output quiet
        pass


def read_targets_file(path):
    """Read a targets list file: one name per line, '#' comment lines skipped."""
    names = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            names.append(line)
    return names


def write_shell(build_dir, name):
    with open(os.path.join(build_dir, f"{name}.html"), "w", encoding="utf-8") as f:
        f.write(HTML_SHELL.format(name=name))


def serve(build_dir):
    handler = partial(_Handler, directory=build_dir)
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    port = httpd.server_address[1]
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return httpd, port


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("build_dir")
    ap.add_argument("games", nargs="*", help="game target names to smoke")
    ap.add_argument(
        "--targets-file",
        help="read target names from this file (one per line, '#' comments "
        "skipped) and smoke only the *_game entries - used by CI with "
        "cmake/smoke_targets.txt so the browser game list tracks the native "
        "smoke list exactly",
    )
    args = ap.parse_args()

    if args.targets_file and args.games:
        ap.error("provide either --targets-file or game names, not both")
    if args.targets_file:
        try:
            names = read_targets_file(args.targets_file)
        except OSError as exc:
            print(f"ERROR: cannot read {args.targets_file}: {exc}", file=sys.stderr)
            return 1
        args.games = [n for n in names if n.endswith("_game")]
        if not args.games:
            print(
                f"ERROR: no *_game entries found in {args.targets_file}",
                file=sys.stderr,
            )
            return 1

    build_dir = os.path.abspath(args.build_dir)
    if not os.path.isdir(build_dir):
        print(f"ERROR: build dir not found: {build_dir}", file=sys.stderr)
        return 1

    for name in args.games:
        for ext in (".js", ".wasm"):
            path = os.path.join(build_dir, f"{name}{ext}")
            if not os.path.exists(path):
                print(f"ERROR: missing {path}", file=sys.stderr)
                return 1

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print(
            "ERROR: Playwright is not installed. Run:\n"
            "  pip install playwright && playwright install --with-deps chromium",
            file=sys.stderr,
        )
        return 1

    for name in args.games:
        write_shell(build_dir, name)

    httpd, port = serve(build_dir)
    base = f"http://127.0.0.1:{port}"
    failed = False

    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(
                headless=True,
                # Software WebGL (SwiftShader) so SDL_Renderer's GLES2 backend
                # works in headless CI without a GPU. --enable-unsafe-swiftshader
                # is required by newer Chromium (M139+) which blocks software
                # fallback by default.
                args=["--use-angle=swiftshader", "--enable-unsafe-swiftshader"],
            )
            try:
                for name in args.games:
                    console = []
                    page = browser.new_page()
                    page.on("console", lambda m: console.append(m.text))
                    page.on("pageerror", lambda e: console.append(f"PAGE ERROR: {e}"))
                    try:
                        page.goto(f"{base}/{name}.html", timeout=PAGE_TIMEOUT_MS)
                        page.wait_for_function(
                            f"window.__umbraFrameCount >= {FRAMES_REQUIRED}",
                            timeout=PAGE_TIMEOUT_MS,
                        )
                        count = page.evaluate("window.__umbraFrameCount")
                        print(f"PASS {name}: reached {count} running frames")
                    except Exception as exc:  # noqa: BLE001 - report, then continue
                        failed = True
                        print(f"FAIL {name}: {exc}", file=sys.stderr)
                        for line in console[-30:]:
                            print(f"  [console] {line}", file=sys.stderr)
                    finally:
                        page.close()
            finally:
                browser.close()
    finally:
        httpd.shutdown()

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
