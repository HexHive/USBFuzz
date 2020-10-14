#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "hw/hw.h"
#include "audio/audio.h"

#define USBWEBCAM_VENDOR_ID 0x045e
#define USBWEBCAM_PRODUCT_ID 0x045e

static const USBDescIface desc_iface[] = {

};

static const USBDescIfaceAssoc desc_ida[] = {
    .bFirstInterface = 0,
    .bInterfaceCount = 2,
    .bFunctionClass = 14,
    .bFunctionSubClass = 3,
    .bFunctionProtocol = 0
};

static const USBDescDevice desc_device = {
    .bcdUSB                        = 0x0101,
    .bMaxPacketSize0               = 64,
    .bNumConfigurations            = 1,

    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 4,
            .bConfigurationValue   = 1,
            .iConfiguration        = STRING_CONFIG,
            .bmAttributes          = USB_CFG_ATT_ONE | USB_CFG_ATT_BUSPOWER,
            .bMaxPower             = 250,

            .if_groups = ,
            
            .nif = ARRAY_SIZE(desc_iface),
            .ifs = desc_iface,
        }
    },
};
