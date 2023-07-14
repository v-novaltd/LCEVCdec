# Copyright 2022 - V-Nova Ltd.
#
if(NOT ANDROID_VENDOR)
	set_target_properties(lcevc_dec::platform PROPERTIES
		INTERFACE_LINK_LIBRARIES "android;log;EGL;z" )
else()
	set_target_properties(lcevc_dec::platform PROPERTIES
		INTERFACE_LINK_LIBRARIES "nativewindow;log;EGL;z" )
endif()
