#!/usr/bin/env python3
"""Reset the device (optional) and capture serial output for N seconds. Usage: serial.py [seconds] [--no-reset] [--send 'x']"""
import serial, sys, time, re
secs = float(next((a for a in sys.argv[1:] if re.match(r'^[\d.]+$', a)), 8))
reset = '--no-reset' not in sys.argv
send = None
if '--send' in sys.argv: send = sys.argv[sys.argv.index('--send') + 1]
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.2)
if reset:
    s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
if send:
    time.sleep(0.5); s.write(send.encode())
buf = b''; t0 = time.time()
while time.time() - t0 < secs: buf += s.read(4096)
s.close()
txt = buf.decode('utf-8', 'replace')
txt = re.sub(r'(?i)(pass(word)?|psk)[^\n]{0,40}', r'\1 <masked>', txt)
print(txt)
