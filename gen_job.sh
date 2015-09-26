#!/bin/bash

APP_NAME="$1"
INITIAL_DIR="$2"
NODES_COUNT="$3"

JOB_FILE=small.job

mkdir -p $INITIAL_DIR/logs
perl -pe "s#___APP_NAME___#$APP_NAME#g;s#___INITIAL_DIR___#$INITIAL_DIR#g;s#___NODES_COUNT___#$NODES_COUNT#g" \
    $INITIAL_DIR/../$JOB_FILE > $INITIAL_DIR/../$APP_NAME.job
