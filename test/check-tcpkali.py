#!/usr/bin/env python

import os
import re
import sys
import time
import tempfile
import datetime
import subprocess


def log(*args):
    ts = datetime.datetime.now()
    sys.stderr.write("[%s]: %s\n" %
                     (ts, ' '.join(map(lambda x: str(x), list(args)))))
    sys.stderr.flush()


# Wrapper around `tcpkali` process and its output.
class Tcpkali(object):
    def __init__(self, args, **kvargs):
        self.proc = None

        exe = os.getenv("TCPKALI", "../src/tcpkali")

        self.fout = tempfile.TemporaryFile()
        self.ferr = tempfile.TemporaryFile()

        full_args = [exe] + args
        if kvargs.get('capture_io', False):
            full_args.append("--dump-one")
        self.proc = subprocess.Popen(full_args,
                                     stdout=self.fout, stderr=self.ferr)
        time.sleep(0.1)
        self.proc.poll()
        log(' '.join(full_args))
        if self.proc.returncode is not None:
            log("Could not start the tcpkali process: %r\n"
                % self.proc.returncode)
            raise
        log("Started tcpkali pid %r" % self.proc.pid)

    def results(self):
        self._wait()
        self.fout.seek(0)
        self.ferr.seek(0)
        out = self.fout.readlines()
        err = self.ferr.readlines()
        return (out, err)

    def _wait(self):
        if self.proc:
            self.proc.wait()
            if self.proc.returncode is None:
                log("Could not stop the tcpkali process\n")
                raise
            pid = self.proc.pid
            self.proc = None
            log("Stopped tcpkali pid %r" % pid)

    def __del__(self):
        if self.proc:
            self.proc.terminate()
            self._wait()


# Results analyzer
class Analyze(object):
    def __init__(self, args):
        self.out_lengths = {}
        self.out_minLength = None
        self.out_maxLength = None
        self.out_num = 0
        self.in_lengths = {}
        self.in_minLength = None
        self.in_maxLength = None
        self.in_num = 0

        # Output and Error lines
        outLines = args[0]
        errLines = args[1]

        outRe = re.compile("^Out\(\d+, (\d+)\): \[(.*)\]$")
        for _, line in enumerate(errLines):
            result = outRe.match(line)
            if result:
                self.out_num = self.out_num + 1
                outLen = int(result.group(1))
                outContent = result.group(2)
                self._record_occurrence(self.out_lengths, outLen)
                if self.out_minLength is None or outLen < self.out_minLength:
                    self.out_minLength = outLen
                if self.out_maxLength is None or outLen > self.out_maxLength:
                    self.out_maxLength = outLen

        inRe = re.compile("^In\(\d+, (\d+)\): \[(.*)\]$")
        for _, line in enumerate(errLines):
            result = inRe.match(line)
            if result:
                self.in_num = self.in_num + 1
                inLen = int(result.group(1))
                inContent = result.group(2)
                self._record_occurrence(self.in_lengths, inLen)
                if self.in_minLength is None or inLen < self.in_minLength:
                    self.in_minLength = inLen
                if self.in_maxLength is None or inLen > self.in_maxLength:
                    self.in_maxLength = inLen

        bwRe = re.compile("^Aggregate bandwidth: "
                          "([\d.]+)[^\d]+, ([\d.]+)[^\d]+ Mbps")
        bws = [bwRe.match(line) for line in outLines if bwRe.match(line)][0]
        self.bw_down_mbps = float(bws.group(1))
        self.bw_up_mbps = float(bws.group(2))

        totalRe = re.compile("^Total data (sent|received):.*"
                             "\(([\d.]+) bytes\)")
        sent = [totalRe.match(line) for line in outLines
                if totalRe.match(line) and 'sent' in line][0]
        self.total_sent_bytes = int(sent.group(2))
        rcvd = [totalRe.match(line) for line in outLines
                if totalRe.match(line) and 'received' in line][0]
        self.total_received_bytes = int(rcvd.group(2))

        self.debug()

    def _record_occurrence(self, d, len):
        if(len > 50 and len <= 100):
            len = 10 * (len / 10)
        elif(len > 100 and len <= 2000):
            len = 100 * (len / 100)
        elif(len > 2000):
            len = 2000
        d[len] = d.get(len, 0) + 1

    def _total_occurrences(self, d):
        return sum(d.values())

    # Int -> Int(0..100)
    def input_length_percentile_lte(self, n):
        return self._length_percentile_lte(self.in_lengths, n)

    # Int -> Int(0..100)
    def output_length_percentile_lte(self, n):
        return self._length_percentile_lte(self.out_lengths, n)

    def _length_percentile_lte(self, d, n):
        total = self._total_occurrences(d)
        if total > 0:
            occurs = sum([v for (k, v) in d.items() if k <= n])
            return (100 * occurs / total)
        else:
            return 0

    def debug(self):
        [log("  '%s': %s" % kv) for kv in sorted(vars(self).items())]

