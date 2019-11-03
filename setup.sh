#!/bin/bash

# running setup.sh will set up any VM to run all programs made for MPs
# this entails adding the authorized keys to .ssh
# adding the scripts directory to path
# and creating machine.i.log if it doesn't already exist

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" > /dev/null 2>&1 && pwd )"
SCRIPTS_SRC=$DIR"/.scripts"
SCRIPTS_DEST=$HOME"/.scripts"

# make .ssh dir if not already, then cp authorized_keys
[ -d ~/.ssh ] || mkdir ~/.ssh
cp authorized_keys ~/.ssh

# make .files dir if not already, then cp authorized_keys
[ -d ~/.sdfs ] || mkdir ~/.sdfs

# create machine.i.log
echo -n '' >> ~/machine.i.log

# copy .scripts and add to path
[ -d $SCRIPTS_DEST ] || mkdir $SCRIPTS_DEST
cp -r $SCRIPTS_SRC/* $SCRIPTS_DEST
if [[ ":$PATH:" != *":$SCRIPTS_DEST:"* ]]; then
  echo export PATH=\"$SCRIPTS_DEST:\$PATH\" >> ~/.bashrc
fi
