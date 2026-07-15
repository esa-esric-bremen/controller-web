rm -Rf build
mkdir build
cd build
cmake  ..
make -j64
mv ./controller_web ..
