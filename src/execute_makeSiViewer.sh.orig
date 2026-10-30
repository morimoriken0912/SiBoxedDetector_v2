cd ../build/
make -j$(nproc)
./exampleB1 run.mac 10000
cd ../src/
g++ -O2 makeSiViewer_all.cc -o makeSiViewer_all $(root-config --cflags --libs)
./makeSiViewer_all ../build/SiDetector.root SiViewer_204RnAll.html
explorer.exe SiViewer_204RnAll.html
