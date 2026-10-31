# MSI/msi-ec-keyboard: a Solwind plugin. solwind plugin install MSI/msi-ec-keyboard builds and installs it; by hand, against an installed Solwind:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build   (-> build/plugins/MSI/msi-ec-keyboard.so)
cmake_minimum_required(VERSION 3.21)
project(solwind-plugin-msi-msi-ec-keyboard LANGUAGES CXX)
# (inside Solwind's own build, -DSOLWIND_PLUGINS_DIR, the SDK is there already)
if(NOT TARGET Solwind::sdk)
    find_package(SolwindSdk REQUIRED)
endif()
solwind_plugin(MSI/msi-ec-keyboard SOURCES MsiEcKeyboard.h MsiEcKeyboard.cpp)
