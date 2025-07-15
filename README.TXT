if have nfs-kernel-server:
	audo apt remove nfs-kernel-server

if running 'rpcbind':
	sudo systemctl stop rpcbind
	sudo systemctl disable rpcbind
	reboot

make sure the port 111 && 2049 empty
OK!

