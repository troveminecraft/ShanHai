import hashlib,json,tempfile,unittest
from pathlib import Path
from check_world import check
from level_dat import write_file

class WorldPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.w=Path(self.temp.name)
        r=self.w/'karma';(r/'native').mkdir(parents=True)
        self.data=dict(version='test-kernel',seedManifest=dict(terrainSeed='-9223372036854775808',ready=True,riversReady=True))
        b=json.dumps(self.data).encode();(r/'native/native-input.json').write_bytes(b)
        self.meta=dict(version='test-kernel',terrainSeed='-9223372036854775808',worldSeed='-9223372036854775808',runtimeFiles={'native/native-input.json':hashlib.sha256(b).hexdigest()})
        p=json.dumps(self.meta).encode();(r/'profile.json').write_bytes(p);self.meta['sha256']=hashlib.sha256(p).hexdigest();(r/'terrain.json').write_text(json.dumps(self.meta))
        self.marker=dict(host='windows-client',terrainSeed='-9223372036854775808',worldSeed='-9223372036854775808',kernel='test-kernel',sha256=self.meta['sha256'])
        (r/'active-world.json').write_text(json.dumps(self.marker));(self.w/'karma-worldgen.json').write_text(json.dumps(self.marker))
        self.tags=dict(LevelName=(8,'世界甲'),RandomSeed=(4,-9223372036854775808),Generator=(3,2),FlatWorldLayers=(8,'{"preset_id":"TheVoid"}'))
        self.write_level()
    def tearDown(self):self.temp.cleanup()
    def write_level(self):write_file(self.w/'level.dat',10,'',self.tags)
    def test_signed_seed_and_rename(self):
        self.assertTrue(check(self.w)['packageVerified'])
        self.tags['LevelName']=(8,'世界乙');self.write_level()
        self.assertEqual(check(self.w)['name'],'世界乙')
    def test_wrong_seed(self):
        self.tags['RandomSeed']=(4,42);self.write_level()
        with self.assertRaisesRegex(ValueError,'seed mismatch'):check(self.w)
    def test_wrong_generator(self):
        self.tags['Generator']=(3,1);self.write_level()
        with self.assertRaisesRegex(ValueError,'generator changed'):check(self.w)
    def test_corrupt_input(self):
        (self.w/'karma/native/native-input.json').write_bytes(b'')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):check(self.w)
    def test_profile_tamper(self):
        (self.w/'karma/profile.json').write_text('{}')
        with self.assertRaisesRegex(ValueError,'Profile hash'):check(self.w)

if __name__=='__main__':unittest.main()