port = 1350

print("Rate limiting cuts packets across message boundaries")
port = port + 1
t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
             "-r3k", "-mABC"], capture_io=True)
a = Analyze(t.results())
assert a.output_length_percentile_lte(4 * len("ABC")) > 90
assert sum([a.out_lengths.get(i, 0) for i in range(1, 10) if i % 3]) == 0


print("Write combining OFF still cuts packets across message boundaries")
port = port + 1
t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
             "-r3k", "-mABC", "--write-combine=off"], capture_io=True)
a = Analyze(t.results())
assert a.output_length_percentile_lte(4 * len("ABC")) > 90
assert sum([a.out_lengths.get(i, 0) for i in range(1, 10) if i % 3]) == 0


print("Rate limiting smoothess with 2kRPS")
port = port + 1
t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
             "-r2k", "-mABC"], capture_io=True)
a = Analyze(t.results())
# Check for not too many long packets outliers (<5%).
assert a.output_length_percentile_lte(4 * len("ABC")) > 95


print("Rate limiting smoothess with 15kRPS")
port = port + 1
t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
             "-r15k", "-mABC"], capture_io=True)
a = Analyze(t.results())
# Check for not too many short packets outliers (<10%).
assert a.output_length_percentile_lte(2 * len("ABC")) < 10


print("Observe write combining at 20kRPS by default")
port = port + 1
t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
             "-w1",  # Multi-core affects (removes) TCP level coalescing
                     # So we disable it here to obtain some for
                     # proper operation of input_length_percentile_lte().
             "-r20k", "-mABC", "--dump-all"], capture_io=True)
a = Analyze(t.results())
# Check for not too many short packets outliers (<10%).
assert a.output_length_percentile_lte(4 * len("ABC")) < 10
assert a.input_length_percentile_lte(1 * len("ABC")) < 10


print("No write combining at 20kRPS with --write-combine=off")
port = port + 1
t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
             "-w1",  # Multi-core affects (removes) TCP level coalescing
                     # So we disable it here to obtain some for
                     # proper operation of input_length_percentile_lte().
             "-r20k", "-mABC", "--dump-all", "--write-combine=off"],
            capture_io=True)
a = Analyze(t.results())
# Check for all writes being short (non-coalesced) ones.
assert a.output_length_percentile_lte(len("ABC")) == 100
# Check that not all reads are de-coalesced. Statistically speaking,
# on one core there must be at least some read()-coalescing.
assert a.input_length_percentile_lte(1 * len("ABC")) < 50


# Perform generic bandwidth limiting in different directions,
# while varying options
for variant in [[], ["--websocket"], ["--write-combine=off"],
                ["--websocket", "--write-combine=off"]]:

    print("Can do more than 100 Mbps if short-cirquited"
          ", opts=" + str(variant))
    port = port + 1
    t = Tcpkali(variant + ["-l" + str(port), "127.1:" + str(port),
                           "-m1", "-T1", "--listen-mode=active"])
    a = Analyze(t.results())
    assert a.bw_down_mbps > 100 and a.bw_up_mbps > 100

    print("Can effectively limit upstream bandwidth from sender"
          ", opts=" + str(variant))
    port = port + 1
    receiver = Tcpkali(variant + ["-l" + str(port), "-T3"])
    sender = Tcpkali(variant + ["127.1:" + str(port), "-m1", "-T3",
                                "--channel-bandwidth-upstream=100kbps"])
    arcv = Analyze(receiver.results())
    asnd = Analyze(sender.results())
    assert(arcv.bw_up_mbps < 0.01 and
           arcv.bw_down_mbps > 0.090 and arcv.bw_down_mbps < 0.110)
    assert(asnd.bw_down_mbps < 0.01 and
           asnd.bw_up_mbps > 0.090 and asnd.bw_up_mbps < 0.110)

    # This test is special because downstream rate limit is not immediately
    # visible on the sender. The feedback loop takes time to stabilize.
    port = port + 1
    print("Can effectively limit downstream bandwidth from receiver"
          ", opts=" + str(variant))
    receiver = Tcpkali(variant + ["-l" + str(port), "-T11", "--rcvbuf=5k",
                                  "--channel-bandwidth-downstream=100kbps"])
    sender = Tcpkali(variant + ["127.1:" + str(port), "-m1", "-T11",
                                "--sndbuf=5k"])
    arcv = Analyze(receiver.results())
    asnd = Analyze(sender.results())
    transfer = ((100 * 1024 / 8) * 11)
    trans_min = 0.9 * transfer
    trans_max = 1.1 * transfer
    assert(arcv.total_sent_bytes < 1000 and
           arcv.total_received_bytes > trans_min and
           arcv.total_received_bytes < trans_max)
    assert(asnd.total_received_bytes < 1000 and
           asnd.total_sent_bytes > trans_min and
           asnd.total_sent_bytes < 2 * trans_max)

print("FINISHED")
