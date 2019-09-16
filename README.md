# cs425-mps
MPs for cs425

Each server needs to clone the repo, move / append authorized_keys, and add the .scripts folder to path and create the machine.i.log file - any VM should then be able to run the server setup script dgrep_setup to ensure all other VMs are running dgrep-server, and any VM should then be able to run dgrep (dgrep.sh) to request a grep of the log file on all other VMs.
