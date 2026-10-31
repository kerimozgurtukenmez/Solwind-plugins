# OpenRGB/openrgb: a Solwind plugin. solwind plugin install OpenRGB/openrgb builds and installs it; by hand, against an installed Solwind:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build   (-> build/plugins/OpenRGB/openrgb.so)
cmake_minimum_required(VERSION 3.21)
project(solwind-plugin-openrgb-openrgb LANGUAGES CXX)
# (inside Solwind's own build, -DSOLWIND_PLUGINS_DIR, the SDK is there already)
if(NOT TARGET Solwind::sdk)
    find_package(SolwindSdk REQUIRED)
endif()
solwind_plugin(OpenRGB/openrgb SOURCES OrgbClient.h OrgbClient.cpp OpenRgbKeyboard.h OpenRgbKeyboard.cpp)
