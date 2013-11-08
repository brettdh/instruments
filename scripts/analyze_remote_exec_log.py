#!/usr/bin/env python

'''
# Things I want this tool to show:
1) Speech recognition times
2) Whether the recognition was done redundantly
3) Whether the winner was local or remote
'''

from argparse import ArgumentParser
import re
import analyze_app_log
from analyze_app_log import AppPlotter

class RemoteExecPlotter(AppPlotter):
    def __init__(self, args):
        AppPlotter.__init__(self, args, "speech_client.log", "speech_server.log")
        

    def _readSessions(self):
        runs = []
        sessions = []
        for linenum, line in enumerate(open(self._app_client_log).readlines()):
            if "Starting experiment" in line:
                sessions = []
                runs.append(sessions)
                continue
                
            fields = line.strip().split()
            timestamp = float(fields[0][1:-1]) / 1000.0

            if "Recognizing utterance" in line:
                filename = fields[3]
                sessions.append({'start': timestamp, 'end': None, 'filename': filename})
            elif "Done with" in line:
                assert sessions
                filename = fields[3][:-1]
                recognition_time = float(fields[5])
                speech_size = int(fields[8])

                session = sessions[-1]
                assert session['filename'] == filename
                session['end'] = session['start'] + recognition_time

        print runs
        return runs
        
        
def main():
    parser = ArgumentParser()
    # no args specific to the remote-exec app
    args, args_list = parser.parse_known_args()
    analyze_app_log.main(args, args_list, RemoteExecPlotter)

if __name__ == '__main__':
    main()
