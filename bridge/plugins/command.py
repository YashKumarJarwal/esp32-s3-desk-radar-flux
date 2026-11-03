"""Run any shell command that prints a JSON card (or a list of cards). Easiest way to add a custom integration."""
import subprocess, json
def poll(cfg, ctx):
    out = subprocess.run(cfg['cmd'], shell=True, capture_output=True, text=True, timeout=30).stdout.strip()
    if not out: return []
    d = json.loads(out); return d if isinstance(d, list) else [d]
