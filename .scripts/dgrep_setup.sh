#!/bin/bash
if [ "$#" -ne 1 ]; then
  echo "dgrep_setup.sh <port-for-remote-servers>"
  exit 1
fi

for i in `seq -w 1 10`; do
  HOST='fa19-cs425-g60-'${i}'.cs.illinois.edu'
  ssh -f -t lawsonp2@fa19-cs425-g60-${i} "dgrep-server $1 & disown"
done

