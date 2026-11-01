add_executable(polkit-flow-test flow.cpp ../flow.cpp ../identity.cpp ../session.hpp)
target_link_libraries(polkit-flow-test PRIVATE
	Qt::Test Qt::Qml Qt::Quick PkgConfig::polkit_agent quickshell-core quickshell-window quickshell-ui quickshell-io
)
add_test(NAME polkit-flow COMMAND $<TARGET_FILE:polkit-flow-test>)
