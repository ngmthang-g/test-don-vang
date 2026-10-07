cmake_minimum_required(VERSION 3.24)
project(ThanLongItemConsolidator VERSION 10.6 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "ThanLongItemConsolidator is Windows-only")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ThanLongItemConsolidator must be built x64")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TL_LICENSE_API_URL "$ENV{TL_LICENSE_API_URL}" CACHE STRING "Public HTTPS Supabase license API URL")
if(TL_LICENSE_API_URL STREQUAL "")
    set(TL_LICENSE_API_URL "https://dklpxxxgapbnjixvbzai.supabase.co/functions/v1/license-api")
endif()
string(HEX "${TL_LICENSE_API_URL}" TL_LICENSE_API_URL_HEX)
string(LENGTH "${TL_LICENSE_API_URL_HEX}" TL_LICENSE_API_URL_HEX_LEN)
math(EXPR TL_LICENSE_API_URL_HEX_LAST "${TL_LICENSE_API_URL_HEX_LEN} - 2")
set(TL_LICENSE_API_XOR_KEY 93)
set(TL_LICENSE_API_URL_ENC "")
foreach(TL_URL_I RANGE 0 ${TL_LICENSE_API_URL_HEX_LAST} 2)
    string(SUBSTRING "${TL_LICENSE_API_URL_HEX}" ${TL_URL_I} 2 TL_URL_PAIR)
    math(EXPR TL_URL_BYTE "0x${TL_URL_PAIR} ^ ${TL_LICENSE_API_XOR_KEY}")
    string(APPEND TL_LICENSE_API_URL_ENC "${TL_URL_BYTE},")
endforeach()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(src/license_build_config.h.in "${CMAKE_BINARY_DIR}/generated/license_build_config.h" @ONLY)

