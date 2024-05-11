import sys

start, end = -1.0, -1.0

duration = float(sys.argv[2])
warmup = duration/3.0

tLatency = []
sLatency = []
fLatency = []

tExtra = 0.0
sExtra = 0.0
fExtra = 0.0
retries = 0.0
starvation = 0.0
readretries = 0.0
readstarvation = 0.0

xLatency = []

for line in open(sys.argv[1]):
  if line.startswith('#') or line.strip() == "":
    continue

  line = line.strip().split()
  if not line[0].isdigit() or len(line) < 4:
    continue

  if start == -1:
    start = float(line[2]) + warmup
    end = start + warmup

  fts = float(line[2])
  
  if fts < start:
    continue

  if fts > end:
    break

  latency = int(line[3])
  status = int(line[4])
  # ttype = int(line[5])
  # retry = int(line[6])
  # if retry == 5:
  #   starvation += 1
  # if ttype == 4 and retry == 5:
  #   readstarvation += 1
  # retries += retry
  ttype = -1
  try:
    ttype = int(line[5])
    extra = int(line[6])
  except:
    extra = 0

  if status == 1 and ttype == 2:
    xLatency.append(latency)

  tLatency.append(latency) 
  tExtra += extra

  if status == 1:
    sLatency.append(latency)
    sExtra += extra
  else:
    fLatency.append(latency)
    fExtra += extra

if len(tLatency) == 0:
  print("Zero completed transactions..")
  sys.exit()

tLatency.sort()
sLatency.sort()
fLatency.sort()

print(len(tLatency))
print(len(sLatency))
print((float)(len(tLatency)-len(sLatency))/len(tLatency))
print(len(tLatency)/(end-start))
print(len(sLatency)/(end-start))
print(sum(tLatency)/float(len(tLatency)))
print(tLatency[(int)(len(tLatency)/2)])
print(tLatency[(int)((len(tLatency) * 99)/100)])
print(sum(sLatency)/float(len(sLatency)))
print(sLatency[(int)(len(sLatency)/2)])
print(sLatency[(int)((len(sLatency) * 99)/100)])
# print(retries/len(tLatency))
# print(retries)
# print(starvation)
# print(readstarvation)
print(tExtra)
print(sExtra)
if len(xLatency) > 0:
  print("X Transaction Latency: ", sum(xLatency)/float(len(xLatency)))
if len(fLatency) > 0:
  print("Average Latency (failure): ", sum(fLatency)/float(len(tLatency)-len(sLatency)))
  print("Extra (failure): ", fExtra)
