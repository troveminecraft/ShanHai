"""Keep diagnostic identities tied to the installable manifests on each platform."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class ReleaseVersionTests(unittest.TestCase):
    def test_diagnostics_match_available_manifests(self):
        header = (ROOT / 'src/ReleaseVersions.h').read_text('utf-8')
        pairs = [('Windows', 'ports/windows-client/manifest.json'),
                 ('Terrain', 'manifest.json'),
                 ('Structures', 'experiments/vanilla-structure-runtime/manifest.json')]
        checked = 0
        for key, name in pairs:
            manifest = ROOT / name
            if not manifest.exists():
                continue  # Public Windows source omits private BDS manifests.
            with self.subTest(product=key):
                match = re.search(r'char\s+' + key + r'\[\]\s*=\s*"([^"]+)"', header)
                self.assertIsNotNone(match)
                self.assertEqual(match.group(1), json.loads(manifest.read_bytes())['version'])
                checked += 1
        self.assertGreaterEqual(checked, 1)

if __name__ == '__main__':
    unittest.main()
