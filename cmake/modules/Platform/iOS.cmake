# Copyright 2022 - V-Nova Ltd.
#
set_target_properties(lcevc_dec::platform PROPERTIES
	INTERFACE_LINK_LIBRARIES "-framework OpenGLES;-framework IOSurface;-framework CoreFoundation"
    INTERFACE_COMPILE_DEFINITIONS "GLES_SILENCE_DEPRECATION;_LIBCPP_DISABLE_DEPRECATION_WARNINGS" )

