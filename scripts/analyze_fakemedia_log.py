#!/usr/bin/env python

from argparse import ArgumentParser
import re
import analyze_app_log
from analyze_app_log import AppPlotter

class FakemediaPlotter(AppPlotter):
    def __init__(self, args):
        AppPlotter.__init__(self, args, "fakemedia_test.log", "fakemedia_server.log")
        self._timestamp_regex = re.compile("^\[([0-9.]+)\]")
        self._start_experiment_regex = re.compile("Starting [0-9]+ second experiment")
        self._total_stall_time_regex = re.compile("stall time: ([0-9.]+) seconds")

        self._matchers = {
            self._start_experiment_regex: self._startNewRun,
            self._total_stall_time_regex: self._addStallTime,
        }
    def _getTimestamp(self, line):
        match = self._timestamp_regex.search(line)
        if match:
            return float(match.group(1))

    def _readSessions(self):
        runs = []
        # regardless of whether it's client or server, buffer info is in client log
        for linenum, line in enumerate(open(self._app_client_log).readlines()):
            for matcher, action in self._matchers.items():
                match = matcher.search(line)
                if match:
                    action(runs, self._getTimestamp(line), match)
                
        return runs
        
    def _startNewRun(self, runs, timestamp, match):
        runs.append([])

    def _addStallTime(self, runs, timestamp, match):
        total_stall_time = float(match.group(1))

        # just using this to draw the total stall over time
        fake_session = {'start': timestamp, 'end': timestamp + total_stall_time}
        runs[-1].append(fake_session)
            

def main():
    parser = ArgumentParser()
    # no args specific to the fakemedia trace replayer
    args, args_list = parser.parse_known_args()
    analyze_app_log.main(args, args_list, FakemediaPlotter)

if __name__ == '__main__':
    main()
