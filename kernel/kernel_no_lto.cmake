# SPDX-License-Identifier: Apache-2.0

# Optional script to disable LTO for kernel files

message(STATUS "[no-LTO] Building kernel files without LTO")

# Retrieve all source files from the kernel library target
if(TARGET kernel)
  get_property(KERNEL_SRCS TARGET kernel PROPERTY SOURCES)
else()
  message(WARNING "[no-LTO] kernel target not found, skipping")
  return()
endif()

# Split allowlist string into list
if(DEFINED CONFIG_KERNEL_LTO_ALLOWLIST)
  separate_arguments(LTO_ALLOWLIST NATIVE_COMMAND "${CONFIG_KERNEL_LTO_ALLOWLIST}")
endif()

# Apply -fno-lto to all C source files, except for some initialization files
# (e.g. init.c, errno.c, fatal.c) and those that are less critical to be
# placed in RAM. These files can be excluded from -fno-lto by using
# CONFIG_KERNEL_LTO_ALLOWLIST.
foreach(src ${KERNEL_SRCS})
  if(src MATCHES "\\.c$")

    # Skip if filename matches any in allowlist
    set(skip FALSE)
    foreach(allow ${LTO_ALLOWLIST})
      get_filename_component(basename ${src} NAME)
      if("${basename}" STREQUAL "${allow}")
        set(skip TRUE)
        break()
      endif()
    endforeach()

    if(NOT skip)
      set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-fno-lto -g")
    endif()

  endif()
endforeach()
