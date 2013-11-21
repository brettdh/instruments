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
import numpy as np

class RemoteExecPlotter(AppPlotter):
    def __init__(self, args):
        AppPlotter.__init__(self, args, "speech_client.log", "speech_server.log")
        self._appFeatureDrawers.append(self._drawRecognitions)

        self._initParsers()
        self._decodings = []
        self._failures = []
        self._cancellations = []

    def _initParsers(self):
        self._parsers = {
            (lambda line: "Recognizing utterance" in line): self._parseSpeechEnd,
            (lambda line: "Done with" in line): self._parseRecognitionEnd,
            (lambda line: "Starting UTT" in line): self._parseDecoderStart,
            (lambda line: "finished decoding" in line): self._parseDecoderEnd,
            (lambda line: "failed decoding" in line): self._parseDecoderFailure,
            (lambda line: "Terminate was called" in line): self._parseDecoderCancelled,
            (lambda line: "Manually terminated" in line): self._parseDecoderCancelled,
        }

    def _readSessions(self):
        self._runs = []
        sessions = []
        for linenum, line in enumerate(open(self._app_client_log).readlines()):
            if "Starting experiment" in line:
                sessions = []
                decodings = {}
                failures = []
                cancellations = []
                self._runs.append(sessions)
                self._decodings.append(decodings)
                self._failures.append(failures)
                self._cancellations.append(cancellations)
                continue
                
            fields = line.strip().split()
            timestamp = float(fields[0][1:-1]) / 1000.0

            for predicate in self._parsers:
                if predicate(line):
                    self._parsers[predicate](timestamp, line, fields)

        return self._runs

    def _parseSpeechEnd(self, timestamp, line, fields):
        filename = fields[3]
        sessions = self._runs[-1]
        sessions.append({'start': timestamp, 'end': None, 'filename': filename})

    def _parseRecognitionEnd(self, timestamp, line, fields):
        sessions = self._runs[-1]
        assert sessions
        filename = fields[3][:-1]
        recognition_time = float(fields[5])
        speech_size = int(fields[8])
        
        session = sessions[-1]
        assert session['filename'] == filename
        session['end'] = session['start'] + recognition_time

    def _parseDecoderStart(self, timestamp, line, fields):
        type = self._getDecoderType(fields)
        decodings = self._decodings[-1]
        if type not in decodings:
            decodings[type] = []
        decodings[type].append({'start': timestamp, 'end': None, 'type': type})

    def _parseDecoderEnd(self, timestamp, line, fields):
        type = self._getDecoderType(fields)
        decodings = self._decodings[-1]
        cur_decoding = decodings[type][-1]
        if cur_decoding['end']:
            return None
        else:
            cur_decoding['end'] = timestamp
            return cur_decoding

    def _parseDecoderFailure(self, timestamp, line, fields):
        failed_decoding = self._parseDecoderEnd(timestamp, line, fields)
        if failed_decoding:
            self._failures[-1].append(failed_decoding)

    def _parseDecoderCancelled(self, timestamp, line, fields):
        cancelled_decoding = self._parseDecoderEnd(timestamp, line, fields)
        if cancelled_decoding:
            self._cancellations[-1].append(cancelled_decoding)

    def _getDecoderType(self, fields):
        return fields[1].replace(":", "")

    def _drawRecognitions(self, run, intnw_window, axes):
        ylim = axes.get_ylim()
        middle = np.mean(ylim)
        height = (ylim[1] - ylim[0]) / 20.0
        positions = {
            "LocalDecoder": middle - height,
            "RemoteDecoder": middle
        }
        colors = {
            "LocalDecoder": "red",
            "RemoteDecoder": "green",
        }
        
        decodings = self._decodings[run]
        for decoder_type in decodings:
            cur_decodings = decodings[decoder_type]
            bars = [[intnw_window.getAdjustedTime(d['start']), 
                     d['end'] - d['start']] for d in cur_decodings]
            axes.broken_barh(bars, [positions[decoder_type], height], color=colors[decoder_type])

        fail_times = []
        fail_positions = []
        for failure in self._failures[run]:
            fail_times.append(intnw_window.getAdjustedTime(failure['end']))
            fail_positions.append(positions[failure['type']] + height / 2.0)
            
        if fail_times:
            axes.plot(fail_times, fail_positions, marker='x', 
                      linestyle='none', markersize=5, markeredgewidth=2, color='red')

        cancel_times = []
        cancel_positions = []
        for cancel in self._cancellations[run]:
            cancel_times.append(intnw_window.getAdjustedTime(cancel['end']))
            cancel_positions.append(positions[cancel['type']] + height / 2.0)

        if cancel_times:
            axes.plot(cancel_times, cancel_positions, marker='x',
                      linestyle='none', markersize=5, markeredgewidth=2, color='black')
        
def main():
    parser = ArgumentParser()
    # no args specific to the remote-exec app
    args, args_list = parser.parse_known_args()
    analyze_app_log.main(args, args_list, RemoteExecPlotter)

if __name__ == '__main__':
    main()
