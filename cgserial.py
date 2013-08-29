#!/usr/bin/python

import simplejson as json

i = open('/proc/cpuinfo')

my_text = i.readlines()
i.close()

username = ""

for line in my_text:
    line = line.strip()
    ar = line.split(' ')
    if ar[0].startswith('Serial'):
        username = "a" + ar[1]

if not username:
    exit(-1)

o = open('/home/pi/.cgminer/cgminer.conf', 'w');


pools = []

pools.append({"url": "stratum+tcp://ghash.io:3333",
              "user": username, "pass": "12345"})

conf = {"pools": pools,
        "api-listen" : "true",
        "api-port" : "4028",
        "api-allow" : "W:127.0.0.1"}

txt = json.dumps(conf, sort_keys=True, indent=4 * ' ')

o.write(txt)
o.write("\n");
o.close()
