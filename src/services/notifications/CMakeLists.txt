qt_add_dbus_adaptor(DBUS_INTERFACES
	org.freedesktop.Notifications.xml
	server.hpp
	qs::service::notifications::NotificationServer
	dbus_notifications
	DBusNotificationServer
)

qt_add_library(quickshell-service-notifications STATIC
	server.cpp
	notification.cpp
	dbusimage.cpp
	qml.cpp
	${DBUS_INTERFACES}
)

# dbus headers
target_include_directories(quickshell-service-notifications PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

qt_add_qml_module(quickshell-service-notifications
	URI Quickshell.Services.Notifications
	VERSION 0.1
)

qs_add_module_deps_light(quickshell-service-notifications quickshell-core)
install_qml_module(quickshell-service-notifications)

target_link_libraries(quickshell-service-notifications PRIVATE Qt::Quick Qt::DBus)
target_link_libraries(quickshell PRIVATE quickshell-service-notificationsplugin)

qs_module_pch(quickshell-service-notifications SET dbus)

if (BUILD_TESTING)
	find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
	add_executable(notification-image-test test/dbusimage.cpp)
	target_link_libraries(notification-image-test PRIVATE
		Qt::Test Qt::Quick Qt::DBus quickshell-service-notifications quickshell-core quickshell-window quickshell-ui quickshell-io
	)
	add_test(NAME notification-image COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:notification-image-test>)
	set_tests_properties(notification-image PROPERTIES TIMEOUT 30)
endif()
