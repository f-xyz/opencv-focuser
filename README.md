# Prerequisites 
```sh
sudo apt update
sudo apt install build-essential cmake gdb
sudo apt install libopencv-dev # OpenCV
sudo apt install libcfitsio-devlibcfitsio-dev # CFITSIO
sudo apt install libindi-dev # INDI
sudo apt install nlohmann-json3-dev
```

# Installing INDI
```sh
sudo apt-add-repository ppa:mutlaqja/ppa
sudo apt update
sudo apt install indi-full indi-asi
```

# Building
```sh
./build.sh
```

# Starting INDI server
```sh
indiserver -v indi_asi_ccd indi_v4l2_ccd indi_andr_focuser
```

# Log
```
OpenCV Focuser
INDI::BaseClient::connectServer: creating new connection...
* Device: Celestron Advanced VX HC
* Device: AndrFocuser
* Device: Manual Filter
* Camera: ZWO CCD ASI294MC Pro
* Property: ZWO CCD ASI294MC Pro / CCD_INFO / 4144x2822
Done!
* Camera: ZWO CCD ASI120MC-S
* Property: ZWO CCD ASI120MC-S / CCD_INFO / 1280x960
Done!
Really done!
Really done!

```