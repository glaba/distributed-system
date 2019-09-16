# cs425-mps
MPs for cs425

Each node in a cluster must clone this repository and run the setup script. Scripts in the .scripts directory are updated by makefiles run in any of the mp directories. The scripts and executables can be run the see what the format of the arguments are.

## MP1
For dgrep.sh, running the script with a port number and query string will query all VMs and print the results as well as per-node and total line counts. Running dgrep_setup.sh with a port number will ensure all 10 VMs are running the server code on that particular port.
Tests for MP1 are in the form of the python script in the mp1 directory, and running the python script will enumerate the arguments that the script requires.
