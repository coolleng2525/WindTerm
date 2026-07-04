find_package(Qt5 REQUIRED COMPONENTS Core Network WebSockets)

add_library(windterm_hubterm STATIC
	Agent.cpp
	Agent.h
	Commander.cpp
	Commander.h
	Config.cpp
	Config.h
	Reporter.cpp
	Reporter.h
	TerminalShare.cpp
	TerminalShare.h
	../Pty/Pty.cpp
	../Pty/Pty.h
)

target_include_directories(windterm_hubterm
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
		${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(windterm_hubterm
	PUBLIC
		Qt5::Core
		Qt5::Network
		Qt5::WebSockets
)
