# love baz
import os, sys, time, socket
from contextlib import closing

def seedtest(address):
  nodesocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  nodesocket.setblocking(0)
  nodesocket.settimeout(1)
  try:
    nodesocket.connect((address, 19117))
    nodesocket.shutdown(0)
    return True
  except:
    try:
      nodesocket.shutdown(0)
      return False
    except:
      return False

def find_between( s, first, last ):
    try:
        start = s.index( first ) + len( first )
        end = s.index( last, start )
        return s[start:end]
    except ValueError:
        return ""

seedlist = []
print ''

# read seed data
seedinclude = open('chainparamsseeds.h','r')
seeddata = seedinclude.read()
seedinclude.closed

# parse it into seedlist
for line in seeddata.split('\n'):
  addr_build = ''
  octet_place = 0
  if '19117' in line:
    seed = find_between(line, '0xff,0xff,', '}, 19117},').split(',')
    for octet in seed:
     try:
      addr_build = addr_build + str(int(octet.replace('0x',''),16))
      if octet_place < 3:
         addr_build = addr_build + '.'
      octet_place = octet_place + 1
     except:
      pass
    if len(addr_build) > 4:
      seedlist.append(addr_build)

print 'loaded ' + str(len(seedlist)) + ' seeds from include'

for seed in seedlist:
  seed_result = seedtest(seed)
  if seed_result == True:
     print seed + '   is good'
  else:
     print seed + '   didnt respond'
  time.sleep(0.25)
