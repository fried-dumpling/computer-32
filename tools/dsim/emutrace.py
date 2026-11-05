# usage: python emutrace.py <program.bin> <keys> [start addr hex] [-raw]
#   program.bin : dlnk output with the 8 byte header (start address, length); -raw for a headerless image
import os,re,sys,struct
HERE=os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0,os.path.join(HERE,'..','emu'))
from isa32emu import CPU, load_image
# opcode and extended register names from the assembler tables (instructionBase, regExBase)
SRC=open(os.path.join(HERE,'..','assembler','assembler.hpp'),encoding='utf-8').read()
def table(name):
    body=re.search(name+r'\s*=\s*\{(.*?)\};',SRC,re.S).group(1)
    t={}
    for n,v in re.findall(r'TokenType::(\w+)\s*,\s*0x([0-9A-Fa-f]+)',body): t.setdefault(int(v,16),n)
    return t
OPS=table('instructionBase'); EXT=table('regExBase')
RN={0x19:'sbp',0x1A:'zero',0x1B:'one',0x1C:'full',0x1D:'pc',0x1E:'stack',0x1F:'flag'}
MD=['32B','16L','8L','8H','S16H','S16L','S8L','S8H']
FL=['zero','neg','pos','carry','carry4','ovf','one','gen']
RR={0,1,3,4,6,8,0xA,0xC,0xE,0x10,0x11,0x13,0x14,0x16,0x18,0x1C,0x7D}   # reg, reg
NONE={0x1D,0x71,0x72,0x73,0x74,0x75,0x76,0x77}                          # no operand
def r(b): i=b>>3; return (RN.get(i,f'gen[{i}]'))+'.'+MD[b&7]
def dis(w):
    op,a,b,c=w>>24,(w>>16)&255,(w>>8)&255,w&255; imm=(b<<8)|c
    if 0x20<=op<0x40: return f'ld.{r((op-0x20)<<3)[:-4]} {r(a)}, {imm:#x}'
    if 0x40<=op<0x60: return f'st.{r((op-0x40)<<3)[:-4]} {r(a)}, {imm:#x}'
    if 0x60<=op<0x70: return f'{"i" if op>=0x68 else ""}jmp.{FL[op&7]} {r(a)}, {imm:#x}'
    if op>=0x80: return f'nop (op{op:02x})'
    n=OPS.get(op,f'op{op:02x}')
    if op in RR: return f'{n} {r(a)}, {r(b)}'
    if op in NONE: return n
    if op in (0x1E,0x1F): return f'{n} {r(a)}'
    # interst R, m, vec|pri | id ; interld R, vec|pri | id  (byte 3 = md (bit 7, 1 = pri) | id (bit 5:0))
    if op==0x78: return f'{n} {r(a)}, {b}, {"pri" if c&0x80 else "vec"} | {c&0x3F}'
    if op==0x79: return f'{n} {r(a)}, {"pri" if c&0x80 else "vec"} | {c&0x3F}'
    if op in (0x7A,0x7B): return f'{n} {r(a)}, {EXT.get(c,f"ext{c}")}'
    if op in (0x7C,0x7F): return f'{n} {c}'
    if op==0x7E: return f'{"flush" if c&0x80 else "inval"} {r(a)}, {b-256 if b&0x80 else b}, {c&0x7F:#x}'
    return f'{n} {r(a)}, {imm:#x}'
raw='-raw' in sys.argv
args=[x for x in sys.argv[1:] if x!='-raw']
img,entry=load_image(open(args[0],'rb').read(),raw)
cpu=CPU(img,key_input=args[1],start=entry)
start=int(args[2],16) if len(args)>2 else None
on=False;n=0
while not cpu.halted and cpu.steps<200000:
    pc=cpu.pc; w=cpu.load(pc)
    if pc==start: on=True
    if on:
        sp=cpu.r[0x1E]
        print(f'{pc:04X}  {w:08X}  {dis(w):32s} sp={sp:04X}')
        n+=1
        if n>140: break
    try: cpu.step()
    except Exception as e: print('ERR',e); break
