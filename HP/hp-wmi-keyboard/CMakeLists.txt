# HP/hp-wmi-keyboard: a Solwind plugin. solwind plugin install HP/hp-wmi-keyboard builds and installs it; by hand, against an installed Solwind:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build   (-> build/plugins/HP/hp-wmi-keyboard.so)
cmake_minimum_required(VERSION 3.21)
project(solwind-plugin-hp-hp-wmi-keyboard LANGUAGES CXX)
# (inside Solwind's own build, -DSOLWIND_PLUGINS_DIR, the SDK is there already)
if(NOT TARGET Solwind::sdk)
    find_package(SolwindSdk REQUIRED)
endif()
solwind_plugin(HP/hp-wmi-keyboard SOURCES HpWmiKeyboard.h HpWmiKeyboard.cpp)
