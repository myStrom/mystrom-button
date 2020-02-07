# myStrom WiFi Button

This repository contains opensource version of the firmware for myStrom WiFi Buttons. Both Plus and Simple buttons are supported.

### How to build the FW from source code

- Install vagrant

- Use git to clone the button-open-source project

  ```bash
  git clone <REPO>
  cd <REPO>
  ```

- Build project

  ```bash
  vagrant up
  vagrant ssh -c '/vagrant/build.sh <simple|plus>'
  ```
  
  where simple is build for button light and plus is build for button plus

After successfully build binary files can be found in `image` path

- `<Simple|Plus>-FLASH-FW-$VERSION.bin` is a full flash image
- `<Simple|Plus>-FW-$VERSION-develop.bin` is a development build - print many logs on console
- `<Simple|Plus>-FW-$VERSION-release.bin` is a release version without printing logs on console

Once the binary FW is available it can be flashed into the device in one of these 2 ways:

1. OTA firmware upgrade function available in the myStrom Button standard firmware
2. By connecting UART to the programming port of the myStrom Button



#### How to flash the firmware into the device using OTA upgrade

To upgrade firmware currently flash in button you can use curl lie follow

```bash
curl -F file=@<Simple|Plus>-FW-$VERSION-release.bin http://<button IP>/load
```

In order for this method to work the  button must be in service mode or AP mode. 

The off-the shelf myStrom buttons will not accept OTA upgrade to FW that is not released (signed) by myStrom. Therefore in the rom directory there are 2 binary firmwares Plus-FW-2.74.12-release.bin and Simple-FW-2.74.12-release.bin that are signed by myStrom which no loger check myStrom signature during the following OTA firmware upgrade.



#### How to flash the firmware into the device using UART

There are several tools available that allow flashing new firmware image to ESP8266. In these insrtructions it is assumed that esptool.py is used. Different tools may require using other options.

To initial flash you can use esptool.py like follow

```bash
esptool.py --chip esp8266 --port <where usb serial adapter is connected> --baud <460800> write_flash --flash_freq keep --flash_mode keep --flash_size 2MB 0x0 <Simple|Plus>-FLASH-FW-$VERSION.bin
```


#### UART Pinout
<img src="/doc/uart_pinout.jpg" width=350/>


#### Baudrates

Device outputs debug messages to the console port, which can be read by connecting UART. The following boudrates are used for outputting debug messages:

- 460800 for develop firmware
- 74880 for release firmware



#### HTTP REST API

The device offers HTTP API, description of that API is available at `http://<button_ip>/help`
