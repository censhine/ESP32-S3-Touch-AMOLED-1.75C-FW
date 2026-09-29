cmake_minimum_required(VERSION 3.20)
project(round_wing LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_library(roundwing_core src/game.cpp)
target_include_directories(roundwing_core PUBLIC include)
target_compile_options(roundwing_core PRIVATE -Wall -Wextra -Werror)
add_executable(game_tests tests/game_tests.cpp)
target_link_libraries(game_tests PRIVATE roundwing_core)
add_test(NAME game_core COMMAND game_tests)
add_executable(power_button_tests tests/power_button_tests.cpp)
target_include_directories(power_button_tests PRIVATE firmware/main)
target_compile_options(power_button_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME power_button COMMAND power_button_tests)
add_executable(app_idle_policy_tests tests/app_idle_policy_tests.cpp)
target_include_directories(app_idle_policy_tests PRIVATE firmware/main)
target_compile_options(app_idle_policy_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME app_idle_policy COMMAND app_idle_policy_tests)
find_package(Threads REQUIRED)
add_executable(system_status_standby_tests tests/system_status_standby_tests.cpp
    firmware/components/system_status/system_status.cpp)
target_include_directories(system_status_standby_tests PRIVATE
    tests/support/system_status firmware/components/system_status/include)
target_compile_options(system_status_standby_tests PRIVATE
    -Wall -Wextra -Werror -Wno-unused-const-variable)
target_link_libraries(system_status_standby_tests PRIVATE Threads::Threads)
add_test(NAME system_status_standby COMMAND system_status_standby_tests)
add_executable(power_manager_tests tests/power_manager_tests.cpp)
target_include_directories(power_manager_tests PRIVATE
    tests/support/power_manager firmware/main)
target_compile_options(power_manager_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME power_manager_transaction COMMAND power_manager_tests)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME brookesia_lifecycle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_brookesia_lifecycle.py)
set_tests_properties(brookesia_lifecycle PROPERTIES
    ENVIRONMENT "CXX=${CMAKE_CXX_COMPILER}")
add_executable(screen_power_tests tests/screen_power_tests.cpp firmware/main/screen_power_control.cpp)
target_include_directories(screen_power_tests PRIVATE tests/support/screen_power firmware/main)
target_compile_options(screen_power_tests PRIVATE -Wall -Wextra -Werror)
foreach(scenario idle_boundary start_setup null_display touch_activity manual_off_on
        wake_window boot_activity power_activity button_test failed_manual_off
        failed_wake failed_idle repeated_cycles tick_wrap standby_wake)
    add_test(NAME screen_power_${scenario} COMMAND screen_power_tests ${scenario})
endforeach()
set(XIAOZHI_TIME_PRIVATE_DIR
    firmware/components/esp_xiaozhi_safe/esp_xiaozhi/src/priv_include)
set(XIAOZHI_CJSON_DIR firmware/managed_components/espressif__cjson/cJSON)
add_executable(xiaozhi_server_time_tests
    tests/xiaozhi_server_time_tests.cpp
    ${XIAOZHI_CJSON_DIR}/cJSON.c)
target_include_directories(xiaozhi_server_time_tests PRIVATE
    ${XIAOZHI_TIME_PRIVATE_DIR} ${XIAOZHI_CJSON_DIR})
target_compile_options(xiaozhi_server_time_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME xiaozhi_server_time COMMAND xiaozhi_server_time_tests)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artifacts")

