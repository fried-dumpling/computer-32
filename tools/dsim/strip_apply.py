# strip_apply.py <in.dig> <list.txt> <out.dig> [old.dig=new.dig ...]
# removes the "E name x y" elements and "W x1 y1 x2 y2" wires printed by Strip, renames subcircuit refs
import re, sys
import xml.etree.ElementTree as ET
# XStream writes repeated objects as reference="../../visualElement[586]/..." (paths with indices);
# deleting elements shifts those indices, so inline every reference first.
def resolve_refs(s):
    root = ET.fromstring(s.encode('utf-8'))
    parent = {c: p for p in root.iter() for c in p}
    reps = []
    for el in root.iter():
        ref = el.get('reference')
        if ref is None: continue
        node = el
        for seg in ref.split('/'):
            if seg == '..': node = parent[node]; continue
            m = re.fullmatch(r'([\w-]+)(?:\[(\d+)\])?', seg)
            node = [c for c in node if c.tag == m.group(1)][int(m.group(2) or 1) - 1]
        assert node.get('reference') is None
        reps.append(ET.tostring(node, encoding='unicode').strip().replace(' />', '/>'))
    it = iter(reps)
    out = re.sub(r'<[\w-]+ reference="[^"]*"\s*/>', lambda m: next(it), s)
    assert next(it, None) is None
    return out, len(reps)

if __name__ == '__main__':
    src, lst, dst = sys.argv[1:4]
    ren = dict(r.split('=') for r in sys.argv[4:])
    s = open(src, encoding='utf-8', newline='').read()

    s, nref = resolve_refs(s)
    E, W = {}, {}
    for line in open(lst):
        t = line.split()
        if t and t[0] == 'E': E[(t[1], int(t[2]), int(t[3]))] = E.get((t[1], int(t[2]), int(t[3])), 0) + 1
        if t and t[0] == 'W': W[tuple(map(int, t[1:5]))] = W.get(tuple(map(int, t[1:5])), 0) + 1
    nE, nW = sum(E.values()), sum(W.values())
    def ve(m):
        b = m.group(0)
        n = re.search(r'<elementName>([^<]+)</elementName>', b).group(1)
        x, y = map(int, re.search(r'<pos x="(-?\d+)" y="(-?\d+)"/>', b).groups())
        k = (n, x, y)
        if E.get(k):
            E[k] -= 1
            return ''
        return b
    def wi(m):
        b = m.group(0)
        k = tuple(map(int, re.findall(r'"(-?\d+)"', b)))
        if W.get(k):
            W[k] -= 1
            return ''
        return b
    s = re.sub(r'[ \t]*<visualElement>.*?</visualElement>\r?\n', ve, s, flags=re.S)
    s = re.sub(r'[ \t]*<wire>.*?</wire>\r?\n', wi, s, flags=re.S)
    for o, n in ren.items():
        s = s.replace('<elementName>%s</elementName>' % o, '<elementName>%s</elementName>' % n)
    left = [k for k, v in E.items() if v] + [k for k, v in W.items() if v]
    open(dst, 'w', encoding='utf-8', newline='').write(s)
    print('removed %d elements, %d wires; unmatched: %s' % (nE - sum(E.values()), nW - sum(W.values()), left[:10]))
