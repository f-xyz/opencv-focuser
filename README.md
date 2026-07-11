# Prerequisites 
```sh
sudo apt update
sudo apt install build-essential cmake gdb
sudo apt install libfmt-dev nlohmann-json3-dev libopencv-dev
```

# Building
```sh
./build.sh
```

# VCPKG
```sh
vcpkg new --application
vcpkg add port fmt
vcpkg add port nlohmann-json
vcpkg add port opencv4
```