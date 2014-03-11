#!/usr/bin/env python

'''
# Things I want this tool to show:
# 1) Application data that's sent or received
#    a) With annotations: IROB numbers, data size, dependencies
# 2) Which network data is transferred on
# 3) When data is acknowledged (IntNW ACK)
# 4) When data is retransmitted
# 5) When networks come and go
# 6) (Maybe) IntNW control messages


# Visualization ideas:
# 1) Which network: a vertical section of the plot for each network
# 2) Sending app data: solid colored bar
# 3) Waiting for ACK: empty bar with solid outline
# 4) Sent vs. received app data: two vertical subsections (send/recv)
#    in each network's vertical section
# 5) Network coming and going: shaded section, same as for IMP
# 6) Annotations: popup box on hover (PyQt)
'''

from argparse import ArgumentParser
import re
import analyze_app_log
from analyze_app_log import AppPlotter

class IntNWPlotter(AppPlotter):
    def __init__(self, args):
        AppPlotter.__init__(self, args, "trace_replayer.log", "replayer_server.log")
        self._session_regex = re.compile("start ([0-9]+\.[0-9]+)" + 
                                         ".+duration ([0-9]+\.[0-9]+)")
        

    def _readSessions(self):
        runs = []
        new_run = True
        for linenum, line in enumerate(open(self._sessions_logfile).readlines()):
            fields = line.strip().split()
            if "Session times:" in line:
                # start over with the better list of sessions
                if len(runs[-1]) == 0:
                    runs = runs[:-1]
                runs[-1] = []
            elif "  Session" in line:
                timestamp, duration = self._getSession(line)
                transfer = {'start': timestamp, 'end': timestamp + duration}

                sessions = runs[-1]
                sessions.append(transfer)
            else:
                try:
                    timestamp = float(fields[0])
                except (ValueError, IndexError):
                    continue

            if "Executing:" in line and fields[2] == "at":
                # start of a session
                transfer = {'start': timestamp, 'end': None}
                runs[-1].append(transfer)
            elif (("Waiting to execute" in line and fields[4] == "at") or
                  "Waiting until trace end" in line):
                if new_run:
                    # new run
                    runs.append([])
                    new_run = False
                elif len(runs[-1]) > 0:
                    # end of a session
                    runs[-1][-1]['end'] = timestamp
            elif "Done with trace replay" in line:
                new_run = True

        if len(runs[-1]) == 0:
            runs = runs[:-1]
        return runs

    def _getSession(self, line):
        match = re.search(self._session_regex, line)
        if not match:
            raise LogParsingError("expected timestamp and duration")

        return [float(s) for s in match.groups()]


def main():
    parser = ArgumentParser()
    # no args specific to the intnw trace replayer
    args, args_list = parser.parse_known_args()
    analyze_app_log.main(args, args_list, IntNWPlotter)

if __name__ == '__main__':
    main()
