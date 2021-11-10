project (einit)

include (platform/image)

project_header_default ("STRICT_WARNINGS:NO")

build_kos_qemu_image (kos-qemu-image
                      EINIT_ENTITY einit_qemu
                      CONNECTIONS_CFG init.yaml
                      SECURITY_CFG security.cfg
                     QEMU_FLAGS "-m 1024 -device usb-ehci,id=ehci -device usb-kbd,bus=ehci.0 -device usb-mouse,bus=ehci.0 -vnc :3"
#                       QEMU_FLAGS "-m 1024 -device usb-ehci,id=ehci -device usb-host,hostbus=3,hostaddr=14,bus=ehci.0 -vnc :3"
                      IMAGE_FILES hello
                      )

set_target_properties (einit_qemu PROPERTIES LINK_FLAGS "-Ttext 0x00200000")
