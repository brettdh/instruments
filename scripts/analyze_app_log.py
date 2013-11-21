#!/usr/bin/env python

'''
Generic functionality for plotting app behavior.

# Borrows heavily from
#   http://eli.thegreenplace.net/2009/01/20/matplotlib-with-pyqt-guis/

'''

import sys, re, os, datetime, time
from argparse import ArgumentParser

from PyQt4.QtCore import *
from PyQt4.QtGui import *

import matplotlib
from matplotlib.backends.backend_qt4agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt4agg import NavigationToolbar2QTAgg as NavigationToolbar
from matplotlib.figure import Figure

import numpy as np
import scipy.stats as stats

from itertools import product
from bisect import bisect_right
from copy import copy

from progressbar import ProgressBar

sys.path.append(os.getenv("HOME") + "/scripts/nistnet_scripts/traces")
import mobility_trace
from mobility_trace import NetworkChooser

debug = False
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
        return ("App log parse error at line %s: %s%s"
                % (self.linenum and str(self.linenum) or "<unknown>",
                   self.msg,
                   self.line != None and ('\n' + self.line) or ""))

class IROBError(Exception):
    pass

def stepwise_mean(data):
    means = []
    mean = 0.0
    n = 0
    for value in data:
        n += 1
        mean = update_running_mean(mean, value, n)
        means.append(mean)
        
    return means

def stepwise_variance(data):
    '''Returns a list with the step-by-step variance computed on data.
    Algorithm borrowed from
    http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm
    which cites Knuth's /The Art of Computer Programming/, Volume 1.

    '''
    n = 0
    mean = 0.0
    M2 = 0.0
    variances = [0.0]
    
    for x in data:
        n += 1
        delta = x - mean
        mean += delta / n
        M2 += delta * (x - mean)
        
        if n > 1:
            variances.append(M2 / (n - 1))

    assert len(variances) == len(data)
    return variances

def update_running_mean(mean, value, n):
    return mean + ((value - mean) / (n+1))

def alpha_to_percent(alpha):
    return 100.0 * (1 - alpha)

def percent_to_alpha(percent):
    return 1.0 - (percent / 100.0)
    
# http://code.activestate.com/recipes/577219-minimalistic-memoization/
def memoize(f):
    cache = {}
    def memf(*args):
        if args not in cache:
            cache[args] = f(*args)
        return cache[args]
    return memf

@memoize
def get_t_value(alpha, df):
    'for two-sided confidence interval.'
    return stats.t.ppf(1 - alpha/2.0, df)

def confidence_interval(alpha, stddev, n):
    t = get_t_value(alpha, n-1)
    return t * stddev / (n ** 0.5)


def shift_right_by_one(l):
    if len(l) > 0:
        l = [l[0]] + l[:-1]
    return l

def init_dict(my_dict, key, init_value):
    if key not in my_dict:
        my_dict[key] = init_value


RELATIVE_ERROR = True

def absolute_error(prev_estimate, cur_observation):
    return prev_estimate - cur_observation

def absolute_error_adjusted_estimate(estimate, error):
    return estimate - error

def relative_error(prev_estimate, cur_observation):
    return cur_observation / prev_estimate

def relative_error_adjusted_estimate(estimate, error):
    return estimate * error

def error_value(prev_estimate, cur_observation):
    err = relative_error if RELATIVE_ERROR else absolute_error
    return err(prev_estimate, cur_observation)

def error_adjusted_estimate(estimate, error):
    adjuster = (relative_error_adjusted_estimate if RELATIVE_ERROR
                else absolute_error_adjusted_estimate)
    return adjuster(estimate, error)

def get_error_values(observations, estimated_values):
    shifted_estimates = shift_right_by_one(estimated_values)
    return [error_value(est, obs) for obs, est in zip(observations, shifted_estimates)]

def get_value_at(timestamps, values, time):
    '''Assuming zip(timestamps, values) is a list of periods
    described (somehow) by the corresponding values,
    return the value in effect at time 'time'.
    A value is 'in effect' from timestamps[i] until timestamps[i+1].

    Assumes timestamps is a sorted list.

    '''
    pos = bisect_right(timestamps, time)
    assert pos != 0 # timestamp must occur after first observation
    return values[pos - 1]

def network_metric_pairs():
    network_types = ['wifi', '3G']
    metrics = ['bandwidth_up', 'latency']
    for n, m in product(network_types, metrics):
        yield n, m


class IROB(object):
    def __init__(self, plot, network_type, direction, start, irob_id):
        self._plot = plot
        self.network_type = network_type
        self.direction = direction
        self.irob_id = irob_id
        
        self._start = start
        self._datalen = None
        self._expected_bytes = None
        self._completion_time = None
        self._drop_time = None
        self._last_activity = None
        self._acked = False

        # if true, mark as strange on plot
        self._abnormal_end = False

        self._checkIfComplete(start)

    def addBytes(self, timestamp, bytes):
        if self._datalen == None:
            self._datalen = 0
        self._datalen += bytes
        self._checkIfComplete(timestamp)

    def finish(self, timestamp, expected_bytes):
        self._expected_bytes = expected_bytes
        self._checkIfComplete(timestamp)

    def ack(self, timestamp):
        self._acked = True
        self._checkIfComplete(timestamp)

    def complete(self):
        # XXX: HACK.  Can be double-counting sent/received app data.
        # TODO: fix it.  maybe still keep track of duplicate data.
        return (self._acked and
                (self._datalen != None and
                 (self.direction == "up" or
                  self._datalen >= self._expected_bytes)))

    def getSize(self):
        if not self._datalen:
            raise IROBError("expected to find IROB size")
        return self._datalen
     
    def _checkIfComplete(self, timestamp):
        self._last_activity = timestamp
        if self.complete():
            self._completion_time = timestamp

    def markDropped(self, timestamp):
        if self._drop_time == None:
            dprint("Dropped %s at %f" % (self, timestamp))
            self._drop_time = timestamp

    def wasDropped(self):
        return bool(self._drop_time)

    def getStart(self):
        return self._start

    def getTimeInterval(self):
        if self.complete():
            return (self._start, self._completion_time)
        else:
            if self._acked:
                end = self._last_activity
            else:
                if self._drop_time != None:
                    end = self._drop_time
                else:
                    end = self._last_activity
                    self._abnormal_end = True
                    
            return (self._start, end)

    def getDuration(self):
        interval = self.getTimeInterval()
        return interval[1] - interval[0]

    def __str__(self):
        return ("{IROB: id %d  direction: %s  network: %s}"
                % (self.irob_id, self.direction, self.network_type))

    def draw(self, axes):
        ypos = self._plot.getIROBPosition(self)
        yheight = self._plot.getIROBHeight(self)
        start, finish = [self._plot.getAdjustedTime(ts) for ts in self.getTimeInterval()]
        dprint("%s %f--%f" % (self, start, finish))
        axes.broken_barh([[start, finish-start]],
                         [ypos - yheight / 2.0, yheight],
                         color=self._plot.getIROBColor(self))

        if self._drop_time != None:
            axes.plot([finish], [ypos], marker='x', color='black', markeredgewidth=2.0)
        elif self._abnormal_end:
            axes.plot([finish], [ypos], marker='*', color='black')

timestamp_regex = re.compile("^\[([0-9]+\.[0-9]+)\]")
choose_network_time_regex = re.compile("  took ([0-9.]+) seconds")

def getTimestamp(line):
    return float(re.search(timestamp_regex, line).group(1))
            

