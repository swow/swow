#!/bin/sh

if [ "$(uname | grep -i darwin)"x != ""x ]; then
  cpu_count="$(sysctl -n machdep.cpu.core_count)"
else
  cpu_count="$(nproc)"
fi
if [ -z ${cpu_count} ]; then
  cpu_count=4
fi

echo "${cpu_count}"
