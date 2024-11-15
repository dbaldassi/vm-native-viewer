#!/bin/bash

set -e

if [ -f "setup_tc.sh" ]
then
    ./setup_tc.sh &
else
    echo "--- no tc commands provided ---"
fi

# start pulse audio otherwise webrtc audio module fails.
# maybe we can think of disabling it since we don't really use audio
echo "--- start pulse audio ---"
pulseaudio --start
# pax11publish -r

echo "--- run viewer ---"
./vmviewer $@
