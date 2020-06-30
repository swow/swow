#!/bin/bash

echo "=== raw php ==="
strace /usr/bin/env php -n -r "usleep(1000);" 2>&1 | grep nanosleep;
echo "=== enable swow ==="
strace /usr/bin/env php -n -dextension=swow -r "usleep(1000);" 2>&1 | grep epoll;
