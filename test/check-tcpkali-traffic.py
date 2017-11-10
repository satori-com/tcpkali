#!/usr/bin/env python
# pylint: disable=invalid-name,print-statement,superfluous-parens
"""
Run several tcpkali processes in source-destination mode and figure out
if they're behaving according to expectations
"""

from __future__ import absolute_import
import os
import re
import sys
import time
import tempfile
import datetime
import subprocess


def log(*args):
    """Print out the arguments in a log-like fashion."""
    timestamp = datetime.datetime.now()
    sys.stderr.write("[%s]: %s\n" %
                     (timestamp, ' '.join([str(x) for x in list(args)])))
    sys.stderr.flush()


class Tcpkali(object):
    """Wrapper around `tcpkali` process and its output."""
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
            raise Exception("Cannot start the tcpkali process")
        log("Started tcpkali pid %r" % self.proc.pid)

    def results(self):
        """Return results of tcpkali operation."""
        self.wait()
        self.fout.seek(0)
        self.ferr.seek(0)
        out = self.fout.readlines()
        err = self.ferr.readlines()
        return (out, err)

    def wait(self):
        """Wait until tcpkali finished."""
        if self.proc:
            self.proc.wait()
            if self.proc.returncode is None:
                log("Could not stop the tcpkali process\n")
                raise Exception("Can't stop tcpkali process")
            pid = self.proc.pid
            self.proc = None
            log("Stopped tcpkali pid %r" % pid)

    def __del__(self):
        if self.proc:
            self.proc.terminate()
            self.wait()


