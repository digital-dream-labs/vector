#
# platform/config/CMakeLists.txt
#
# cmake rules to determine which files get deployed to robot under /anki/{bin,etc}
#
# Build options
#
# Configure with -DANKI_RESOURCE_SHIPPING=1 to deploy shipping config files.
# Configure with -DANKI_RESOURCE_SHIPPING=0 to deploy development config files.
# Default is ANKI_RESOURCE_SHIPPING=0.
#
# Configure with -DANKI_USER_DEV=1 to deploy the userdev version of config files
# Default is ANKI_USER_DEV=0
#
# Configure with -DANKI_BETA=1 to deploy the beta version of config files
# Default is ANKI_BETA=0
#
# Note use of add_dependencies() to serialize copy operations. This is done to prevent random build failures
# that occur when parallel processes attempt to create the same directory structure.
#
include(anki_build_copy_assets)

anki_build_copy_assets(
    TARGET platform_config_bin
    SRCLIST_DIR ${ANKI_SRCLIST_DIR}
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)

anki_build_copy_assets(
    TARGET platform_config_etc
    SRCLIST_DIR ${ANKI_SRCLIST_DIR}
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)

#
# platform_config_etc_development, platform_config_etc_shipping
# platform_config_bin_development, (none for shipping)
# Choose extra config files from shipping or development,
# then copy them into etc
#

  if(ANKI_RESOURCE_SHIPPING AND OSKR_ENDPOINTS)
    set(BASE_URL "https://ota.global.anki-services.com/vic/oskr/")
    set(ANKIDEV_BASE_URL "https://ota.global.anki-dev-services.com/vic/oskr/")
    configure_file(oskr/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_oskr)
  elseif(ANKI_RESOURCE_SHIPPING AND ANKI_RESOURCE_ESCAPEPOD)
    set(BASE_URL "https://ota.global.anki-services.com/vic/ep/")
    set(ANKIDEV_BASE_URL "https://ota.global.anki-dev-services.com/vic/ep/")
    configure_file(oskr/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_ep)  
  elseif(ANKI_RESOURCE_SHIPPING AND NOT OSKR_ENDPOINTS AND NOT ANKI_RESOURCE_ESCAPEPOD)
    set(BASE_URL "https://ota.global.anki-services.com/vic/prod/")
    set(ANKIDEV_BASE_URL "https://ota.global.anki-dev-services.com/vic/master/lo8awreh23498sf/")
    configure_file(shipping/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_shipping)
  elseif(ANKI_RESOURCE_ESCAPEPOD AND NOT ANKI_RESOURCE_SHIPPING)
    set(BASE_URL "https://ota.global.anki-dev-services.com/vic/master-ep/lo8awreh23498sf/")
    set(BASE_URL_LATEST "https://ota.global.anki-dev-services.com/vic/master-ep-latest/lo8awreh23498sf/")
    configure_file(development/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_epdev)
    set(platform_config_bin_extra platform_config_bin_ebdev)
  else()
    set(BASE_URL "https://ota.global.anki-dev-services.com/vic/master-dev/lo8awreh23498sf/")
    set(BASE_URL_LATEST "https://ota.global.anki-dev-services.com/vic/master-dev-latest/lo8awreh23498sf/")
    configure_file(development/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_development)
    set(platform_config_bin_extra platform_config_bin_development)
  endif()

  if (ANKI_USER_DEV)
    set(BASE_URL "https://ota.global.anki-dev-services.com/vic/master-userdev/lo8awreh23498sf/")
    configure_file(userdev/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_userdev)
  endif()

  if (ANKI_BETA)
    set(BASE_URL "https://ota.global.anki-dev-services.com/vic/beta/lo8awreh23498sf/")
    configure_file(beta/etc-template/update-engine.env.in ${CMAKE_BINARY_DIR}/etc/update-engine.env)
    set(platform_config_etc_extra platform_config_etc_beta)
  endif()
endif()

anki_build_copy_assets(
  TARGET ${platform_config_etc_extra}
  SRCLIST_DIR ${ANKI_SRCLIST_DIR}
  OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)

if (NOT ANKI_RESOURCE_SHIPPING)
  anki_build_copy_assets(
    TARGET ${platform_config_bin_extra}
    SRCLIST_DIR ${ANKI_SRCLIST_DIR}
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
  )
endif()

add_dependencies(platform_config_etc ${platform_config_etc_extra})
if (NOT ANKI_RESOURCE_SHIPPING)
  add_dependencies(platform_config_bin ${platform_config_bin_extra})
endif()
