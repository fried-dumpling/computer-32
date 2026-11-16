# fix_cpu32.py <cpu-32.dig> PIN=NET ...
# Joins an In pin of cpu-32 that is left unconnected in cpu-test.dig to the internal net NET, with a
# tunnel 20 away in the direction the pin points (the same shape as the other pins: pin - wire - tunnel).
# cpu-test.dig does not need these pins: its memory/IO harness drives the nets directly (D_mid -> D_amid,
# D_LD -> D_responce loopbacks, I_responce, D_recieved, MMU-D outputs), and Strip cuts those links out.
import re, sys

f, pairs = sys.argv[1], [a.split('=', 1) for a in sys.argv[2:]]
s = open(f, encoding='utf-8', newline='').read()
wires = re.findall(r'<wire>\s*<p1 x="(-?\d+)" y="(-?\d+)"/>\s*<p2 x="(-?\d+)" y="(-?\d+)"/>', s)
ends = {(int(w[0]), int(w[1])) for w in wires} | {(int(w[2]), int(w[3])) for w in wires}
pins = {}
for b in re.findall(r'<visualElement>\s*<elementName>In</elementName>.*?</visualElement>', s, re.S):
    lab = re.search(r'<string>Label</string>\s*<string>([^<]*)<', b)
    if lab:
        rot = re.search(r'<rotation rotation="(\d)"/>', b)
        pins[lab.group(1)] = tuple(map(int, re.search(r'<pos x="(-?\d+)" y="(-?\d+)"/>', b).groups())) + (int(rot.group(1)) if rot else 0,)
STEP = {0: (20, 0), 1: (0, -20), 2: (-20, 0), 3: (0, 20)}   # the way an In with that rotation points
E, W = [], []
for pin, net in pairs:
    if pin not in pins: sys.exit('fix_cpu32: no In %s' % pin)
    x, y, rot = pins[pin]
    if (x, y) in ends: sys.exit('fix_cpu32: In %s is already wired (fixed in cpu-test? remove it from the list)' % pin)
    tx, ty = x + STEP[rot][0], y + STEP[rot][1]
    r = '' if rot == 0 else ('        <entry>\n          <string>rotation</string>\n'
                             '          <rotation rotation="%d"/>\n        </entry>\n' % rot)
    E.append('    <visualElement>\n      <elementName>Tunnel</elementName>\n      <elementAttributes>\n%s'
             '        <entry>\n          <string>NetName</string>\n          <string>%s</string>\n        </entry>\n'
             '      </elementAttributes>\n      <pos x="%d" y="%d"/>\n    </visualElement>\n' % (r, net, tx, ty))
    W.append('    <wire>\n      <p1 x="%d" y="%d"/>\n      <p2 x="%d" y="%d"/>\n    </wire>\n' % (tx, ty, x, y))
    print('fix_cpu32: In %s -> %s' % (pin, net))
s = s.replace('  </visualElements>', ''.join(E) + '  </visualElements>', 1)
s = s.replace('  </wires>', ''.join(W) + '  </wires>', 1)
open(f, 'w', encoding='utf-8', newline='').write(s)
