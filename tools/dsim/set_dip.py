# set_dip.py <circuit.dig> x,y... : set the default of the DipSwitches at those positions to on
import re, sys
f = sys.argv[1]
s = open(f, encoding='utf-8').read()
for xy in sys.argv[2:]:
    x, y = xy.split(',')
    pat = r'(<elementName>DipSwitch</elementName>\s*)<elementAttributes/>(\s*<pos x="%s" y="%s"/>)' % (x, y)
    on = ('<elementAttributes>\n        <entry>\n          <string>dipDefault</string>\n'
          '          <boolean>true</boolean>\n        </entry>\n      </elementAttributes>')
    s, n = re.subn(pat, lambda m: m.group(1) + on + m.group(2), s)
    if n != 1: sys.exit('DipSwitch without attributes not found at ' + xy)
open(f, 'w', encoding='utf-8', newline='').write(s)
