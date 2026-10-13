# SPDX-FileCopyrightText: 2026 Dirk Wahrheit
# SPDX-License-Identifier: Apache-2.0
#
# espos-nmea-gateway: a pure ESP-IDF 6 project. WiFi, config, the web UI,
# SignalK discovery, OTA and the NMEA 2000 bus itself come from the espOS
# submodule (espos/); this project picks the components, the CAN pins and the
# few sdkconfig lines that differ from espOS's.
#
# Build:  . <IDF v6.0.3>/export.sh
#         scripts/build.sh -B build-esp32c5 -DSDKCONFIG=build-esp32c5/sdkconfig -DIDF_TARGET=esp32c5 build
cmake_minimum_required(VERSION 3.22)

# espOS's bundled tables, by chip. The ESP32-C5 and ESP32-P4 boards this is
# released for carry 16 MB of flash, and the 16 MB table gives their images
# 6.5 MB slots instead of 1.7 MB (the C5 image is ~1.45 MB, 85% of a 1.7 MB
# slot). The ESP32, S3, C3 and C6 dev kits commonly carry 4 MB, and a table
# larger than the chip's flash fails to flash, so those stay on 4 MB. A device
# keeps the table it was first flashed with (OTA replaces app slots, never the
# table), so a change here reaches only boards flashed over USB afterwards.
include("${CMAKE_CURRENT_LIST_DIR}/espos/cmake/espos_project.cmake")
set(_partitions "${ESPOS_PARTITIONS_DIR}/4mb.csv")
if(IDF_TARGET STREQUAL "esp32c5" OR IDF_TARGET STREQUAL "esp32p4")
    set(_partitions "${ESPOS_PARTITIONS_DIR}/16mb.csv")
endif()
espos_project_prologue(NAME "nmea-gateway" PARTITIONS "${_partitions}"
                       COMPONENTS espos_n2k espos_eth)

project(nmea_gateway)

# The committed espOS web UI (espos/ui/dist-gz) -> LittleFS "storage" image,
# flashed with `idf.py flash`. No Node step anywhere.
espos_project_ui_partition(NAME "nmea-gateway")
