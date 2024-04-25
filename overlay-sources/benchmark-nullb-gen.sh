#!/bin/bash

total_num=100

count_write_a=30
count_write_b=10
count_write_c=20
count_read=40

random() {
  echo $(($RANDOM % $1))
}

declare -a commands

for i in $(seq 1 $count_write_a); do
  random_num=$(random $total_num)
  commands[$random_num]="echo 'this_is_a_normal_test' | dd of=/dev/nullb0 bs=1024 count=10"
done

for i in $(seq 1 $count_write_b); do
  random_num=$(random $total_num)
  commands[$random_num]="echo 'this_is_a_long_long_long_long_long_long_long_test' | dd of=/dev/nullb0 bs=1024 count=10"
done

for i in $(seq 1 $count_write_c); do
  random_num=$(random $total_num)
  commands[$random_num]="echo 'test' | dd of=/dev/nullb0 bs=1024 count=10"
done

for i in $(seq 1 $count_read); do
  random_num=$(random $total_num)
  commands[$random_num]="dd if=/dev/nullb0 bs=1024 count=10"
done

echo "#!/bin/sh"
echo ""
echo "set -x"
echo ""

for command in "${commands[@]}"; do
  if [[ -n $command ]]; then
    echo "$command"
  fi
done
