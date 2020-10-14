#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"

#include "afl.h"


struct req_data {
    int size;
    uint8_t data[0];
};


#define USB_DT_DEVICE_SIZE		18
#define USB_DT_CONFIG_SIZE		9

static int read_from_file(int fd, uint8_t *buf, int n_bytes) {
    ssize_t n_read;
    int pos = 0;

    if (n_bytes <= 0)
        return n_bytes;

    while ((n_bytes - pos > 0) && ((n_read = read(fd, buf + pos, n_bytes - pos)) > 0)) {
        pos += n_read;
    }

    return pos;
}

struct afl_usb_fuzz_data {
    int desc_fd;
    int data_fd;
    bool datafile_init;
    bool configured;
    bool initialized;
    struct req_data *device_desc;
    struct req_data *config_desc;
};

static struct afl_usb_fuzz_data fuzz_data;

static void init_afl_usb_fuzz_data(void) {
    memset(&fuzz_data, 0, sizeof(fuzz_data));

    if (usbDescFile == NULL)
        goto __failed;

    if (access(usbDescFile, F_OK) == -1)
        goto __failed;

    fuzz_data.desc_fd =  open(usbDescFile, O_RDONLY);
    if (fuzz_data.desc_fd == -1)
        goto __failed;

    // we first read device desc from the file
    struct req_data *device_desc = g_malloc0 (sizeof(struct req_data) + USB_DT_DEVICE_SIZE);
    device_desc->size = read_from_file(fuzz_data.desc_fd, device_desc->data, USB_DT_DEVICE_SIZE);
    USBDescriptor *udesc = (USBDescriptor *)device_desc->data;
    udesc->bLength = USB_DT_DEVICE_SIZE;
    udesc->bDescriptorType = USB_DT_DEVICE;

    fuzz_data.device_desc = device_desc;
    if (device_desc->size < USB_DT_DEVICE_SIZE) {
        goto __end;
    }

    // we allocate more than enough for this
    struct req_data *config_desc = g_malloc0(sizeof(struct req_data) + 65536);

    fuzz_data.config_desc = config_desc;

    int size1 = read_from_file(fuzz_data.desc_fd, config_desc->data, USB_DT_CONFIG_SIZE);
    config_desc->size = size1;
    udesc = (USBDescriptor *)config_desc->data;

    udesc->bLength = USB_DT_CONFIG_SIZE;
    udesc->bDescriptorType = USB_DT_CONFIG;

    if (size1 < USB_DT_CONFIG_SIZE) {
        goto __end;
    }

    int real_size = (udesc->u.config.wTotalLength_hi << 8) +    \
        (udesc->u.config.wTotalLength_lo);

    int size2 = read_from_file(fuzz_data.desc_fd, config_desc->data + size1, real_size - size1);
    config_desc->size += size2;

    if (usbDataFile == NULL)
        goto __end;

    if (access(usbDescFile, F_OK) == -1)
        goto __end;

    fuzz_data.data_fd = open(usbDescFile, O_RDONLY);
    if (fuzz_data.data_fd == -1)
        goto __end;

    fuzz_data.datafile_init = true;

 __end:
    fuzz_data.initialized = true;
    return;

 __failed:
    fuzz_data.initialized = false;
    return;
}

static void cleanup_afl_usb_fuzz_data(void) {
    close(fuzz_data.desc_fd);
    fuzz_data.initialized = false;

    if (fuzz_data.device_desc) {
        g_free(fuzz_data.device_desc);
        fuzz_data.device_desc = NULL;
    }

    if (fuzz_data.config_desc) {
        g_free(fuzz_data.config_desc);
        fuzz_data.config_desc = NULL;
    }

    if (fuzz_data.datafile_init) {
        close(fuzz_data.data_fd);
        fuzz_data.datafile_init = false;
    }
}

static void reset_afl_usb_fuzz_data(void) {
    cleanup_afl_usb_fuzz_data();

    init_afl_usb_fuzz_data();
}

static void usb_fuzz_realize(USBDevice *dev, Error **errp)
{
    dev->speedmask |= USB_SPEED_MASK_LOW;
    dev->speedmask |= USB_SPEED_MASK_FULL;
    dev->speedmask |= USB_SPEED_MASK_HIGH;
    // superspeed is a little different
    // TO test in the future
    // dev->speedmask |= USB_SPEED_MASK_SUPER;
    reset_afl_usb_fuzz_data();
}

static int read_from_fuzz_data(uint8_t *buf, int max_length, int expected_length) {
    int len = expected_length < max_length ? expected_length : max_length;

    if (fuzz_data.datafile_init)
        return read_from_file(fuzz_data.data_fd, buf, len);
    else
        return read_from_file(fuzz_data.desc_fd, buf, len);
}

