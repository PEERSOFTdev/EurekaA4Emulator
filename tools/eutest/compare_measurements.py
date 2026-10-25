#!/usr/bin/env python3
"""Parse 128-byte CP/M text records; binary RTC values are printed in hexadecimal.
This is analysis of supplied measurements, not a new hardware test.
"""
import argparse,csv,json,re,sys
from pathlib import Path
CLOCK=r'([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2}):([0-9A-F]{2})'
RX=re.compile(r'TIME P(\d\d) (CPU|READ|WRITE)\s+START='+CLOCK+r' END='+CLOCK)
def load(path):
 raw=Path(path).read_bytes();s=raw.decode('ascii');d={}
 if len(raw)%128:raise ValueError('Truncated CP/M record: '+str(path))
 if 'TEST COMPLETE' not in s:raise ValueError('Incomplete test: '+str(path))
 for m in RX.finditer(s):
  fields=list(map(lambda x:int(x,16),m.groups()[2:]));beg=fields[:4];end=fields[4:]
  for c in [beg,end]:
   if not(0<=c[0]<24 and 0<=c[1]<60 and 0<=c[2]<60 and 0<=c[3]<100):raise ValueError('Invalid binary RTC value')
  cv=lambda c:((c[0]*60+c[1])*60+c[2])*100+c[3]
  k=(int(m[1]),m[2]);assert k not in d;d[k]=(cv(end)-cv(beg))%8640000
 if len(d)!=33:raise ValueError('Expected 33 measurements, got '+str(len(d)))
 return d
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('hardware',type=Path);p.add_argument('simulator',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 h,s=load(a.hardware),load(a.simulator);rows=[]
 for k in sorted(h):
  rows.append({'phase':k[0],'test':k[1],'hardware_cs':h[k],'simulator_cs':s[k],'difference_cs':s[k]-h[k]})
 a.out.write_text(json.dumps({'rows':rows,'equal':sum(r['difference_cs']==0 for r in rows),'max_difference_cs':max(abs(r['difference_cs']) for r in rows)},indent=2)+'\n')
 print('rows',len(rows),'equal',sum(r['difference_cs']==0 for r in rows),'max_difference_cs',max(abs(r['difference_cs']) for r in rows))
