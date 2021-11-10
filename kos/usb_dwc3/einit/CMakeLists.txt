project (einit)

include (platform/image)

project_header_default ("STRICT_WARNINGS:NO")

build_kos_hw_image (kos-image
                      EINIT_ENTITY einit_qemu
                      CONNECTIONS_CFG init.yaml.in
                      SECURITY_CFG security.cfg.in
#                      QEMU_FLAGS "-m 1024 -device usb-ehci,id=ehci -device usb-kbd,bus=ehci.0 -device usb-mouse,bus=ehci.0 -vnc :3"
#                       QEMU_FLAGS "-m 1024 -device usb-ehci,id=ehci -vnc :3"
                      IMAGE_FILES hello
                      )

