 source pitch_change.sh
 g++ -O2 -std=c++17 makeSiViewer_all.cc -o makeSiViewer_all $(root-config --cflags --libs)
 ./makeSiViewer_all ../build/pitch_changed/SiDetector_*.root SiViewer_204Rn_All.html
 explorer.exe SiViewer_204Rn_All.html
