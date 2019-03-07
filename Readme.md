PlayMusic

Supported filetypes:
.mp3 which is played by mpg123.  Only mp3 type streams can be played
Required Libraries -> ssl, DLiriumLib, crypto, mysqlclient

install
sudo apt-get install mpg321
sudo apt-get install openssl libssl-dev
sudo apt-get install mysql-server
sudo apt-get install mysql-client
sudo apt-get install libmysqlclient-dev


to start and stop from the run levels place musicServer.sh into /etc/init.d directory and run this command. The application will run automatically at bootup
sudo update-rc.d musicServer.sh defaults 99
to remove musicServer.sh run this command
sudo update-rc.d -f musicServer.sh remove


GCC C++ Compiler
All Options:
-I/usr/local/ssl/include -I"/home/jdellaria/workspace/DLiriumLib" -O3 -Wall -c -fmessage-length=0
Includes:
/usr/local/ssl/include
"${workspace_loc:/DLiriumLib}"

GCC C Compiler
All Options:
-O3 -Wall -c -fmessage-length=0
Includes:

GCC C++ Linker
All Options:
-L/usr/lib/mysql -L"/home/jdellaria/workspace/DLiriumLib/Release" -L/usr/local/ssl/lib
Libraries:
ssl
DLiriumLib
crypto
mysqlclient
Library search path:
/usr/lib/mysql
"${workspace_loc:/DLiriumLib/Release}"
/usr/local/ssl/lib


USEFUL GIT COMMANDS git diff @{upstream}

git init

git add NAME_OF_FILE


git add .     -> to add all new files and changes git status

git commit -a -m "First Commit"

git remote add origin https://github.com/jdellaria/PlayMusic.git

git push -u origin master


If mounting a USB drive on Raspberry Pi perform the folowing
sudo mkdir /RAID
sudo chmod -R 777 /RAID
sudo fdisk -l    -> this will list the devices
sudo ls -l /dev/disk/by-uuid  -> this will give you the uuid for making perminate changes in the etc/fstab file. look for sdaX

make changes in the /etc/fstab file to make changes for auto mounting

Add to /etc/fstab

/dev/disk/by-uuid/05b9950a-778e-4bc8-ad62-2e3ade8aa5bf  /RAID ext4  defaults,noatime  0 0

sudo mkdir /RAID 

sudo chmod -R 777 /RAID

sudo mount -a 


Install DLirium Lib

Install PlayMusic

Install Alsa 

  sudo apt-get install libasound2-dev

Install Mpg123

Install Boost


to bring up/down network conections:
sudo ifconfig wlan0 up/down

to set static IP address change each interface in /etc/dhcpcd.conf to:
   ***
interface eth0

static ip_address=192.168.19.5

static routers=192.168.19.1

static domain_name_servers=208.67.222.222 208.67.220.220
   ***

To start the application automaicially from boot.
Make sure musicServer.sh is working by typing: 
 ./musicServer.sh start -> music should start playing
 
 -->copy musicServer.sh to /etc/init.d
 
 sudo cp musicServer.sh /etc/init.d
 
 cd /etc/rc3.d
 
 sudo ln -s /etc/init.d/musicServer.sh S99musicServer.sh 
 
 cd /etc/rc5.d
 
 sudo ln -s /etc/init.d/musicServer.sh S99musicServer.sh 
