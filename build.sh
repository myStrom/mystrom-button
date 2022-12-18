#! /bin/bash

set -xe

[ $# -eq 1 ] || {
	echo 'Wrong args count'
	echo "Use $0 simple|plus"
	exit 1
}

[ "$1" == 'simple' -o "$1" == 'plus' ] || {
	echo "Wrong build type: $1"
	echo "Use $0 simple|plus"
	exit 1
}

SDK_MAJOR=2
SDK_MINOR=1
SDK_BUILD=0

SDK_VERSION="$SDK_MAJOR.$SDK_MINOR.$SDK_BUILD"

rm -f /vagrant/*.bin
cd 

[ -d "crosstool-NG" ] || {
	git clone --recursive -b xtensa-1.22.x 'https://github.com/frostworx/crosstool-NG.git'
	cd 'crosstool-NG'
	./bootstrap
	./configure --enable-local
	make
	sudo make install
	./ct-ng xtensa-lx106-elf
	./ct-ng build
	chmod -R u+w builds/xtensa-lx106-elf
	cd
}

[ -d "ESP8266_NONOS_SDK-$SDK_VERSION" ] || {
	wget -q "https://github.com/espressif/ESP8266_NONOS_SDK/archive/v$SDK_VERSION.tar.gz"
	tar -xf "v$SDK_VERSION.tar.gz"
	rm "v$SDK_VERSION.tar.gz"
	mkdir -p "ESP8266_NONOS_SDK-$SDK_VERSION/button"
}

[ -d 'esp_microc' ] || {
	git clone --recursive 'https://github.com/anakod/esp_microc.git'
	cp 'esp_microc/build/libmicroc.a' "ESP8266_NONOS_SDK-$SDK_VERSION/lib/."
}

export PATH="$PATH:$HOME/crosstool-NG/builds/xtensa-lx106-elf/bin"

cd "ESP8266_NONOS_SDK-$SDK_VERSION"
mount | grep "/home/vagrant/ESP8266_NONOS_SDK-$SDK_VERSION/button" || {
	sudo mount --bind /vagrant/ button
}

cd 'button'

VERSION=$(./version.sh include/version.h)

make clean
make "$1"
make clean
make DEBUG=1 "$1"

[ -d '/vagrant/image' ] || {
	mkdir "/vagrant/image"
}

cd '../bin'
if [ "$1" == 'simple' ]
then
	mv "upgrade/Simple-FW-$VERSION-release.bin" '/vagrant/image/.'
	mv "upgrade/Simple-FW-$VERSION-develop.bin" '/vagrant/image/.'
	cd '../button/rom'
	./button_rom.sh "/vagrant/image/Simple-FW-$VERSION-release.bin" "/vagrant/image/Simple-FLASH-FW-$VERSION.bin"
else
	mv "upgrade/Plus-FW-$VERSION-release.bin" '/vagrant/image/.'
	mv "upgrade/Plus-FW-$VERSION-develop.bin" '/vagrant/image/.'
	cd '../button/rom'
	./button_rom.sh "/vagrant/image/Plus-FW-$VERSION-release.bin" "/vagrant/image/Plus-FLASH-FW-$VERSION.bin"
fi

echo 'CONGRATULATION! BUTID SUCCESSFULLY'
