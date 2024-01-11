#!/bin/sh

set -e

echo "run miniweb at background"
./miniweb &
echo "sleep 1 second"
sleep 1
echo "index.html"
busybox wget -O - http://localhost:8888/index.html
echo "magic.html"
busybox wget -O - http://localhost:8888/magic.html
echo "register"
busybox wget --post-data "name=Alex&email=alex@email.com" -O - http://localhost:8888/cgi/register
