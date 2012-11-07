#!/usr/bin/env python2.7

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

# Borrows heavily from
#   http://eli.thegreenplace.net/2009/01/20/matplotlib-with-pyqt-guis/

import sys, re
from argparse import ArgumentParser

from PyQt4.QtCore import *
from PyQt4.QtGui import *

import matplotlib
from matplotlib.backends.backend_qt4agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt4agg import NavigationToolbar2QTAgg as NavigationToolbar
from matplotlib.figure import Figure

from progressbar import ProgressBar

debug = False
#debug = True
def dprint(msg):
    if debug:
        print msg

class LogParsingError(Exception):
    def __init__(self, msg):
        self.msg = msg

    def setLine(self, linenum, line):
        self.linenum = linenum
        self.line = line

    def __repr__(self):
        return "LogParsingError: " + str(self)

    def __str__(self):
        return ("IntNW log parse error at line %s: %s%s"
                % (self.linenum and str(self.linenum) or "<unknown>",
                   self.msg,
                   self.line != None and ('\n' + self.line) or ""))

class IROB(object):
    def __init__(self, plot, network_type, direction, start, irob_id):
        self.__plot = plot
        self.network_type = network_type
        self.direction = direction
        self.irob_id = irob_id
        
        self.__start = start
        self.__datalen = None
        self.__expected_bytes = None
        self.__completion_time = None
        self.__drop_time = None
        self.__last_activity = None
        self.__acked = False
        
    def addBytes(self, timestamp, bytes):
        if self.__datalen == None:
            self.__datalen = 0
        self.__datalen += bytes
        self.__checkIfComplete(timestamp)

    def finish(self, timestamp, expected_bytes):
        self.__expected_bytes = expected_bytes
        self.__checkIfComplete(timestamp)

    def ack(self, timestamp):
        self.__acked = True
        self.__checkIfComplete(timestamp)

    def complete(self):
        # XXX: HACK.  Can be double-counting sent/received app data.
        # TODO: fix it.  maybe still keep track of duplicate data.
        return (self.__acked and
                (self.__datalen != None and
                 (self.direction == "up" or
                  self.__datalen >= self.__expected_bytes)))
     
    def __checkIfComplete(self, timestamp):
        self.__last_activity = timestamp
        if self.complete():
            self.__completion_time = timestamp

    def markDropped(self, timestamp):
        if self.__drop_time == None:
            dprint("Dropped %s at %f" % (self, timestamp))
            self.__drop_time = timestamp

    def getDuration(self):
        if self.complete():
            return (self.__start, self.__completion_time)
        else:
            try:
                assert self.__drop_time != None
            except AssertionError as e:
                import pdb; pdb.set_trace()
                raise e
            
            if self.__acked:
                end = self.__last_activity
            else:
                end = self.__drop_time
            return (self.__start, end)

    def __str__(self):
        return ("{IROB: id %d  direction: %s  network: %s}"
                % (self.irob_id, self.direction, self.network_type))

    def draw(self, axes):
        ypos = self.__plot.getIROBPosition(self)
        yheight = self.__plot.getIROBHeight(self)
        start, finish = [self.__plot.getAdjustedTime(ts) for ts in self.getDuration()]
        dprint("%s %f--%f" % (self, start, finish))
        axes.broken_barh([[start, finish-start]],
                         [ypos - yheight / 2.0, yheight],
                         color=self.__plot.getIROBColor(self))

    def drawIfDropped(self, axes):
        if self.__drop_time != None:
            timestamp = self.getDuration()[1]
            ypos = self.__plot.getIROBPosition(self)
            axes.plot([self.__plot.getAdjustedTime(timestamp)],
                      [ypos], marker='x', color='black', markeredgewidth=2.0)

