# Copyright 2022 - V-Nova Ltd.
#
# Make sure winsock2 is linked - some dependencies (notably: ffmpeg) omit it.
#
set_target_properties(lcevc_dec::platform PROPERTIES
	INTERFACE_LINK_LIBRARIES "ws2_32" )
