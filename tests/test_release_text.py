import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from check_release_text import violations

class ReleaseTextTests(unittest.TestCase):
    def test_handoff_language_is_rejected(self):
        for text in ['等待游戏体验反馈','Gameplay acceptance remains pending.',
                     '这是待用户游戏内验收的测试交付','0.1.13（待游戏内验收）',
                     'Temporary files in O:/仓库/agent-object/2026-10-29/build']:
            self.assertTrue(violations(text),text)
    def test_product_limits_and_developer_options_are_allowed(self):
        for text in ['Known issue: chunk loading can stall during exploration.',
                     '预发布版；已生成区块不会自动重建。',
                     'Use -BdsAcceptance to supply the BDS test record.']:
            self.assertFalse(violations(text),text)
if __name__=='__main__':unittest.main()
