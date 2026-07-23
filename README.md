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
indiserver -v indi_andr_focuser indi_asi_ccd indi_v4l2_ccd
indiserver -v indi_andr_focuser indi_asi_ccd indi_v4l2_ccd 2>&1 indi.log
```

# Sample data

Focus / sharpness:
  *      0: 0.07976023812586236
  *    500: 0.08211323039925811
  *   1000: 0.09475903884947931
  *   1500: 0.1054983824250897
  *   2000: 0.11340603184768241
  *   2500: 0.11541614653152252
  *   3000: 0.11337531248443268
  *   3500: 0.10773520211663194
  *   4000: 0.0994022731854923
  *   4500: 0.08853414403454851

Focus / sharpness:
  *      0: 0.08516493069328064
  *   -500: 0.08935311790965729
  *  -1000: 0.0989919994371464
  *  -1500: 0.10852248119370853
  *  -2000: 0.11292242643028011
  *  -2500: 0.11431311359547124
  *  -3000: 0.11111537894446195
  *  -3500: 0.10408137934155348
  *  -4000: 0.09397886474838678
  *  -4500: 0.0815563077060534

Focus / sharpness:
  *      0: 0.08108181884793955
  *    500: 0.08345691747123754
  *   1000: 0.09538617472150529
  *   1500: 0.10563130981918875
  *   2000: 0.11204928086345903
  *   2500: 0.11448166394176001
  *   3000: 0.11288111592285663
  *   3500: 0.10752798384020804
  *   4000: 0.09917710810701537
  *   4500: 0.08832662646482994