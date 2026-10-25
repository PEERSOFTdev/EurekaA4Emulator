from pathlib import Path
class Asm:
 def __init__(self): self.b=bytearray();self.lab={};self.fix=[]
 def pos(self):return 0x100+len(self.b)
 def mark(self,n):self.lab[n]=self.pos()
 def e(self,*x):self.b.extend(x)
 def word(self,n):self.fix.append((len(self.b),n));self.e(0,0)
 def ldde(self,n):self.e(0x11);self.word(n)
 def ldhl(self,n):self.e(0x21);self.word(n)
 def call(self,n):self.e(0xcd);self.word(n)
 def jp(self,n,cc=0xc3):self.e(cc);self.word(n)
 def done(self):
  for i,n in self.fix:
   v=self.lab[n];self.b[i:i+2]=bytes((v&255,v>>8))
  return bytes(self.b)
a=Asm();a.e(0x31);a.word('stacktop')
a.ldde('intro');a.call('print')
# fail if file already exists, avoid clobber
# BDOS 15 open existing: FF means not found

def bdos(fn):a.ldde('fcb');a.e(0x0e,fn);a.call('bdos')
def errff(lbl):a.e(0xfe,0xff);a.jp(lbl,0xca)
def errnz(lbl):a.e(0xb7);a.jp(lbl,0xc2)
bdos(15);a.e(0xfe,0xff);a.jp('exists',0xc2)
bdos(22);errff('errmake')
# header, file remains open
a.ldde('rec_header');a.call('write_current')
a.call('close_file')
# before
# build record with live values
a.ldhl('rec_before');a.call('capture')
a.ldde('rec_before');a.call('append')
# Test HD64180 multiplication without touching I/O
a.ldhl('rec_cpu');a.call('cpu_capture');a.ldde('rec_cpu');a.call('append')
# independent RTC seconds before and after a deterministic instruction loop
a.ldhl('rec_cpu_before');a.call('measure_cpu');a.ldde('rec_cpu_before');a.call('append')
a.ldhl('rec_read_before');a.call('measure_read');a.ldde('rec_read_before');a.call('append')
a.ldhl('rec_write_before');a.call('measure_write');a.ldde('rec_write_before');a.call('append')
# write OMCR value 1, read it back and capture other registers
# register B must be 0 for full 003e address
a.e(0x01,0x3e,0x00,0x3e,0x01,0xed,0x79)
a.ldhl('rec_after');a.call('capture')
a.ldde('rec_after');a.call('append')
a.ldhl('rec_cpu_after');a.call('measure_cpu');a.ldde('rec_cpu_after');a.call('append')
a.ldhl('rec_read_after');a.call('measure_read');a.ldde('rec_read_after');a.call('append')
a.ldhl('rec_write_after');a.call('measure_write');a.ldde('rec_write_after');a.call('append')
# restore the OMCR control register to its observed initial readable value
# WARNING: readback FF alone does not establish restored bus timing.
a.e(0x01,0x3e,0x00,0x3e,0xff,0xed,0x79) # LD BC,003Eh; LD A,FFh; OUT (C),A
a.ldhl('rec_restored');a.call('capture')
a.ldde('rec_restored');a.call('append')
a.ldhl('rec_cpu_restored');a.call('measure_cpu');a.ldde('rec_cpu_restored');a.call('append')
a.ldhl('rec_read_restored');a.call('measure_read');a.ldde('rec_read_restored');a.call('append')
a.ldhl('rec_write_restored');a.call('measure_write');a.ldde('rec_write_restored');a.call('append')
a.ldde('rec_done');a.call('append')
a.ldde('done_msg');a.call('print');a.jp('exit')
# capture: HL points to 128-byte record; placeholders at offsets 14,19,24,29? use markers via computed offsets from string
# save HL in ptr; register readings via IN A,(C) and write two hex chars
# record is 'BEFORE: P50=XX P54=XX P62=XX P63=XX' etc

a.mark('capture');a.e(0x22);a.word('ptr') # LD (ptr),HL
for port, offset in [(50,12),(54,19),(62,26),(63,33)]:
 a.e(0x01,port,0,0xed,0x78) # LD BC, port; IN A,(C)
 a.e(0x2a);a.word('ptr') # LD HL,(ptr)
 a.e(0x11,offset&255,offset>>8,0x19) # LD DE, offset; ADD HL,DE
 a.call('hex2')
a.e(0xc9)
# cpu_capture: MLT BC and MLT DE. Fixed DE preservation. Expected BC=003F (7*9), DE=008F (13*11).
# These are HD64180 instructions; a generic Z80 may treat them differently.
a.mark('cpu_capture');a.e(0x22);a.word('ptr')
a.e(0x01,0x09,0x07,0xed,0x4c) # LD BC,0709; MLT BC
# output B then C at offsets 7 and 9
for reg,off in [(0x78,7),(0x79,9)]:
 a.e(reg);a.e(0x2a);a.word('ptr');a.e(0x11,off,0,0x19);a.call('hex2')