class IntNWBehaviorPlot(QDialog):
    def __init__(self, measurements_only, network_trace_file, parent=None):
        QDialog.__init__(self, parent)

        self.__initRegexps()
        self.__networks = {}
        self.__network_periods = {}
        self.__network_type_by_ip = {}
        self.__network_type_by_sock = {}

        self.__measurements_only = measurements_only
        self.__network_trace_file = network_trace_file
        self.__estimates = {} # estimates[network_type][bandwidth|latency] -> [values]

        self.__irob_height = 0.25
        self.__network_pos_offsets = {'wifi': 1.0, '3G': -1.0}
        self.__direction_pos_offsets = {'down': self.__irob_height / 2.0,
                                        'up': -self.__irob_height / 2.0}

        self.__irob_colors = {'wifi': 'blue', '3G': 'red'}

        self.__start = None

        # TODO: infer plot title from file path
        self.__title = "IntNW"
        self.setWindowTitle(self.__title)

        self.create_main_frame()
        self.on_draw()

    def create_main_frame(self):
        self.__frame = QWidget()
        self.__dpi = 100
        self.__figure = Figure((5,4), self.__dpi) # 5" x 4"
        self.__canvas = FigureCanvas(self.__figure)
        self.__canvas.setParent(self.__frame)
        self.__axes = self.__figure.add_subplot(111)

        self.__mpl_toolbar = NavigationToolbar(self.__canvas, self.__frame)

        #
        # Layout with box sizers
        # 
        hbox = QHBoxLayout()

        if self.__measurements_only:
            self.__setupMeasurementWidgets(hbox)
            
        vbox = QVBoxLayout(self)
        vbox.addWidget(self.__canvas)
        vbox.addWidget(self.__mpl_toolbar)
        vbox.addLayout(hbox)

    def __setupMeasurementWidgets(self, hbox):
        self.__show_wifi = QCheckBox("wifi")
        self.__show_threeg = QCheckBox("3G")
        self.__show_trace = QCheckBox("Trace display")
        checks = [self.__show_wifi, self.__show_threeg, self.__show_trace]

        networks = QVBoxLayout()
        networks.addWidget(self.__show_wifi)
        networks.addWidget(self.__show_threeg)
        hbox.addLayout(networks)

        hbox.addWidget(self.__show_trace)

        for check in checks:
            check.setChecked(True)
            self.connect(check, SIGNAL("stateChanged(int)"), self.on_draw)
        
    def on_draw(self):
        self.__axes.clear()

        if self.__measurements_only:
            self.__plotTrace()
            self.__plotMeasurements()
        else:
            self.__setupAxes()
            self.__drawWifi()
            self.__drawIROBs(False)
            self.__drawIROBs(True)
        
        self.__canvas.draw()

    def __plotTrace(self):
        colors = {'wifi': (.7, .7, 1.0), '3G': (1.0, .7, .7)}
        field_group_indices = {'wifi': 1, '3G': 4}
        rtt_field_offset = 2
        start = None
        timestamps = {'wifi': [], '3G': []}
        rtts = {'wifi': [], '3G': []}
        checks = {'wifi': self.__show_wifi, '3G': self.__show_threeg}
        
        if self.__network_trace_file:
            for line in open(self.__network_trace_file).readlines():
                fields = line.strip().split()
                timestamp = float(fields[0])
                if not start:
                    start = timestamp
                rel_timestamp = timestamp - start

                for network_type in self.__estimates:
                    last_estimate = self.__estimates[network_type]['latency'][-1]
                    if rel_timestamp > self.getAdjustedTime(last_estimate['timestamp']):
                        continue
                    
                    field_index = field_group_indices[network_type] + rtt_field_offset
                    rtt = float(fields[field_index]) / 1000.0
                    timestamps[network_type].append(rel_timestamp)
                    rtts[network_type].append(rtt)
                    
            for network_type in timestamps:
                if self.__show_trace.isChecked() and checks[network_type].isChecked():
                    self.__axes.plot(timestamps[network_type], rtts[network_type],
                                     color=colors[network_type],
                                     label=network_type + " trace")

    def __plotMeasurements(self):
        markers = {'wifi': 's', '3G': 'o'}
        checks = {'wifi': self.__show_wifi, '3G': self.__show_threeg}
        for network_type in self.__estimates:
            if not checks[network_type].isChecked():
                continue
            
            # TODO: support toggle between bandwidth & latency
            estimates = self.__estimates[network_type]['latency']
            times = [self.getAdjustedTime(e['timestamp']) for e in estimates]

             # RTT = latency * 2
            observations = [e['observation'] * 2.0 for e in estimates]
            estimated_values = [e['estimate'] * 2.0 for e in estimates]

            # shift estimtates one to the right, so we're plotting
            #  each observation at the same time as the PREVIOUS estimate
            #  (this visualizes the error samples that the decision algorithm uses)
            estimated_values = [estimated_values[0]] + estimated_values[:-1]

            color = self.__irob_colors[network_type]
            self.__axes.plot(times, estimated_values, label=network_type + " prev-estimates",
                             color=color)
            self.__axes.plot(times, observations, label=network_type + " observations",
                             linestyle='none', marker=markers[network_type], markersize=3,
                             color=color)
            self.__axes.set_xlabel("Time (seconds)")
            self.__axes.set_ylabel("RTT (seconds)")
            self.__axes.legend()

    def __setupAxes(self):
        yticks = []
        yticklabels = []
        for network, pos in self.__network_pos_offsets.items():
            for direction, offset in self.__direction_pos_offsets.items():
                label = "%s %s" % (network, direction)
                yticks.append(pos + offset)
                yticklabels.append(label)
        self.__axes.set_yticks(yticks)
        self.__axes.set_yticklabels(yticklabels)

    def __drawIROBs(self, drops):
        for network_type in self.__networks:
            network = self.__networks[network_type]
            for direction in network:
                irobs = network[direction]
                for irob_id in irobs:
                    irob = irobs[irob_id]
                    if drops:
                        irob.drawIfDropped(self.__axes)
                    else:
                        irob.draw(self.__axes)

    def __drawWifi(self):
        if "wifi" not in self.__network_periods:
            # not done parsing yet
            return
        
        bars = [(self.getAdjustedTime(period['start']),
                 period['end'] - period['start'])
                for period in self.__network_periods['wifi']]
        vertical_bounds = self.__axes.get_ylim()
        height = [vertical_bounds[0] - self.__irob_height / 2.0,
                  vertical_bounds[1] - vertical_bounds[0] + self.__irob_height]
        self.__axes.broken_barh(bars, height, color="green", alpha=0.3)
        

    def getIROBPosition(self, irob):
        # TODO: allow for simultaneous (stacked) IROB plotting.
        
        return (self.__network_pos_offsets[irob.network_type] +
                self.__direction_pos_offsets[irob.direction])
        
    def getIROBHeight(self, irob):
        # TODO: adjust based on the number of stacked IROBs.
        return self.__irob_height

    def getIROBColor(self, irob):
        return self.__irob_colors[irob.network_type]

    def getAdjustedTime(self, timestamp):
        return timestamp - self.__start

    def parseLine(self, line):
        if self.__start == None:
            self.__start = self.__getTimestamp(line)

        if "Got update from scout" in line:
            #[time][pid][tid] Got update from scout: 192.168.1.2 is up,
            #                 bandwidth_down 43226 bandwidth_up 12739 bytes/sec RTT 97 ms
            #                 type wifi
            self.__modifyNetwork(line)
        elif "Successfully bound" in line:
            # [time][pid][CSockSender 57] Successfully bound osfd 57 to 192.168.1.2:0
            self.__addConnection(line)
        elif "Adding connection" in line:
            # [time][pid][Listener 13] Adding connection 14 from 192.168.1.2
            #                          bw_down 244696 bw_up 107664 RTT 391
            #                          type wifi(peername 141.212.110.115)
            pass # No accepting-side log analysis yet.
        elif re.search(self.__csocket_destroyed_regex, line) != None:
            # [time][pid][CSockSender 57] CSocket 57 is being destroyed
            self.__removeConnection(line)
        elif "Getting bytes to send from IROB" in line:
            # [time][pid][CSockSender 57] Getting bytes to send from IROB 6
            timestamp = self.__getTimestamp(line)
            irob = int(line.strip().split()[-1])
            network = self.__getNetworkType(line)
            
            self.__currentSendingIROB = irob
            self.__addIROB(timestamp, network, irob, 'up')
        elif "...returning " in line:
            # [time][pid][CSockSender 57] ...returning 1216 bytes, seqno 0
            assert self.__currentSendingIROB != None
            timestamp = self.__getTimestamp(line)
            datalen = int(line.strip().split()[3])
            network = self.__getNetworkType(line)
            self.__addIROBBytes(timestamp, network, self.__currentSendingIROB,
                                datalen, 'up')
        elif "About to send message" in line:
            # [time][pid][CSockSender 57] About to send message:  Type: Begin_IROB(1)
            #                             Send labels: FG,SMALL IROB: 0 numdeps: 0
            self.__addTransfer(line, 'up')
        elif "Received message" in line:
            # [time][pid][CSockReceiver 57] Received message:  Type: Begin_IROB(1)
            #                               Send labels: FG,SMALL IROB: 0 numdeps: 0
            self.__addTransfer(line, 'down')
        elif "network estimator" in line:
            timestamp = self.__getTimestamp(line)
            network_type = re.search(self.__network_estimator_regex, line).group(1)
            if network_type not in self.__estimates:
                self.__estimates[network_type] = {}
                
            print "got observation: ", line
            bw_match = re.search(self.__network_bandwidth_regex, line)
            lat_match = re.search(self.__network_latency_regex, line)
            for match, name in zip((bw_match, lat_match), ("bandwidth", "latency")):
                if match:
                    obs, est = match.groups()
                    all_estimates = self.__estimates[network_type]
                    if name not in all_estimates:
                        all_estimates[name] = []
                    estimates = all_estimates[name]
                    estimates.append({'timestamp': float(timestamp),
                                      'observation': float(obs),
                                      'estimate': float(est)})
        else:
            pass # ignore it

    def __initRegexps(self):
        self.__irob_regex = re.compile("IROB: ([0-9]+)")
        self.__datalen_regex = re.compile("datalen: ([0-9]+)")
        self.__expected_bytes_regex = re.compile("expected_bytes: ([0-9]+)")
        self.__network_regex = re.compile("scout: (.+) is (down|up).+ type ([A-Za-z0-9]+)")
        self.__ip_regex = re.compile("([0-9]+(?:\.[0-9]+){3})")
        self.__socket_regex = re.compile("\[CSock(?:Sender|Receiver) ([0-9]+)\]")
        self.__timestamp_regex = re.compile("^\[([0-9]+\.[0-9]+)\]")
        self.__intnw_message_type_regex = \
            re.compile("(?:About to send|Received) message:  Type: ([A-Za-z_]+)")
        self.__csocket_destroyed_regex = re.compile("CSocket .+ is being destroyed")
        self.__network_estimator_regex = \
            re.compile("Adding new stats to (.+) network estimator")

        float_regex = "([0-9]+\.[0-9]+)"
        stats_regex = "obs %s est %s" % (float_regex, float_regex)
        self.__network_bandwidth_regex = re.compile("bandwidth: " + stats_regex)
        self.__network_latency_regex = re.compile("latency: " + stats_regex)
        
    def __getIROBId(self, line):
        return int(re.search(self.__irob_regex, line).group(1))

    def __getSocket(self, line):
        return int(re.search(self.__socket_regex, line).group(1))

    def __getIP(self, line):
        return re.search(self.__ip_regex, line).group(1)

    def __modifyNetwork(self, line):
        timestamp = self.__getTimestamp(line)
        ip, status, network_type = re.search(self.__network_regex, line).groups()
        if network_type not in self.__networks:
            self.__networks[network_type] = {
                'down': {}, # download IROBs
                'up': {}    # upload IROBs
                }
            self.__network_periods[network_type] = []

        if status == 'down':
            period = self.__network_periods[network_type][-1]
            assert period['end'] == None
            period['end'] = timestamp
            
            assert ip in self.__network_type_by_ip
            del self.__network_type_by_ip[ip]
        elif status == 'up':
            periods = self.__network_periods[network_type]
            if len(periods) > 0 and periods[-1]['end'] == None:
                # two perfectly adjacent periods with no 'down' in between.  whatevs.
                periods[-1]['end'] = timestamp
                
            periods.append({
                'start': timestamp, 'end': None,
                'ip': ip, 'sock': None
                })
            self.__network_type_by_ip[ip] = network_type
        else: assert False
        
    def __addConnection(self, line):
        sock = self.__getSocket(line)
        ip = self.__getIP(line)
        network_type = self.__network_type_by_ip[ip]
        network_period = self.__network_periods[network_type][-1]

        assert network_period['start'] != None
        assert network_period['ip'] == ip
        assert network_period['sock'] == None
        network_period['sock'] = sock

        assert sock not in self.__network_type_by_sock
        self.__network_type_by_sock[sock] = network_type
        
    def __removeConnection(self, line):
        timestamp = self.__getTimestamp(line)
        sock = self.__getSocket(line)
        if sock in self.__network_type_by_sock:
            network_type = self.__network_type_by_sock[sock]
            network_period = self.__network_periods[network_type][-1]
            
            network_period['sock'] = None
            del self.__network_type_by_sock[sock]

            self.__markDroppedIROBs(timestamp, network_type)

    def __getTimestamp(self, line):
        return float(re.search(self.__timestamp_regex, line).group(1))
        
    def __getNetworkType(self, line):
        sock = self.__getSocket(line)
        return self.__network_type_by_sock[sock]

    def __getDatalen(self, line):
        return int(re.search(self.__datalen_regex, line).group(1))

    def __getExpectedBytes(self, line):
        return int(re.search(self.__expected_bytes_regex, line).group(1))

    def __getIntNWMessageType(self, line):
        return re.search(self.__intnw_message_type_regex, line).group(1)

    def __addTransfer(self, line, direction):
        # [time][pid][CSockSender 57] About to send message:  Type: Begin_IROB(1)
        #                             Send labels: FG,SMALL IROB: 0 numdeps: 0
        # [time][pid][CSockReceiver 57] Received message:  Type: IROB_chunk(3)
        #                               Send labels: FG,SMALL IROB: 0
        #                               seqno: 0 offset: 0 datalen: 1024
        # [time][pid][CSockReceiver 57] Received message:  Type: End_IROB(2)
        #                               Send labels: FG,SMALL IROB: 0
        #                               expected_bytes: 1024 expected_chunks: 1
        # [time][pid][CSockReceiver 57] Received message:  Type: Ack(7)
        #                               Send labels:  num_acks: 0 IROB: 0
        #                               srv_time: 0.000997 qdelay: 0.000000
        timestamp = self.__getTimestamp(line)
        network_type = self.__getNetworkType(line)
        intnw_message_type = self.__getIntNWMessageType(line)

        if (intnw_message_type == "Begin_IROB" or
            intnw_message_type == "Data_Check"):
            irob_id = self.__getIROBId(line)
            self.__currentSendingIROB = None
            self.__addIROB(timestamp, network_type, irob_id, direction)
        elif intnw_message_type == "IROB_chunk":
            irob_id = self.__getIROBId(line)
            datalen = self.__getDatalen(line)
            self.__addIROBBytes(timestamp, network_type, irob_id, datalen, direction)
        elif intnw_message_type == "End_IROB" and direction == 'down':
            irob_id = self.__getIROBId(line)
            expected_bytes = self.__getExpectedBytes(line)
            self.__finishReceivedIROB(timestamp, network_type, irob_id, expected_bytes)
        elif intnw_message_type == "Ack" and direction == 'down':
            irob_id = self.__getIROBId(line)
            self.__ackIROB(timestamp, network_type, irob_id, 'up')
        else:
            pass # ignore other types of messages

    def __getIROB(self, network_type, irob_id, direction, start=None):
        if network_type not in self.__networks:
            raise LogParsingError("saw data on unknown network '%s'" % network_type)
        irobs = self.__networks[network_type][direction]
        if irob_id not in irobs:
            if start != None:
                irobs[irob_id] = IROB(self, network_type, direction, start, irob_id)
            else:
                return None
            
        return irobs[irob_id]

    def __getIROBOrThrow(self, network_type, irob_id, direction, start=None):
        irob = self.__getIROB(network_type, irob_id, direction, start)
        if irob == None:
            raise LogParsingError("Unknown IROB %d" % irob_id)
        return irob
    
    def __addIROB(self, timestamp, network_type, irob_id, direction):
        # TODO: deal with the case where the IROB announcement arrives after the data
        irob = self.__getIROBOrThrow(network_type, irob_id, direction, start=timestamp)
        dprint("Adding %s at %f" % (irob, timestamp))


    def __addIROBBytes(self, timestamp, network_type, irob_id, datalen, direction):
        # XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        # XXX: this is double-counting when chunk messages do appear.  fix it.
        # XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        irob = self.__getIROBOrThrow(network_type, irob_id, direction)
        irob.addBytes(timestamp, datalen)

    def __finishReceivedIROB(self, timestamp, network_type, irob_id, expected_bytes):
        irob = self.__getIROBOrThrow(network_type, irob_id, 'down')
        irob.finish(timestamp, expected_bytes)

        # "finish" the IROB by marking it ACK'd.
        # Strictly speaking, a received IROB isn't "finished" until
        #  all expected bytes are received, but until the end_irob messasge
        #  arrives, we don't know how many to expect, so we hold off on
        #  marking it finished until then.
        dprint("Finished %s at %f" % (irob, timestamp))
        irob.ack(timestamp)

    def __markDroppedIROBs(self, timestamp, network_type):
        for direction in ['down', 'up']:
            for irob in self.__networks[network_type][direction].values():
                if not irob.complete():
                    irob.markDropped(timestamp)

    def __ackIROB(self, timestamp, network_type, irob_id, direction):
        irob = self.__getIROBOrThrow(network_type, irob_id, direction)
        irob.ack(timestamp)
        dprint("Acked %s at %f" % (irob, timestamp))

    
