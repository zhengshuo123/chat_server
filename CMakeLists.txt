cmake_minimum_required(VERSION 3.16)

project(chat_server LANGUAGES CXX)

enable_testing()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Core
    Network
    Sql
    Test
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Network
    Sql
    Test
)

add_library(chat_server_protocol STATIC
    protocol/protocolcodec.h
    protocol/protocolcodec.cpp
)

target_link_libraries(chat_server_protocol PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(chat_server_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(chat_server
    main.cpp
    chatserver.h
    chatserver.cpp
    persistence/sqliterepository.h
    persistence/sqliterepository.cpp
)

target_link_libraries(chat_server PRIVATE
    chat_server_protocol
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

add_executable(tst_protocolcodec
    tests/tst_protocolcodec.cpp
)

target_link_libraries(tst_protocolcodec PRIVATE
    chat_server_protocol
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME protocolcodec
    COMMAND tst_protocolcodec
)

add_executable(tst_sqliterepository
    tests/tst_sqliterepository.cpp
    persistence/sqliterepository.h
    persistence/sqliterepository.cpp
)

target_link_libraries(tst_sqliterepository PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME sqliterepository
    COMMAND tst_sqliterepository
)

include(GNUInstallDirs)

install(TARGETS chat_server
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
