#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/delay.h>

#define DEBUG
#define DEV_MAJOR         60
#define DEV_NAME          "hc-sr04"
#define HCSR04_TRIGGER    27
#define HCSR04_ECHO       17

struct class *dev_class;

struct hc_sr04_device {
    struct device dev;
    dev_t no;
};

struct hc_sr04_device hc_sr04_dev = {
    .no = MKDEV(DEV_MAJOR,0),
};

static int hc_sr04_open(struct inode *inode, struct file *filp) {
    printk("HC-SR04: open\n");
    return 0;
}

static int hc_sr04_close(struct inode *inode, struct file *filp) {
    printk("HC-SR04: close\n");
    return 0;
}

static ssize_t hc_sr04_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	int ret, timeout_count;
	ssize_t bytes, out_buf_size;
	uint64_t distance = 0;
	char out_buf[64] = {0};
	ktime_t echo_start, echo_end;

	/* Check output size to avoid infinite loop or double trigger */
	out_buf_size = sizeof(out_buf) - *f_pos;
	bytes = (out_buf_size < count) ? out_buf_size : count;
	if (!bytes)
		return 0;

	/* Trigger for 10 us */
	gpio_set_value(HCSR04_TRIGGER, 1);
	udelay(10);
	gpio_set_value(HCSR04_TRIGGER, 0);

	/* Wait echo return, timeout if over 40ms */
	timeout_count = 0;
	while (gpio_get_value(HCSR04_ECHO) == 0) {
		if (timeout_count++ > 40000)
			break;
		udelay(1);
	}
	echo_start = ktime_get();
	timeout_count = 0;
	while (gpio_get_value(HCSR04_ECHO) == 1) {
		if (timeout_count++ > 40000)
			break;
		udelay(1);
	}
	echo_end = ktime_get();

	/* Calculate distance, it have to divide by 50 to get in cm */
	distance = ktime_to_us(ktime_sub(echo_end, echo_start));

    printk("HC-SR04: distance = %lld\n", distance);
	sprintf(out_buf, "%lld\n", distance);
	ret = copy_to_user(buf, out_buf, bytes);
	if (ret) {
		printk("copy_to_user() could not copy %d bytes.\n", ret);
		return -EFAULT;
	} else {
		printk("copy_to_user() succeeded!\n");
		*f_pos += bytes;
        return bytes;
	}
}

static ssize_t hc_sr04_write(struct file *filp, const char *buf, size_t size, loff_t *f_pos) {
    size_t pos;
    uint8_t byte;
    printk("HC-SR04: write  (size=%zu)\n", size);
    for (pos = 0; pos < size; ++pos) {
        if (copy_from_user(&byte, buf + pos, 1) != 0) {
            break;
        }
        printk("HC-SR04: write  (buf[%zu] = %02x)\n", pos, (unsigned)byte);
    }
    return pos;
}

static struct file_operations hc_sr04_fops = {
    .open = hc_sr04_open,
    .release = hc_sr04_close,
    .read = hc_sr04_read,
    .write = hc_sr04_write,
};


MODULE_LICENSE("Dual BSD/GPL");

static int hc_sr04_init(void) {
    int result, ret;
    pr_info("HC-SR04: init\n");

	/* Initial GPIO */
	if ((ret = gpio_request(HCSR04_TRIGGER, "HC-SR04 trigger pin"))) {
		printk(KERN_INFO "gpio_request for HC-SR04 trigger pin failed\n");
		return -1;
	}

	if ((ret = gpio_request(HCSR04_ECHO, "HC-SR04 echo pin"))) {
		printk(KERN_INFO "gpio_request for HC-SR04 echo pin failed\n");
		return -1;
	}

	if ((ret = gpio_direction_output(HCSR04_TRIGGER, 0))) {
		printk(KERN_INFO "gpio_direction_output for HC-SR04 trigger pin failed\n");
		return -1;
	}

	if ((ret = gpio_direction_input(HCSR04_ECHO))) {
		printk(KERN_INFO "gpio_direction_input for HC-SR04 echo pin failed\n");
		return -1;
	}

    /* Register character device */
    result = register_chrdev(DEV_MAJOR, DEV_NAME, &hc_sr04_fops);
    if (result < 0) {
        printk("HC-SR04: Failed to register character device\n");
        return result;
    }
	dev_class = class_create(THIS_MODULE, "dev_class");
	device_create(dev_class, NULL, hc_sr04_dev.no, &(hc_sr04_dev.dev), DEV_NAME);        
    
    return 0;
}

static void hc_sr04_exit(void) {
    pr_info("HC-SR04: exit\n");

	gpio_free(HCSR04_TRIGGER);
	gpio_free(HCSR04_ECHO);
	device_destroy(dev_class, hc_sr04_dev.no); 
	class_destroy(dev_class);
    /* Unregister character device */
    unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

module_init(hc_sr04_init);
module_exit(hc_sr04_exit);

MODULE_AUTHOR("Jay Chuang <kokacal@gmail.com>");
MODULE_DESCRIPTION("HC-SR04 driver on Raspberry Pi 3");
MODULE_LICENSE("GPL");
