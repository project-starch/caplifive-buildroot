#!/bin/bash

total_num=100

count_index_html=20
count_null_html=10
count_register_html=20
count_register_succ_a=20
count_register_succ_b=20
count_register_fail=10

random() {
  echo $(($RANDOM % $1))
}

declare -a commands

for i in $(seq 1 $count_index_html); do
  random_num=$(random $total_num)
  commands[$random_num]="busybox wget -O - http://localhost:8888/index.html"
done

for i in $(seq 1 $count_null_html); do
  random_num=$(random $total_num)
  commands[$random_num]="busybox wget -O - http://localhost:8888/null.html"
done

for i in $(seq 1 $count_register_html); do
  random_num=$(random $total_num)
  commands[$random_num]="busybox wget -O - http://localhost:8888/register.html"
done

for i in $(seq 1 $count_register_succ_a); do
  random_num=$(random $total_num)
  commands[$random_num]="busybox wget --post-data 'name=Alex&email=alex@email.com' -O - http://localhost:8888/cgi/cgi_register_success.dom"
done

for i in $(seq 1 $count_register_succ_b); do
  random_num=$(random $total_num)
  commands[$random_num]="busybox wget --post-data 'name=Bob&email=bob@email.com' -O - http://localhost:8888/cgi/cgi_register_success.dom"
done

for i in $(seq 1 $count_register_fail); do
  random_num=$(random $total_num)
  commands[$random_num]="busybox wget --post-data 'name=Alex&email=alex@email.com' -O - http://localhost:8888/cgi/cgi_register_fail.dom"
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
