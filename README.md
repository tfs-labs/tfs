![title](./wiki/img/title.png)
The special architecture design of TFSC leads to the potential competition of node performance in the process of the continuous increase of TPS demand of the main network,The hardware performance of added nodes improves,The TPS of TFSC main network will also be continuously improved

## Installation

### The minimum recommended hardware specification for nodes connected to Mainnet is:

```
cpu  :Intel CPU series or higher with 8 or more cores are recommended.
RAM	 :At least 16 GB.
Disk :Disk A high-performance solid state drive with at least 1 terabyte of available space.
System : CentOS(Community Enterprise Operating System) 7.
Net:25+ MBit/ s to download Internet service.
```

### Required package

```
gcc (v9.3)
cmake(v3.21)
git
unzip
zip
autoconf
automake
libtool
perl-IPC-Cmd

Dependent library:
zlib
zlib-devel
command:
yum install -y zlib zlib-devel  
yum install -y unzip zip  
yum install -y autoconf automake libtool
yum -y install perl-IPC-Cmd
yum install -y devtoolset-9-toolchain
scl enable devtoolset-9 bash 
scl enable devtoolset-9 bash  (Is used to set the gcc toolchain to devtoolset-9)

```

CMake(Note: The CMake version must be at least 3.15.x or later, and 3.21.x is recommended)

```plaintext
curl -O https://cmake.org/files/v3.21/cmake-3.21.4.tar.gz
tar zxvf cmake-3.21.4.tar.gz
cd cmake-3.21.4
./configure 
gmake && make install
```

Verify the time of the CentOS server(Ensure that the time of each node is the same, and ensure that the transaction on the chain may be reliable)

```plaintext
1.Install NTP
sudo yum -y install ntp

2.Test NTP using ntpdate
ntpdate pool.ntp.org

3.Viewing server time
date

4.Start ntpd daemon and keep calibrating time
systemctl start ntpd

5.Check whether the ntpd daemon is started
systemctl status ntpd
```

### Compiled binary

Obtain source code

```plaintext
git clone https://github.com/TFSC-Transformers/tfs.git
```

### Compile and install test net debug version

```
mkdir build_test_debug && cd build_test_debug
cmake .. -DTESTCHAIN=ON
make
```

### Run 

```
cd build_dev_debug/bin
./tfs_xx_xx_devnet
```

### The official website link and the official Twitter link are as follows:

[TFSC](https://www.tfsc.io/)

[Twitter](https://mobile.twitter.com/TFSCChain)
