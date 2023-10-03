# Copyright 2022 - V-Nova Ltd.
#
set_target_properties(lcevc_dec::platform PROPERTIES
	INTERFACE_LINK_LIBRARIES "pthread;dl" )
