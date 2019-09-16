# cs425-mps
MPs for cs425

Each server needs to clone the repo, move / append authorized_keys, and add the .scripts folder to path and create the machine.i.log file - any VM should then be able to run the server setup script dgrep_setup to ensure all other VMs are running dgrep-server, and any VM should then be able to run dgrep (dgrep.sh) to request a grep of the log file on all other VMs.

Each node in a cluster must clone this repository and run the setup script. Scripts in the .scripts directory are updated by makefiles run in any of the mp directories. The scripts and executables can be run the see what the format of the arguments are. For dgrep.sh, running the script with a port number and query string will query all VMs and print the results as well as per-node and total line counts. Running dgrep_setup.sh with a port number will ensure all 10 VMs are running the server code on that particular port.
