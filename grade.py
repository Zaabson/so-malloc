#!/usr/bin/env python3

import subprocess
import sys


TRACEFILES = [
        "traces/amptjp-bal.rep",
        "traces/amptjp.rep",
        "traces/binary2-bal.rep",
        "traces/binary2.rep",
        "traces/binary-bal.rep",
        "traces/binary.rep",
        "traces/cccp-bal.rep",
        "traces/cccp.rep",
        "traces/coalescing-bal.rep",
        "traces/coalescing.rep",
        "traces/cp-decl-bal.rep",
        "traces/cp-decl.rep",
        "traces/expr-bal.rep",
        "traces/expr.rep",
        "traces/random2-bal.rep",
        "traces/random2.rep",
        "traces/random-bal.rep",
        "traces/random.rep",
        "traces/realloc2-bal.rep",
        "traces/realloc2.rep",
        "traces/realloc-bal.rep",
        "traces/realloc.rep",
        "traces/short1-bal.rep",
        "traces/short1.rep",
        "traces/short2-bal.rep",
        "traces/short2.rep"]

TRACEFILES_EXTRA = [
        "traces-private/alaska.rep",
        "traces-private/bash.rep",
        "traces-private/chrome.rep",
        "traces-private/coalesce-big.rep",
        "traces-private/corners.rep",
        "traces-private/firefox-reddit2.rep",
        "traces-private/firefox-reddit.rep",
        "traces-private/firefox.rep",
        "traces-private/freeciv.rep",
        "traces-private/fs.rep",
        "traces-private/hostname.rep",
        "traces-private/login.rep",
        "traces-private/lrucd.rep",
        "traces-private/ls.1.rep",
        "traces-private/ls.rep",
        "traces-private/malloc-free.rep",
        "traces-private/malloc.rep",
        "traces-private/merry-go-round.rep",
        "traces-private/nlydf.rep",
        "traces-private/perl.1.rep",
        "traces-private/perl.2.rep",
        "traces-private/perl.3.rep",
        "traces-private/perl.rep",
        "traces-private/pulseaudio.rep",
        "traces-private/qyqyc.rep",
        "traces-private/rm.1.rep",
        "traces-private/rm.rep",
        "traces-private/rulsr.rep",
        "traces-private/seglist.rep",
        "traces-private/stty.rep",
        "traces-private/tty.rep",
        "traces-private/xterm.rep"]


if __name__ == '__main__':
    all_ops = []
    all_insn = []
    all_util = []

    for trace in TRACEFILES:
        try:
            mdriver = subprocess.run([
                "valgrind",
                "--tool=callgrind",
                "--callgrind-out-file=callgrind.out",
                "--toggle-collect=mm_malloc",
                "--toggle-collect=mm_free",
                "--toggle-collect=mm_realloc",
                "--toggle-collect=mm_calloc",
                "--", "./mdriver", "-f", trace],
                capture_output=True, timeout=30)
        except subprocess.TimeoutExpired:
            print("Killed due to timeout of 30s.")
            continue

        print(mdriver.stdout.decode())
        if mdriver.returncode:
            sys.exit(1)

        # Process statistics from mdriver
        stats = mdriver.stdout.decode().splitlines()[3][4:].split()
        try:
            util = float(stats[1][:-1])
        except ValueError:
            util = 0.0
        all_util.append(util)

        all_ops.append(int(stats[2]))

        annotate = subprocess.run([
            "callgrind_annotate", "--tree=calling", "callgrind.out"],
            capture_output=True)

        # Process output from callgrind_annotate
        for line in annotate.stdout.decode().splitlines()[26:]:
            if not line:
                continue
            print(line)
            all_insn.append(int(line.split()[0].replace(',', '')))

        sys.stdout.flush()

    # Grade solution
    weighted_util = 0
    for util, ops in zip(all_util, all_ops):
        weighted_util += util * (ops / sum(all_ops))

    print("\nWeighted memory utilization: %.1f%%" % weighted_util)
    print("Instructions per operation: %d" % (sum(all_insn) / sum(all_ops)))
