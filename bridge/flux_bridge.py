#!/usr/bin/env python3
"""FLUX bridge: polls local sources (Spotify via MPRIS, GitHub, system, custom commands) and pushes PULSE cards to the orb.
Run: python3 flux_bridge.py   (config in bridge.json next to it). Plugins live in ./plugins and expose poll(cfg, ctx) -> list[dict] cards."""
import json, time, os, sys, importlib, urllib.request, traceback, pathlib, base64
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
CFG = json.loads((HERE / 'bridge.json').read_text())

class Device:
    def __init__(s, cfg): s.urls = [cfg['device'], cfg.get('fallback_device', '')]; s.tok = cfg.get('token', ''); s.last_ok = 0
    def _req(s, path, data=None, method='POST', ctype='application/json'):
        for u in s.urls:
            if not u: continue
            try:
                r = urllib.request.Request(u + path, data=data, method=method)
                if data is not None: r.add_header('Content-Type', ctype)
                if s.tok: r.add_header('X-Token', s.tok)
                with urllib.request.urlopen(r, timeout=3) as resp: s.last_ok = time.time(); return resp.read()
            except Exception as e: err = e
        print('[device] unreachable:', err); return None
    def push(s, card): return s._req('/api/pulse', json.dumps(card).encode())
    def image(s, cid, rgb565_bytes): return s._req('/api/pulse/image', json.dumps({'id': cid, 'b64': base64.b64encode(rgb565_bytes).decode()}).encode())
    def remove(s, cid): return s._req(f'/api/pulse?id={cid}', method='DELETE')

def main():
    dev = Device(CFG); plugins = {}; ctx = {'device': dev, 'state': {}}
    for name, pc in CFG.get('plugins', {}).items():
        if not pc.get('enabled'): continue
        try: plugins[name] = (importlib.import_module(f'plugins.{name}'), pc, 0)
        except Exception: print(f'[bridge] plugin {name} failed to load'); traceback.print_exc()
    print('[bridge] running with plugins:', ', '.join(plugins) or 'none')
    base = CFG.get('interval', 5)
    while True:
        now = time.time()
        for name, (mod, pc, last) in list(plugins.items()):
            if now - last < pc.get('interval', base): continue
            plugins[name] = (mod, pc, now)
            try:
                for card in (mod.poll(pc, ctx) or []):
                    img = card.pop('_image', None)
                    dev.push(card)
                    if img: dev.image(card['id'], img)
            except Exception: print(f'[bridge] plugin {name} error'); traceback.print_exc()
        time.sleep(1)

if __name__ == '__main__': main()
