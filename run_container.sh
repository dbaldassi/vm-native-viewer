!/bin/bash

docker run -d --rm -p 7000-7100:7000-7100 -v $PWD/tc_scripts/bitrate.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7000 7100

docker run -it --rm -p 7200-7300:7200-7300 -v $PWD/tc_scripts/lossdelay.sh:/root/vm-native-viewer/build/setup_tc.sh --cap-add=NET_ADMIN sharkalash/vm-viewer 7200 7300

docker run -it --rm -p 7400-7500:7400-7500 --cap-add=NET_ADMIN sharkalash/vm-viewer 7400 7500
