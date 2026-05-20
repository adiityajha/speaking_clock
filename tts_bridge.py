#!/usr/bin/env python3
"""
tts_bridge.py – Speaking Clock TTS Bridge

Reads QEMU semihosting output (piped via stdin) and speaks any time
announcement assembled from the TOKEN/END protocol.

Usage:
    make qemu-tts          (recommended – Makefile handles the pipe)
    qemu-system-arm ... 2>&1 | python3 tts_bridge.py

Platform support:
    macOS   : built-in 'say' command
    Linux   : espeak / espeak-ng
    Windows : PowerShell SpeechSynthesizer
"""

import sys
import subprocess
import platform
import time
import os
import fcntl

# ── Force stdout to blocking mode ───────────────────────────────────────────
try:
    _fd = sys.stdout.fileno()
    _flags = fcntl.fcntl(_fd, fcntl.F_GETFL)
    fcntl.fcntl(_fd, fcntl.F_SETFL, _flags & ~os.O_NONBLOCK)
except Exception:
    pass


def safe_print(*args, **kwargs) -> None:
    """print() wrapper that retries once on BlockingIOError (EAGAIN)."""
    for attempt in range(2):
        try:
            print(*args, **kwargs)
            return
        except BlockingIOError:
            if attempt == 0:
                time.sleep(0.01)
                continue
            break

# ── ANSI colour codes ────────────────────────────────────────────────────────
_USE_COLOUR = sys.stdout.isatty()

def _c(code: str, text: str) -> str:
    return f"\033[{code}m{text}\033[0m" if _USE_COLOUR else text

def BOLD(t):    return _c("1",  t)
def DIM(t):     return _c("2",  t)
def GREEN(t):   return _c("32", t)
def CYAN(t):    return _c("36", t)
def YELLOW(t):  return _c("33", t)
def BLUE(t):    return _c("34", t)
def MAGENTA(t): return _c("35", t)
def RED(t):     return _c("31", t)
def WHITE(t):   return _c("97", t)

# ── Tag → colour map ─────────────────────────────────────────────────────────
TAG_COLOURS = {
    "[MAIN]":       CYAN,
    "[KEY]":        YELLOW,
    "[NTP]":        BLUE,
    "[NTP_CLIENT]": BLUE,
    "[LWIP]":       MAGENTA,
    "[NIC]":        DIM,
    "[SPEECH]":     GREEN,
    "[BRIDGE]":     lambda t: BOLD(GREEN(t)),
    "[PANIC]":      RED,
}

def colourise(line: str) -> str:
    for tag, fn in TAG_COLOURS.items():
        if line.startswith(tag):
            return fn(tag) + line[len(tag):]
    return line

def speak(text: str) -> None:
    os_name = platform.system()
    try:
        if os_name == "Darwin":
            subprocess.run(["say", text], check=True)
        elif os_name == "Linux":
            for cmd in [["espeak-ng", text], ["espeak", text]]:
                try:
                    subprocess.run(cmd, check=True)
                    return
                except FileNotFoundError:
                    continue
            print(_c("31", "[BRIDGE] No TTS engine found – install espeak-ng"))
        elif os_name == "Windows":
            ps_cmd = (
                f"Add-Type -AssemblyName System.Speech; "
                f"$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
                f"$s.Speak('{text}')"
            )
            subprocess.run(["powershell", "-Command", ps_cmd], check=True)
        else:
            print(f"[BRIDGE] Unsupported OS – text: {text}")
    except subprocess.CalledProcessError as e:
        print(RED(f"[BRIDGE] TTS error: {e}"))

def print_banner() -> None:
    w = 62
    border = CYAN("─" * w)
    safe_print()
    safe_print(border)
    safe_print(CYAN("│") + BOLD(WHITE("  Speaking Clock".center(w - 2))) + CYAN("│"))
    safe_print(border)
    safe_print()


def main() -> None:
    print_banner()
    safe_print(DIM("  Watching for TOKEN/END stream from QEMU firmware…"))
    safe_print(DIM("  Press Ctrl+C to stop.\n"))

    tokens: list[str] = []
    announced = 0

    try:
        for raw_line in sys.stdin:
            line = raw_line.rstrip("\r\n")
            stripped = line.strip()

            if not stripped:
                continue

            # ── TOKEN/END protocol ────────────────────────────────────────
            if stripped.startswith("TOKEN "):
                token = stripped[6:].strip()
                if token:
                    tokens.append(token)
                    safe_print("  " + DIM(f"    ▸ TOKEN {token}"), flush=True)
                continue

            if stripped == "END":
                if tokens:
                    safe_print("  " + DIM("    ▸ END"), flush=True)

                    raw_sentence = " ".join(t.lower() for t in tokens)
                    spoken = raw_sentence.capitalize()

                    announced += 1
                    ts = time.strftime("%H:%M:%S")

                    safe_print()
                    safe_print(CYAN("  ┌─ Announcement #%d " % announced) +
                               DIM("(%s) " % ts) +
                               CYAN("─" * max(0, 40 - len(str(announced)) - len(ts))))
                    safe_print(CYAN("  │ ") + BOLD(WHITE("🔊  " + spoken)))
                    safe_print(CYAN("  └" + "─" * 52))
                    safe_print()

                    speak(spoken)
                    tokens.clear()
                continue

            # ── Section dividers ──────────────────────────────────────────
            if "=== Speaking Clock" in stripped:
                safe_print("\n" + CYAN("━" * 62))
                safe_print(BOLD(CYAN("  FIRMWARE BOOT")))
                safe_print(CYAN("━" * 62))

            elif stripped.startswith("[LWIP] DHCP started"):
                safe_print()
                safe_print(CYAN("  ── Network Init " + "─" * 44))

            elif stripped.startswith("[NTP] Ready"):
                safe_print()
                safe_print(CYAN("  ── Ready " + "─" * 51))

            elif stripped.startswith("[KEY] Time request"):
                safe_print()
                safe_print(CYAN("  ── NTP Request " + "─" * 45))

            elif stripped.startswith("[SPEECH] Generating"):
                continue

            elif stripped.startswith("[SPEECH] Done"):
                continue

            # ── Echo all other firmware lines with colour ─────────────────
            safe_print("  " + colourise(stripped), flush=True)

    except KeyboardInterrupt:
        safe_print()
        safe_print(CYAN("━" * 62))
        safe_print(BOLD(WHITE(f"  Session ended — {announced} announcement(s) made.")))
        safe_print(CYAN("━" * 62))
        safe_print()


if __name__ == "__main__":
    main()