class IntNWBehaviorPlot(QDialog):
    CONFIDENCE_ALPHA = 0.10
    
    def __init__(self, run, start, measurements_only, network_trace_file,
                 bandwidth_measurements_file, cross_country_latency, error_history, 
                 is_server, appFeatureDrawers, appMeasurementDrawers, parent=None):
        QDialog.__init__(self, parent)

        self._appFeatureDrawers = appFeatureDrawers
        self._appMeasurementDrawers = appMeasurementDrawers

        self._initRegexps()
        self._run = run
        self._networks = {} # {network_type => {direction => {id => IROB} } }
        self._network_periods = {}
        self._network_type_by_ip = {}
        self._network_type_by_sock = {}
        self._placeholder_sockets = {} # for when connection begins before scout msg
        self._error_history = error_history
        self._is_server = is_server

        self._measurements_only = measurements_only
        self._network_trace_file = network_trace_file
        self._bandwidth_measurements_file = bandwidth_measurements_file
        self._cross_country_latency = cross_country_latency
        self._trace = None

        # estimates[network_type][bandwidth_up|latency] -> [values]
        self._estimates = {'wifi': {'bandwidth_up': [], 'latency': []},
                            '3G': {'bandwidth_up': [], 'latency': []}}

        # second axes to plot times on
        self._session_axes = None
        self._user_set_max_trace_duration = None
        self._user_set_max_time = None

        self._alpha = IntNWBehaviorPlot.CONFIDENCE_ALPHA

        # app-level sessions from the trace_replayer.log file
        self._sessions = []
        self._debug_sessions = [] # sessions to highlight on the plot for debugging

        # redundancy decision calculations from instruments.log
        self._redundancy_decisions = []

        self._radio_switches = []

        self._irob_height = 0.25
        self._network_pos_offsets = {'wifi': 1.0, '3G': -1.0}
        self._direction_pos_offsets = {'down': self._irob_height / 2.0,
                                        'up': -self._irob_height / 2.0}

        self._irob_colors = {'wifi': 'blue', '3G': 'red'}

        self._choose_network_calls = []

        self._start = start
        self._xlim_max = 1200.0

        # TODO: infer plot title from file path
        client_or_server = "server-side" if self._is_server else "client-side"
        self._title = ("IntNW %s - Run %d" % 
                        (client_or_server, self._run))

        self.create_main_frame()

    def setXlimMax(self, xlim_max):
        self._xlim_max = xlim_max

    def setSessions(self, sessions):
        self._sessions = sessions

    def setRedundancyDecisions(self, redundancy_decisions):
        self._redundancy_decisions = redundancy_decisions
        
    def setRadioSwitches(self, radio_switches):
        self._radio_switches = []
        for switch in radio_switches:
            timestamp, prev_type, new_type = switch
            rel_timestamp = self.getAdjustedTime(timestamp)
            if timestamp >= self._start:
                self._radio_switches.append([rel_timestamp, prev_type, new_type])
        
    def create_main_frame(self):
        self._frame = QWidget()
        self._dpi = 100
        self._figure = Figure((5,4), self._dpi) # 5" x 4"
        self._canvas = FigureCanvas(self._figure)
        self._canvas.setParent(self._frame)

        self._mpl_toolbar = NavigationToolbar(self._canvas, self._frame)

        #
        # Layout with box sizers
        # 
        hbox = QHBoxLayout()

        if self._measurements_only:
            self._setupMeasurementWidgets(hbox)
        else:
            self._setupActivityWidgets(hbox)
            
        vbox = QVBoxLayout(self)
        vbox.addWidget(self._canvas, stretch=100)
        vbox.addWidget(self._mpl_toolbar, stretch=1)
        vbox.addLayout(hbox, stretch=1)

    def _setupActivityWidgets(self, hbox):
        self._show_sessions = QCheckBox("Session times")
        self._show_decisions = QCheckBox("Redundancy decisions")
        self._show_debugging = QCheckBox("Debugging")
        checks = [self._show_sessions, self._show_decisions,
                  self._show_debugging]

        left_box = QVBoxLayout()
        for check in checks:
            if check is not self._show_debugging:
                check.setChecked(True)
            left_box.addWidget(check)
            self.connect(check, SIGNAL("stateChanged(int)"), self.on_draw)

        hbox.addLayout(left_box, stretch=1)

        self._max_trace_duration = QLineEdit("")
        self.connect(self._max_trace_duration, SIGNAL("returnPressed()"), 
                     self.updateMaxTraceDuration)

        labeled_input = QVBoxLayout()
        labeled_input.addWidget(QLabel("Max trace duration"))
        labeled_input.addWidget(self._max_trace_duration)
        hbox.addLayout(labeled_input, stretch=1)

        self._max_time = QLineEdit("")
        self.connect(self._max_time, SIGNAL("returnPressed()"), self.updateMaxTime)

        labeled_input = QVBoxLayout()
        labeled_input.addWidget(QLabel("Max time"))
        labeled_input.addWidget(self._max_time)
        hbox.addLayout(labeled_input, stretch=1)


    def _updateUserSetField(self, field, attrname):
        try:
            value = float(field.text())
            print "Setting %s to %f" % (attrname, value)
            setattr(self, attrname, value)
            self.on_draw()
        except ValueError:
            pass

    def updateMaxTraceDuration(self):
        self._updateUserSetField(self._max_trace_duration, "_user_set_max_trace_duration")

    def updateMaxTime(self):
        self._updateUserSetField(self._max_time, "_user_set_max_time")

    def updateAlpha(self):
        try:
            alpha = percent_to_alpha(float(self._ci_percent.text()))
            self._alpha = alpha
            self.on_draw()
        except ValueError:
            pass

    def _setupMeasurementWidgets(self, hbox):
        self._show_wifi = QCheckBox("wifi")
        self._show_threeg = QCheckBox("3G")
        self._show_measurements = QCheckBox("Measurements")
        self._show_trace = QCheckBox("Trace display")
        self._show_trace_variance = QCheckBox("Trace variance (std-dev)")
        self._show_legend = QCheckBox("Legend")
        self._show_decisions = QCheckBox("Redundancy decisions")
        checks = [self._show_wifi, self._show_threeg,
                  self._show_measurements,
                  self._show_trace, self._show_trace_variance,
                  self._show_legend, self._show_decisions]

        networks = QVBoxLayout()
        networks.addWidget(self._show_wifi)
        networks.addWidget(self._show_threeg)
        hbox.addLayout(networks)

        options = QVBoxLayout()
        options.addWidget(self._show_measurements)
        options.addWidget(self._show_trace)
        options.addWidget(self._show_trace_variance)
        options.addWidget(self._show_legend)
        options.addWidget(self._show_decisions)
        hbox.addLayout(options)

        for check in checks:
            check.setChecked(True)
            self.connect(check, SIGNAL("stateChanged(int)"), self.on_draw)

        error_plot_method_label = QLabel("Error plotting")
        self._plot_error_bars = QRadioButton("Mean error bars")
        self._plot_colored_error_regions = QRadioButton("Colored regions")
        
        self._error_error_ci = \
            QRadioButton("%d%% CI" % alpha_to_percent(IntNWBehaviorPlot.CONFIDENCE_ALPHA))
        self._error_error_stddev = QRadioButton("std-dev")

        self._plot_colored_error_regions.setChecked(True)
        self._error_error_ci.setChecked(True)
        
        self.connect(self._plot_error_bars, SIGNAL("toggled(bool)"), self.on_draw)
        self.connect(self._plot_colored_error_regions, SIGNAL("toggled(bool)"), self.on_draw)
        self.connect(self._error_error_ci, SIGNAL("toggled(bool)"), self.on_draw)
        self.connect(self._error_error_stddev, SIGNAL("toggled(bool)"), self.on_draw)

        percent = alpha_to_percent(IntNWBehaviorPlot.CONFIDENCE_ALPHA)
        self._ci_percent = QLineEdit("%d" % percent)
        self._ci_percent.setFixedWidth(80)
        self.connect(self._ci_percent, SIGNAL("returnPressed()"), self.updateAlpha)

        error_toggles = QVBoxLayout()
        error_toggles.addWidget(self._plot_error_bars)
        error_toggles.addWidget(self._plot_colored_error_regions)

        error_box = QGroupBox()
        error_box.setLayout(error_toggles)

        error_interval_toggles = QVBoxLayout()
        ci_toggle = QHBoxLayout()
        ci_toggle.addWidget(self._error_error_ci)
        ci_toggle.addWidget(self._ci_percent)
        ci_toggle.addWidget(QLabel("%"))
        error_interval_toggles.addLayout(ci_toggle)
        error_interval_toggles.addWidget(self._error_error_stddev)

        self._error_interval_box = QGroupBox()
        self._error_interval_box.setLayout(error_interval_toggles)

        all_error_toggles = QHBoxLayout()
        all_error_toggles.addWidget(error_box)
        all_error_toggles.addWidget(self._error_interval_box)

        hbox.addLayout(all_error_toggles)

        self._bandwidth_up_toggle = QRadioButton("Bandwidth (up)")
        self._latency_toggle = QRadioButton("Latency")
        self._tx_time_toggle = QRadioButton("Predicted transfer time (up)")
        
        self._latency_toggle.setChecked(True)
        self.connect(self._bandwidth_up_toggle, SIGNAL("toggled(bool)"), self.on_draw)
        self.connect(self._latency_toggle, SIGNAL("toggled(bool)"), self.on_draw)
        self.connect(self._tx_time_toggle, SIGNAL("toggled(bool)"), self.on_draw)

        toggles = QVBoxLayout()
        toggles.addWidget(self._bandwidth_up_toggle)
        toggles.addWidget(self._latency_toggle)
        toggles.addWidget(self._tx_time_toggle)

        toggle_box = QGroupBox()
        toggle_box.setLayout(toggles)
        hbox.addWidget(toggle_box)
        
    def on_draw(self):
        self.setWindowTitle(self._title)

        self._figure.clear()
        self._axes = self._figure.add_subplot(111)
        self._axes.set_title(self._title)
        self._session_axes = None
        
        if self._measurements_only:
            self._plotTrace()
            if self._bandwidth_measurements_file:
                self._plotActiveMeasurements()
            else:
                self._plotMeasurements()

            self._drawAppMeasurements()

            self._axes.set_xlabel("Time (seconds)")
            self._axes.set_ylabel(self._getYAxisLabel())
            if self._show_legend.isChecked():
                self._axes.legend()

            self._drawRedundancyDecisions()

            self._axes.set_ylim(0.0, self._axes.get_ylim()[1])
            self._drawWifi()
        else:
            self._setupAxes()
            self._setTraceEnd()
            self._drawIROBs()
            self._drawSessions()
            self._drawRedundancyDecisions()
            self._drawRadioSwitches()
            self._drawAppFeatures()

            max_time = self._session_axes.get_ylim()[1]
            if self._user_set_max_time:
                max_time = self._user_set_max_time
            self._session_axes.set_ylim(0.0, max_time)

            if self._show_debugging.isChecked():
                self._drawDebugging()

            self._drawWifi()

        #max_trace_duration = self._session_axes.get_xlim()[1]
        if self._user_set_max_trace_duration:
            max_trace_duration = self._user_set_max_trace_duration
        else:
            max_trace_duration = self._xlim_max

        buffer_perc = 0.05
        buffer = buffer_perc * max_trace_duration
        xlims = [-buffer, max_trace_duration + buffer]
        self._axes.set_xlim(*xlims)
        if not self._measurements_only:
            self._session_axes.set_xlim(*xlims)

        self._canvas.draw()

    def _drawAppFeatures(self):
        for drawer in self._appFeatureDrawers:
            drawer(self._run-1, self, self._axes)

    def _drawAppMeasurements(self):
        for drawer in self._appMeasurementDrawers:
            drawer(self._run-1, self, self._axes)

    def saveErrorTable(self):
        for network_type, metric in network_metric_pairs():
            filename = ("/tmp/%s_%s_%s_error_table_%d.txt"
                        % ("server" if self._is_server else "client",
                           network_type, metric, self._run))
            f = open(filename, "w")
            f.write("Time,Observation,Prev estimate,New estimate,Error\n")
            
            cur_times, observations, estimated_values = \
                self._getAllEstimates(network_type, metric)
            shifted_estimates = shift_right_by_one(estimated_values)
            error_values = get_error_values(observations, estimated_values)

            for values in zip(cur_times, observations, 
                              shifted_estimates, estimated_values, error_values):
                f.write(",".join(str(v) for v in values) + "\n")
                
            f.close()
            

    def _whatToPlot(self):
        if self._bandwidth_up_toggle.isChecked():
            return 'bandwidth_up'
        elif self._latency_toggle.isChecked():
            return 'latency'
        elif self._tx_time_toggle.isChecked():
            return 'tx_time'
        else: assert False

    def _getYAxisLabel(self):
        labels = {'bandwidth_up': 'Bandwidth (up) (bytes/sec)',
                  'latency': 'RTT (seconds)',
                  'tx_time': 'Transfer time (up) (seconds)'}
        return labels[self._whatToPlot()]

    class NetworkTrace(object):
        def __init__(self, trace_file, window, estimates, is_server):
            self._window = window
            self._priv_trace = mobility_trace.NetworkTrace(trace_file)
            
            value_names = ['bandwidth_up', 'latency']
            
            field_group_indices = {'wifi': 1, '3G': 4}
        
            # XXX: assuming bandwidth-up.  not the case on the server side.
            field_offsets = {'bandwidth_up': 1, 'latency': 2}

            self._start = None

            self._timestamps = {'wifi': {'bandwidth_up': [], 'latency': []},
                                 '3G': {'bandwidth_up': [], 'latency': []},}
            self._values = {'wifi': {'bandwidth_up': [], 'latency': []},
                             '3G': {'bandwidth_up': [], 'latency': []},}

            conversions = {'bandwidth_up': 1.0, 'latency': 1.0 / 1000.0}

            def getTraceKey(net_type, value_type):
                net_type = net_type.replace("3G", "cellular")
                value_type = value_type.replace("bandwidth_", "").replace("latency", "RTT")
                if is_server:
                    # "upstream" bandwidth is server->client, so it's the 'downstream'
                    #  bandwidth as recorded in the trace
                    value_type = value_type.replace("up", "down")
                return ("%s_%s" % (net_type, value_type))

            last_estimates = {}
            for network_type, what_to_plot in network_metric_pairs():
                cur_estimates = estimates[network_type][what_to_plot]
                if len(cur_estimates) > 0:
                    last_estimates[network_type] = cur_estimates[-1]['timestamp']

            for network_type, what_to_plot in network_metric_pairs():
                key = getTraceKey(network_type, what_to_plot)
                if False: #network_type in last_estimates:
                    last_estimate_time = \
                        window.getAdjustedTime(last_estimates[network_type])
                else:
                    last_estimate_time = 1200.0 # XXX: hardcoding hack.

                times = self._priv_trace.getData('start', 0, last_estimate_time)
                values = self._priv_trace.getData(key, 0, last_estimate_time)

                def convert(value):
                    value = conversions[what_to_plot] * value
                    if what_to_plot == "latency":
                        value = self._window.getAdjustedTraceLatency(value)
                    return value
                
                self._start = times[0]
                self._timestamps[network_type][what_to_plot] = \
                    map(lambda x: x-self._start, times)
                self._values[network_type][what_to_plot] = map(convert, values)

            self._computeVariance()

            
        def _computeVariance(self):
            # values dictionary is populated in __init__
            self._upper_variance_values = {}
            self._lower_variance_values = {}
            self._variance_values = {}
            
            for network_type, what_to_plot in network_metric_pairs():
                values = self._values[network_type][what_to_plot]
                variances = stepwise_variance(values)
                std_devs = [v ** .5 for v in variances]

                uppers = [v + stddev for v, stddev in zip(values, std_devs)]
                lowers = [v - stddev for v, stddev in zip(values, std_devs)]

                if network_type not in self._upper_variance_values:
                    self._upper_variance_values[network_type] = {}
                    self._lower_variance_values[network_type] = {}
                    self._variance_values[network_type] = {}
                self._upper_variance_values[network_type][what_to_plot] = uppers
                self._lower_variance_values[network_type][what_to_plot] = lowers
                self._variance_values[network_type][what_to_plot] = std_devs

        def addTransfers(self, transfers):
            '''Add transfers to the trace for the purpose of
            plotting their transfer times.

            transfers -- [(start_time, size),...]   (upstream transmissions only).
            '''
            
            self._transfers = transfers

            class SingleNetwork(NetworkChooser):
                def __init__(self, network_type):
                    types = {'wifi': NetworkChooser.WIFI, '3G': NetworkChooser.CELLULAR}
                    self.network_type = types[network_type]
                    
                def chooseNetwork(self, duration, bytelen):
                    return self.network_type

            network_choosers = dict([(type_name, SingleNetwork(type_name)) 
                                     for type_name in self._values.keys()])

            for network_type in self._values:
                network_chooser = network_choosers[network_type]
                tx_results = [self._priv_trace.timeToUpload(network_chooser, *txfer)
                              for txfer in self._transfers]

                def get_time(tx_result):
                    if (tx_result.getNetworkUsed() == network_chooser.network_type and
                        (tx_result.getNetworkUsed() == NetworkChooser.CELLULAR or
                         tx_result.cellular_recovery_start is None)):
                        return tx_result.tx_time
                    else:
                        return 0.0
                    
                times = map(get_time, tx_results)
                self._timestamps[network_type]['tx_time'] = [t[0] for t in self._transfers]
                self._values[network_type]['tx_time'] = times

            # redo these calculations.  XXX: is this stdev calculation
            #  still appropriate?  probably not.
            self._computeVariance()

        plot_colors = {'wifi': (.7, .7, 1.0), '3G': (1.0, .7, .7)}

        # whiten up the colors for the variance plotting
        variance_colors = dict([(name, color + (0.25,)) for name, color in plot_colors.items()])

        def _ploteach(self, plotter, checks):
            for network_type in self._timestamps:
                if (checks[network_type].isChecked()):
                    plotter(network_type)
                
        def _plot(self, axes, what_to_plot, checks, values, colors, labeler):
            def plotter(network_type):
                axes.plot(self._timestamps[network_type][what_to_plot], 
                          self._values[network_type][what_to_plot],
                          color=colors[network_type],
                          label=labeler(network_type))
            
            self._ploteach(plotter, checks)

        def plot(self, axes, what_to_plot, checks):
            self._plot(axes, what_to_plot, checks, self._values,
                        type(self).plot_colors,
                        lambda network_type: network_type + " trace")

        def plotVariance(self, axes, what_to_plot, checks):
            def plotter(network_type):
                if (what_to_plot in self._values[network_type] and
                    what_to_plot in self._variance_values[network_type]):
                    cur_values = self._values[network_type][what_to_plot]
                    cur_errors = self._variance_values[network_type][what_to_plot]
                    
                    axes.errorbar(self._timestamps[network_type][what_to_plot],
                                  cur_values, yerr=cur_errors,
                                  color=type(self).variance_colors[network_type])
            
            self._ploteach(plotter, checks)


    def _plotTrace(self):
        if self._network_trace_file and not self._trace:
            self._trace = IntNWBehaviorPlot.NetworkTrace(self._network_trace_file,
                                                          self, self._estimates, 
                                                          self._is_server)
            transfers = self._getAllUploads()
            self._trace.addTransfers(transfers)
            
        checks = {'wifi': self._show_wifi, '3G': self._show_threeg}
        if self._show_trace.isChecked():
            self._trace.plot(self._axes, self._whatToPlot(), checks)
            if self._show_trace_variance.isChecked():
                self._trace.plotVariance(self._axes, self._whatToPlot(), checks)

    def _plotMeasurements(self):
        what_to_plot = self._whatToPlot()
        if what_to_plot == "tx_time":
            self._plotEstimatedTransferTimes()
        else:
            self._plotMeasurementsSimple()
            

    class ActiveMeasurements(object):
        def __init__(self, infile):
            rx = re.compile("^\[([0-9]+\.[0-9]+)\] total .+ new .+ bandwidth ([0-9.]+) bytes/sec")
            self._start = None
            self._times = []
            self._values = []
            for line in open(infile).readlines():
                m = rx.search(line)
                if m:
                    ts, bandwidth = [float(v) for v in m.groups()]
                    if self._start is None:
                        self._start = ts
                    self._times.append(ts - self._start)
                    self._values.append(bandwidth)

        def plot(self, axes):
            axes.plot(self._times, self._values, label="active bandwidth measurements",
                      linestyle="-", linewidth=0.5)
        
    def _plotActiveMeasurements(self):
        measurements = \
            IntNWBehaviorPlot.ActiveMeasurements(self._bandwidth_measurements_file)
        measurements.plot(self._axes)

    def _plotEstimatedTransferTimes(self):
        checks = {'wifi': self._show_wifi, '3G': self._show_threeg}

        rel_times = {}
        network_errors = self._error_history.getAllErrors()

        for network_type, metric in network_metric_pairs():
            init_dict(rel_times, network_type, {})
            init_dict(network_errors, network_type, {})
            init_dict(network_errors[network_type], metric, [])

            cur_times, observations, estimated_values = \
                self._getAllEstimates(network_type, metric)
            rel_times[network_type][metric] = cur_times
            network_errors[network_type][metric].extend(
                get_error_values(observations, estimated_values))

        def get_errors(network_type, tx_start):
            errors = {}
            for metric in rel_times[network_type]:
                cur_times = rel_times[network_type][metric]
                cur_errors = network_errors[network_type][metric]
                
                pos = bisect_right(cur_times, tx_start)
                offset = len(cur_errors) - len(cur_times)
                assert offset >= 0
                errors[metric] = cur_errors[:offset+pos]

            return errors

        def get_estimates(network_type, tx_start):
            estimates = {}
            for metric in rel_times[network_type]:
                cur_times = rel_times[network_type][metric]
                cur_estimates = self._getAllEstimates(network_type, metric)[2]
                
                pos = bisect_right(cur_times, tx_start)
                assert pos > 0
                estimates[metric] = cur_estimates[pos - 1]
            
            return estimates
            
        def transfer_time(bw, latency, size):
            return (size / bw) + latency

        transfers = self._getAllUploads()
        
        predicted_transfer_durations = {'wifi': [], '3G': []}
        transfer_error_bounds = {'wifi': ([],[]), '3G': ([],[])}
        transfer_error_means = {'wifi': [], '3G': []}
        for network_type in ['wifi', '3G']:
            if not checks[network_type].isChecked():
                continue

            for tx_start, tx_size in transfers:
                cur_estimates = get_estimates(network_type, tx_start)
                est_tx_time = transfer_time(cur_estimates['bandwidth_up'],
                                            cur_estimates['latency'], tx_size)
                predicted_transfer_durations[network_type].append(est_tx_time)

                cur_errors = get_errors(network_type, tx_start)
                transfer_times_with_error = []
                for bandwidth_err, latency_err in product(cur_errors['bandwidth_up'], 
                                                          cur_errors['latency']):
                    bandwidth = error_adjusted_estimate(cur_estimates['bandwidth_up'],
                                                        bandwidth_err)
                    latency = error_adjusted_estimate(cur_estimates['latency'], 
                                                      latency_err)

                    if not RELATIVE_ERROR:
                        bandwidth = max(1.0, bandwidth)
                        latency = max(0.0, latency)
                    
                    tx_time = transfer_time(bandwidth, latency, tx_size)
                    if tx_time < 0.0 or tx_time > 500:
                        debug_trace()
                    transfer_times_with_error.append(tx_time)
                
                transfer_time_errors = [error_value(est_tx_time, tx_time) for tx_time in 
                                        transfer_times_with_error]
                transfer_time_error_means = stepwise_mean(transfer_time_errors)
                error_calculator = self._getErrorCalculator()
                lower_errors, upper_errors = \
                    error_calculator([est_tx_time] * len(transfer_time_errors),
                                     transfer_time_errors, transfer_time_error_means)

                # use the last error as the cumulative error for transfer time
                transfer_error_bounds[network_type][0].append(lower_errors[-1])
                transfer_error_bounds[network_type][1].append(upper_errors[-1])
                transfer_error_means[network_type].append(transfer_time_error_means[-1])
        
            plotter = self._getErrorPlotter()
            times = [tx_start for tx_start, tx_size in transfers]
            estimates_array = np.array(predicted_transfer_durations[network_type])
            error_adjusted_estimates = error_adjusted_estimate(
                estimates_array, np.array(transfer_error_means[network_type]))
            plotter(times, predicted_transfer_durations[network_type], 
                    error_adjusted_estimates,
                    transfer_error_bounds[network_type], network_type)
            
            self._plotEstimates(times, predicted_transfer_durations[network_type], network_type)
            

    def _getAllUploads(self):
        all_irobs = {}
        
        def get_transfer(irob):
            return (self.getAdjustedTime(irob.getStart()), irob.getSize())
                
        for network_type in self._networks:
            irobs = self._networks[network_type]['up']
            for irob_id in irobs:
                new_irob = irobs[irob_id]
                if irob_id in all_irobs:
                    old_irob = all_irobs[irob_id]
                    all_irobs[irob_id] = min(old_irob, new_irob, 
                                             key=lambda x: x.getStart())
                else:
                    all_irobs[irob_id] = new_irob
                        
        irob_list = sorted(all_irobs.values(), key=lambda x: x.getStart())
        def within_bounds(irob):
            start = self.getAdjustedTime(irob.getStart())
            return start > 0.0
        transfers = [get_transfer(irob) for irob in irob_list if within_bounds(irob)]
        return transfers

    def _getAllEstimates(self, network_type, what_to_plot):
        estimates = self._estimates[network_type][what_to_plot]
        times = [self.getAdjustedTime(e['timestamp']) for e in estimates]
        
        txform = 1.0
        if what_to_plot == 'latency':
            # RTT = latency * 2
            txform = 2.0
            
        observations = [e['observation'] * txform for e in estimates]
        estimated_values = [e['estimate'] * txform for e in estimates]

        return times, observations, estimated_values

    def _plotMeasurementsSimple(self):
        checks = {'wifi': self._show_wifi, '3G': self._show_threeg}
        what_to_plot = self._whatToPlot()
        
        for network_type in self._estimates:
            if not checks[network_type].isChecked():
                continue

            times, observations, estimated_values = \
                self._getAllEstimates(network_type, what_to_plot)

            if self._show_measurements.isChecked() and len(observations) > 0:
                error_history = \
                    self._error_history.getErrors(network_type, what_to_plot)
                error_values = error_history + get_error_values(observations, estimated_values)
                
                error_means = stepwise_mean(error_values)

                error_calculator = self._getErrorCalculator()
                plotter = self._getErrorPlotter()

                error_bounds = error_calculator(estimated_values, error_values, error_means)
                estimates_array = np.array(estimated_values)
                error_means_array = np.array(error_means[-len(estimated_values):]) # tail of array
                error_adjusted_estimates = error_adjusted_estimate(
                    estimates_array, error_means_array)

                plotter(times, estimated_values, error_adjusted_estimates, 
                        error_bounds, network_type)
                
                self._plotEstimates(times, estimated_values, network_type)
                self._plotObservations(times, observations, network_type)

    def _getErrorCalculator(self):
        '''Return a function that calculates upper and lower error bounds
        based on three arrays: estimates, error_values, and error_means.
        
        estimates: value of estimator at different points in time
        error_values: samples of the estimator error, as measured by comparing
                      a new measured value to the previous estimate
                      (treating it as a prediction)
        error_means: the stepwise mean of the error values

        error_values and error_means must have the same length.
        if (M = len(estimates)) < (N = len(error_values)), the first (N-M) elements 
        of estimates are treated as *history* - not plotted, but used in the
        error interval calculations.'''
        if self._error_error_ci.isChecked():
            return self._getConfidenceInterval
        elif self._error_error_stddev.isChecked():
            return self._getErrorStddev
        else:
            assert False

    def _getErrorPlotter(self):
        if self._plot_error_bars.isChecked():
            return self._plotMeasurementErrorBars
        elif self._plot_colored_error_regions.isChecked():
            return self._plotColoredErrorRegions
        else:
            assert False

    def _getConfidenceInterval(self, estimated_values, error_values, error_means):
        error_variances = stepwise_variance(error_values)
        error_stddevs = [v ** 0.5 for v in error_variances]

        error_confidence_intervals = [0.0]
        error_confidence_intervals += \
            [confidence_interval(self._alpha, stddev, max(n+1, 2)) 
             for n, stddev in enumerate(error_stddevs)][1:]

        return self._error_range(estimated_values, error_means, 
                                  error_confidence_intervals, 
                                  error_confidence_intervals)

    def _error_range(self, estimates, error_means, 
                      lower_error_intervals, upper_error_intervals):
        num_estimates = len(estimates)
        estimates = np.array(estimates)

        # use the last num_estimates items in each of these arrays.
        # if they have the same size, we'll use all of them.
        # if len(error_means) > len(estimated_values), 
        #   we'll use the later error values, treating the rest as history.
        assert len(estimates) <= len(error_means)
        error_means = np.array(error_means[-num_estimates:])
        lower_error_intervals = np.array(lower_error_intervals[-num_estimates:])
        upper_error_intervals = np.array(upper_error_intervals[-num_estimates:])

        lower_errors = error_means - lower_error_intervals
        upper_errors = error_means + upper_error_intervals

        adjusted_estimates = error_adjusted_estimate(estimates, error_means)
        lowers = error_adjusted_estimate(estimates, lower_errors)
        uppers = error_adjusted_estimate(estimates, upper_errors)
        return lowers, uppers

    def _getErrorStddev(self, estimated_values, error_values, error_means):
        error_variances = stepwise_variance(error_values)
        error_stddevs = [v ** 0.5 for v in error_variances]

        return self._error_range(estimated_values, error_means, 
                                  error_stddevs, error_stddevs)

    def _plotEstimates(self, times, estimated_values, network_type):
        self._axes.plot(times, estimated_values,
                         label=network_type + " estimates",
                         color=self._irob_colors[network_type])

    def _plotObservations(self, times, observations, network_type):
        markers = {'wifi': 's', '3G': 'o'}
        self._axes.plot(times, observations, label=network_type + " observations",
                         linestyle='none', marker=markers[network_type],
                         markersize=3, color=self._irob_colors[network_type])



    def _plotMeasurementErrorBars(self, times, estimated_values,
                                   error_adjusted_estimates, error_bounds, network_type):
        estimates = np.array(estimated_values)
        yerr = [estimates - error_bounds[0],  error_bounds[1] - estimates]
        
        self._axes.errorbar(times, estimated_values, yerr=yerr,
                             color=self._irob_colors[network_type])


    def _plotColoredErrorRegions(self, times, estimated_values, 
                                  error_adjusted_estimates, error_bounds, network_type):
        where = [True] * len(times)

        #if network_type == "wifi":
        if False: # this doesn't work too well yet, so I'm leaving it out until I fix it.
            wifi_periods = self._getWifiPeriods()

            def get_mid(mid, left, right, left_val, right_val):
                slope = (right_val - left_val) / (right - left)
                delta = slope * (mid - left)
                return left_val + delta

            def split_region(times, values, split_time, new_value=None):
                pos = bisect_right(times, split_time)
                if new_value is not None:
                    mid = new_value
                elif pos == 0:
                    mid = values[0]
                elif pos == len(times):
                    mid = values[pos - 1]
                else:
                    mid = get_mid(split_time, times[pos - 1], times[pos],
                                  values[pos - 1], values[pos])
                    
                times.insert(pos, split_time)
                values.insert(pos, mid)
                return pos

            def insert_inflection_points_for_wifi(values):
                my_times = copy(times)
                my_where_times = copy(times)
                where = [True] * len(values)

                end_pos = None
                for start, length in wifi_periods:
                    split_region(my_times, values, start)
                    split_region(my_times, values, start + length)
                    start_pos = split_region(my_where_times, where, start, True)
                    if end_pos is not None:
                        for i in xrange(end_pos, start_pos):
                            # mark slices between last wifi end and current wifi start
                            #  as no-wifi
                            where[i] = False

                    end_pos = split_region(my_where_times, where, start + length, False)
                    for i in xrange(start_pos, end_pos+1):
                        # mark slices during this wifi period to be plotted
                        where[i] = True

                return my_times, where

            new_values = []
            value_lists = [list(v) for v in [estimated_values,
                                             error_adjusted_estimates, 
                                             error_bounds[0],
                                             error_bounds[1]]]
            for value_list in value_lists:
                new_times, new_where = insert_inflection_points_for_wifi(value_list)
                new_values.append(value_list)

            estimated_values, error_adjusted_estimates = new_values[:2]
            error_bounds = new_values[2:]
            times = new_times
            where = new_where

        where = np.array(where)

        for the_where, alpha in [(where, 0.5), (~where, 0.1)]:
            self._axes.fill_between(times, error_bounds[1], error_bounds[0], where=the_where,
                                     facecolor=self._irob_colors[network_type],
                                     alpha=alpha)

        self._axes.plot(times, error_adjusted_estimates,
                         color=self._irob_colors[network_type],
                         linestyle='--', linewidth=0.5)
                                 
    def _setupAxes(self):
        yticks = {}
        for network, pos in self._network_pos_offsets.items():
            for direction, offset in self._direction_pos_offsets.items():
                label = "%s %s" % (network, direction)
                yticks[pos + offset] = label

        min_tick = min(yticks.keys())
        max_tick = max(yticks.keys())
        self._axes.set_ylim(min_tick - self._irob_height,
                             max_tick + self._irob_height)
        self._axes.set_yticks(yticks.keys())
        self._axes.set_yticklabels(yticks.values())

    def _setTraceEnd(self):
        for network_type in self._network_periods:
            periods = self._network_periods[network_type]
            if periods:
                periods[-1]['end'] = self._end

    def _drawIROBs(self):
        for network_type in self._networks:
            network = self._networks[network_type]
            for direction in network:
                irobs = network[direction]
                for irob_id in irobs:
                    irob = irobs[irob_id]
                    irob.draw(self._axes)

    def _getSessionAxes(self, reset=False):
        if self._session_axes == None or reset:
            self._session_axes = self._axes.twinx()
        return self._session_axes

    def _drawSessions(self, **kwargs):
        if self._sessions and self._show_sessions.isChecked():
            self._drawSomeSessions(self._sessions, **kwargs)

        if self._choose_network_calls:
            timestamps, calls = zip(*self._choose_network_calls)
            timestamps = [self.getAdjustedTime(t) for t in timestamps]
            self._getSessionAxes().plot(timestamps, calls, label="choose_network")

    def _drawSomeSessions(self, sessions, **kwargs):
        timestamps = [self.getAdjustedTime(s['start']) for s in sessions]
        session_times = [s['end'] - s['start'] for s in sessions]
        
        if 'marker' not in kwargs:
            kwargs['marker'] = 'o'
        if 'markersize' not in kwargs:
            kwargs['markersize'] = 3
        if 'color' not in kwargs:
            kwargs['color'] = 'black'
            
        ax = self._getSessionAxes()
        ax.plot(timestamps, session_times, **kwargs)

    def _drawRedundancyDecisions(self):
        if self._redundancy_decisions and self._show_decisions.isChecked():
            timestamps = [self.getAdjustedTime(d.timestamp)
                          for d in self._redundancy_decisions]
            redundancy_benefits = [d.benefit for d in self._redundancy_decisions]
            redundancy_costs = [d.cost for d in self._redundancy_decisions]

            ax = self._getSessionAxes()
            ax.plot(timestamps, redundancy_benefits, marker='o',
                    markersize=3, color='green')
            ax.plot(timestamps, redundancy_costs, marker='o', markersize=3, color='orange')

    def _drawRadioSwitches(self):
        positions = {
            "HSDPA": -0.15,
            "UMTS": -0.20,
            "unknown": -0.30
        }
        height = 0.05
        colors = {
            "HSDPA": "green",
            "UMTS": "yellow",
            "unknown": "red"
        }
        print self._radio_switches
        if self._radio_switches:
            bars = {
                "HSDPA": [],
                "UMTS": [],
                "unknown": []
            }
            last_timestamp = self._radio_switches[0][0]
            for timestamp, prev_type, new_type in self._radio_switches:
                bars[prev_type].append([last_timestamp, timestamp-last_timestamp])

            for type in bars:
                print bars[type]
                self._axes.broken_barh(bars[type], [positions[type] - height/2.0, height],
                                        colors[type])

    def _getWifiPeriods(self):
        wifi_periods = filter(lambda p: p['end'] is not None,
                              self._network_periods['wifi'])
        return [(self.getAdjustedTime(period['start']),
                 period['end'] - period['start'])
                for period in wifi_periods]

    def _drawWifi(self):
        if "wifi" not in self._network_periods:
            # not done parsing yet
            return

        bars = self._getWifiPeriods()
        vertical_bounds = self._axes.get_ylim()
        height = [vertical_bounds[0] - self._irob_height / 2.0,
                  vertical_bounds[1] - vertical_bounds[0] + self._irob_height]
        self._axes.broken_barh(bars, height, color="green", alpha=0.2)

    def printStats(self):
        if self._choose_network_calls:
            choose_network_times = [c[1] for c in self._choose_network_calls]
            print ("%f seconds in chooseNetwork (%d calls)" %
                   (sum(choose_network_times), len(choose_network_times)))

        self._printRedundancyBenefitAnalysis()
        self._printIROBTimesByNetwork()

    def _printRedundancyBenefitAnalysis(self):
        if "wifi" not in self._network_periods:
            # not done parsing yet
            return

        wifi_periods = self._getWifiPeriods()
        num_sessions = len(self._sessions)

        def one_network_only(session):
            session_start = self.getAdjustedTime(session['start'])
            for start, length in wifi_periods:
                if session_start >= start and session_start <= (start + length):
                    return False
            return True

        def duration(session):
            return session['end'] - session['start']

        # XXX: could do this more efficiently by marking it
        # XXX: when we first read the log.
        single_network_sessions = filter(one_network_only, self._sessions)
        print ("Single network sessions: %d/%d (%.2f%%), total time %f seconds" %
               (len(single_network_sessions), num_sessions,
                len(single_network_sessions)/float(num_sessions) * 100,
                sum([duration(s) for s in single_network_sessions])))


        def failed_over(session):
            session_start = self.getAdjustedTime(session['start'])
            session_end = session_start + duration(session)

            if one_network_only(session):
                return False
            
            for start, length in wifi_periods:
                if (session_start >= start and session_start <= (start + length) and
                    session_end > (start + length)):
                        return True

            matching_irobs = self._getIROBs(session_start, session_end)
            # irobs['wifi']['down'] => [IROB(),...]
            
            # get just the irobs without the dictionary keys
            irobs = sum([v.values() for v in matching_irobs.values()], [])
            # list of lists of IROBs
            irobs = sum(irobs, [])
            # list of IROBs


            dropped_irobs = filter(lambda x: x.wasDropped(), irobs)
            fail = (len(dropped_irobs) > 0)
            if fail:
                dprint("Session: %s" % session)
                dprint("  IROBs: " % irobs)
            return fail

        failover_sessions = filter(failed_over, self._sessions)
        print ("Failover sessions: %d/%d (%.2f%%), total %f seconds" %
               (len(failover_sessions), num_sessions,
                len(failover_sessions)/float(num_sessions) * 100,
                sum([duration(s) for s in failover_sessions])))
        
        # check the sessions that started in a single-network period
        # but finished after wifi arrived.
        def needed_reevaluation(session):
            session_start = self.getAdjustedTime(session['start'])
            session_end = session_start + duration(session)
            for start, length in wifi_periods:
                if session_start >= start and session_start <= (start + length):
                    return False
                # didn't start during this wifi period.
                # did this wifi period come in the middle of the session?
                if start > session_start and start < session_end:
                    # wifi arrived sometime during session
                    return True
            return False

        reevaluation_sessions = filter(needed_reevaluation, self._sessions)
        print ("Needed-reevaluation sessions: %d/%d (%.2f%%)" %
               (len(reevaluation_sessions), num_sessions,
                len(reevaluation_sessions)/float(num_sessions) * 100))

        # TODO: print average wifi, 3G session times

        self._debug_sessions = failover_sessions
        #self._debug_sessions = reevaluation_sessions

    def _printIROBTimesByNetwork(self):
        irobs = self._getIROBs()
        print "Average IROB durations:"
        for network_type, direction in product(['wifi', '3G'], ['down', 'up']):
            dprint("%s sessions:" % network_type)
            times = [irob.getDuration() for irob in irobs[network_type][direction]]

            if len(times) > 0:
                avg = sum(times) / len(times)
                print "  %5s, %4s: %f" % (network_type, direction, avg)
            else:
                print "  %5s, %4s: (no IROBs)" % (network_type, direction)


    def _getIROBs(self, start=-1.0, end=None):
        """Get all IROBs that start in the specified time range.

        Returns a dictionary: d[network_type][direction] => [IROB(),...]

        start -- relative starting time
        end -- relative ending time

        """
        if end is None:
            end = self._end + 1.0

        matching_irobs = {'wifi': {'down': [], 'up': []}, 
                          '3G': {'down': [], 'up': []}}
        def time_matches(irob):
            irob_start, irob_end = [self.getAdjustedTime(t) for t in irob.getTimeInterval()]
            return (irob_start >= start and irob_end <= end)
            
        for network_type, direction in product(['wifi', '3G'], ['down', 'up']):
            if network_type in self._networks and direction in self._networks[network_type]:
                irobs = self._networks[network_type][direction].values()
                matching_irobs[network_type][direction] = filter(time_matches, irobs)
        return matching_irobs

    def _drawDebugging(self):
        self._drawSomeSessions(self._debug_sessions,
                                marker='s', color='red', markersize=10,
                                markerfacecolor='none', linestyle='none')


    def getIROBPosition(self, irob):
        # TODO: allow for simultaneous (stacked) IROB plotting.
        
        return (self._network_pos_offsets[irob.network_type] +
                self._direction_pos_offsets[irob.direction])
        
    def getIROBHeight(self, irob):
        # TODO: adjust based on the number of stacked IROBs.
        return self._irob_height

    def getIROBColor(self, irob):
        return self._irob_colors[irob.network_type]

    def getAdjustedTime(self, timestamp):
        return timestamp - self._start

    def getAdjustedTraceLatency(self, latency):
        if not self._cross_country_latency or latency < 0.0001:
            return latency
        
        LATENCY_ADJUSTMENT = 0.100 # 100ms cross-country
        return latency + LATENCY_ADJUSTMENT

    def setStart(self, start):
        # for resetting the experiment start, to avoid including the
        #  setup transfers and waiting time at the server.
        self._start = start

    def parseLine(self, line):
        timestamp = getTimestamp(line)
        if self._start == None:
            self._start = timestamp
            
        self._end = timestamp

        if "Got update from scout" in line:
            #[time][pid][tid] Got update from scout: 192.168.1.2 is up,
            #                 bandwidth_down 43226 bandwidth_up 12739 bytes/sec RTT 97 ms
            #                 type wifi
            ip, status, network_type = re.search(self._network_regex, line).groups()
            if not self._is_server:
                self._modifyNetwork(timestamp, ip, status, network_type)
        elif "Successfully bound" in line:
            # [time][pid][CSockSender 57] Successfully bound osfd 57 to 192.168.1.2:0
            self._addConnection(line)
        elif "Adding connection" in line:
            # [time][pid][Listener 13] Adding connection 14 from 192.168.1.2
            #                          bw_down 43226 bw_up 12739 RTT 97
            #                          type wifi(peername 141.212.110.115)
            self._addIncomingConnection(line, timestamp)
        elif re.search(self._csocket_destroyed_regex, line) != None:
            # [time][pid][CSockSender 57] CSocket 57 is being destroyed
            self._removeConnection(line)
        elif "Getting bytes to send from IROB" in line:
            # [time][pid][CSockSender 57] Getting bytes to send from IROB 6
            irob = int(line.strip().split()[-1])
            network = self._getNetworkType(line)
            
            self._currentSendingIROB = irob
            self._addIROB(timestamp, network, irob, 'up')
        elif "...returning " in line:
            # [time][pid][CSockSender 57] ...returning 1216 bytes, seqno 0
            assert self._currentSendingIROB != None
            datalen = int(line.strip().split()[3])
            network = self._getNetworkType(line)
            self._addIROBBytes(timestamp, network, self._currentSendingIROB,
                                datalen, 'up')
        elif "About to send message" in line:
            # [time][pid][CSockSender 57] About to send message:  Type: Begin_IROB(1)
            #                             Send labels: FG,SMALL IROB: 0 numdeps: 0
            self._addTransfer(line, 'up')
        elif "Received message" in line:
            # [time][pid][CSockReceiver 57] Received message:  Type: Begin_IROB(1)
            #                               Send labels: FG,SMALL IROB: 0 numdeps: 0
            self._addTransfer(line, 'down')
        elif "network estimator" in line:
            network_type = re.search(self._network_estimator_regex, line).group(1)
            dprint("got observation: %s" % line)
            bw_match = re.search(self._network_bandwidth_regex, line)
            lat_match = re.search(self._network_latency_regex, line)
            bw, latency = None, None
            if bw_match and float(bw_match.groups()[0]) > 0.0:
                bw = bw_match.groups()
            if lat_match and float(lat_match.groups()[0]) > 0.0:
                latency = lat_match.groups()
            
            self._addEstimates(network_type, timestamp, bw=bw, latency=latency)
        elif "New spot values" in line:
            # TODO: parse values, call self._addNetworkObservation
            network_type = self._getNetworkType(line)
            pass
        elif "New estimates" in line:
            # TODO: parse values, call self._addNetworkEstimate
            network_type = self._getNetworkType(line)
            pass
        elif "chooseNetwork" in line:
            duration = timestamp - getTimestamp(self._last_line)
            time_match = re.search(choose_network_time_regex, line)
            if time_match:
                duration = float(time_match.group(1))
            self._choose_network_calls.append((timestamp, duration))
        elif "redundancy_strategy_type" in line:
            # [timestamp][pid][Bootstrapper 49] Sending hello:  Type: Hello(0)
            #                                   Send labels:  listen port: 42424
            #                                   num_ifaces: 2 
            #                                   redundancy_strategy_type: intnw_redundant
            redundancy_strategy = \
                re.search(self._redundancy_strategy_regex, line).group(1)
            if redundancy_strategy not in self._title:
                self._title += " - " + redundancy_strategy
        else:
            pass # ignore it
            
        self._last_line = line

    def _initRegexps(self):
        self._irob_regex = re.compile("IROB: ([0-9]+)")
        self._datalen_regex = re.compile("datalen: ([0-9]+)")
        self._expected_bytes_regex = re.compile("expected_bytes: ([0-9]+)")
        self._network_regex = re.compile("scout: (.+) is (down|up).+ type ([A-Za-z0-9]+)")

        ip_regex_string = "([0-9]+(?:\.[0-9]+){3})"
        self._ip_regex = re.compile(ip_regex_string)
        
        self._socket_regex = re.compile("\[CSock(?:Sender|Receiver) ([0-9]+)\]")
        self._intnw_message_type_regex = \
            re.compile("(?:About to send|Received) message:  Type: ([A-Za-z_]+)")
        self._csocket_destroyed_regex = re.compile("CSocket ([0-9]+) is being destroyed")
        self._network_estimator_regex = \
            re.compile("Adding new stats to (.+) network estimator")

        float_regex = "([0-9]+" + "(?:\.[0-9]+)?)"
        stats_regex = "obs %s est %s" % (float_regex, float_regex)
        self._network_bandwidth_regex = re.compile("bandwidth: " + stats_regex)
        self._network_latency_regex = re.compile("latency: " + stats_regex)

        self._redundancy_strategy_regex = \
            re.compile("redundancy_strategy_type: ([a-z_]+)\s*")

        self._incoming_connection_regex = \
            re.compile("Adding connection ([0-9]+) from " + ip_regex_string +
                       ".+type ([A-Za-z0-9]+)")
        
    def _getIROBId(self, line):
        return int(re.search(self._irob_regex, line).group(1))

    def _getSocket(self, line):
        return int(re.search(self._socket_regex, line).group(1))

    def _getIP(self, line):
        return re.search(self._ip_regex, line).group(1)

    def _addNetworkType(self, network_type):
        if network_type not in self._networks:
            self._networks[network_type] = {
                'down': {}, # download IROBs
                'up': {}    # upload IROBs
                }
            self._network_periods[network_type] = []

    def _modifyNetwork(self, timestamp, ip, status, network_type):
        self._addNetworkType(network_type)
        
        if status == 'down':
            period = self._network_periods[network_type][-1]
            if period['end'] is not None:
                print "Warning: double-ending %s period at %f" % (network_type, timestamp)
            period['end'] = timestamp
            
            if ip in self._network_type_by_ip:
                del self._network_type_by_ip[ip]
        elif status == 'up':
            self._startNetworkPeriod(network_type, ip,
                                      start=timestamp, end=None, sock=None)
            
            placeholder = (ip in self._network_type_by_ip and
                           self._network_type_by_ip[ip] == "placeholder")
            self._network_type_by_ip[ip] = network_type
            if placeholder:
                sock = self._placeholder_sockets[ip]
                del self._placeholder_sockets[ip]
                self._addNetworkPeriodSocket(sock, ip)
        else: assert False

    def _startNetworkPeriod(self, network_type, ip, start, end=None, sock=None):
        periods = self._network_periods[network_type]
        if len(periods) > 0 and periods[-1]['end'] == None:
            # two perfectly adjacent periods with no 'down' in between.  whatevs.
            periods[-1]['end'] = start

        periods.append({
            'start': start, 'end': end,
            'ip': ip, 'sock': sock
            })
        
    def _addConnection(self, line):
        sock = self._getSocket(line)
        ip = self._getIP(line)
        if ip not in self._network_type_by_ip:
            self._network_type_by_ip[ip] = "placeholder"
            assert ip not in self._placeholder_sockets
            self._placeholder_sockets[ip] = sock
        else:
            self._addNetworkPeriodSocket(sock, ip)

    def _addEstimates(self, network_type, timestamp, bw=None, latency=None):
        if network_type not in self._estimates:
            self._estimates[network_type] = {}
                
        for values, name in zip((bw, latency), ("bandwidth_up", "latency")):
            if values:
                obs, est = values
                self._addNetworkObservation(network_type, name,
                                             float(timestamp), float(obs))
                self._addNetworkEstimate(network_type, name, float(est))

    def _getEstimates(self, network_type, name):
        all_estimates = self._estimates[network_type]
        if name not in all_estimates:
            all_estimates[name] = []
        return all_estimates[name]
                
    def _addNetworkObservation(self, network_type, name, timestamp, obs):
        if obs == 0.0:
            debug_trace()
            
        estimates = self._getEstimates(network_type, name)
        estimates.append({'timestamp': float(timestamp),
                          'observation': float(obs),
                          'estimate': None})

    def _addNetworkEstimate(self, network_type, name, est):
        estimates = self._getEstimates(network_type, name)
        assert estimates[-1]['estimate'] is None
        estimates[-1]['estimate'] = est

    def _addNetworkPeriodSocket(self, sock, ip):
        network_type = self._network_type_by_ip[ip]
        network_period = self._network_periods[network_type][-1]

        assert network_period['start'] != None
        assert network_period['ip'] == ip
        assert network_period['sock'] == None
        network_period['sock'] = sock

        assert sock not in self._network_type_by_sock
        self._network_type_by_sock[sock] = network_type

    def _addIncomingConnection(self, line, timestamp):
        match = re.search(self._incoming_connection_regex, line)
        sock, ip, network_type = match.groups()
        sock = int(sock)

        self._addNetworkType(network_type)
        self._startNetworkPeriod(network_type, ip, start=timestamp, end=None, sock=None)
        
        self._network_type_by_ip[ip] = network_type
        self._addNetworkPeriodSocket(sock, ip)
        
    def _removeConnection(self, line):
        timestamp = getTimestamp(line)
        sock = int(re.search(self._csocket_destroyed_regex, line).group(1))
        if sock in self._network_type_by_sock:
            network_type = self._network_type_by_sock[sock]
            network_period = self._network_periods[network_type][-1]
            
            network_period['sock'] = None
            del self._network_type_by_sock[sock]

            self._markDroppedIROBs(timestamp, network_type)

            if self._is_server:
                # client will get update from scout; server won't
                self._modifyNetwork(timestamp, network_period['ip'], 'down', network_type)

    def _getNetworkType(self, line):
        sock = self._getSocket(line)
        return self._network_type_by_sock[sock]

    def _getDatalen(self, line):
        return int(re.search(self._datalen_regex, line).group(1))

    def _getExpectedBytes(self, line):
        return int(re.search(self._expected_bytes_regex, line).group(1))

    def _getIntNWMessageType(self, line):
        return re.search(self._intnw_message_type_regex, line).group(1)

    def _addTransfer(self, line, direction):
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
        timestamp = getTimestamp(line)
        network_type = self._getNetworkType(line)
        intnw_message_type = self._getIntNWMessageType(line)

        if (intnw_message_type == "Begin_IROB" or
            intnw_message_type == "Data_Check"):
            irob_id = self._getIROBId(line)
            if direction == "up":
                self._currentSendingIROB = None
            self._addIROB(timestamp, network_type, irob_id, direction)
        elif intnw_message_type == "IROB_chunk":
            irob_id = self._getIROBId(line)
            datalen = self._getDatalen(line)
            self._addIROBBytes(timestamp, network_type, irob_id, datalen, direction)
        elif intnw_message_type == "End_IROB" and direction == 'down':
            irob_id = self._getIROBId(line)
            expected_bytes = self._getExpectedBytes(line)
            self._finishReceivedIROB(timestamp, network_type, irob_id, expected_bytes)
        elif intnw_message_type == "Ack" and direction == 'down':
            irob_id = self._getIROBId(line)
            self._ackIROB(timestamp, network_type, irob_id, 'up')
        else:
            pass # ignore other types of messages

    def _getIROB(self, network_type, irob_id, direction, start=None):
        if network_type not in self._networks:
            raise LogParsingError("saw data on unknown network '%s'" % network_type)
        irobs = self._networks[network_type][direction]
        if irob_id not in irobs:
            if start != None:
                irobs[irob_id] = IROB(self, network_type, direction, start, irob_id)
            else:
                return None
            
        return irobs[irob_id]

    def _getIROBOrThrow(self, network_type, irob_id, direction, start=None):
        irob = self._getIROB(network_type, irob_id, direction, start)
        if irob == None:
            raise LogParsingError("Unknown IROB %d" % irob_id)
        return irob
    
    def _addIROB(self, timestamp, network_type, irob_id, direction):
        # TODO: deal with the case where the IROB announcement arrives after the data
        irob = self._getIROBOrThrow(network_type, irob_id, direction, start=timestamp)
        dprint("Adding %s at %f" % (irob, timestamp))


    def _addIROBBytes(self, timestamp, network_type, irob_id, datalen, direction):
        # XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        # XXX: this is double-counting when chunk messages do appear.  fix it.
        # XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        irob = self._getIROBOrThrow(network_type, irob_id, direction)
        irob.addBytes(timestamp, datalen)

    def _finishReceivedIROB(self, timestamp, network_type, irob_id, expected_bytes):
        irob = self._getIROBOrThrow(network_type, irob_id, 'down')
        irob.finish(timestamp, expected_bytes)

        # "finish" the IROB by marking it ACK'd.
        # Strictly speaking, a received IROB isn't "finished" until
        #  all expected bytes are received, but until the end_irob messasge
        #  arrives, we don't know how many to expect, so we hold off on
        #  marking it finished until then.
        dprint("Finished %s at %f" % (irob, timestamp))
        irob.ack(timestamp)

    def _markDroppedIROBs(self, timestamp, network_type):
        for direction in ['down', 'up']:
            for irob in self._networks[network_type][direction].values():
                if not irob.complete():
                    irob.markDropped(timestamp)

    def _ackIROB(self, timestamp, network_type, irob_id, direction):
        irob = self._getIROBOrThrow(network_type, irob_id, direction)
        irob.ack(timestamp)
        dprint("Acked %s at %f" % (irob, timestamp))

