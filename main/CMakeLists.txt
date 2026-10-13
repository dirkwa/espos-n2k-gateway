# SPDX-FileCopyrightText: 2026 Dirk Wahrheit
# SPDX-License-Identifier: Apache-2.0
#
# espos_core is the boot, espos_n2k the bus and the candump server, espos_config
# the CAN pin settings. The other two are listed for the build, not for
# main.cpp: espOS consumers build with IDF's MINIMAL_BUILD, so a component that
# nothing requires is not compiled, and espos_start() starts only what is in
# the build. Without espos_ota the gateway could never be updated in the field;
# without espos_eth a P4 on PoE would ignore its cable. On a target with no
# EMAC espos_eth compiles to stubs and the station is the network.
idf_component_register(
    SRCS main.cpp
    PRIV_REQUIRES espos_core espos_config espos_n2k espos_eth espos_ota
                  esp_driver_gpio esp_hw_support)

# The CAN settings page (namespace "can"). After idf_component_register.
espos_config_add_descriptor(config/can.json)
