file(GLOB SRC "Source/*.cpp")
file(GLOB HDR "Include/*")

include_directories(
	Include
	${CMAKE_SOURCE_DIR}/Shared
)

add_library(AssemblyLoader SHARED ${SRC} ${HDR})