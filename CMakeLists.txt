cmake_minimum_required(VERSION 3.10)
project(DirectX11Triangle)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the path to your FindDirectX.cmake module
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# Specify the source files
set(SOURCES
    main.cpp
)

# Add the executable target
add_executable(DirectX11Triangle ${SOURCES})

set(WINDOWS_SDK_PATH "C:/Program Files (x86)/Windows Kits/10/")
set(WINDOWS_SDK_VERSION "10.0.22621.0")


# Manually specify the paths to DirectX libraries
set(D3D11_LIBRARY "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/d3d11.lib")
set(DXGI_LIBRARY "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/dxgi.lib")
set(D3D_COMPILER_LIBRARY "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/d3dcompiler.lib")

# Add the include directories
include_directories("${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/um")

# Link the DirectX libraries
target_link_libraries(DirectX11Triangle PRIVATE ${D3D11_LIBRARY} ${DXGI_LIBRARY} ${D3D_COMPILER_LIBRARY})
