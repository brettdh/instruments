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
            (lambda line: "Done with output_" in line): self._parseRecognitionEnd,
            (lambda line: "Starting UTT" in line): self._parseDecoderStart,
            (lambda line: "finished decoding" in line): self._parseDecoderEnd,
            (lambda line: "failed decoding" in line): self._parseDecoderFailure,
            (lambda line: "Terminate was called" in line): self._parseDecoderCancelled,
            (lambda line: "Manually terminated" in line): self._parseDecoderCancelled,
            (lambda line: "Wifi lost" in line): self._parseWifiLost,
        }

    def _lineStartsNewSessionsRun(self, line):
        return "Starting experiment" in line or "Speech server starting up" in line

    def _readSessions(self):
        self._runs = []
        sessions = []
        for linenum, line in enumerate(open(self._sessions_log).readlines()):
            if self._lineStartsNewSessionsRun(line):
                sessions = []
                decodings = {}
                failures = []
                cancellations = []
                self._runs.append(sessions)
                self._decodings.append(decodings)
                self._failures.append(failures)
                self._cancellations.append(cancellations)
                self._remote_decodings = {}
                continue
                
            fields = line.strip().split()
            if not fields:
                continue
            
            for predicate in self._parsers:
                if predicate(line):
                    timestamp = float(fields[0][1:-1]) / 1000.0

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
        decoding = {'start': timestamp, 'end': None, 'type': type}
        decodings[type].append(decoding)
        if type == "RemoteDecoder":
            filename = self._getDecoderFilename(fields)
            decoding['filename'] = filename
            if filename not in self._remote_decodings:
                self._remote_decodings[filename] = decoding

    def _parseDecoderEnd(self, timestamp, line, fields):
        type = self._getDecoderType(fields)
        decodings = self._decodings[-1]
        if type not in decodings:
            return None

        cur_decoding = decodings[type][-1]
        if 'filename' in cur_decoding:
            filename = cur_decoding['filename']
            if filename in self._remote_decodings:
                del self._remote_decodings[filename]

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

    def _parseWifiLost(self, timestamp, line, fields):
        filename = self._getDecoderFilename(fields)
        if filename in self._remote_decodings:
            decoding = self._remote_decodings[filename]
            if "wifi_loss" not in decoding:
                decoding["wifi_loss"] = timestamp
            

    decoder_type_re = re.compile("((?:Local|Remote)Decoder)")
    def _getDecoderType(self, fields):
        return self.decoder_type_re.search(fields[1]).group(1)

    filename_re = re.compile("RemoteDecoder\[(.+)\]")
    def _getDecoderFilename(self, fields):
        match = self.filename_re.search(fields[1])
        return match.group(1) if match else None

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

        def get_start(decoding):
            return intnw_window.getAdjustedTime(decoding['start'])
            
        fake_length = 10.0
        def get_length(decoding):
            if decoding['end']:
                return decoding['end'] - decoding['start']
            elif "wifi_loss" in decoding:
                return decoding['wifi_loss'] - decoding['start']
            else:
                unending_decodings.append(decoding)
                return fake_length # fake, just have something for these weird ones
                

        unending_decodings = []
        for decoder_type in decodings:
            cur_decodings = decodings[decoder_type]
            for decoding in cur_decodings:
                if not decoding['end'] and "wifi_lost" in decoding:
                    decoding['end'] = decoding['wifi_lost']
                
            bars = [[get_start(d), get_length(d)] for d in cur_decodings]
            axes.broken_barh(bars, [positions[decoder_type], height], color=colors[decoder_type],
                             linewidth=0.33, edgecolor="black")

        if unending_decodings:
            fake_endings = [get_start(d) + fake_length for d in unending_decodings]
            fake_end_positions = [positions[d['type']] + height / 2.0 for d in unending_decodings]
            axes.plot(fake_endings, fake_end_positions, marker='?',
                      markersize=10, markeredgewidth=2, linestyle='none')

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
    parser.add_argument("--server", action="store_true", default=False)
    # no args specific to the remote-exec app
    args, args_list = parser.parse_known_args()
    analyze_app_log.main(args, args_list, RemoteExecPlotter)

if __name__ == '__main__':
    main()
