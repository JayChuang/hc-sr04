#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DEBUG
#define DEV_MAJOR 60
#define DEV_NAME "hc-sr04"

struct class *dev_class;
//struct class_device *example_cla_dev;

struct hc_sr04_device {
    struct device dev;
    dev_t no;
};

struct hc_sr04_device hc_sr04_dev = {
    .no = MKDEV(DEV_MAJOR,0),
};

static int hc_sr04_open(struct inode *inode, struct file *filp) {
    pr_debug("HC-SR04: open\n");
    return 0;
}

static int hc_sr04_close(struct inode *inode, struct file *filp) {
    pr_debug("HC-SR04: close\n");
    return 0;
}

static ssize_t hc_sr04_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	//static uint8_t byte = 1;
	static const char str[]="Hello Irene from kernel!\n";
	static const ssize_t str_size = sizeof(str);
	
    pr_debug("HC-SR04: read  (size=%zu)\n", count);
	//copy_to_user(buf, &byte, 1);
	//copy_to_user(buf, str, str_size);
	//byte++;
	if( *f_pos >= str_size)
		return 0;
	if( *f_pos + count > str_size)
		count = str_size - *f_pos;
	if (copy_to_user(buf, str + *f_pos, count))
		return -EFAULT;
	*f_pos += count;
	return count;
    //return 0;
}

static ssize_t hc_sr04_write(struct file *filp, const char *buf, size_t size, loff_t *f_pos) {
    size_t pos;
    uint8_t byte;
    pr_debug("HC-SR04: write  (size=%zu)\n", size);
    for (pos = 0; pos < size; ++pos) {
        if (copy_from_user(&byte, buf + pos, 1) != 0) {
            break;
        }
        pr_debug("HC-SR04: write  (buf[%zu] = %02x)\n", pos, (unsigned)byte);
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
    int result;
    pr_info("HC-SR04: init\n");

    /* Register character device */
    result = register_chrdev(DEV_MAJOR, DEV_NAME, &hc_sr04_fops);
    if (result < 0) {
        pr_debug("HC-SR04: Failed to register character device\n");
        return result;
    }
	dev_class = class_create(THIS_MODULE, "dev_class");
	//example_cla_dev = device_create(dev_class, NULL, hc_sr04_dev.no, &(hc_sr04_dev.dev), "exampledev_name");        
	device_create(dev_class, NULL, hc_sr04_dev.no, &(hc_sr04_dev.dev), DEV_NAME);        
    
    return 0;
}

static void hc_sr04_exit(void) {
    pr_info("HC-SR04: exit\n");

	device_destroy(dev_class, hc_sr04_dev.no); 
	class_destroy(dev_class);
    /* Unregister character device */
    unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

module_init(hc_sr04_init);
module_exit(hc_sr04_exit);