class RedundancyDecision(object):
    def __init__(self, timestamp):
        self.timestamp = timestamp;
        self.benefit = None
        self.cost = None

class ErrorHistory(object):
    def __init__(self):
        self._history = {}

    def read(self, filename):
        with open(filename) as f:
            num_estimators = int(f.readline().split()[0])
            for i in xrange(num_estimators):
                fields = f.readline().split()
                network_type, what_to_plot = fields[0].split("-")
                error_distribution_type = fields[1] #TODO: use this?
                num_samples = int(fields[2])

                network_type = network_type.replace("cellular", "3G")
                what_to_plot = what_to_plot.replace("bandwidth", "bandwidth_up")
                what_to_plot = what_to_plot.replace("RTT", "latency")
                if network_type not in self._history:
                    self._history[network_type] = {}
                if what_to_plot not in self._history[network_type]:
                    self._history[network_type][what_to_plot] = []
                    
                errors = self._history[network_type][what_to_plot]
                while len(errors) < num_samples:
                    line = f.readline().strip()
                    new_errors = [float(x) for x in line.split()]
                    errors.extend(new_errors)
        
    def getErrors(self, network_type, what_to_plot):
        if network_type in self._history and what_to_plot in self._history[network_type]:
            return self._history[network_type][what_to_plot]
        
        return []

    def getAllErrors(self):
        return dict(self._history)

