#!/bin/bash

R=$1

netunset() {
    if sudo tc qdisc list dev ens5 | grep -q netem; then
        sudo tc qdisc del dev ens5 root; 
    fi
}

alias netctrl1="sudo tc qdisc add dev ens5 root netem delay 800us rate 3gbit"
alias netctrl2="sudo tc qdisc add dev ens5 root netem delay 800us rate 1gbit"
alias netctrl3="sudo tc qdisc add dev ens5 root netem delay 40ms rate 200mbit"
alias netctrl4="sudo tc qdisc add dev ens5 root netem delay 80ms rate 100mbit"

cmake -S . -B build -DLUT_INPUT_SIZE=24 -DLUT_OUTPUT_SIZE=24
cmake --build ./build --target all --parallel

for s in {1..4..3}
do
    netunset
    ${BASH_ALIASES[netctrl$s]}
    for logthr in {0..5}
    do
        ./build/bin/batchlut 172.31.19.77 l=1 s=4096 r=$R h=0 par=1 thr=$((2**$logthr))
    done
done

# ./build/bin/batchlut 172.31.19.77 l=1 s=4096 h=0 par=1 r=$R
# ./build/bin/batchlut 127.0.0.1 l=1 s=4096 h=0 par=1 r=$R

# ./build/bin/splut l=20 t=20 r=1 s=4096
# ./build/bin/splut l=20 t=20 r=2 s=4096
# ./build/bin/splut ip=172.31.19.77 l=22 t=1 s=256 r=1
# ./build/bin/splut ip=172.31.19.77 l=22 t=1 s=256 r=2
# ./build/bin/splut ip=172.31.19.77 l=$size t=64 s=4096 r=$R