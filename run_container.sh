#!/bin/bash

export MEDOOZE_HOST=134.59.133.76
export MEDOOZE_PORT=8084
export MONITOR_HOST=134.59.133.57
export MONITOR_PORT=9000

# docker run -d --rm  -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7000-7100:7000-7100 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7000 7100

# docker run -it --rm -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7200-7300:7200-7300 -v $PWD/tc_scripts/lossdelay.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7200 7300

# docker run -it --rm -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7400-7500:7400-7500 --cap-add=NET_ADMIN sharkalash/vm-viewer 7400 7500

docker run -it --rm  -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7000-7100:7000-7100 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN --entrypoint /bin/bash sharkalash/vm-viewer # 7000 7100

# docker run -it --rm -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7400-7500:7400-7500 --cap-add=NET_ADMIN sharkalash/vm-viewer 7400 7500

# docker run -d --rm  -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7200-7300:7200-7300 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7200 7300

# docker run -d --rm  -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7400-7500:7400-7500 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7400 7500

# docker run -d --rm  -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7600-7700:7600-7700 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7600 7700

# docker run -d --rm  -e MEDOOZE_HOST=$MEDOOZE_HOST -e MEDOOZE_PORT=$MEDOOZE_PORT -e MONITOR_HOST=$MONITOR_HOST -e MONITOR_PORT=$MONITOR_PORT -p 7800-7900:7800-7900 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7800 7900
