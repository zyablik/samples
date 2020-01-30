#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

int LIBUSB_CALL hotplug_callback(libusb_context * ctx, libusb_device * dev, libusb_hotplug_event event, void * user_data) {
    struct libusb_device_descriptor desc;
    int rc = libusb_get_device_descriptor(dev, &desc);
    if (LIBUSB_SUCCESS != rc) {
        printf("libusb_get_device_descriptor() failed: %d: %s\n", rc, libusb_error_name(rc));
        return rc;
    }

    switch(event) {
        case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED: printf("device attached :"); break;
        case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:    printf("device detached :"); break;
        default: printf("unknown event %d: ", event);
    }

    printf (": %04x:%04x\n", desc.idVendor, desc.idProduct);
    return 0;
}

int main(void) {
	int rc = libusb_init(NULL);
	if (rc != LIBUSB_SUCCESS) {
            printf("libusb_init() failed: %d: %s", rc, libusb_error_name(rc));
            return EXIT_FAILURE;
        }

        // libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);

	libusb_device ** devs;
        ssize_t ndevices = libusb_get_device_list(NULL, &devs);
        if (ndevices <= 0) {
            printf("libusb_get_device_list() failed: %d: %s", rc, libusb_error_name(rc));
            return EXIT_FAILURE;
        }

        printf("libusb_get_device_list(): ndevices = %ld\n", ndevices);

	int i = 0, j = 0;
	uint8_t path[32];
        libusb_device * dev;

        uint16_t vendor_id = 0x0b05;    // asus
        uint16_t product_id = 0x5601;
        libusb_device * device = NULL;

	while ((dev = devs[i++]) != NULL)  {
            struct libusb_device_descriptor desc;
            rc = libusb_get_device_descriptor(dev, &desc);
            if (rc < 0) {
                printf("libusb_get_device_descriptor() failed: : %d: %s", rc, libusb_error_name(rc));
                return EXIT_FAILURE;
            }

            printf("%04x:%04x (bus %d, device %d)", desc.idVendor, desc.idProduct, libusb_get_bus_number(dev), libusb_get_device_address(dev));

            rc = libusb_get_port_numbers(dev, path, sizeof(path));
            if (rc > 0) {
                printf(" port: %d", path[0]);
                for (j = 1; j < rc; j++)
                    printf(".%d", path[j]);
            }
            printf("\n");

            if(desc.idVendor == vendor_id && desc.idProduct == product_id) {
                device = dev;
                printf("found nct phone %x:%x: device = %p\n", vendor_id, product_id, device);
            }
	}

        libusb_device_handle * dev_handle = NULL;
        rc = libusb_open(device, &dev_handle);
        if (rc != LIBUSB_SUCCESS) {
            printf("libusb_open() failed: : %d: %s", rc, libusb_error_name(rc));
            return EXIT_FAILURE;
        }

        // dev_handle = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
        // printf("libusb_open_device_with_vid_pid(0x%x, 0x%x): dev_handle = %p\n", vendor_id, product_id, dev_handle);

        libusb_set_auto_detach_kernel_driver(dev_handle, 1);

	rc = libusb_claim_interface(dev_handle, 0);
        if (rc != LIBUSB_SUCCESS) {
            printf("libusb_claim_interface() failed: : %d: %s", rc, libusb_error_name(rc));
            return EXIT_FAILURE;
        }

        device = libusb_get_device(dev_handle);
        libusb_config_descriptor * config;
        rc = libusb_get_config_descriptor(device, 0, &config);
        const libusb_interface_descriptor * intf = config->interface[0].altsetting;

        uint8_t intf_name[256];
        rc = libusb_get_string_descriptor_ascii(dev_handle,  intf->iInterface, intf_name, sizeof(intf_name));
        if (rc < 0) {
            printf("libusb_get_string_descriptor_ascii() failed: : %d: '%s'", rc, libusb_error_name(rc));
        }

        printf("intf: bNumEndpoints = %d iInterface = %d: %s\n", intf->bNumEndpoints, intf->iInterface, intf_name);
        for(int i = 0; i < intf->bNumEndpoints; i++) {
            printf("intf: endpoint[%d]: addr = 0x%x dir: %s wMaxPacketSize = %d\n", i,
                   intf->endpoint[i].bEndpointAddress & LIBUSB_ENDPOINT_ADDRESS_MASK,
                   (intf->endpoint[i].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN ? "IN " : "OUT",
                   intf->endpoint[i].wMaxPacketSize);
        }

        libusb_hotplug_callback_handle cb_handle;
        rc = libusb_hotplug_register_callback(NULL, (libusb_hotplug_event) (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), LIBUSB_HOTPLUG_ENUMERATE,
                                              vendor_id, product_id, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, &cb_handle, &cb_handle);
	if (rc != LIBUSB_SUCCESS) {
            printf("libusb_hotplug_register_callback() failed: : %d: %s", rc, libusb_error_name(rc));
            return EXIT_FAILURE;
	}

	while (true) {
            rc = libusb_handle_events(NULL);
            if (rc < 0)
                printf("libusb_handle_events() failed: %s\n", libusb_error_name(rc));
	}

	libusb_exit(NULL);
	return 0;
}
