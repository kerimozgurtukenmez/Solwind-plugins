# NBFC/nbfc: a Solwind plugin. solwind plugin install NBFC/nbfc builds and installs it; by hand, against an installed Solwind:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build   (-> build/plugins/NBFC/nbfc.so)
cmake_minimum_required(VERSION 3.21)
project(solwind-plugin-nbfc-nbfc LANGUAGES CXX)
# (inside Solwind's own build, -DSOLWIND_PLUGINS_DIR, the SDK is there already)
if(NOT TARGET Solwind::sdk)
    find_package(SolwindSdk REQUIRED)
endif()
solwind_plugin(NBFC/nbfc SOURCES NbfcMonitor.h NbfcMonitor.cpp NbfcParse.h NbfcParse.cpp NbfcPlugin.h NbfcPlugin.cpp)