class Analyze(object):
    """Tcpkali output analyzer."""
    # pylint: disable=too-many-instance-attributes,too-many-locals
    def __init__(self, args):
        self.out_lengths = {}
        self.out_num = 0
        self.in_lengths = {}
        self.in_num = 0

        # Output and Error lines
        outLines = args[0]
        errLines = args[1]

        def _record_occurrence(d, length):
            if length > 50 and length <= 100:
                length = 10 * (length // 10)
            elif length > 100 and length <= 2000:
                length = 100 * (length // 100)
            elif length > 2000:
                length = 2000
            d[length] = d.get(length, 0) + 1

        outRe = re.compile(r"^Snd\(\d+, (\d+)\): \[(.*)\]$")
        for _, line in enumerate(errLines):
            result = outRe.match(line)
            if result:
                self.out_num = self.out_num + 1
                outLen = int(result.group(1))
                _record_occurrence(self.out_lengths, outLen)

        inRe = re.compile(r"^Rcv\(\d+, (\d+)\): \[(.*)\]$")
        for _, line in enumerate(errLines):
            result = inRe.match(line)
            if result:
                self.in_num = self.in_num + 1
                inLen = int(result.group(1))
                _record_occurrence(self.in_lengths, inLen)

        bwRe = re.compile(r"^Aggregate bandwidth: "
                          r"([\d.]+)[^\d]+, ([\d.]+)[^\d]+ Mbps")
        bws = [bwRe.match(line) for line in outLines if bwRe.match(line)][0]
        self.bw_down_mbps = float(bws.group(1))
        self.bw_up_mbps = float(bws.group(2))

        totalRe = re.compile(r"^Total data (sent|received):.*"
                             r"\(([\d.]+) bytes\)")
        sent = [totalRe.match(line) for line in outLines
                if totalRe.match(line) and 'sent' in line][0]
        self.total_sent_bytes = int(sent.group(2))
        rcvd = [totalRe.match(line) for line in outLines
                if totalRe.match(line) and 'received' in line][0]
        self.total_received_bytes = int(rcvd.group(2))

        sockoptRe = re.compile(r"^WARNING: --(snd|rcv)buf option "
                               r"makes no effect.")
        self.sockopt_works = True
        for _, line in enumerate(errLines):
            result = sockoptRe.match(line)
            if result:
                self.sockopt_works = False

        self.debug()

    # Int -> Int(0..100)
    def input_length_percentile_lte(self, n):
        """
        Determine percentile value of all occurrences of input lengths
        less or equal to n.
        """
        return self._length_percentile_lte(self.in_lengths, n)

    # Int -> Int(0..100)
    def output_length_percentile_lte(self, n):
        """
        Determine percentile value of all occurrences of output lengths
        less or equal to n.
        """
        return self._length_percentile_lte(self.out_lengths, n)

    def _length_percentile_lte(self, d, n):
        # pylint: disable=no-self-use
        total = sum(d.values())
        if total > 0:
            occurs = sum([v for (k, v) in d.items() if k <= n])
            return 100 * occurs // total
        return 0

    def debug(self):
        """Print the variables representing analyzed tcpkali output."""
        for kv in sorted(vars(self).items()):
            log("  '%s': %s" % kv)


def check_segmentation(prefix, lines, contains):
    """Check that the output consists of the neat repetition of (contents)"""
    reg = re.compile(r"^" + prefix + r"\(\d+, \d+\): \[(.*)\]$")
    allOutput = ""
    for _, line in enumerate(lines):
        result = reg.match(line)
        if result:
            allOutput += result.group(1)
    assert(allOutput != "")
    reg = re.compile(r"^((" + contains + ")+)(.*)$")
    result = reg.match(allOutput)
    if not result:
        print("Expected repetition of \"%s\" is not found in \"%s\"..." %
              (contains, allOutput[0:len(contains)+1]))
        return False
    if not result.group(3):
        return True
    print("Output is not consistent after byte %d (...\"%s\");"
          " continuing with \"%s\"..." %
          (len(result.group(1)), result.group(2),
           result.group(3)[0:len(contains)+1]))
    return False


def main():
    """Run multiple tests with tcpkali and see if results are correct"""
    # pylint: disable=too-many-statements
    port = 1350

    if os.environ.get('CONTINUOUS_INTEGRATION', 'false') == 'false':
        print("Correctness of data packetization")
        port = port + 1
        # rate 162222 = 1460 (tapkali constant) * 1000 (hz) / 9 (message len)
        t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
                     "-mFOOBARBAZ", "-r162222"], capture_io=True)
        (_, errLines) = t.results()
        assert check_segmentation("Snd", errLines, "FOOBARBAZ")

    print("Slow rate limiting cuts packets at message boundaries")
    port = port + 1
    t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
                 "-r20", "-mABC"], capture_io=True)
    a = Analyze(t.results())
    assert a.output_length_percentile_lte(len("ABC")) == 100

    print("Rate limiting at 2k does not create single-message writes")
    port = port + 1
    t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
                 "-r2k", "-mABC"], capture_io=True)
    a = Analyze(t.results())
    assert a.output_length_percentile_lte(len("ABC")) < 2
    assert sum([a.out_lengths.get(i, 0) for i in range(1, 10) if i % 3]) == 0

    print("Rate limiting cuts packets at message boundaries")
    port = port + 1
    t = Tcpkali(["-l" + str(port), "127.1:" + str(port), "-T1",
                 "-r3k", "-mABC"], capture_io=True)
    a = Analyze(t.results())
    assert a.output_length_percentile_lte(4 * len("ABC")) > 90
    assert sum([a.out_lengths.get(i, 0) for i in range(1, 10) if i % 3]) == 0

    print("Write combining OFF still cuts packets at message boundaries")
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
        receiver = Tcpkali(variant +
                           ["-l" + str(port), "-T11", "--rcvbuf=5k",
                            "--channel-bandwidth-downstream=100kbps"])
        sender = Tcpkali(variant + ["127.1:" + str(port), "-m1", "-T11",
                                    "--sndbuf=5k"])
        arcv = Analyze(receiver.results())
        asnd = Analyze(sender.results())
        transfer = ((100 * 1024 // 8) * 11)
        trans_min = 0.85 * transfer
        trans_max = 1.10 * transfer
        assert((arcv.total_sent_bytes < 1000 and
                arcv.total_received_bytes > trans_min and
                arcv.total_received_bytes < trans_max) or
               not arcv.sockopt_works)
        assert((asnd.total_received_bytes < 1000 and
                asnd.total_sent_bytes > trans_min and
                asnd.total_sent_bytes < 3 * trans_max) or
               not asnd.sockopt_works)

    print("FINISHED")

if __name__ == "__main__":
    main()
