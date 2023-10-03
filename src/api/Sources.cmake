# Copyright 2023 - V-Nova Ltd.
#
list(APPEND SOURCES
	"src/api.cpp"
	"src/buffer_manager.cpp"
	"src/decoder.cpp"
	"src/decoder_config.cpp"
	"src/decoder_pool.cpp"
	"src/event_manager.cpp"
	"src/interface.cpp"
	"src/lcevc_processor.cpp"
	"src/picture.cpp"
	"src/picture_lock.cpp"
	"src/pool.cpp"
	"utility/src/uLog.cpp"
	"utility/src/uPictureFormatDesc.cpp"
	"utility/src/uPlatform.cpp"
	"utility/src/uString.cpp"
	)

list(APPEND HEADERS
	"src/accel_context.h"
	"src/buffer_manager.h"
	"src/clock.h"
	"src/decoder.h"
	"src/decoder_config.h"
	"src/decoder_pool.h"
	"src/event_manager.h"
	"src/handle.h"
	"src/interface.h"
	"src/lcevc_processor.h"
	"src/picture.h"
	"src/picture_lock.h"
	"src/pool.h"
	"utility/include/uChrono.h"
	"utility/include/uConfigMap.h"
	"utility/include/uEnumMap.h"
	"utility/include/uEnums.h"
	"utility/include/uLog.h"
	"utility/include/uPictureCopy.h"
	"utility/include/uPictureFormatDesc.h"
	"utility/include/uPlatform.h"
	"utility/include/uString.h"
	"utility/include/uTimestamps.h"
	"utility/include/uTypes.h"
	"utility/include/uTypeTraits.h"
	)

list(APPEND INTERFACES
	"include/LCEVC/lcevc_dec.h"
    )

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
