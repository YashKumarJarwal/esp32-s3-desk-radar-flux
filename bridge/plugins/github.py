"""GitHub counts via REST API. Needs a token with notifications + repo read scope (env GITHUB_TOKEN or token_file)."""
import os, json, urllib.request, pathlib
def _tok(cfg):
    t = os.environ.get(cfg.get('token_env', 'GITHUB_TOKEN'), '')
    if not t:
        f = pathlib.Path(os.path.expanduser(cfg.get('token_file', '~/.config/flux-bridge/github_token')))
        if f.exists(): t = f.read_text().strip()
    return t
def _get(url, tok):
    r = urllib.request.Request(url, headers={'Authorization': f'Bearer {tok}', 'Accept': 'application/vnd.github+json', 'User-Agent': 'flux-bridge'})
    with urllib.request.urlopen(r, timeout=10) as resp: return json.loads(resp.read()), resp.headers
def poll(cfg, ctx):
    tok = _tok(cfg)
    if not tok: print('[github] no token configured'); return []
    notif, _ = _get('https://api.github.com/notifications?per_page=50', tok)
    review, _ = _get('https://api.github.com/search/issues?q=is:pr+is:open+review-requested:@me', tok)
    mine, _ = _get('https://api.github.com/search/issues?q=is:pr+is:open+author:@me', tok)
    n, rv, my = len(notif), review.get('total_count', 0), mine.get('total_count', 0)
    st = ctx['state'].setdefault('github', {}); prev = st.get('n', 0); st['n'] = n
    card = {'id': 'github', 'icon': 'github', 'title': 'GitHub', 'text': f'{n} notif - {rv} to review - {my} open PRs', 'style': 'pulse' if rv else 'static',
            'color': '#c9d1d9' if not rv else '#f59e0b', 'progress': -1, 'ttl': cfg.get('interval', 120) * 3, 'overlay': rv > 0 or n > prev}
    if n > prev: card['takeover'] = 6; card['priority'] = 2
    return [card]
