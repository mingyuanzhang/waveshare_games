#!/usr/bin/env python3
"""Trigger a read-only benchmark in Echo, or summarize its captured output.

Uses Python's standard library; serial transport is for macOS/Linux.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import select
import termios
import time

ROOT = Path(__file__).resolve().parent.parent


class Results:
    def __init__(self):
        self.platform = None
        self.expected = 0
        self.rooms = []
        self.finished = False

    def line(self, line):
        if not line.startswith('[bench] '):
            return
        row = next(csv.reader([line[len('[bench] '):]]))
        if not row:
            return
        if row[0] == 'ERROR':
            raise RuntimeError('Device benchmark failed: ' + ','.join(row[1:]))
        if row[0] == 'BEGIN':
            if self.platform is not None or len(row) != 4:
                raise RuntimeError('Unexpected benchmark header.')
            self.platform, self.expected = row[1], int(row[2])
            print('Benchmark on {}: {} rooms, 24 calls and 96 frames per room.'.format(self.platform, self.expected), flush=True)
            print('Room                    Call avg / max    Frame avg / max', flush=True)
        elif row[0] == 'ROOM':
            if self.platform is None or len(row) not in (12, 16):
                raise RuntimeError('Unexpected room record.')
            keys = ('walls', 'calls', 'call_total_us', 'call_max_us', 'frames',
                    'frame_total_us', 'frame_max_us', 'frames_over_40ms', 'max_echoes')
            if len(row) == 16:
                keys += ('render_total_us', 'render_max_us', 'transfer_total_us', 'transfer_max_us')
            record = dict(zip(keys, map(int, row[3:])))
            record.update(room=int(row[1]), name=row[2])
            if record['room'] != len(self.rooms) + 1 or record['calls'] != 24 or record['frames'] != 96:
                raise RuntimeError('Incomplete or out-of-order benchmark workload.')
            self.rooms.append(record)
            print('{:02d} {:18s} {:6.2f} / {:6.2f} ms  {:6.2f} / {:6.2f} ms'.format(
                record['room'], record['name'], record['call_total_us'] / record['calls'] / 1000,
                record['call_max_us'] / 1000, record['frame_total_us'] / record['frames'] / 1000,
                record['frame_max_us'] / 1000), flush=True)
        elif row[0] == 'END':
            if not self.platform or len(self.rooms) != self.expected:
                raise RuntimeError('Benchmark ended before every room was measured.')
            self.finished = True

    def summary(self):
        if not self.finished:
            raise RuntimeError('Benchmark output is incomplete; no successful result was saved.')
        call = max(self.rooms, key=lambda r: r['call_max_us'])
        frame = max(self.rooms, key=lambda r: r['frame_max_us'])
        over = sum(r['frames_over_40ms'] for r in self.rooms)
        print('\n{} rooms measured; {} calls and {} frames.'.format(
            len(self.rooms), sum(r['calls'] for r in self.rooms), sum(r['frames'] for r in self.rooms)))
        print('Slowest sampled call: {:.2f} ms ({})'.format(call['call_max_us'] / 1000, call['name']))
        print('Slowest sampled frame: {:.2f} ms ({})'.format(frame['frame_max_us'] / 1000, frame['name']))
        print('Frames over the 40 ms budget for 25 fps: {}'.format(over))
        if all('render_max_us' in r for r in self.rooms):
            render = max(self.rooms, key=lambda r: r['render_max_us'])
            transfer = max(self.rooms, key=lambda r: r['transfer_max_us'])
            print('Slowest sampled rendering: {:.2f} ms ({})'.format(render['render_max_us'] / 1000, render['name']))
            print('Slowest sampled copy + LCD transfer: {:.2f} ms ({})'.format(transfer['transfer_max_us'] / 1000, transfer['name']))
        if self.platform == 'esp32s3':
            print('Frame timing includes rendering, PSRAM copy, and LCD DMA transfers.')
        else:
            print('Native CPU timings exclude LCD transfer; use the ESP32 run to judge device responsiveness.')
        output = ROOT / '.build' / ('benchmark-device.json' if self.platform == 'esp32s3' else 'benchmark-native.json')
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists():
            old = json.loads(output.read_text())
            if old.get('platform') == self.platform and len(old.get('rooms', [])) == len(self.rooms):
                old_call = max(r['call_max_us'] for r in old['rooms']) / 1000
                old_frame = max(r['frame_max_us'] for r in old['rooms']) / 1000
                print('Previous run: call max {:.2f} ms; frame max {:.2f} ms.'.format(old_call, old_frame))
                output.with_suffix('.previous.json').write_text(output.read_text())
        output.write_text(json.dumps({'platform': self.platform, 'rooms': self.rooms}, indent=2) + '\n')
        print('Saved ' + str(output))


def device(port, results):
    connect_deadline = time.monotonic() + 30
    while True:
        try:
            fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            break
        except FileNotFoundError:
            if time.monotonic() >= connect_deadline:
                raise RuntimeError('USB port did not return: ' + port)
            time.sleep(0.25)
    try:
        attributes = termios.tcgetattr(fd)
        attributes[0] = attributes[1] = attributes[3] = 0
        attributes[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attributes[4] = attributes[5] = termios.B115200
        attributes[6][termios.VMIN] = 0
        attributes[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attributes)
        termios.tcflush(fd, termios.TCIFLUSH)
        print('Starting the on-device check. The game resumes afterward; scores are preserved.', flush=True)
        buffer = b''
        started = time.monotonic()
        next_request = started
        with (ROOT / '.build/benchmark-device.txt').open('w') as capture:
            while not results.finished:
                now = time.monotonic()
                if now - started > 600:
                    raise RuntimeError('Benchmark timed out; its captured output may help diagnose the device.')
                if results.platform is None:
                    if now - started > 20:
                        raise RuntimeError('Benchmark did not start. Check that the new Echo firmware is running and close other serial monitors.')
                    if now >= next_request:
                        os.write(fd, b'B\n')
                        next_request = now + 1
                ready, _, _ = select.select([fd], [], [], 0.25)
                if not ready:
                    continue
                chunk = os.read(fd, 8192)
                if not chunk:
                    continue
                buffer += chunk
                if len(buffer) > 65536:
                    raise RuntimeError('Serial input contained no complete lines.')
                while b'\n' in buffer:
                    line, buffer = buffer.split(b'\n', 1)
                    text = line.decode('utf-8', errors='replace').strip()
                    capture.write(text + '\n')
                    capture.flush()
                    results.line(text)
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port', nargs='?')
    parser.add_argument('--report', type=Path, help='Summarize a saved benchmark transcript instead of opening USB.')
    args = parser.parse_args()
    results = Results()
    if args.report:
        for line in args.report.read_text().splitlines():
            results.line(line.strip())
    elif args.port:
        (ROOT / '.build').mkdir(parents=True, exist_ok=True)
        device(args.port, results)
    else:
        parser.error('Supply a serial port or --report FILE.')
    results.summary()


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(str(error))