set(TL_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/generated_runtime")
foreach(f bridge.cpp controller.cpp image_scan_test.cpp)
    if(NOT EXISTS "${TL_RUNTIME_DIR}/${f}")
        message(FATAL_ERROR "Missing generated runtime source: ${f}")
    endif()
endforeach()

add_library(ThanLongCleanRouteBridge SHARED
    "${TL_RUNTIME_DIR}/bridge.cpp"
)
target_include_directories(ThanLongCleanRouteBridge PRIVATE "${TL_RUNTIME_DIR}" src)
target_compile_definitions(ThanLongCleanRouteBridge PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ThanLongCleanRouteBridge PRIVATE user32 kernel32)

set_source_files_properties("${TL_RUNTIME_DIR}/controller.cpp" PROPERTIES COMPILE_DEFINITIONS "wWinMain=ThanLongCoreMain")
add_executable(ThanLongItemConsolidator WIN32
    src/license_launcher.cpp
    "${TL_RUNTIME_DIR}/controller.cpp"
    "${TL_RUNTIME_DIR}/image_scan_test.cpp"
    src/telegram_notifier.cpp
    src/telegram_account_filter.cpp
    src/gold_history_store.cpp
    resources/app.rc)
target_include_directories(ThanLongItemConsolidator PRIVATE "${TL_RUNTIME_DIR}" src "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(ThanLongItemConsolidator PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_link_libraries(ThanLongItemConsolidator PRIVATE comctl32 comdlg32 user32 kernel32 gdi32 windowscodecs ole32 winhttp crypt32 advapi32 bcrypt)
target_link_options(ThanLongItemConsolidator PRIVATE /MANIFEST:NO)
set_target_properties(ThanLongItemConsolidator PROPERTIES OUTPUT_NAME "CongCuHoTroGameRanhTay_10.6")

add_executable(route_logic_tests src/route_logic_test.cpp)
add_executable(trade_coordinator_logic_tests src/trade_coordinator_logic_test.cpp)
add_executable(background_ui_logic_tests src/background_ui_logic_test.cpp)
add_executable(fixed_slot_sell_logic_tests src/fixed_slot_sell_logic_test.cpp)
add_executable(unity_geometry_logic_tests src/unity_geometry_logic_test.cpp)
add_executable(internal_ui_click_logic_tests src/internal_ui_click_logic_test.cpp)
add_executable(travel_fight_guard_logic_tests src/travel_fight_guard_logic_test.cpp)
add_executable(auto_fight_retry_logic_tests src/auto_fight_retry_logic_test.cpp)
add_executable(telegram_logic_tests src/telegram_logic_test.cpp)
add_executable(gold_history_store_tests src/gold_history_store_test.cpp src/gold_history_store.cpp)
add_executable(automation_bulk_logic_tests src/automation_bulk_logic_test.cpp)
add_executable(party_build_logic_tests src/party_build_logic_test.cpp)
add_executable(equip_point_db_tests src/equip_point_db_test.cpp)
add_executable(bag_filter_v2_logic_tests src/bag_filter_v2_logic_test.cpp)
add_executable(main_semantic_sell_logic_tests src/main_semantic_sell_logic_test.cpp)
add_executable(main_macro_sell_logic_tests src/main_macro_sell_logic_test.cpp)
add_executable(trade_quota_v18_logic_tests src/trade_quota_v18_logic_test.cpp)
add_executable(con_filter_nosleep_logic_tests src/con_filter_nosleep_logic_test.cpp)
add_executable(sell_filter_grid_v2_logic_tests src/sell_filter_grid_v2_logic_test.cpp)
add_executable(auto_loot_logic_tests src/auto_loot_logic_test.cpp)
add_executable(pk_tl_lm_logic_tests src/pk_tl_lm_logic_test.cpp)
add_executable(travel_network_logic_tests src/travel_network_logic_test.cpp)
add_executable(integration_hardening_logic_tests src/integration_hardening_logic_test.cpp)
add_executable(auto_role_logic_tests src/auto_role_logic_test.cpp)

set(TL_TEST_TARGETS
    route_logic_tests trade_coordinator_logic_tests background_ui_logic_tests fixed_slot_sell_logic_tests
    unity_geometry_logic_tests internal_ui_click_logic_tests travel_fight_guard_logic_tests auto_fight_retry_logic_tests
    telegram_logic_tests gold_history_store_tests automation_bulk_logic_tests party_build_logic_tests equip_point_db_tests bag_filter_v2_logic_tests
    main_semantic_sell_logic_tests main_macro_sell_logic_tests trade_quota_v18_logic_tests con_filter_nosleep_logic_tests
    sell_filter_grid_v2_logic_tests auto_loot_logic_tests pk_tl_lm_logic_tests travel_network_logic_tests integration_hardening_logic_tests auto_role_logic_tests)
foreach(t IN LISTS TL_TEST_TARGETS)
    target_include_directories(${t} PRIVATE src)
endforeach()

if(MSVC)
    set(TL_HARDEN_COMPILE_OPTIONS /O2 /Ob3 /Oi /GL /GS /sdl /guard:cf /Gy /Gw /GF /GR- /Zc:inline /Brepro)
    set(TL_HARDEN_LINK_OPTIONS /LTCG /OPT:REF /OPT:ICF /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf /CETCOMPAT /INCREMENTAL:NO /RELEASE /Brepro)
    target_compile_options(ThanLongCleanRouteBridge PRIVATE /utf-8 /W4 /permissive- /FIstdlib.h ${TL_HARDEN_COMPILE_OPTIONS})
    target_link_options(ThanLongCleanRouteBridge PRIVATE ${TL_HARDEN_LINK_OPTIONS})
    target_compile_options(ThanLongItemConsolidator PRIVATE ${TL_HARDEN_COMPILE_OPTIONS})
    target_link_options(ThanLongItemConsolidator PRIVATE ${TL_HARDEN_LINK_OPTIONS})
    foreach(t ThanLongItemConsolidator ${TL_TEST_TARGETS})
        target_compile_options(${t} PRIVATE /utf-8 /W4 /permissive-)
    endforeach()
endif()
