import hashlib,json,tempfile,unittest
from pathlib import Path
from check_world import check
from level_dat import write_file
from prepare_world import install_height_pack, HEIGHT_PACK_FOLDER, validate_new_world_recipe

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
    def enable_height(self):
        self.data['config']=dict(generatedMaxYExclusive=512,mountainPeakY=480,seaLevel=64)
        self.save_native_data()
        self.meta.update(maxY=511,buildMaxYExclusive=512,generatedMaxYExclusive=512,buildHeightPack=install_height_pack(self.w))
        self.marker.update(buildMaxYExclusive=512,generatedMaxYExclusive=512)
        self.save_metadata()
    def save_native_data(self):
        data=json.dumps(self.data).encode()
        (self.w/'karma/native/native-input.json').write_bytes(data)
        self.meta['runtimeFiles']['native/native-input.json']=hashlib.sha256(data).hexdigest()
    def save_metadata(self):
        self.meta.pop('sha256',None)
        profile=json.dumps(self.meta).encode();digest=hashlib.sha256(profile).hexdigest()
        (self.w/'karma/profile.json').write_bytes(profile)
        self.meta['sha256']=digest;self.marker['sha256']=digest
        (self.w/'karma/terrain.json').write_text(json.dumps(self.meta))
        for path in (self.w/'karma-worldgen.json',self.w/'karma/active-world.json'):
            path.write_text(json.dumps(self.marker))
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
    def test_legacy_defaults_to_320(self):
        self.assertEqual(check(self.w)['buildMaxYExclusive'],320)
        self.assertEqual(check(self.w)['generatedMaxYExclusive'],320)
    def test_expanded_world_requires_valid_selected_pack(self):
        self.enable_height()
        self.assertEqual(check(self.w)['buildMaxYExclusive'],512)
        self.assertEqual(check(self.w)['generatedMaxYExclusive'],512)
        (self.w/'world_behavior_packs.json').write_text('[]')
        with self.assertRaisesRegex(ValueError,'selection mismatch'):check(self.w)
    def test_height_marker_must_match_profile(self):
        self.enable_height();self.marker['generatedMaxYExclusive']=320;self.save_metadata()
        with self.assertRaisesRegex(ValueError,'Marker height mismatch'):check(self.w)
    def test_dimension_file_tamper_is_detected(self):
        self.enable_height()
        (self.w/'behavior_packs'/HEIGHT_PACK_FOLDER/'dimensions/overworld.json').write_text('{}')
        with self.assertRaisesRegex(ValueError,'Height pack checksum mismatch'):check(self.w)
    def test_duplicate_selection_is_rejected(self):
        self.enable_height()
        path=self.w/'world_behavior_packs.json';refs=json.loads(path.read_bytes());path.write_text(json.dumps(refs+refs))
        with self.assertRaisesRegex(ValueError,'selection mismatch'):check(self.w)
    def test_package_checksum_paths_are_confined(self):
        self.enable_height();self.meta['buildHeightPack']['files']={'../../../level.dat':'invalid'};self.save_metadata()
        with self.assertRaisesRegex(ValueError,'Unsafe height pack file'):check(self.w)
    def test_legacy_max_y_cannot_describe_expanded_generation(self):
        self.enable_height();self.meta['maxY']=319;self.save_metadata()
        with self.assertRaisesRegex(ValueError,'Generated height metadata mismatch'):check(self.w)
    def test_old_native_recipe_cannot_back_expanded_profile(self):
        self.enable_height();self.data['config'].pop('generatedMaxYExclusive');self.save_native_data();self.save_metadata()
        with self.assertRaisesRegex(ValueError,'Native recipe height'):check(self.w)
    def test_expanded_native_recipe_cannot_back_legacy_profile(self):
        self.data['config']=dict(generatedMaxYExclusive=512);self.save_native_data();self.save_metadata()
        with self.assertRaisesRegex(ValueError,'Native recipe height'):check(self.w)
    def test_new_preparation_requires_current_recipe(self):
        config=dict(generatedMaxYExclusive=512,mountainPeakY=480,seaLevel=64)
        validate_new_world_recipe(dict(config=config))
        for key in config:
            invalid=config.copy();invalid.pop(key)
            with self.subTest(missing=key),self.assertRaisesRegex(ValueError,'build_streaming_seed'):
                validate_new_world_recipe(dict(config=invalid))
        for key,bad in [('generatedMaxYExclusive',320),('generatedMaxYExclusive',512.0),('mountainPeakY',0),('seaLevel',31)]:
            invalid=config.copy();invalid[key]=bad
            with self.subTest(field=key,value=bad),self.assertRaisesRegex(ValueError,'build_streaming_seed'):
                validate_new_world_recipe(dict(config=invalid))

if __name__=='__main__':unittest.main()
