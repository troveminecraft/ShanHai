"""Stage-weighted creation progress; advances only with completed work."""
import json, os, tempfile
from pathlib import Path

class ProgressReporter:
    def __init__(self, path=None, start=0., end=1.):
        self.path=Path(path) if path else None
        self.start=float(start);self.end=float(end);self.value=self.start
        self.text='正在准备世界'
    def stage(self,start,end,text):
        self.start=float(start);self.end=float(end)
        self.update(text,0.)
    def update(self,text=None,fraction=None):
        if text is not None:self.text=text
        if fraction is not None:
            value=self.start+(self.end-self.start)*max(0.,min(1.,float(fraction)))
            self.value=max(self.value,min(.995,value))
        if not self.path:return
        # Unique files also support a caller reporting completed parallel tiles.
        fd,name=tempfile.mkstemp(prefix='.progress-',suffix='.tmp',dir=self.path.parent)
        try:
            with os.fdopen(fd,'w',encoding='utf-8') as f:
                json.dump(dict(state='running',phase=self.text,progress=self.value,
                               progressMode='stage-weighted'),f,ensure_ascii=False)
            os.replace(name,self.path)
        finally:
            if os.path.exists(name):os.unlink(name)

def add_arguments(parser):
    parser.add_argument('--status',type=Path)
    parser.add_argument('--progress-start',type=float,default=0.)
    parser.add_argument('--progress-end',type=float,default=1.)

def from_arguments(args):
    return ProgressReporter(args.status,args.progress_start,args.progress_end)
