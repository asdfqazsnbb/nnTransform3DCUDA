cmake_minimum_required(VERSION 3.15)

project(nnTransform3DCUDA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_compile_definitions(_USE_MATH_DEFINES)
add_compile_definitions(APP_BRANCH="nnTransform3DCUDA" APP_COMMIT="Independent")

find_package(FFTW3 REQUIRED)
find_package(Qt6 COMPONENTS Core Sql REQUIRED)


add_library(lddecode-chroma STATIC
    lib/lddecodemetadata.cpp  
    lib/sourcevideo.cpp       
    lib/logging.cpp
    lib/sqliteio.cpp
    lib/dropouts.cpp
    lib/jsonio.cpp

    comb.cpp
    componentframe.cpp
    framecanvas.cpp
    outputwriter.cpp
    sourcefield.cpp
    decoder.cpp
    decoderpool.cpp
    monodecoder.cpp
    ntscdecoder.cpp
)


set_target_properties(lddecode-chroma PROPERTIES AUTOMOC ON)


target_include_directories(lddecode-chroma PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)



target_link_libraries(lddecode-chroma PRIVATE Qt6::Core Qt6::Sql FFTW3::fftw3)


add_executable(ld-chroma-decoder
    main.cpp
)


target_link_libraries(ld-chroma-decoder PRIVATE lddecode-chroma Qt6::Core Qt6::Sql FFTW3::fftw3)