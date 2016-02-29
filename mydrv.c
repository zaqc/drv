#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of_irq.h>
#include <linux/phy.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <linux/time.h>
#include <linux/wait.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <linux/irq.h>   

#include <linux/module.h>
#include <linux/types.h>

#include <linux/slab.h>

#include <linux/delay.h>
#include <linux/fs.h>

#include <linux/irqdesc.h>
#include <linux/gpio.h>

#include <asm/hardirq.h>
#include <asm/hw_irq.h>

#define	LWH2F_BASE	0xFF200000
#define	LWH2F_SIZE	0x100000

#define	H2F_BASE	0xC0000000
#define	H2F_SIZE	0x100000

#define	PIO_LED_BASE	0x10040
#define PIO_BTN_BASE	0x100C0

#define	PIO_STREAM_BASE	0x10000

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;

void *ptr1;
void *ptr2;

#define	BLOCK_SIZE	0x2000

static ssize_t my_drv_read(struct file * f, char * buf, size_t count,
		loff_t *ppos) {

	if(count == BLOCK_SIZE) {
		wait_event_interruptible(wq, flag != 0);
		int fl = flag;
		flag = 0;
		if(fl == 1) {
			copy_to_user(buf, ptr1, BLOCK_SIZE);
			return BLOCK_SIZE;
		} 
		else if(fl = 2) {
			copy_to_user(buf, ptr2, BLOCK_SIZE);
			return BLOCK_SIZE;
		}
	}

	char *hello_str = "Hello, world!\n";
	int len = strlen(hello_str);
	if (count < len)
		return -EINVAL;
	if (*ppos != 0)
		return 0;
	if (copy_to_user(buf, hello_str, len))
		return -EINVAL;
	*ppos += len;

	return len;
}

static ssize_t my_drv_write(struct file *f, char *buf, size_t count,
		loff_t *ppos) {

	printk(KERN_DEBUG "my drv write pos=%i count=%i buf=[%s]\n", (int)f->f_pos, count);

	*ppos += count;

	return count;
}

static loff_t my_drv_seek (struct file *f, loff_t off, int whence)
{
        loff_t newpos;

	printk(KERN_DEBUG "my drv seek %i \n", (int)off);

        switch(whence) {

        case SEEK_SET:
                newpos = off;
                break;

        case SEEK_CUR:
                newpos = f->f_pos + off;
                break;

        case SEEK_END:
                newpos = 16 * 1024 * 1024 + off;
                break;

        default: 
                return -EINVAL;
        }

        if (newpos < 0) 
                return -EINVAL;

        if (newpos >= 16 * 1024 * 1024)
                return -EINVAL;

        f->f_pos = newpos;

        return newpos;
}

static const struct file_operations my_drv_fops = { 
	.owner = THIS_MODULE,
	.read = my_drv_read, 
	.write = my_drv_write, 
	.llseek = my_drv_seek, 
};

static struct miscdevice my_dev = { MISC_DYNAMIC_MINOR,
	"mydrv", 
	&my_drv_fops, 
};

#ifdef CONFIG_OF
static const struct of_device_id my_drv_match[] = {
	{ .compatible = "mydrv", },
	{},
};

MODULE_DEVICE_TABLE(of, my_drv_match);
#endif

int my_drv_remove(struct platform_device *pdev) {
	return 0;
}


static struct platform_driver my_drv_driver = {
	.remove = my_drv_remove,
	.driver = {
		.name = "mydrv",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(my_drv_match),
	},
};

struct resource *h2s_reg;
struct resource *lw_h2s_reg;

void *h2f_ptr;
void *lw_h2f_ptr;

int irq_number;
int buf_num = 0;

