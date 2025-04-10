import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator

period = 85
extendBy = 100
upl = 128
idx = 0
fbk = 32686

##################################################
seed = 1
lo = seed
def pseudorand(lo):
    hi = 16807*(lo // 65536)
    lo = 16807*(lo % 65536)
    lo += (hi % 32768)*65536
    lo += hi // 32768
    lo = (lo & 0x7FFF_FFFF) + (1 if lo & 0x8000_0000 else 0)
    return lo


##################################################
def initRand(len):
    global lo
    r = []
    for i in range(0,len):
        lo = pseudorand(lo)
        us = lo % 65536
        if us > 32767:
            us = us - 65536 
        r += [us]

    return r 


def initSweep(len):
    global lo
    r = []
    for i in range(0,len):
        us=30000*(i-len//4)//len
        r += [us]

    return r 


def initFixed():
    return [2166, 785, 792, 1053, 1206, 1307, 646, -1610, 
            -5231, -8847, -10907, -10341, -7398, -4033, -2191, -1791, 
            -1583, -1186, -720, 535, 3152, 5994, 7398, 7322, 6643, 
            4943, 1122, -3694, -6328, -5649, -4221, -4382, -5024, 
            -4079, -1856, -3, 1012, 1608, 1568, 662, 202, 2123, 6425, 
            10796, 12885, 12284, 8558, 4929, 1768, 789, 2650, 6005, 
            8173, 7689, 5920, 5858, 8828, 12926, 15424, 15652, 14406, 
            11564, 6071, -1334, -7072, -8050, -4977, -1180, 740, 599, 
            201, 1067, 2245, 1623, -692, -2289, -1883, -949, -1191, 
            -1803, -626, 2697, 5979, 6781, 4840]

def initBuf(len):
    # return initRand(len)
    return initFixed()

##################################################
"""
Minimal buffer: 1 period + 1 sample long
idx indexes the oldest sample in the buffer, and
idx-1 the newest. We average those and multiply
by a value <1.0 to generate a new sample, then
overwrite the oldest sample and advance the index
"""
def updateMinimal(buf,n):
    global idx
    wrap = len(buf)
    prior = buf[idx-1]
    blk = [prior]
    while n > 0:
        inv = buf[idx]
        new = ((inv + prior) * fbk) // 65536
        buf[idx] = new
        blk += [new]
        idx += 1
        if idx >= wrap:
            idx = 0
        prior = inv
        n -= 1

    return buf, blk

"""
Extended buffer
idx indexes the oldest sample in the buffer, whose
length may not be an integer multiple of the period, 
but is guaranteed to be longer.

We therefore have to mix idx-1 with idx-period
"""
def updateExtended(buf,n):
    global idx
    wrap = len(buf)
    prior = buf[idx-1]
    blk = [prior]
    while n > 0:
        inv = buf[idx-period]
        new = ((inv + prior) * fbk) // 65536
        if idx >= wrap:
            idx = 0
        buf[idx] = new
        blk += [new]
        idx += 1
        prior = inv
        n -= 1

    return buf, blk


def update(buf,n):
    global extendBy
    if 0 == extendBy:
        return updateMinimal(buf,n)
    else:
        return updateExtended(buf,n)

##################################################
buf = initBuf(period) # one cycle stimulus
if 0 != extendBy:
    buf = [0]*extendBy + buf
    idx = len(buf)
xl = [(x-idx)/period for x in range(len(buf))] # 0.0 .. 1.0 is one cycle

# plot initial stimulus before t=0
fig,ax = plt.subplots()
ax.grid(visible=True,which='both',axis='x')
ax.plot(xl,buf)

# subsequent plots are one block+1sample, not one period
perFrac = 1/period
xl = [x*perFrac for x in range(upl+1)]

# iterate a number of blocks
for i in range(20):
    buf,blk = update(buf,upl)
    if False and 6 == i:
        print(buf)
    ax.plot(xl,blk)
    xs = [xl[-1]]
    xl = [x+upl*perFrac for x in xl]
    xl = xs + xl[:upl]

    period += 4

plt.minorticks_on()    
ax.xaxis.set_major_locator(MultipleLocator(5))
ax.xaxis.set_minor_locator(MultipleLocator(1))
plt.show()