class IntNWPlotter(object):
    def __init__(self, filename, measurements_only, network_trace_file):
        self.__windows = []
        self.__currentPid = None
        self.__pid_regex = re.compile("^\[[0-9]+\.[0-9]+\]\[([0-9]+)\]")

        self.__measurements_only = measurements_only
        self.__network_trace_file = network_trace_file
        self.__readFile(filename)
        self.draw()

    def draw(self):
        for window in self.__windows:
            window.on_draw()
        
    def __getPid(self, line):
        match = re.search(self.__pid_regex, line)
        if match:
            return int(match.group(1))

        return None

    def __readFile(self, filename):
        print "Parsing log file..."
        progress = ProgressBar()
        for linenum, line in enumerate(progress(open(filename).readlines())):
            try:
                pid = self.__getPid(line)
                if pid == None:
                    continue
                    
                if pid != self.__currentPid:
                    self.__windows.append(IntNWBehaviorPlot(self.__measurements_only,
                                                            self.__network_trace_file))
                    self.__currentPid = pid
                    
                self.__windows[-1].parseLine(line)
            except LogParsingError as e:
                trace = sys.exc_info()[2]
                e.setLine(linenum + 1, line)
                raise e, None, trace
            except Exception as e:
                trace = sys.exc_info()[2]
                e = LogParsingError(repr(e) + ": " + str(e))
                e.setLine(linenum + 1, line)
                raise e, None, trace

    def show(self):
        for window in self.__windows:
            window.show()
    
def main():
    parser = ArgumentParser()
    parser.add_argument("filename")
    parser.add_argument("--measurements", action="store_true", default=False)
    parser.add_argument("--network-trace-file", default=None)
    args = parser.parse_args()

    app = QApplication(sys.argv)
    
    plotter = IntNWPlotter(args.filename, args.measurements, args.network_trace_file)
    plotter.show()
    app.exec_()

if __name__ == '__main__':
    main()