class AppPlotter(object):
    def __init__(self, args, app_client_log, app_server_log):
        self._windows = []
        self._currentPid = None
        self._pid_regex = re.compile("^\[[0-9]+\.[0-9]+\]\[([0-9]+)\]")

        intnw_log = "intnw.log"
        timing_log = "timing.log"
        instruments_log = "instruments.log"
        self._is_server = args.server
        if args.server:
            intnw_log = app_client_log = instruments_log = app_server_log

        self._measurements_only = args.measurements
        self._network_trace_file = args.network_trace_file
        self._bandwidth_measurements_file = args.bandwidth_measurements_file
        self._intnw_log = args.basedir + "/" + intnw_log
        self._timing_log = args.basedir + "/" + timing_log
        self._app_client_log = args.basedir + "/" + app_client_log
        self._redundancy_eval_log = args.basedir + "/" + instruments_log
        self._radio_logs = args.radio_log
        self._cross_country_latency = args.cross_country_latency
        self._history_dir = args.history

        # subclasses may append functions here to draw extra data
        #  functions should be void(IntNWBehaviorPlot, matplotlib.Axes)
        self._appFeatureDrawers = []
        self._appMeasurementDrawers = []

    def readLogs(self):
        self._readIntNWLog()
        self.draw()
        self.printStats()

    def draw(self):
        for window in self._windows:
            window.on_draw()
            window.saveErrorTable()

    def printStats(self):
        for window in self._windows:
            window.printStats()
        
    def _getPid(self, line):
        match = re.search(self._pid_regex, line)
        if match:
            return int(match.group(1))

        return None

    def _readDurations(self):
        filename = self._timing_log
        duration_regex = re.compile("([0-9]+)-minute runs")
        durations = []
        for line in open(filename).readlines():
            m = re.search(duration_regex, line)
            if m:
                minutes = int(m.group(1))
                durations.append(minutes)
        return durations

    def _readSessions(self):
        'Implement in subclass.'
        raise NotImplementedError()

    def _lineStartsNewRun(self, line, current_pid):
        if self._is_server:
            return ("Accepting connection from" in line)
        else:
            pid = self._getPid(line)
            return pid != current_pid

    def _readRedundancyDecisions(self):
        filename = self._redundancy_eval_log
        if not os.path.exists(filename):
            return
        
        benefit_regex = re.compile("Redundant strategy benefit: ([0-9.-]+)")
        cost_regex = re.compile("Redundant strategy additional cost: ([0-9.-]+)")
        
        runs = []
        last_pid = 0
        for linenum, line in enumerate(open(filename).readlines()):
            pid = self._getPid(line)
            if self._lineStartsNewRun(line, last_pid):
                runs.append([])
                
            last_pid = pid

            benefit_match = re.search(benefit_regex, line)
            cost_match = re.search(cost_regex, line)
            if benefit_match or cost_match:
                current_run = runs[-1]

            if benefit_match:
                timestamp = getTimestamp(line)
                decision = RedundancyDecision(timestamp)
                decision.benefit = float(benefit_match.group(1))
                dprint("Redundancy benefit: %f" % decision.benefit)
                
                current_run.append(decision)
            elif cost_match:
                decision = current_run[-1]
                decision.cost = float(cost_match.group(1))
                dprint("Redundancy cost: %f" % decision.cost)
        return runs

    def _readRadioLogs(self):
        switches = []
        rx = re.compile("(.+) D/GSM.+RAT switched (.+) -> (.+) at cell")
        with open(self._radio_logs) as f:
            for line in f.readlines():
                match = rx.search(line)
                if match:
                    timestamp_str, prev_type, new_type = match.groups()

                    # strip millis and parse
                    timestamp = datetime.datetime.strptime(timestamp_str[:-4], "%m-%d-%Y %H:%M:%S")
                    timestamp_secs = time.mktime(timestamp.timetuple())
                    switches.append((timestamp_secs, prev_type, new_type))
        return switches

    def _readIntNWLog(self):
        session_runs = None
        redundancy_decisions = None
        radio_switches = None
        if self._app_client_log:
            session_runs = self._readSessions()
        if self._radio_logs:
            radio_switches = self._readRadioLogs()
        if self._redundancy_eval_log:
            redundancy_decisions_runs = self._readRedundancyDecisions()
        
        error_history = ErrorHistory()
        if self._history_dir:
            side = "server" if self._is_server else "client"
            history_filename = "%s/%s_error_distributions.txt" % (self._history_dir, side)
            
            error_history.read(history_filename)

        durations = self._readDurations()
        
        print "Parsing log file..."
        progress = ProgressBar()
        for linenum, line in enumerate(progress(open(self._intnw_log).readlines())):
            try:
                pid = self._getPid(line)
                if pid == None:
                    continue

                if self._lineStartsNewRun(line, self._currentPid):
                    if len(self._windows) > 0:
                        if radio_switches:
                            self._windows[-1].setRadioSwitches(radio_switches)


                    start = session_runs[len(self._windows)][0]['start']
                    window = IntNWBehaviorPlot(len(self._windows) + 1, start, 
                                               self._measurements_only,
                                               self._network_trace_file,
                                               self._bandwidth_measurements_file,
                                               self._cross_country_latency,
                                               error_history, self._is_server,
                                               self._appFeatureDrawers,
                                               self._appMeasurementDrawers)
                    self._windows.append(window)
                    window_num = len(self._windows) - 1
                    if session_runs:
                        sessions = session_runs[window_num]
                        window.setSessions(sessions)
                    if redundancy_decisions_runs:
                        redundancy_decisions = redundancy_decisions_runs[window_num]
                        window.setRedundancyDecisions(redundancy_decisions)

                    window.setXlimMax(durations[window_num] * 60)

                self._currentPid = pid
                
                if len(self._windows) > 0:
                    self._windows[-1].parseLine(line)
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
        for window in self._windows:
            window.show()

           
