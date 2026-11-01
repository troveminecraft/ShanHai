import sys, unittest, copy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from forest_profile import enable, palette

class ForestProfileTest(unittest.TestCase):
    def test_old_recipe_exact_prefix(self):
        old=palette({'config':{}})
        self.assertEqual((len(old['palette']),len(old['biomeNames'])),(179,58))
        self.assertNotIn('capabilityProfile',old)
        newer=palette(enable({'config':{}},'26.51'))
        self.assertEqual(old['palette'],newer['palette'][:179])
        self.assertEqual(old['biomeNames'],newer['biomeNames'][:58])
    def test_unsupported_names_never_enter_old_registry(self):
        common=palette(enable({'config':{}},'26.20'))
        modern=palette(enable({'config':{}},'26.51'))
        self.assertEqual((len(common['palette']),len(common['biomeNames'])),(223,62))
        self.assertEqual((len(modern['palette']),len(modern['biomeNames'])),(239,63))
        for name in ['poplar','red_shrub','shelf_mushroom','dappled_forest']:
            self.assertNotIn(name,str(common))
        self.assertIn('dappled_forest',modern['biomeNames'])
        self.assertEqual(common['palette'],modern['palette'][:223])
    def test_invalid_capabilities_are_not_silent_fallback(self):
        for data in [{'config':{'forestEcologyRevision':1}},
                     {'config':{},'capabilityProfile':'bedrock-26.51-forests-v1'},
                     {'config':{'forestEcologyRevision':1},'capabilityProfile':'future'}]:
            with self.assertRaises(ValueError):palette(data)
        with self.assertRaises(ValueError):enable({'config':{}},'26.40')

if __name__=='__main__':unittest.main()
