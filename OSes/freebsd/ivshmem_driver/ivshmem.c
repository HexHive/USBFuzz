#include <sys/param.h>		/* defines used in kernel.h */
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>		/* types used in module initialization */
#include <sys/conf.h>		/* cdevsw struct */
#include <sys/uio.h>		/* uio struct */
#include <sys/malloc.h>
#include <sys/bus.h>		/* structs, prototypes for pci bus stuff and DEVMETHOD macros! */
#include<sys/ioccom.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>	/* For pci_get macros! */
#include <dev/pci/pcireg.h>

/* The softc holds our per-instance data. */
struct ivshmem_softc {
	device_t	my_dev;
	struct cdev	*my_cdev;
	struct resource *bar0_res;
};

/* Function prototypes */
static d_open_t		ivshmem_open;
static d_close_t	ivshmem_close;
static d_read_t		ivshmem_read;
static d_write_t	ivshmem_write;
static d_ioctl_t	ivshmem_ioctl;

/* Character device entry points */

static struct cdevsw ivshmem_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ivshmem_open,
	.d_close =	ivshmem_close,
	.d_read =	ivshmem_read,
	.d_write =	ivshmem_write,
	.d_ioctl =	ivshmem_ioctl,
	.d_name =	"ivshmem",
};

/*
 * In the cdevsw routines, we find our softc by using the si_drv1 member
 * of struct cdev.  We set this variable to point to our softc in our
 * attach routine when we create the /dev entry.
 */

int
ivshmem_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct ivshmem_softc *sc;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->my_dev, "Opened successfully.\n");
	return (0);
}

int
ivshmem_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct ivshmem_softc *sc;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->my_dev, "Closed.\n");
	return (0);
}

int
ivshmem_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct ivshmem_softc *sc;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->my_dev, "Asked to read %zd bytes.\n", uio->uio_resid);
	return (0);
}

int
ivshmem_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct ivshmem_softc *sc;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->my_dev, "Asked to write %zd bytes.\n", uio->uio_resid);
	return (0);
}

#define IVSHMEM_IOCTL_COMM _IOW('E', 1, u_int32_t)
#define UMA_COMM (0x10)
static void handle_ivshmem_cmd(struct ivshmem_softc *sc, u_int32_t arg) {
	device_printf(sc->my_dev, "ioctl arg=%d.\n", arg);
	bus_write_4(sc->bar0_res, UMA_COMM, arg);
}

int ivshmem_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct ivshmem_softc *sc;
	u_int32_t arg;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->my_dev, "ioctl called\n");
	switch (cmd) {

	case IVSHMEM_IOCTL_COMM:
		// device_printf("data=%lx\n", data);
		arg = *(u_int32_t *)data;
		handle_ivshmem_cmd(sc, arg);
		break;

	default:
		device_printf(sc->my_dev, "Bad ioctl\n");
		break;
	}
	

	return (0);
}

/* PCI Support Functions */

/*
 * Compare the device ID of this device against the IDs that this driver
 * supports.  If there is a match, set the description and return success.
 */
static int
ivshmem_probe(device_t dev)
{

	device_printf(dev, "Ivshmem Probe\nVendor ID : 0x%x\nDevice ID : 0x%x\n",
	    pci_get_vendor(dev), pci_get_device(dev));

	if (pci_get_vendor(dev) == 0x1af4) {
		printf("We've got the ivshmem device, probe successful!\n");
		device_set_desc(dev, "ivshmem");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

/* Attach function is only called if the probe is successful. */

static int
ivshmem_attach(device_t dev)
{
	struct ivshmem_softc *sc;

	printf("Ivshmem Attach for : deviceID : 0x%x\n", pci_get_devid(dev));

	/* Look up our softc and initialize its fields. */
	sc = device_get_softc(dev);
	sc->my_dev = dev;

	/*
	 * Create a /dev entry for this device.  The kernel will assign us
	 * a major number automatically.  We use the unit number of this
	 * device as the minor number and name the character device
	 * "ivshmem<unit>".
	 */
	sc->my_cdev = make_dev(&ivshmem_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "ivshmem%u", device_get_unit(dev));
	sc->my_cdev->si_drv1 = sc;
	int bar0 = PCIR_BAR(0);
	sc->bar0_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &bar0, 0ul, ~0ul, 1, RF_ACTIVE);

	if (sc->bar0_res == NULL) {
		printf("Ivshmem failed to load bar0 resource.\n");
		return (ENXIO);
	}

	printf("Ivshmem device loaded.\n");
	return (0);
}

/* Detach device. */

static int
ivshmem_detach(device_t dev)
{
	struct ivshmem_softc *sc;

	/* Teardown the state in our softc created in our attach routine. */
	sc = device_get_softc(dev);
	destroy_dev(sc->my_cdev);
	printf("Ivshmem detach!\n");
	return (0);
}

/* Called during system shutdown after sync. */

static int
ivshmem_shutdown(device_t dev)
{

	printf("Ivshmem shutdown!\n");
	return (0);
}

/*
 * Device suspend routine.
 */
static int
ivshmem_suspend(device_t dev)
{

	printf("Ivshmem suspend!\n");
	return (0);
}

/*
 * Device resume routine.
 */
static int
ivshmem_resume(device_t dev)
{

	printf("Ivshmem resume!\n");
	return (0);
}

static device_method_t ivshmem_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ivshmem_probe),
	DEVMETHOD(device_attach,	ivshmem_attach),
	DEVMETHOD(device_detach,	ivshmem_detach),
	DEVMETHOD(device_shutdown,	ivshmem_shutdown),
	DEVMETHOD(device_suspend,	ivshmem_suspend),
	DEVMETHOD(device_resume,	ivshmem_resume),

	DEVMETHOD_END
};

static devclass_t ivshmem_devclass;

DEFINE_CLASS_0(ivshmem, ivshmem_driver, ivshmem_methods, sizeof(struct ivshmem_softc));
DRIVER_MODULE(ivshmem, pci, ivshmem_driver, ivshmem_devclass, 0, 0);
