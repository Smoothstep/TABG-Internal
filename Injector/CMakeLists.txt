file(GLOB SRC "Source/*.cpp")

add_executable(Injector ${SRC})
set_property(TARGET Injector PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")

include_directories(
	${CMAKE_SOURCE_DIR}/Shared
)

target_link_libraries(Injector 
	AssemblyLoader)