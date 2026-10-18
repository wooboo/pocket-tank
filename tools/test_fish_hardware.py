#!/usr/bin/env python3
"""Exercise two consenting, connected Pocket Tanks through the bench console.

Temporarily moves one existing fish each way; never creates fish or
restores old saves. Run only on devices authorized for testing.
"""
import argparse
import re
import time
from pathlib import Path
import serial

ap = argparse.ArgumentParser()
ap.add_argument('--a', required=True, help='tank with at least 10 dollars')
ap.add_argument('--b', required=True)
ap.add_argument('--log', required=True)
a = ap.parse_args()
ports = [serial.Serial(p, 115200, timeout=0) for p in (a.a, a.b)]
buffers = [b'', b'']
lines = [[], []]
status = [None, None]
fish_status = [None, None]
pattern = re.compile(r'friends active=(\d+) page=(\d+) code=(\d+) peer=([0-9a-f]+) balance=(\d+) out=(\d+) in=(\d+)')
log = Path(a.log).open('w')

def pump():
    for i, port in enumerate(ports):
        buffers[i] += port.read(port.in_waiting)
        while b'\n' in buffers[i]:
            raw, buffers[i] = buffers[i].split(b'\n', 1)
            line = raw.decode(errors='replace').strip()
            lines[i].append(line)
            log.write(f'{i}: {line}\n'); log.flush()
            fm = re.search(r'fish count=(\d+) out=(\d+) in=(\d+) offered=(\d+) name=(\S+)',line)
            if fm:
                v=fm.groups(); fish_status[i]=dict(count=int(v[0]),out=int(v[1]),incoming=int(v[2]),offered=int(v[3]),name=v[4])
            m = pattern.search(line)
            if m:
                v = m.groups()
                status[i] = dict(active=int(v[0]), page=int(v[1]), code=v[2], peer=v[3], balance=int(v[4]), out=int(v[5]), incoming=int(v[6]))
            if any(s in line for s in ('Guru Meditation', 'assert failed', 'Backtrace:', 'ble_hs_stop_terminate_timeout_cb', 'failed to terminate connection')):
                raise AssertionError(f'device {i} hardware error: {line}')

def send(i, cmd):
    log.write(f'COMMAND {i}: {cmd}\n'); log.flush()
    ports[i].write((cmd+'\n').encode()); ports[i].flush()

def wait(predicate, seconds=15):
    end = time.monotonic()+seconds
    next_query = 0
    while time.monotonic()<end:
        pump()
        if predicate(): return
        if time.monotonic()>next_query:
            send(0, 'friends'); send(1, 'friends'); next_query=time.monotonic()+1
        time.sleep(.04)
    raise AssertionError(f'timeout; last state: {status}')

def pair(confirm=True):
    status[:] = [None,None]
    for i in range(2): send(i, 'friends open')
    wait(lambda: all(s and s['active'] and s['page']==1 for s in status), 25)
    assert status[0]['code']==status[1]['code'], status
    print('Matching code:',status[0]['code'],flush=True)
    if not confirm: return
    for i in range(2): send(i, 'friends confirm')
    wait(lambda: all(s['page']==3 for s in status))
    assert all(s['peer']!='0000000000000000' for s in status)

def close():
    for i in range(2): send(i,'friends close')
    status[:]=[None,None]
    wait(lambda: all(s and not s['active'] for s in status))

def fish_wait(predicate):
    fish_status[:]=[None,None]
    wait(lambda: all(fish_status) and predicate(),25)

def roster(i):
    start=len(lines[i]); send(i,'friends roster')
    end=time.monotonic()+2
    while time.monotonic()<end: pump();time.sleep(.04)
    result=[]
    for line in lines[i][start:]:
        m=re.search(r'roster (\d+) name=(\S+) id=(\S+) age=([\d.]+) body=(\S+) accent=(\S+) paid=(\d+) parents=(.*)',line)
        if m:
            v=m.groups(); result.append(dict(index=int(v[0]),name=v[1],id=v[2],age=float(v[3]),body=v[4],accent=v[5],paid=int(v[6]),parents=v[7]))
    assert result,lines[i][start:]
    return result

def offer_fish(i,index):
    send(i,f'friends fish {index}')
    fish_wait(lambda: fish_status[1-i]['offered']>0)

try:
    close()
    pair()
    before=[roster(0),roster(1)]
    baseline=[s['balance'] for s in status]
    na,nb=map(len,before)
    assert na>2 and nb<5,(na,nb)
    chosen=before[0][-1]
    # Recipient's founding pair cannot leave.
    if nb==2:
        send(1,'friends fish 0')
        fish_wait(lambda: fish_status[1]['count']==2 and fish_status[1]['out']==0)
    offer_fish(0,na-1); send(1,'friends decline')
    fish_wait(lambda: fish_status[0]['count']==na and not fish_status[0]['out'] and fish_status[1]['count']==nb)
    print('PASS: decline restores fish; two-fish minimum enforced',flush=True)
    offer_fish(0,na-1); close(); pair()
    fish_wait(lambda: fish_status[1]['offered']>0)
    send(1,'friends accept')
    fish_wait(lambda: fish_status[0]['count']==na-1 and fish_status[1]['count']==nb+1 and not any(s['out'] or s['incoming'] for s in fish_status))
    received=roster(1)[-1]
    for key in ('name','body','accent','paid'): assert received[key]==chosen[key],(key,chosen,received)
    assert chosen['age']<=received['age']<chosen['age']+120,(chosen,received)
    assert not received['id'].startswith('0000000000000000:'),received
    print('PASS: disconnected gift resumes; name, colours, age and paid history preserved',flush=True)
    offer_fish(1,nb);send(0,'friends accept')
    fish_wait(lambda: fish_status[0]['count']==na and fish_status[1]['count']==nb and not any(s['out'] or s['incoming'] for s in fish_status))
    after=[roster(0),roster(1)]
    assert [f['name'] for f in after[0]]==[f['name'] for f in before[0]]
    assert [f['name'] for f in after[1]]==[f['name'] for f in before[1]]
    assert after[0][-1]['id']==received['id'] and after[0][-1]['parents']==received['parents']
    assert [s['balance'] for s in status]==baseline,(baseline,status)
    print('PASS: fish returned with stable identity and family; both original rosters and balances restored',flush=True)
finally:
    for i in range(2): send(i,'friends close')
    end=time.monotonic()+2
    while time.monotonic()<end: pump();time.sleep(.04)
    for port in ports: port.close()
    log.close()
