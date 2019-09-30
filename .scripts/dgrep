#!/bin/bash
if [ "$#" -ne 2 ]; then
  echo "usage: dgrep <port> 'query-string'"
  exit
fi

total_wc=0
for i in `seq -w 1 10`; do
  dgrep-client fa19-cs425-g60-${i} $1 $2 > tmp.txt
  wc_l=$(cat tmp.txt | wc -l)
  total_wc=$(($total_wc + $wc_l))
  echo "============================"
  echo "MACHINE $i FOUND $wc_l LINES"
  echo "============================"
  (cat tmp.txt ; echo) && rm tmp.txt
done
echo "============================"
echo "TOTAL LINES FOUND: $total_wc"
echo "============================"
