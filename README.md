# esp32_lock

Emulator network controller with lock_device (for testing external control server of parking)

esp32(DevKitC board) + st7789(spi) + leds for indication of events


# Файлы пакета:

* sdkconfing	- файл конфигурации проекта

* Makefile	- make файл (файл сценария компиляции проекта)

* version	- файл версии ПО

* README.md	- файл справки

* main/		- папка исходников


Требуемые компоненты:
```
- Cross compiler xtensa-esp32-elf (http://esp-idf-fork.readthedocs.io/en/stable/linux-setup.html#step-0-prerequisites)
- SDK esp-idf (https://github.com/espressif/esp-idf)
- Python2 (https://www.python.org/)
```

# Компиляция и загрузка

make menuconfig - конфигурация проекта

make app	- компиляция проекта

