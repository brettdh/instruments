#!/usr/bin/env python

import sys

last_sequence = ""
cur_sequence = ""

sequence_count = 0

estimator_labels = {}

def get_estimator_label(estimator):
    if estimator not in estimator_labels:
        label = chr(ord('A') + len(estimator_labels))
        estimator_labels[estimator] = label
    return estimator_labels[estimator]

for line in sys.stdin.readlines():
    fields = line.strip().split()
    if len(fields) < 2:
        continue
        
    estimator = int(fields[1], 16)

    label = get_estimator_label(estimator)

    if cur_sequence and label in cur_sequence:
        # found the end of a sequence
        if last_sequence == cur_sequence:
            sequence_count += 1
        else:
            sequence_count = 1
            sys.stdout.write("\n")
            
        sys.stdout.write("\r%s : %d times" % (last_sequence, sequence_count))
        sys.stdout.flush()

        last_sequence = cur_sequence
        cur_sequence = ""
        
    cur_sequence += label

sys.stdout.write("\n")
