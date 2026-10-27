# HP/hp-wmi-rgb-zones: a Solwind plugin. solwind plugin install HP/hp-wmi-rgb-zones builds and installs it; by hand, against an installed Solwind:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build   (-> build/plugins/HP/hp-wmi-rgb-zones.so)
cmake_minimum_required(VERSION 3.21)
project(solwind-plugin-hp-hp-wmi-rgb-zones LANGUAGES CXX)
# (inside Solwind's own build, -DSOLWIND_PLUGINS_DIR, the SDK is there already)
if(NOT TARGET Solwind::sdk)
    find_package(SolwindSdk REQUIRED)
endif()
solwind_plugin(HP/hp-wmi-rgb-zones SOURCES HpWmiRgbZones.h HpWmiRgbZones.cpp)
