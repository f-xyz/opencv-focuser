# Prerequisites 
```sh
sudo apt update
sudo apt install build-essential cmake gdb
sudo apt install libopencv-dev # OpenCV
sudo apt install libcfitsio-devlibcfitsio-dev # CFITSIO
sudo apt install libindi-dev # INDI
sudo apt install libfmt-dev nlohmann-json3-dev
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
indiserver -v indi_simulator_ccd indi_andr_focuser
```