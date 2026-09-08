"""Keep this Windows machine awake for the explicitly bounded local night run.

Process/thread-scoped request only: no persistent power settings are changed,
and the display may turn off. Exits and releases the request at the UTC cutoff.
"""
import argparse
import ctypes
from datetime import datetime, timezone
import time

parser=argparse.ArgumentParser()
parser.add_argument('--until',required=True)
args=parser.parse_args()
deadline=datetime.fromisoformat(args.until.replace('Z','+00:00'))
if deadline.tzinfo is None:
    raise ValueError('An explicit UTC offset is required')
remaining=(deadline-datetime.now(timezone.utc)).total_seconds()
if not 0<remaining<=12*3600:
    raise ValueError('Awake request must end within the next twelve hours')
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.SetThreadExecutionState.argtypes=[ctypes.c_uint32]
kernel.SetThreadExecutionState.restype=ctypes.c_uint32
if not kernel.SetThreadExecutionState(0x80000001):
    raise OSError('Unable to register the temporary awake request')
try:
    print('Temporary system-awake request active until '+deadline.isoformat(),flush=True)
    while (remaining:=(deadline-datetime.now(timezone.utc)).total_seconds())>0:
        time.sleep(min(30.0,remaining))
finally:
    kernel.SetThreadExecutionState(0x80000000)
    print('Temporary system-awake request released',flush=True)
