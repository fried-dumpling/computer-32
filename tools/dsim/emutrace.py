# usage: python emutrace.py <program.bin (-raw)> <keys> [start addr hex]
import os,sys,struct
sys.path.insert(0,os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','emu'))
from isa32emu import CPU
OPS={0:'add',1:'addc',2:'addi',3:'sub',5:'subi',6:'bxr',7:'bxri',8:'bor',9:'bori',0xA:'bnd',0xB:'bndi',0x10:'shiftl',0x12:'shiftli',0x13:'shiftr',0x15:'shiftri',0x16:'cmp',0x17:'cmpi',0x18:'test',0x19:'testi',0x1A:'set',0x1C:'mov',0x1D:'block',0x1E:'pop',0x1F:'push',0x70:'call',0x71:'ret',0x72:'nop',0x75:'halt'}
RN={0x19:'sbp',0x1A:'zero',0x1B:'one',0x1C:'full',0x1D:'pc',0x1E:'stack',0x1F:'flag'}
MD=['32B','16L','8L','8H','S16H','S16L','S8L','S8H']
FL=['zero','neg','pos','carry','carry4','ovf','one','gen']
def r(b): i=b>>3; return (RN.get(i,f'gen[{i}]'))+'.'+MD[b&7]
def dis(w):
    op,a,b,c=w>>24,(w>>16)&255,(w>>8)&255,w&255; imm=(b<<8)|c
    if 0x20<=op<0x40: return f'ld.{r((op-0x20)<<3)[:-4]} {r(a)}, {imm:#x}'
    if 0x40<=op<0x60: return f'st.{r((op-0x40)<<3)[:-4]} {r(a)}, {imm:#x}'
    if 0x60<=op<0x70: return f'{"i" if op>=0x68 else ""}jmp.{FL[op&7]} {r(a)}, {imm:#x}'
    n=OPS.get(op,f'op{op:02x}')
    if op in (0,1,3,6,8,0xA,0x10,0x13,0x16,0x18,0x1C): return f'{n} {r(a)}, {r(b)}'
    if op in (0x71,0x72,0x1D,0x75): return n
    if op in (0x1E,0x1F): return f'{n} {r(a)}'
    return f'{n} {r(a)}, {imm:#x}'
img=open(sys.argv[1],'rb').read()
cpu=CPU(img,key_input=sys.argv[2])
start=int(sys.argv[3],16) if len(sys.argv)>3 else None
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
