# OpenCV Focuser

## Prerequisites

```sh
sudo apt update
sudo apt install build-essential cmake gdb
sudo apt install libopencv-dev # OpenCV
sudo apt install libcfitsio-dev # CFITSIO
sudo apt install libindi-dev # INDI
```

## Installing INDI

```sh
sudo apt-add-repository ppa:mutlaqja/ppa
sudo apt update
sudo apt install indi-full indi-asi
```

## Building

```sh
./build.sh -n10 -a3 -e1 -s500 2>/dev/null
```

## Starting INDI server

```sh
indiserver -v indi_andr_focuser indi_asi_ccd indi_v4l2_ccd
```
