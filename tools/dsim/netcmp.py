# netcmp.py <orig.sig> <new.sig>
# For every pin present in both, compare the set of co-net pins (restricted to pins present in both).
# Element names are normalized by dropping "-debug".
import sys
from collections import defaultdict
def load(f):
    pin2net, net2pins = {}, defaultdict(set)
    for l in open(f):
        if '\t' not in l: continue
        p, n = l.rstrip('\n').split('\t')
        p = p.replace('-debug.dig', '.dig')
        pin2net[p] = n
        if n != 'open': net2pins[n].add(p)
    return pin2net, net2pins
a, an = load(sys.argv[1]); b, bn = load(sys.argv[2])
common = set(a) & set(b)
bad = 0
for p in sorted(common):
    # a pin alone on its net is the same as an open pin
    sa = ((an[a[p]] & common) if a[p] != 'open' else set()) - {p}
    sb = ((bn[b[p]] & common) if b[p] != 'open' else set()) - {p}
    if sa != sb:
        bad += 1
        if bad <= 15: print('DIFF', p, '\n   only orig:', sorted(sa - sb)[:6], '\n   only new :', sorted(sb - sa)[:6])
print('pins orig %d new %d common %d, new-only %d, mismatched %d' % (len(a), len(b), len(common), len(set(b) - set(a)), bad))
for p in sorted(set(b) - set(a))[:10]: print('  new-only', p)
