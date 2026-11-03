"""Host CPU/memory as a progress card (no deps)."""
import os
def poll(cfg, ctx):
    st = ctx['state'].setdefault('sys', {})
    with open('/proc/stat') as f: a = [int(x) for x in f.readline().split()[1:8]]
    idle, total = a[3] + a[4], sum(a); pi, pt = st.get('idle', idle), st.get('total', total); st['idle'], st['total'] = idle, total
    cpu = int(100 * (1 - (idle - pi) / max(1, total - pt))) if pt else 0
    mem = {}
    with open('/proc/meminfo') as f:
        for line in f: k, v = line.split(':'); mem[k] = int(v.split()[0])
    used = int(100 * (1 - mem['MemAvailable'] / mem['MemTotal']))
    return [{'id': 'system', 'icon': 'ci', 'title': os.uname().nodename, 'text': f'CPU {cpu}%  MEM {used}%', 'style': 'static', 'color': '#60a5fa', 'progress': cpu, 'ttl': cfg.get('interval', 10) * 3}]