a.e(0x11,0x0b,0x0d,0xed,0x5c) # LD DE,0D0B; MLT DE
# Preserve both result bytes BEFORE using DE as a pointer offset.
a.e(0xed,0x53);a.word('de_result') # LD (de_result),DE
for mem,off in [('de_result_hi',19),('de_result_lo',21)]:
 a.e(0x3a);a.word(mem) # LD A,(saved byte)
 a.e(0x2a);a.word('ptr');a.e(0x11,off,0,0x19);a.call('hex2')
a.e(0xc9)
# hex2 A -> 2 ASCII hex at HL
# Measure raw binary seconds and hundredths (Eureka RTC is NOT BCD).
# Record format: CPU/READ/WRITE START=SS:HH END=SS:HH; LOOP=16x65536
# Reading port 90h latches clock; port 93h returns seconds.
# Use two temporary bytes, because LD BC destroys loop counters.
branches=[]
for kind in ('cpu','read','write'):
 a.mark('measure_'+kind);a.e(0x22);a.word('ptr')
 a.call('rtc_snapshot')
 for label,off in [('rtc_seconds',12),('rtc_hundredths',15)]:
  a.e(0x3a);a.word(label);a.e(0x2a);a.word('ptr');a.e(0x11,off,0,0x19);a.call('hex2')
 # 16 times 65536 loops, identical test repeated before and after OUT.
 a.e(0x3e,16,0x32);a.word('outer')
 a.mark('outer_loop_'+kind);a.e(0x01,0,0) # LD BC,0000
 if kind != 'cpu': a.ldhl('test_byte')
 a.mark('inner_loop_'+kind)
 if kind == 'cpu': a.e(0x00,0x00) # 2 NOPs
 elif kind == 'read': a.e(0x7e,0x7e) # LD A,(HL) twice, no side effects
 else: a.e(0x77,0x77) # LD (HL),A twice, scratch RAM only
 a.e(0x0b,0x78,0xb1) # DEC BC; LD A,B; OR C
 ix=len(a.b);a.e(0x20,0);branches.append((ix,'inner_loop_'+kind))
 a.e(0x3a);a.word('outer');a.e(0x3d,0x32);a.word('outer')
 ix=len(a.b);a.e(0x20,0);branches.append((ix,'outer_loop_'+kind))
 a.call('rtc_snapshot')
 for label,off in [('rtc_seconds',22),('rtc_hundredths',25)]:
  a.e(0x3a);a.word(label);a.e(0x2a);a.word('ptr');a.e(0x11,off,0,0x19);a.call('hex2')
 a.e(0xc9)
a.mark('rtc_snapshot')
a.e(0x01,0x90,0x00,0xed,0x78) # IN A,(0090h) hundredths; latch
#a: store hundredths
a.e(0x32);a.word('rtc_hundredths')
a.e(0x01,0x93,0x00,0xed,0x78) # IN A,(0093h) seconds
a.e(0x32);a.word('rtc_seconds');a.e(0xc9)
a.mark('hex2');a.e(0xf5,0x0f,0x0f,0x0f,0x0f,0xe6,0x0f);a.call('nibble');a.e(0x77,0x23,0xf1,0xe6,0x0f);a.call('nibble');a.e(0x77,0xc9)
a.mark('nibble');a.e(0xc6,0x30,0xfe,0x3a);a.jp('nibble_ret',0xda);a.e(0xc6,0x07);a.mark('nibble_ret');a.e(0xc9)
# append: DE record pointer saved in recordptr; reset FCB fields, open, read to EOF, then sequential write and close
# scan limit 128 records using B=128; BDOS20 modifies BC so push B
a.mark('append');a.e(0xed,0x53);a.word('recordptr')
a.e(0xaf)
for off in [12,13,14,15,32,33,34,35]:a.e(0x32);a.word('fcb'+str(off))
bdos(15);errff('erropen')
a.ldde('scratch');a.call('setdma');a.e(0x06,128)
a.mark('scan');a.e(0xc5);bdos(20);a.e(0xc1,0xb7);a.jp('append_write',0xc2)
# DJNZ with manual rel patch
a.e(0x10);djnzidx=len(a.b);a.e(0)
a.jp('errlimit')
a.mark('append_write');a.e(0xed,0x5b);a.word('recordptr');a.call('write_current');a.call('close_file');a.e(0xc9)
# write_current: DE points to record
a.mark('write_current');a.call('setdma');bdos(21);errnz('errwrite');a.e(0xc9)
a.mark('close_file');bdos(16);errff('errclose');a.e(0xc9)
a.mark('setdma');a.e(0x0e,26);a.call('bdos');a.e(0xc9)
a.mark('print');a.e(0x0e,9);a.call('bdos');a.e(0xc9)
a.mark('bdos');a.e(0xcd,5,0,0xc9)
for lbl,msg in [('exists','EUTEST10.TXT EXISTS - NOT OVERWRITTEN'),('errmake','ERROR BDOS MAKE 22'),('erropen','ERROR BDOS OPEN 15'),('errlimit','ERROR APPEND LIMIT'),('errwrite','ERROR BDOS WRITE 21'),('errclose','ERROR BDOS CLOSE 16')]:
 a.mark(lbl);a.ldde(lbl+'txt');a.call('print');a.jp('exit')