def exception_hook(type, value, tb):
    from PyQt4.QtCore import pyqtRemoveInputHook 
    import traceback, pdb
    traceback.print_exception(type, value, tb)
    pyqtRemoveInputHook()
    pdb.pm()

def debug_trace():
    from PyQt4.QtCore import pyqtRemoveInputHook 
    import pdb
    pyqtRemoveInputHook()
    pdb.set_trace()

import sys
sys.excepthook = exception_hook
    
def main(args, args_list, plotter_class):
    parser = ArgumentParser()
    parser.add_argument("basedir")
    parser.add_argument("--measurements", action="store_true", default=False)
    parser.add_argument("--network-trace-file", default=None)
    parser.add_argument("--bandwidth-measurements-file", default=None)
    parser.add_argument("--noplot", action="store_true", default=False)
    parser.add_argument("--cross-country-latency", action="store_true", default=False,
                        help="Add 100ms latency when plotting the trace.")
    parser.add_argument("--server", action="store_true", default=False,
                        help="Look in server logfile for all logging.")
    parser.add_argument("--absolute-error", action="store_true", default=False,
                        help="Use absolute error rather than relative error in calculations")
    parser.add_argument("--history", default=None,
                        help=("Start the plotting with error history from files in this directory:"
                              +" {client,server}_error_distributions.txt"))
    parser.add_argument("--radio-log", default=None)
    parser.add_argument("-d", "--debug", action="store_true", default=False)
    parser.parse_args(args_list, namespace=args)

    global debug
    debug = args.debug

    app = QApplication(sys.argv)

    if args.absolute_error:
        global RELATIVE_ERROR
        RELATIVE_ERROR = False
    
    plotter = plotter_class(args)
    plotter.readLogs()
    if not args.noplot:
        plotter.show()
        app.exec_()

if __name__ == '__main__':
    print "Should be run from another script that subclasses AppPlotter."
    sys.exit(1)
