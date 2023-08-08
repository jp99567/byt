import csv
import sys


def parity(v):
    par = 0
    for i in range(31):
        if (v & (1 << i)) > 0:
            par += 1
    if (par & 1) > 0:
        return v | (1 << 31)
    else:
        return v & ~(1 << 31)

tqbit = 5

class ParseOt:
    name = 'noname'
    t0 = 0
    ta0 = 0
    wv = True
    cnt = -1
    v = 0

    def doit(self, t, v):
        #print(f'{self.name} {t-self.t0} state:{self.wv} cnt:{self.cnt} v:{v}')
        if self.wv and v:
            #if t - self.ta0 > 10*tqbit:
            self.t0 = t - 2 * tqbit
            print(f'{self.name} start {t-self.ta0}')
            self.wv = False
            self.cnt = 32
            self.v = 0
        else:
            dt = t - self.t0
            if (tqbit*3)+2 < dt < (tqbit*(4+1))-2:
                self.t0 = t
                if self.cnt == -1 or self.cnt == 32:
                    if v:
                        print(f' {self.name} error1 cnt:{self.cnt}')
                        self.wv = True
                    elif self.cnt == -1:
                        if self.v == parity(self.v):
                            print(f'ok {self.name} {self.v:08X}')
                        else:
                            print(f'error parity {self.name} {self.v:08X}')
                        self.wv = True
                elif self.cnt < 32:
                    print(f'{self.name} bitval {dt} {not v} cnt:{self.cnt}')
                    if not v:
                        self.v |= (1 << self.cnt)
                self.cnt -= 1
            elif dt >= (5*tqbit):
                self.wv = True
                if v:
                    print(f' {self.name} {dt} error3 cnt:{self.cnt}')
                    if dt > 20*tqbit:
                        self.doit(t, v)
                else:
                    print(f' {self.name} {dt} error2 cnt:{self.cnt}')
            else:
                print(f'{self.name} return {dt} {v} cnt:{self.cnt}')
        self.ta0 = t


def readCsv(path):
    with open(path, newline='') as csvfile:
        reader = csv.reader(csvfile, delimiter=',')
        for skip in range(5):
            print(f"skip header: {next(reader)}")

        t = 0
        prev1 = False
        prev2 = False
        cntZ1 = 0
        otm = ParseOt()
        ots = ParseOt()
        otm.name = 'master'
        ots.name = 'slave'
        try:
            while True:
                row = list(next(reader))
                v1 = float(row[1]) > 12.5
                v2 = float(row[0]) > 0.15 and cntZ1 > 100
                # print(f'{t} {v1} {v2}')
                if v1 != prev1:
                    otm.doit(t, v1)
                    prev1 = v1

                cntZ1 += 1
                if v1:
                    cntZ1 = 0

                if v2 != prev2:
                    ots.doit(t, v2)
                    prev2 = v2
                t += 1
        except StopIteration:
            pass



readCsv(sys.argv[1])