static void handle_get_descriptor_requests(USBDevice *dev, USBPacket *p,
                                           int request, int value, int index,
                                           int length, uint8_t *data) {
    uint8_t type = value >> 8;
    // uint8_t v_index = value & 0xff;
    struct req_data *req = NULL;
    int xlen = 0;
    
    switch(type) {
    case USB_DT_DEVICE:
        req = fuzz_data.device_desc;
        break;

    case USB_DT_CONFIG:
        req = fuzz_data.config_desc;
        break;
    }

    if (req != NULL) {
        if (req->size < length)
            xlen = req->size;
        else
            xlen = length;

        memcpy(data, req->data, xlen);
        p->actual_length = xlen;
        return;
    }

    uint8_t buf[255];
    memset(buf, 0, 255);
    switch(type) {
    case USB_DT_STRING:
        // we read 32 bytes
        xlen = read_from_fuzz_data(buf, length, 0x20);
        buf[1] = USB_DT_STRING;
        break;

    case USB_DT_DEVICE_QUALIFIER:
        xlen = read_from_fuzz_data(buf, length, length);
        buf[1] = USB_DT_DEVICE_QUALIFIER;
        break;

    case USB_DT_OTHER_SPEED_CONFIG:
        xlen = read_from_fuzz_data(buf, length, length);
        buf[1] = USB_DT_OTHER_SPEED_CONFIG;
        break;

    case USB_DT_BOS:
        xlen = read_from_fuzz_data(buf, length, length);
        buf[1] = USB_DT_BOS;
        break;

    /* case USB_DT_DEBUG: */
    /*     /\* ignore silently *\/ */
    /*     break; */

    default:
        p->status = USB_RET_STALL;
        // xlen = read_from_fuzz_data(buf, length, length);
        // buf[1] = type;
        break;
    }

    memcpy(data, buf, xlen);
    p->actual_length = xlen;
}

static void usb_fuzz_handle_control(USBDevice *dev, USBPacket *p,
                                    int request, int value, int index,
                                    int length, uint8_t *data) {

    int xlen;
    dump_request(request, value, index, length);
    // int xlen;
    // setting device addr, do nothing 
    if (request == (DeviceOutRequest | USB_REQ_SET_ADDRESS)) {
        dev->addr = value;

        return;
    }


    switch(request) {
    case DeviceRequest | USB_REQ_GET_DESCRIPTOR: {
        // important part, delegate it to
        // handle_get_descriptor_requests function
        handle_get_descriptor_requests(dev, p, request, value, index, length, data);
        break;
    }

    case DeviceRequest | USB_REQ_GET_CONFIGURATION:
         /*
         * 9.4.2: 0 should be returned if the device is unconfigured, otherwise
         * the non zero value of bConfigurationValue.
         */
        data[0] =  0;
        p->actual_length = 1;
        break;
    case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
        // configured = true;
        break;
    default:
        xlen = read_from_fuzz_data(data, length, length);
        p->actual_length = xlen;
    }
}

static void usb_fuzz_handle_data(USBDevice *dev, USBPacket *p) {
#define __BUF_SIZE 2048
    unsigned char buf[__BUF_SIZE];
    int len;
    int xlen;

    switch (p->pid) {
    case USB_TOKEN_IN:
        len = p->iov.size < __BUF_SIZE? p->iov.size : __BUF_SIZE;
        xlen = read_from_fuzz_data(buf, len, len);
        usb_packet_copy(p, buf, xlen);
        break;
    default:
        break;
    }
}

static void usb_fuzz_handle_reset(USBDevice *dev) {

}

static void usb_fuzz_class_initfn(ObjectClass *klass, void *data)
{
    // DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    // gen_random_bytes(sizeof(USBDescDevice), &desc_fuzz_device);

    uc->product_desc   = "QEMU USB Fuzz";
    // uc->usb_desc = &usbdesc;
    uc->realize = usb_fuzz_realize;
    uc->handle_reset = usb_fuzz_handle_reset;
    uc->handle_control = usb_fuzz_handle_control;
    uc->handle_data = usb_fuzz_handle_data;

  // set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
}

static const TypeInfo usbfuzz_info = {
    .name          = "usb-fuzz",
    .parent        = TYPE_USB_DEVICE,
    .class_init    = usb_fuzz_class_initfn,
};

static void usb_fuzz_register_type(void)
{
    type_register_static(&usbfuzz_info);
}

type_init(usb_fuzz_register_type)