static irqreturn_t my_drv_interrupt(int irq, void *dev_id)
{
//	printk("btn int\n");

	if (irq != irq_number)
		return IRQ_NONE;

//	printk(KERN_DEBUG "interrupt IRQ=%i\n", irq);

/*	spin_lock(&interrupt_flag_lock);
	interrupt_flag = 1;
	input_state = ioread8(fpga_uinput_mem);
	spin_unlock(&interrupt_flag_lock);
	wake_up_interruptible(&interrupt_wq);
*/



	//flag = ioread8(lw_h2f_ptr + PIO_BTN_BASE + 3 * 4);

//	printk("btn int\n");

	if(buf_num == 0) {
		flag = 1;
		buf_num = 1;
		memcpy_fromio(ptr1, h2f_ptr, BLOCK_SIZE);
	} 
	else {
		flag = 2;
		buf_num = 0;
		memcpy_fromio(ptr2, h2f_ptr, BLOCK_SIZE);
	}

//	iowrite8(0xFF, lw_h2f_ptr + PIO_BTN_BASE + 3 * 4);

	wake_up_interruptible(&wq);

	return IRQ_HANDLED;
}

int my_drv_probe(struct platform_device *pdev) {

	printk(KERN_DEBUG "pdev_addr=%s\n", pdev->dev.of_node);

	irq_number = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if(irq_number > 0) {
		printk(KERN_DEBUG "Interrupt number is normaly accepted [IRQ=%i]\n", irq_number);
	} 
	else {
		printk(KERN_DEBUG "fail to irq_of_parse_and_map [result=%i]\n", irq_number);
		return -EINVAL;
	}

	return 0;
}

