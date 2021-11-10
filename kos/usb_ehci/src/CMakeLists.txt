project (hello)

include (platform/nk)

build_entity (${PROJECT_NAME} EDL_FILES "")

add_executable (${PROJECT_NAME} "main.cpp" ${EDL_FILES})

target_link_libraries (${PROJECT_NAME} ${vfs_CLIENT_LIB} pci_client acpi pthread)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Ttext 0x08500000")
