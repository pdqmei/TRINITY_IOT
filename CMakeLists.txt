cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS

    # Add actuator components here
    components/actuators/fan
    components/actuators/led
    components/actuators/buzzer

    # Add sensor components here
    components/sensors/sht31
    components/sensors/mq135

    # Add connectivity components here
    components/connectivity/wifi_manager
    components/connectivity/mqtt_handler

    # Add utility components here
    components/utils/moving_average
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(TRINITY_IOT)
