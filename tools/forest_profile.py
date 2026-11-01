"""Immutable target capabilities and append-only terrain palette selection."""
from copy import deepcopy
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
TARGETS={'26.20':'bedrock-26.20-forests-v1','26.51':'bedrock-26.51-forests-v1'}

def enable(data,target):
    if target not in TARGETS: raise ValueError('Unsupported forest game target: '+str(target))
    data['capabilityProfile']=TARGETS[target]
    data['config']['forestEcologyRevision']=2
    return data

def palette(data):
    contract=json.loads((ROOT/'data/palette-contract.json').read_bytes())
    name=data.get('capabilityProfile')
    revision=data.get('config',{}).get('forestEcologyRevision',0)
    if name is None:
        if revision: raise ValueError('Forest recipe lacks its target capabilities')
        name='elevated'
    elif name not in TARGETS.values() or revision not in (1,2):
        raise ValueError('Invalid forest capability profile or recipe revision')
    sizes=contract['profiles'][name]
    result={k:deepcopy(contract[k][:sizes[k]]) for k in ['palette','biomeNames']}
    if name in TARGETS.values():result['capabilityProfile']=name
    return result
