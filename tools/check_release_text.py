"""Reject internal handoff language in product documentation and release notes."""
import argparse
from pathlib import Path
import re

FORBIDDEN = re.compile(
    r'验收|工作区|agent-object|(?:主|侧|下一)对话|待用户|由用户|测试交付|交付用户|'
    r'等待.{0,15}(?:游戏|体验|反馈)|'
    r'(?:gameplay|in-game|visual|new-world|creation/loading).{0,60}acceptance|'
    r'awaiting\s+(?:user|gameplay|native Windows)|'
    r'(?:C:|O:|X:)[\\/].*(?:仓库|Users)[\\/]', re.I)

def violations(text):
    return [(i,line.strip()) for i,line in enumerate(text.splitlines(),1) if FORBIDDEN.search(line)]

def check(path):
    errors=violations(Path(path).read_text('utf-8-sig'))
    if errors:
        raise ValueError('\n'.join(f'{path}:{i}: product wording required: {line}' for i,line in errors))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('paths',nargs='+',type=Path);a=p.parse_args()
    for path in a.paths:check(path)
