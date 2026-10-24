#!/usr/bin/env python3
"""Capture a 45-second crowded-world and six-theme benchmark using standard Python."""
import argparse
import json
import os
from pathlib import Path
import re
import select
import termios
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    args = parser.parse_args()
    output = Path(__file__).resolve().parent.parent / '.build'
    output.mkdir(parents=True, exist_ok=True)
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attributes = termios.tcgetattr(fd)
        attributes[0] = attributes[1] = attributes[3] = 0
        attributes[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attributes[4] = attributes[5] = termios.B115200
        attributes[6][termios.VMIN] = attributes[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attributes)
        termios.tcflush(fd, termios.TCIFLUSH)
        started = False
        deadline = time.monotonic() + 65
        next_request = 0
        buffer = b''
        with (output / 'benchmark-device.txt').open('w') as capture:
            while time.monotonic() < deadline:
                now = time.monotonic()
                if not started and now >= next_request:
                    os.write(fd, b'B\n')
                    next_request = now + 1
                if not select.select([fd], [], [], .25)[0]:
                    continue
                buffer += os.read(fd, 8192)
                if len(buffer) > 65536:
                    raise RuntimeError('Serial input contained no complete lines')
                while b'\n' in buffer:
                    line, buffer = buffer.split(b'\n', 1)
                    text = line.decode('utf8', 'replace').strip()
                    print(text, flush=True)
                    capture.write(text + '\n')
                    capture.flush()
                    if 'BENCH BEGIN' in text:
                        started = True
                    if started and 'BENCH END' in text:
                        values = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', text)}
                        (output / 'benchmark-device.json').write_text(json.dumps({
                            'platform': 'esp32s3', 'duration_ms': 45000,
                            'average_fps': values['frames'] / 45, 'metrics': values,
                        }, indent=2) + '\n')
                        return
            raise RuntimeError('Device benchmark did not complete; transcript retained, no new successful result saved')
    finally:
        os.close(fd)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(str(error))
