#!/bin/bash

APP_NAME="$1"
INITIAL_DIR="$2"
NODES_COUNT="$3"

JOB_FILE=$INITIAL_DIR/../small.job

perl -pe "s#___APP_NAME___#$APP_NAME#; s#___INITIAL_DIR___#$INITIAL_DIR#; s#___NODES_COUNT___#$NODES_COUNT#" $JOB_FILE > $INITIAL_DIR/small.job
