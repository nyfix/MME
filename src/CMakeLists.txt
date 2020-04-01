include_directories(..)
include_directories(${MAMA_ROOT}/include)

# Default to installing directly to MAMA directory
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${MAMA_ROOT}" CACHE PATH "default install path" FORCE)
endif()

link_directories(${MAMA_ROOT}/lib)

add_library(mme SHARED
  mamaManagedEnvironment.c mamaEnvConnection.c mamaEnvInbox.c mamaEnvSession.c mamaEnvSubscription.c mamaEnvTimer.c mamaSynchronizedMap.c mamaEnvEvent.c)

if(WIN32)
    message(FATAL_ERROR "Windows not supported")
elseif(UNIX)
   target_link_libraries(mme mama)
   install(TARGETS mme DESTINATION lib)
endif()

