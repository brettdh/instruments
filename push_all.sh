#!/bin/bash

#DIR=obj/local/armeabi-v7a
DIR=libs/armeabi-v7a

adb root \
&& adb remount \
&& adb push $DIR/libinstruments.so /system/lib/ \
&& adb push $DIR/run_perf_test /system/bin