static int __init my_drv_init(void) {
	int i, n;

	struct timeval t0, t1;

	long int temp;


	int error_count = 0;

	int reg_res = 0;
	unsigned char tst_val[4] = { 0x00, 0xFF, 0xAA, 0x55 };

	int result = -1;

	if (platform_driver_probe(&my_drv_driver, my_drv_probe)) {
		printk("Failed to probe MYDRV platform driver\n");

		platform_driver_unregister(&my_drv_driver);

		return -ENXIO;
	}

	reg_res = misc_register(&my_dev);
	if (0 != reg_res) {
		printk(KERN_DEBUG "filed to misc_register [result=%i]\n", reg_res);
		result = reg_res; 
		goto fail_misc_register;
	} else 
		printk(KERN_DEBUG "register driver OK... result=%i \n", reg_res);
	
	// === Request Memory Region HPS TO FPGA ===
	
	h2s_reg = request_mem_region(H2F_BASE, H2F_SIZE, "mydrv");
	if (NULL == h2s_reg) {
		printk(KERN_DEBUG "filed to call request_mem_region(h2s) \n");
		goto fail_h2s_request_mem_region;
	}

	h2f_ptr = ioremap(H2F_BASE, H2F_SIZE);
	if (NULL == h2f_ptr) {
		printk(KERN_DEBUG "fail to call ioremap(h2s) \n");
		goto fail_h2s_ioremap;
	}

	// === Request Memory Region Light Weight (LW) HPS TO FPGA ===
	
	lw_h2s_reg = request_mem_region(LWH2F_BASE, LWH2F_SIZE, "mydrv");
	if (NULL == lw_h2s_reg) {
		printk(KERN_DEBUG "filed to call request_mem_region(lw_h2s) \n");
		goto fail_lw_h2s_request_mem_region;
	}

	lw_h2f_ptr = ioremap(LWH2F_BASE, LWH2F_SIZE);
	if (NULL == lw_h2f_ptr) {
		printk(KERN_DEBUG "fail to call ioremap(lw_h2s) \n");
		goto fail_lw_h2s_ioremap;
	}

	// === Allocate memory for buffers 2 x [64 * 1024] ===

	ptr1 = (unsigned int*) kmalloc(BLOCK_SIZE, GFP_KERNEL);
	printk(KERN_DEBUG "ptr1 = 0x%08X \n", (int) ptr1);
	if(NULL == ptr1) {
		printk(KERN_DEBUG "fail to call kmalloc(ptr) 64Kb \n");
		goto fail_kmalloc_ptr1;
	}

	ptr2 = (unsigned int*) kmalloc(BLOCK_SIZE, GFP_KERNEL);
	printk(KERN_DEBUG "ptr = 0x%08X \n", (int) ptr2);
	if(NULL == ptr2) {
		printk(KERN_DEBUG "fail to kmalloc(in_ptr) 64Kb\n");
		goto fail_kmalloc_ptr2;
	}


	// === Request IRQ (PIO Button IRQ=39 GIC-0=73 $(cat /proc/interrupts)) ===

	result = request_threaded_irq(irq_number, my_drv_interrupt, NULL, 0, "mydrv", NULL);
	if(result < 0) {
		printk(KERN_DEBUG "fail to call request_irq [result=%i] \n", result);
		goto fail_request_irq;
	}

	// ===  PIO Button Set Interrupt Mask (enable all pin's) ===
	iowrite8(0x00, lw_h2f_ptr + PIO_BTN_BASE + 1 * 4);	// direction = In
	iowrite8(0xFF, lw_h2f_ptr + PIO_BTN_BASE + 2 * 4);	// IRQ = enable
	iowrite8(0x00, lw_h2f_ptr + PIO_BTN_BASE + 3 * 4);	// edge

	for(i = 0; i < 16; i++)
		iowrite8(0xFF, (int)lw_h2f_ptr + (int)PIO_BTN_BASE + i);

	printk(KERN_DEBUG "Button PIO[data]=%02X\n", ioread8(lw_h2f_ptr + PIO_BTN_BASE));
	printk(KERN_DEBUG "Interrupt MASK=%02X\n", ioread8(lw_h2f_ptr + PIO_BTN_BASE + 2 * 4));

	iowrite8(0x00, h2f_ptr);	// enable IRQ
	msleep(200);
	iowrite8(0x00, h2f_ptr + 1 * 4);	// clear IRQ
	msleep(200);
	iowrite8(0x00, h2f_ptr);	// disable IRQ
	msleep(200);
	iowrite8(0x00, h2f_ptr + 1 * 4);	// clear IRQ

	error_count = 0;
	/*
	for (n = 0; n < 4; n++) {
		memset(in_ptr, tst_val[n], 64 * 1024);
		memcpy_toio(h2f_ptr, in_ptr, 64 * 1024);
		memcpy_fromio(ptr, h2f_ptr, 64 * 1024);
		for (i = 0; i < 16 * 1024; i++)
			if (((int *) in_ptr)[i] != ((int *) ptr)[i]) {
				printk(KERN_DEBUG "error n=%i i=%i val=0x%08X\n", n, i,
						((unsigned int *) ptr)[i]);
				error_count++;
			}
	}
	*/
	printk(KERN_DEBUG "error count=%i\n", error_count);

	do_gettimeofday(&t0);
	for (i = 0; i < 1024; i++)
		memcpy(ptr1, ptr2, 8 * 1024); //_fromio(ptr, h2f_ptr, 64 * 1024);
	do_gettimeofday(&t1);
	temp = (unsigned int) (t1.tv_sec - t0.tv_sec) * 1000000
			+ (unsigned int) (t1.tv_usec - t0.tv_usec);
	printk(KERN_DEBUG " interval = %i \n", (int) temp);

	return 0;

fail_request_irq:
	
	kfree(ptr2);

fail_kmalloc_ptr2:

	kfree(ptr1);

fail_kmalloc_ptr1:

	iounmap(lw_h2f_ptr);

fail_lw_h2s_ioremap:

	release_mem_region(LWH2F_BASE, LWH2F_SIZE);

fail_lw_h2s_request_mem_region:

	iounmap(h2f_ptr);

fail_h2s_ioremap:

	release_mem_region(H2F_BASE, H2F_SIZE);

fail_h2s_request_mem_region:

	misc_deregister(&my_dev);

fail_misc_register:

	return result;
}

static void __exit my_drv_exit(void) {

	free_irq(irq_number, NULL);

	kfree(ptr2);
	kfree(ptr1);

	iounmap(lw_h2f_ptr);
	release_mem_region(LWH2F_BASE, LWH2F_SIZE);

	iounmap(h2f_ptr);
	release_mem_region(H2F_BASE, H2F_SIZE);

	misc_deregister(&my_dev);

	platform_driver_unregister(&my_drv_driver);
}

MODULE_AUTHOR("Sergey Zaytcev teamcoders@gmail.com");
MODULE_DESCRIPTION("Driver for [HPS]<->[FPGA] interface");
MODULE_LICENSE("GPL");

module_init( my_drv_init);
module_exit( my_drv_exit);
