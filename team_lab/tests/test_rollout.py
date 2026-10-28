"""Check expanded flash layout and backup guards without touching a device."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class RolloutTests(unittest.TestCase):
    def test_partition_layout(self):
        rows = []
        for line in (ROOT/'firmware/round_wonders/partitions.csv').read_text().splitlines():
            if not line or line.startswith('#'):
                continue
            fields = [field.strip() for field in line.split(',')]
            rows.append((fields[0], int(fields[3], 16), int(fields[4], 16)))
        self.assertEqual(next(size for name, _, size in rows if name == 'app0'), 10*1048576)
        for (_, start, size), (_, following, _) in zip(rows, rows[1:]):
            self.assertLessEqual(start+size, following)
        self.assertEqual(rows[-1][1]+rows[-1][2], 16*1048576)
        self.assertIn('upload.maximum_size=10485760', (ROOT/'scripts/build.sh').read_text())

    def test_full_backup_reuse_and_corruption_guard(self):
        script = (ROOT/'scripts/run.sh').read_text()
        backup_script = script[script.index('backup="$WAVESHARE_ROOT/backups/'):script.index('waveshare_cli upload')]
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            fake = temp/'esptool'
            fake.write_text('#!/usr/bin/env python3\nimport pathlib,sys\n'
                            'assert "read_flash" in sys.argv\n'
                            'pathlib.Path(sys.argv[-1]).write_bytes(bytes(range(256))*65536)\n'
                            'with open(__file__+".calls","a") as f:f.write("read\\n")\n')
            fake.chmod(0o755)
            env = dict(os.environ, WAVESHARE_ROOT=folder, device_mac='testboard', esptool=str(fake), port='mock')
            def run():
                return subprocess.run(['bash', '-euo', 'pipefail', '-c', backup_script], env=env, capture_output=True)
            self.assertEqual(run().returncode, 0)
            prefix = temp/'backups/testboard-before-round-wonders.bin'
            full = temp/'backups/testboard-before-round-wonders-v030-full.bin'
            self.assertEqual(full.stat().st_size, 16*1048576)
            self.assertEqual(prefix.read_bytes(), full.read_bytes()[:6356992])
            self.assertEqual(run().returncode, 0)
            self.assertEqual(Path(str(fake)+'.calls').read_text(), 'read\n')
            # Same-size corruption must fail hash verification, not be overwritten.
            with full.open('r+b') as f:
                f.write(b'X')
            self.assertNotEqual(run().returncode, 0)
            self.assertEqual(Path(str(fake)+'.calls').read_text(), 'read\n')
            full.write_bytes(b'incomplete')
            self.assertNotEqual(run().returncode, 0)

if __name__ == '__main__':
    unittest.main()
