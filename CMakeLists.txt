cmake_minimum_required(VERSION 3.20)

project(nfsv2 CXX)

set(${CXX_STANDARD} 17)

add_executable(${CMAKE_PROJECT_NAME})

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	src/main.cpp
	src/nfs_service.cpp
	src/portmap_service.cpp
	src/utils.cpp
	src/rpc.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
	./src/
)


