# netcmp.py <orig.sig> <new.sig> [-split a|b ...]
# For every pin present in both, compare the set of co-net pins (restricted to pins present in both).
# Element names are normalized by dropping "-debug".
# -split a|b : the orig net that has the tunnel names a and b may lose pins on purpose (Strip ALIAS-REMOVED,
#              e.g. a loopback wire in the stripped harness). Such diffs are counted as "expected split".
import sys
from collections import defaultdict
def load(f):
    pin2net, net2pins, labels = {}, defaultdict(set), {}
    for l in open(f):
        if '\t' not in l: continue
        p, n, lab = (l.rstrip('\n').split('\t') + [''])[:3]
        p = p.replace('-debug.dig', '.dig')
        pin2net[p] = n
        labels[n] = set(lab.split('|')) - {''}
        if n != 'open': net2pins[n].add(p)
    return pin2net, net2pins, labels
args = sys.argv[1:]
splits = [set(args[i + 1].split('|')) for i, x in enumerate(args) if x == '-split']
files = [x for i, x in enumerate(args) if x != '-split' and (i == 0 or args[i - 1] != '-split')]
a, an, al = load(files[0]); b, bn, _ = load(files[1])
common = set(a) & set(b)
bad = expected = 0
for p in sorted(common):
    # a pin alone on its net is the same as an open pin
    sa = ((an[a[p]] & common) if a[p] != 'open' else set()) - {p}
    sb = ((bn[b[p]] & common) if b[p] != 'open' else set()) - {p}
    if sa != sb:
        if sb <= sa and any(g <= al.get(a[p], set()) for g in splits):
            expected += 1
            continue
        bad += 1
        if bad <= 15: print('DIFF', p, '\n   only orig:', sorted(sa - sb)[:6], '\n   only new :', sorted(sb - sa)[:6])
print('pins orig %d new %d common %d, new-only %d, mismatched %d' % (len(a), len(b), len(common), len(set(b) - set(a)), bad)
      + (', expected split %d (%s)' % (expected, ' '.join('|'.join(sorted(g)) for g in splits)) if splits else ''))
for p in sorted(set(b) - set(a))[:10]: print('  new-only', p)