a.mark('exit');a.e(0x0e,0);a.call('bdos');a.e(0xc3,0,0)
for lbl,msg in [('intro','EUTEST10 - CPU arithmetic and port diagnostic'),('done_msg','TEST COMPLETE - EUTEST10.TXT')]+[(l+'txt',m) for l,m in [('exists','EUTEST10.TXT EXISTS - NOT OVERWRITTEN'),('errmake','ERROR BDOS MAKE 22'),('erropen','ERROR BDOS OPEN 15'),('errlimit','ERROR APPEND LIMIT'),('errwrite','ERROR BDOS WRITE 21'),('errclose','ERROR BDOS CLOSE 16')]]:
 a.mark(lbl);a.b.extend((msg+'\r\n$').encode('ascii'))
a.mark('fcb');a.b.extend(bytes([0])+b'EUTEST10'+b'TXT'+bytes(24))
for off in [12,13,14,15,32,33,34,35]:a.lab['fcb'+str(off)]=a.lab['fcb']+off

def record(name,s):
 a.mark(name);raw=(s+'\r\n').encode('ascii');assert len(raw)<=128;a.b.extend(raw+b' '*(128-len(raw)))
record('rec_header','EUREKA A4 EUTEST10 - BEFORE / AFTER OUT 62,1 / RESTORE OUT 62,255')
record('rec_before','BEFORE: P50=XX P54=XX P62=XX P63=XX')
record('rec_after','AFTER : P50=XX P54=XX P62=XX P63=XX')
record('rec_cpu','MLT BC=XXXX MLT DE=XXXX EXPECT 003F/008F')
record('rec_cpu_before','CPU   START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_read_before','READ  START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_write_before','WRITE START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_cpu_after','CPU   START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_read_after','READ  START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_write_after','WRITE START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_restored','RESTOR: P50=XX P54=XX P62=XX P63=XX')
record('rec_cpu_restored','CPU   START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_read_restored','READ  START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_write_restored','WRITE START=XX:XX END=XX:XX; LOOP=16x65536; BINARY SS:HH')
record('rec_done','TEST COMPLETE')
a.mark('de_result');a.e(0,0);a.lab['de_result_lo']=a.lab['de_result'];a.lab['de_result_hi']=a.lab['de_result']+1
a.mark('test_byte');a.e(0x5a);a.mark('rtc_seconds');a.e(0);a.mark('rtc_hundredths');a.e(0);a.mark('outer');a.e(0);a.mark('scratch');a.b.extend(bytes(128));a.mark('ptr');a.e(0,0);a.mark('recordptr');a.e(0,0);a.mark('stack');a.b.extend(bytes(128));a.mark('stacktop')
# Patch both loop branches (previous EUTEST7 had zero offsets, so its loop was not actually executed).
for idx,target in branches:
 rel=a.lab[target]-(0x100+idx+2)
 assert -128<=rel<=127,(target,rel)
 a.b[idx+1]=rel&255
 print('patched',target,'JR displacement',rel)
# patch relative DJNZ
rel=a.lab['scan']-(0x100+djnzidx+1);assert -128<=rel<=127,rel;a.b[djnzidx]=rel&255
# validate offsets for fields
for s in ['BEFORE: P50=XX P54=XX P62=XX P63=XX','AFTER : P50=XX P54=XX P62=XX P63=XX']:
 print(s, [(p,s.index('XX',s.index('P'+str(p)))) for p in [50,54,62,63]])
 # override incorrect offsets above if necessary
assert [12,19,26,33]==[('BEFORE: P50=XX P54=XX P62=XX P63=XX').index('XX',('BEFORE: P50=XX P54=XX P62=XX P63=XX').index('P'+str(p))) for p in [50,54,62,63]]
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
args.out.mkdir(parents=True,exist_ok=True)
raw=a.done();out=args.out/'EUTEST10.COM';out.write_bytes(raw)
print('built',len(raw),'bytes', 'scan djnz',rel)
