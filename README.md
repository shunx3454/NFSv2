# NFSv2
## Describe
The NFSv2 protocol is no longer supported on Ubuntu 22/24. To solve the problem of downloading files using the NFS protocol in older versions of u-boot, we manually implement some of the NFSv2 functions using C/C++.

## Build:
`mkdir build;cd build;make`

## Usage:
if have nfs-kernel-server: \
`audo apt remove nfs-kernel-server`

if running 'rpcbind': \
`sudo systemctl stop rpcbind` \
`sudo systemctl disable rpcbind` \
`reboot`

first run this NFSv2 server \
`./nfsv2 /path/dir`

then use NFS commands in u-boot to download files \
`nfs 80800000 192.168.xx.xx:/path/dir/file`

make sure the port 111 && 2049 empty
OK!

