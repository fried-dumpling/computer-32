# make_computer.py <harness.dig> <cpu-test.dig> <pins.txt> <out.dig>
# Builds computer-32.dig: the memory/IO harness cut out of cpu-test.dig (harness.dig, made with
# Strip right:-1200) plus one cpu-32.dig instance. Every cpu-32 pin gets a short wire and a tunnel with
# the pin name, so the harness tunnels (I_D-_bus, I_LD, C, ...) connect by name. Pins the harness does
# not use get a copy of the cpu-test In/Out/Clock with that label (same bits, default, high-Z).
# pins.txt is the output of "Pins <any.dig> cpu-32.dig" (lines "cpu-32.dig <pin> <x> <y>").
import re, sys
from strip_apply import resolve_refs

harness_f, test_f, pins_f, dst = sys.argv[1:5]
CX, CY = -900, -500          # cpu-32 position (the harness is at x < -1200)
IO_X, IO_Y = -300, -1000     # column of the In/Out/Clock elements for the other pins
ALIAS = {'C': ['C_i']}       # harness tunnels joined to a pin net through tunnels inside cpu-test

s = open(harness_f, encoding='utf-8', newline='').read()
t, _ = resolve_refs(open(test_f, encoding='utf-8', newline='').read())

def elements(xml):
    for b in re.findall(r'<visualElement>.*?</visualElement>', xml, re.S):
        n = re.search(r'<elementName>([^<]+)<', b).group(1)
        lab = re.search(r'<string>(?:Label|NetName)</string>\s*<string>([^<]*)<', b)
        yield n, lab.group(1) if lab else None, b

harness_nets = {lab for n, lab, _ in elements(s) if n == 'Tunnel'}
io = {lab: (n, b) for n, lab, b in elements(t) if n in ('In', 'Out', 'Clock') and lab}
pins = [(l.split()[1], int(l.split()[2]), int(l.split()[3])) for l in open(pins_f) if l.startswith('cpu-32.dig ')]
W = max(x for _, x, _ in pins); H = max(y for _, _, y in pins)

def tunnel(name, x, y, rot):
    r = '' if rot == 0 else ('\n        <entry>\n          <string>rotation</string>\n'
                             '          <rotation rotation="%d"/>\n        </entry>' % rot)
    return ('    <visualElement>\n      <elementName>Tunnel</elementName>\n      <elementAttributes>%s\n'
            '        <entry>\n          <string>NetName</string>\n          <string>%s</string>\n'
            '        </entry>\n      </elementAttributes>\n      <pos x="%d" y="%d"/>\n    </visualElement>\n'
            % (r, name, x, y))

def wire(x1, y1, x2, y2):
    return '    <wire>\n      <p1 x="%d" y="%d"/>\n      <p2 x="%d" y="%d"/>\n    </wire>\n' % (x1, y1, x2, y2)

def copy_at(b, x, y):   # cpu-test element moved to (x, y), facing right
    b = re.sub(r'\s*<entry>\s*<string>rotation</string>\s*<rotation rotation="\d"/>\s*</entry>', '', b)
    b = re.sub(r'<pos x="-?\d+" y="-?\d+"/>', '<pos x="%d" y="%d"/>' % (x, y), b)
    b = b.replace('<elementAttributes>\n      </elementAttributes>', '<elementAttributes/>')
    return '    ' + b + '\n'

E = ['    <visualElement>\n      <elementName>cpu-32.dig</elementName>\n      <elementAttributes/>\n'
     '      <pos x="%d" y="%d"/>\n    </visualElement>\n' % (CX, CY)]
Wr = []
for name, px, py in pins:
    x, y = CX + px, CY + py
    if px == 0:   dx, dy, rot = -40, 0, 2
    elif px == W: dx, dy, rot = 40, 0, 0
    elif py == 0: dx, dy, rot = 0, -40, 1
    else:         dx, dy, rot = 0, 40, 3
    E.append(tunnel(name, x + dx, y + dy, rot)); Wr.append(wire(x, y, x + dx, y + dy))

iy = IO_Y
for name, _, _ in pins:
    if name in harness_nets and name not in ALIAS: continue
    kind, b = io[name]
    if kind == 'Out':      # tunnel -> Out
        E.append(tunnel(name, IO_X + 200, iy, 2)); E.append(copy_at(b, IO_X + 240, iy))
        Wr.append(wire(IO_X + 200, iy, IO_X + 240, iy))
    else:                  # In/Clock -> tunnel (+ aliases)
        E.append(copy_at(b, IO_X, iy)); E.append(tunnel(name, IO_X + 40, iy, 0))
        Wr.append(wire(IO_X, iy, IO_X + 20, iy)); Wr.append(wire(IO_X + 20, iy, IO_X + 40, iy))
        for k, a in enumerate(ALIAS.get(name, []), 1):   # branch at the IO_X + 20 wire joint
            E.append(tunnel(a, IO_X + 40, iy + 20 * k, 0))
            Wr.append(wire(IO_X + 20, iy, IO_X + 20, iy + 20 * k)); Wr.append(wire(IO_X + 20, iy + 20 * k, IO_X + 40, iy + 20 * k))
        iy += 20 * len(ALIAS.get(name, []))
    iy += 40

s = s.replace('  </visualElements>', ''.join(E) + '  </visualElements>', 1)
s = s.replace('  </wires>', ''.join(Wr) + '  </wires>', 1)
open(dst, 'w', encoding='utf-8', newline='').write(s)
print('cpu-32 pins %d, harness nets %s, io elements %d' % (len(pins), sorted(harness_nets), (iy - IO_Y) // 40))
