cmake_minimum_required(VERSION 4.0)

set(CMAKE_CXX_STANDARD 23)
set(CXX_EXTENSIONS FALSE)

project(generator)

if(MSVC)
        set(CMAKE_FIND_LIBRARY_PREFIXES "")
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
        set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
        set(BUILD_SHARED_LIBS TRUE)
        set(WINDOWS_LIBS 
                bcrypt
                secur32
                mfplat
                mfuuid
                strmiids
                Ws2_32
        )
        set(CompilerFlags
                CMAKE_CXX_FLAGS
                CMAKE_CXX_FLAGS_DEBUG
                CMAKE_CXX_FLAGS_RELEASE
                CMAKE_C_FLAGS
                CMAKE_C_FLAGS_DEBUG
                CMAKE_C_FLAGS_RELEASE
        )        
        foreach(CompilerFlag ${CompilerFlags})
                string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
        endforeach()
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")
else()    
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

find_package(OpenMP REQUIRED)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h REQUIRED)
find_library(AVCODEC_LIBRARY avcodec REQUIRED)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h REQUIRED)
find_library(AVFORMAT_LIBRARY avformat REQUIRED)

find_library(AVUTIL_LIBRARY avutil REQUIRED)

find_library(SWRESAMPLE_LIBRARY swresample REQUIRED)


include_directories(
        3rdparty/libfvad/include
        ${AVCODEC_INCLUDE_DIR}
        ${AVFORMAT_INCLUDE_DIR}
)

add_subdirectory(3rdparty/libfvad)

set(sources 
        src/generator.cpp
        src/pipeline/fileToPCM.cpp
        src/pipeline/pcmToCut.cpp
)

add_library(${PROJECT_NAME} SHARED ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        fvad
        OpenMP::OpenMP_CXX
        ${AVCODEC_LIBRARY}
        ${AVFORMAT_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
        ${WINDOWS_LIBS}
)
