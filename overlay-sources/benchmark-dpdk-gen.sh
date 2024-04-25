#!/bin/bash

total_num=100

count_send_0=30
count_send_1=20
count_receive_0=30
count_receive_1=20

random() {
  echo $(($RANDOM % $1))
}

declare -a commands

for i in $(seq 1 $count_send_0); do
  random_num=$(random $total_num)
  commands[$random_num]="send_to_domain 0 5912 214 9124124 94 2014 1294 1204 29 25 34455 1249 2763 23131 321311"
done

for i in $(seq 1 $count_send_1); do
  random_num=$(random $total_num)
  commands[$random_num]="send_to_domain 1 592 5012 59 10 27 249 2419 20 22332 3244 134 22356 1888920 154665"
done

for i in $(seq 1 $count_receive_0); do
  random_num=$(random $total_num)
  commands[$random_num]="receive_from_domain 0"
done

for i in $(seq 1 $count_receive_1); do
  random_num=$(random $total_num)
  commands[$random_num]="receive_from_domain 1"
done

for command in "${commands[@]}"; do
  if [[ -n $command ]]; then
    echo "$command"
  fi
done

echo "quit"
