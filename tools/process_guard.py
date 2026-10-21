"""Bounded helper execution. The client additionally owns the complete Windows Job."""
import os, signal, subprocess, tempfile
from pathlib import Path

def launch_options():
    if os.name=='nt': return {'creationflags': subprocess.CREATE_NO_WINDOW}
    return {'start_new_session': True}

def stop_tree(process):
    if process.poll() is not None: return
    if os.name=='nt':
        try: subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                       timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
        except (OSError,subprocess.TimeoutExpired): pass
    else:
        try: os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError: pass
    if process.poll() is None: process.kill()
    process.wait(timeout=10)

def tail(stream):
    stream.flush();stream.seek(0,2);length=stream.tell()
    stream.seek(max(0,length-8192))
    return stream.read().decode('utf-8',errors='replace').strip()

def run_checked(args, *, timeout=300, stage='helper', input=None,
                capture_output=False, cwd=None, env=None):
    """No stderr PIPE deadlock; retain a bounded error tail for UI/diagnostics."""
    with tempfile.TemporaryFile() as errors, tempfile.TemporaryFile() as output:
        try:
            process=subprocess.Popen(args,stdin=subprocess.PIPE if input is not None else subprocess.DEVNULL,
                stdout=output if capture_output else None,stderr=errors,cwd=cwd,env=env,**launch_options())
        except OSError as error:
            raise RuntimeError(f'{stage}: cannot start {Path(args[0]).name}: {error}') from error
        try:
            process.communicate(input.encode('utf-8') if isinstance(input,str) else input,timeout=timeout)
        except subprocess.TimeoutExpired as error:
            stop_tree(process)
            raise RuntimeError(f'{stage}: timed out after {timeout:g}s; owned helper stopped. {tail(errors)}') from error
        if process.returncode:
            raise RuntimeError(f'{stage}: {Path(args[0]).name} exited {process.returncode}: {tail(errors)}')
        output.seek(0)
        return subprocess.CompletedProcess(args,process.returncode,
            output.read().decode('utf-8') if capture_output else None,tail(errors))
