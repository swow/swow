#!/bin/bash

if [ "$(uname | grep -i darwin)"x != ""x ]; then
  cpu_count="$(sysctl -n machdep.cpu.core_count)"
else
  cpu_count="$(nproc)"
fi

echo "${cpu_count-4}"
