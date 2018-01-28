
import json
tim = '/sdcard/qpython/bkmrks.json'
jess = [json.loads(raw) for raw in open(tim)]

print(jess)
print()