option(ROUNDWING_BUILD_PREVIEW "Build the native round-screen preview" ON)
if(ROUNDWING_BUILD_PREVIEW)
    find_package(SDL2 REQUIRED CONFIG)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lvgl/lvgl.h")
        message(FATAL_ERROR "LVGL 9.4.0 missing. Build firmware, then run tools/prepare-native-deps.sh.")
    endif()
    file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS vendor/lvgl/src/*.c)
    add_library(lvgl STATIC ${LVGL_SOURCES})
    target_include_directories(lvgl PUBLIC vendor/lvgl "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
    add_library(roundwing_view src/game_view.cpp assets/roundwing_font_22.c assets/roundwing_font_30.c)
    target_include_directories(roundwing_view PUBLIC include assets)
    target_link_libraries(roundwing_view PUBLIC roundwing_core lvgl)
    target_compile_options(roundwing_view PRIVATE -Wall -Wextra)
    add_executable(round-wing-preview platform/native/main.cpp)
    target_link_libraries(round-wing-preview PRIVATE roundwing_view SDL2::SDL2)
    target_compile_options(round-wing-preview PRIVATE -Wall -Wextra -Werror)
    add_executable(view_tests tests/view_tests.cpp)
    target_link_libraries(view_tests PRIVATE roundwing_view)
    target_compile_options(view_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME view_interaction COMMAND view_tests)
    set_tests_properties(view_interaction PROPERTIES DEPENDS game_core)
    add_library(wifi_password_keyboard
        firmware/components/Settings/ui/WifiPasswordKeyboard.cpp)
    target_include_directories(wifi_password_keyboard PUBLIC firmware/components/Settings/ui)
    target_link_libraries(wifi_password_keyboard PUBLIC lvgl)
    target_compile_options(wifi_password_keyboard PRIVATE -Wall -Wextra -Werror)
    add_executable(wifi_keyboard_tests tests/wifi_keyboard_tests.cpp)
    target_link_libraries(wifi_keyboard_tests PRIVATE wifi_password_keyboard)
    target_compile_options(wifi_keyboard_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME wifi_keyboard_interaction COMMAND wifi_keyboard_tests)

    set(XIAOZHI_UI_DIR firmware/components/XiaozhiApp)
    set(XIAOZHI_FONT_DIR firmware/managed_components/78__xiaozhi-fonts)
    add_library(xiaozhi_cat_avatar
        ${XIAOZHI_UI_DIR}/XiaozhiCatAvatar.cpp
        ${XIAOZHI_UI_DIR}/assets/cat_avatar_assets.c)
    target_include_directories(xiaozhi_cat_avatar PUBLIC ${XIAOZHI_UI_DIR})
    target_link_libraries(xiaozhi_cat_avatar PUBLIC lvgl)
    target_compile_options(xiaozhi_cat_avatar PRIVATE -Wall -Wextra -Werror)
    add_executable(xiaozhi_cat_tests tests/xiaozhi_cat_tests.cpp)
    target_link_libraries(xiaozhi_cat_tests PRIVATE xiaozhi_cat_avatar)
    target_compile_options(xiaozhi_cat_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME xiaozhi_cat_avatar COMMAND xiaozhi_cat_tests)

    # The device's complete Chinese font uses 32-bit pointers in its binary
    # resource. Native rendering uses the same 30px glyph set as C source.
    add_library(xiaozhi_ui_native
        ${XIAOZHI_UI_DIR}/XiaozhiUi.cpp
        ${XIAOZHI_UI_DIR}/XiaozhiSubtitlePager.cpp
        ${XIAOZHI_FONT_DIR}/src/font_puhui_30_4.c
        ${XIAOZHI_FONT_DIR}/src/font_awesome_30_4.c)
    target_include_directories(xiaozhi_ui_native PRIVATE
        tests/support/xiaozhi ${XIAOZHI_FONT_DIR}/include)
    target_compile_definitions(xiaozhi_ui_native PUBLIC
        font_puhui_basic_30_4=font_puhui_30_4)
    target_link_libraries(xiaozhi_ui_native PUBLIC xiaozhi_cat_avatar)
    target_compile_options(xiaozhi_ui_native PRIVATE -Wall -Wextra -Werror)
    add_executable(xiaozhi_ui_tests tests/xiaozhi_ui_tests.cpp)
    target_link_libraries(xiaozhi_ui_tests PRIVATE xiaozhi_ui_native)
    target_compile_options(xiaozhi_ui_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME xiaozhi_round_ui COMMAND xiaozhi_ui_tests)
endif()
