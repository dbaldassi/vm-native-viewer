
echo "--- set up ifb ---"
ip link add ifb0 type ifb
ip link set dev ifb0 up

echo "--- mirror ingress traffic ---"
tc qdisc add dev eth0 handle ffff: ingress
tc filter add dev eth0 parent ffff: protocol ip u32 match u32 0 0 action mirred egress redirect dev ifb0

echo "--- setup network constraints ---"
tc qdisc add dev ifb0 root handle 1: netem delay 200ms loss 1%
# tc qdisc add dev ifb0 root handle &: tbf rate 1mbit burst 20KB latency 400ms
