#!/bin/bash
if [ "$#" -ne 2 ]; then
  echo "usage: dgrep <port> 'query-string'"
  exit
fi

for i in `seq -w 1 10`; do
  dgrep-client fa19-cs425-g60-${i} $1 $2
done
