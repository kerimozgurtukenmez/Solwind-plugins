# HP/hp-wmi-fan: a Solwind plugin. solwind plugin install HP/hp-wmi-fan builds and installs it; by hand, against an installed Solwind:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build   (-> build/plugins/HP/hp-wmi-fan.so)
cmake_minimum_required(VERSION 3.21)
project(solwind-plugin-hp-hp-wmi-fan LANGUAGES CXX)
# (inside Solwind's own build, -DSOLWIND_PLUGINS_DIR, the SDK is there already)
if(NOT TARGET Solwind::sdk)
    find_package(SolwindSdk REQUIRED)
endif()
solwind_plugin(HP/hp-wmi-fan SOURCES HpWmiFan.h HpWmiFan.cpp)
