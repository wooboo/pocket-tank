#!/usr/bin/env python3
"""Exercise two consenting, connected Pocket Tanks through the bench console.

Temporarily moves ten existing dollars each way; never mints currency or
restores old saves. Run only on devices authorized for testing.
--pair-only checks confirmation, cancellation and reopening without transfers.
"""
import argparse
import json
import re
import time
from pathlib import Path
import serial

ap = argparse.ArgumentParser()
ap.add_argument('--a', required=True, help='tank with at least 10 dollars')
ap.add_argument('--b', required=True)
ap.add_argument('--log', required=True)
ap.add_argument('--pair-only', action='store_true', help='verify pairing and cancellation without transferring dollars')
a = ap.parse_args()
ports = [serial.Serial(p, 115200, timeout=0) for p in (a.a, a.b)]
buffers = [b'', b'']
lines = [[], []]
status = [None, None]
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

def balances(expected):
    status[:] = [None,None]
    wait(lambda: all(s and s['balance']==b and not s['out'] and not s['incoming'] for s,b in zip(status,expected)))

def offer(sender):
    receiver=1-sender; start=len(lines[receiver]); send(sender,'friends give 10')
    wait(lambda: any(re.search(r'gift op 1 .* reply 1 ',l) for l in lines[receiver][start:]))

def close():
    for i in range(2): send(i,'friends close')
    status[:]=[None,None]
    wait(lambda: all(s and not s['active'] for s in status))

try:
    for i in range(2):
        send(i,'friends close')
        send(i,'setup off')
    # Let setup dismissal happen before opening Friends.
    end=time.monotonic()+1
    while time.monotonic()<end: pump();time.sleep(.04)
    pair(confirm=False)
    send(0,'friends confirm')
    wait(lambda: status[0]['page']==2 and status[1]['page']==1)
    assert all(s['peer']=='0000000000000000' for s in status)
    send(1,'friends close')
    wait(lambda: status[0]['page']==4 and not status[1]['active'])
    close()
    print('PASS: both confirmations required; cancelled pairing closes cleanly',flush=True)
    pair()
    base=[s['balance'] for s in status]
    if a.pair_only:
        close()
        assert [s['balance'] for s in status] == base
        assert all(not s['out'] and not s['incoming'] for s in status)
        print(json.dumps(dict(result='passed', mode='pair-only', balances=base)), flush=True)
        raise SystemExit(0)
    assert base[0]>=10
    offer(0);send(1,'friends accept');balances([base[0]-10,base[1]+10])
    offer(1);send(0,'friends accept');balances(base)
    print('PASS: gifts in both directions, original balances restored',flush=True)
    offer(0);send(1,'friends decline');balances(base)
    print('PASS: declined gift refunded',flush=True)
    offer(0);close();pair()
    wait(lambda: status[0]['out']==1)
    # OFFER is retried every second; allow receiver to display it.
    end=time.monotonic()+2
    while time.monotonic()<end: pump();time.sleep(.04)
    send(1,'friends accept');balances([base[0]-10,base[1]+10])
    offer(1);send(0,'friends accept');balances(base)
    print('PASS: pending gift resumes after disconnect/re-pair; balances restored',flush=True)
    close()
    print(json.dumps({'result':'passed','balances':base,'devices':[a.a,a.b]}),flush=True)
finally:
    # Close radio sessions; unresolved journals are deliberately left intact.
    for i,p in enumerate(ports):
        try: send(i,'friends close')
        except serial.SerialException: pass
        p.close()
    log.close()
