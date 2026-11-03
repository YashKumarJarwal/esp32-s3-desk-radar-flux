"""Now-playing card from any MPRIS player (Spotify desktop, browsers, VLC...) via dbus-send; no API keys needed."""
import subprocess, re, io, urllib.request
def _dbus(player, prop):
    out = subprocess.run(['dbus-send', '--session', '--print-reply', f'--dest={player}', '/org/mpris/MediaPlayer2',
                          'org.freedesktop.DBus.Properties.Get', 'string:org.mpris.MediaPlayer2.Player', f'string:{prop}'],
                         capture_output=True, text=True, timeout=3)
    return out.stdout if out.returncode == 0 else ''
def _players():
    out = subprocess.run(['dbus-send', '--session', '--print-reply', '--dest=org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus.ListNames'], capture_output=True, text=True, timeout=3).stdout
    names = re.findall(r'org\.mpris\.MediaPlayer2\.[\w.]+', out)
    return sorted(names, key=lambda n: 0 if 'spotify' in n else 1)
def _meta(player):
    raw = _dbus(player, 'Metadata'); m = {}
    for key in ('xesam:title', 'xesam:album', 'mpris:artUrl'):
        mm = re.search(rf'string "{key}"\s+variant\s+string "([^"]*)"', raw); m[key] = mm.group(1) if mm else ''
    mm = re.search(r'string "xesam:artist"\s+variant\s+array \[\s*string "([^"]*)"', raw); m['xesam:artist'] = mm.group(1) if mm else ''
    mm = re.search(r'string "mpris:length"\s+variant\s+(?:u?int64) (\d+)', raw); m['length'] = int(mm.group(1)) if mm else 0
    return m
def _art(url, cache):
    if not url or url in cache: return cache.get(url)
    try:
        from PIL import Image
        data = urllib.request.urlopen(url, timeout=5).read() if url.startswith('http') else open(url.replace('file://', ''), 'rb').read()
        im = Image.open(io.BytesIO(data)).convert('RGB').resize((64, 64))
        buf = bytearray()
        for r, g, b in im.getdata():
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); buf += v.to_bytes(2, 'big')
        cache[url] = bytes(buf)
    except Exception as e: print('[spotify] art failed:', e); cache[url] = None
    return cache[url]
def poll(cfg, ctx):
    st = ctx['state'].setdefault('spotify', {'last': None, 'art': {}})
    players = _players()
    if not players:
        if st['last']: ctx['device'].remove('music'); st['last'] = None
        return []
    p = players[0]; status = 'Playing' if 'Playing' in _dbus(p, 'PlaybackStatus') else 'Paused'
    m = _meta(p); title = m['xesam:title'] or 'Unknown'; artist = m['xesam:artist']
    pos = re.search(r'int64 (\d+)', _dbus(p, 'Position')); pos = int(pos.group(1)) if pos else 0
    prog = int(pos * 100 / m['length']) if m['length'] else -1
    key = (title, artist); changed = key != st['last']; st['last'] = key
    card = {'id': 'music', 'icon': 'music', 'title': title[:22], 'text': (artist + (' - ' + m['xesam:album'] if m['xesam:album'] else ''))[:46],
            'style': 'play' if status == 'Playing' else 'static', 'color': '#1db954' if status == 'Playing' else '#7f92b0', 'progress': prog, 'ttl': 60, 'overlay': status == 'Playing'}
    if changed and cfg.get('takeover_on_change', 0): card['takeover'] = cfg['takeover_on_change']; card['priority'] = 3
    if changed and cfg.get('art', True):
        art = _art(m['mpris:artUrl'], st['art'])
        if art: card['_image'] = art
    return [card]
