# Persistent clients tps and latency inside burst windows.
# usage: stats.py <burst result dir>
#   <dir>/bursts.txt   lines from burst.py: <start_ns> <end_ns> <ok>
#   <dir>/steady/s.*   pgbench per-transaction logs (-l)
import glob
import statistics
import sys


def pct(values, p):
    values = sorted(values)
    return values[min(len(values) - 1, int(len(values) * p))]


d = sys.argv[1]
bursts = [tuple(map(int, line.split())) for line in open(d + "/bursts.txt")]
windows = [(s // 1000, e // 1000) for s, e, _ in bursts]

# (end time us, latency us)
tx = []
for f in glob.glob(d + "/steady/s.*"):
    for line in open(f):
        p = line.split()
        tx.append((int(p[4]) * 1000000 + int(p[5]), int(p[2])))
tx.sort()

inside = [t for t in tx if any(s <= t[0] <= e for s, e in windows)]

# quiet period: from 1s after start up to the first burst
start = tx[0][0] + 1000000
quiet = [t for t in tx if start <= t[0] < windows[0][0]]

quiet_tps = len(quiet) / ((windows[0][0] - start) / 1e6)
burst_tps = len(inside) / (sum(e - s for s, e in windows) / 1e6)
took = [(e - s) / 1e3 for s, e in windows]

print(f"quiet {quiet_tps / 1000:.1f}k tps, "
      f"during burst {burst_tps / 1000:.1f}k tps "
      f"({burst_tps / quiet_tps * 100:.0f}%), "
      f"p99 {pct([t[1] for t in inside], 0.99) / 1000:.2f} ms, "
      f"max {max(t[1] for t in inside) / 1000:.1f} ms, "
      f"100 connections took {statistics.mean(took):.0f} ms, "
      f"failed bursts {sum(1 for b in bursts if not b[2])}")
