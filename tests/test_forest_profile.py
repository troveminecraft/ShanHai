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
        self.assertEqual((len(common['palette']),len(common['biomeNames'])),(251,62))
        self.assertEqual((len(modern['palette']),len(modern['biomeNames'])),(251,63))
        for name in ['poplar','red_shrub','shelf_mushroom','dappled_forest']:
            self.assertNotIn(name,str(common))
        self.assertIn('dappled_forest',modern['biomeNames'])
        self.assertEqual(common['palette'][:223],modern['palette'][:223])
        self.assertEqual(common['palette'][223:239],['air']*16)
        self.assertEqual(common['palette'][239:],modern['palette'][239:])
    def test_saved_v1_contracts_are_unchanged(self):
        for target,count,biomes in [('26.20',223,62),('26.51',239,63)]:
            old=palette({'config':{'forestEcologyRevision':2},'capabilityProfile':f'bedrock-{target}-forests-v1'})
            self.assertEqual((len(old['palette']),len(old['biomeNames'])),(count,biomes))
        for profile,revision in [('bedrock-26.20-forests-v1',3),('bedrock-26.20-forests-v2',2)]:
            with self.assertRaises(ValueError):palette({'config':{'forestEcologyRevision':revision},'capabilityProfile':profile})
    def test_invalid_capabilities_are_not_silent_fallback(self):
        for data in [{'config':{'forestEcologyRevision':1}},
                     {'config':{},'capabilityProfile':'bedrock-26.51-forests-v1'},
                     {'config':{'forestEcologyRevision':1},'capabilityProfile':'future'}]:
            with self.assertRaises(ValueError):palette(data)
        with self.assertRaises(ValueError):enable({'config':{}},'26.40')

if __name__=='__main__':unittest.main()
