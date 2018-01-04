#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>

#define DEBUG
#define DEV_MAJOR         60
#define DEV_NAME          "hc-sr04"
#define HCSR04_TRIGGER    27
#define HCSR04_ECHO       17
#define SIZE_IN_BUF       8
#define SIZE_OUT_BUF      16
#define TIMER_PERIOD      (1*HZ) 

static char in_buf[SIZE_IN_BUF] = {0};
static char out_buf[SIZE_OUT_BUF] = {0};
static int irq_echo = -1;
uint64_t distance = 0;
ktime_t echo_start, echo_end;

struct class *dev_class;

struct hc_sr04_device {
    struct device dev;
    dev_t no;
};

struct hc_sr04_device hc_sr04_dev = {
    .no = MKDEV(DEV_MAJOR,0),
};

struct timer_list hc_sr04_trigger_timer;

static irqreturn_t hc_sr04_echo_irq_handler(int irq, void *data) {
	int echo_status;

	pr_debug("HC-SR04: %s enter\n", __func__);
	/* Calculate distance with time period, it have to divide by 58 to get in cm */
	echo_status = gpio_get_value(HCSR04_ECHO);
	if (echo_status == 1) {
		echo_start = ktime_get();
	} else if (echo_status == 0) {
		echo_end = ktime_get();
		distance = ktime_to_us(ktime_sub(echo_end, echo_start));
		memset(out_buf, '\0', SIZE_OUT_BUF);
		sprintf(out_buf, "%lld\n", distance);
	}

	return IRQ_HANDLED;
}

static void hc_sr04_trigger_timer_handler(void) {
	pr_debug("HC-SR04: %s enter\n", __func__);
	/* Trigger for 10 us, and set timer period to 1s */
	gpio_set_value(HCSR04_TRIGGER, 1);
	udelay(10);
	gpio_set_value(HCSR04_TRIGGER, 0);
	hc_sr04_trigger_timer.expires = jiffies + TIMER_PERIOD;
	add_timer(&hc_sr04_trigger_timer);
}

static int hc_sr04_trigger_timer_init(void) {  
	pr_debug("HC-SR04: %s enter\n", __func__);
	init_timer(&hc_sr04_trigger_timer);
	hc_sr04_trigger_timer.function = (void*) hc_sr04_trigger_timer_handler;
	hc_sr04_trigger_timer.data = ((unsigned long) 0);
	hc_sr04_trigger_timer.expires = jiffies + TIMER_PERIOD;

	return 0;
}

static int hc_sr04_open(struct inode *inode, struct file *filp) {
	pr_debug("HC-SR04: %s enter\n", __func__);
    return 0;
}

static int hc_sr04_close(struct inode *inode, struct file *filp) {
	pr_debug("HC-SR04: %s enter\n", __func__);
    return 0;
}

static ssize_t hc_sr04_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	int len;

	pr_debug("HC-SR04: %s enter\n", __func__);
	len = (count >= SIZE_OUT_BUF) ? SIZE_OUT_BUF : count;
	if (*f_pos >= SIZE_OUT_BUF)
		return 0;

	if (strcmp(in_buf, "1")) {
		memset(out_buf, '\0', SIZE_OUT_BUF);
		sprintf(out_buf, "0\n");
	}
	if (copy_to_user(buf, out_buf, len))
		return -EFAULT;

	*f_pos += len;

	return len;
}

static ssize_t hc_sr04_write(struct file *filp, const char *buf, size_t size, loff_t *f_pos) {
	pr_debug("HC-SR04: %s enter\n", __func__);
	if (size > SIZE_IN_BUF)
		return -EFAULT;
	memset(in_buf, '\0', SIZE_IN_BUF);
	if (copy_from_user(in_buf, buf, size))
		return -EFAULT;
	pr_debug("HC-SR04: write to in_buf = %s\n", in_buf);

	if (!strcmp(in_buf, "1")) {
		pr_debug("HC-SR04: enable trrigger timer with period = %d\n", TIMER_PERIOD);
		add_timer(&hc_sr04_trigger_timer);
	} else if (!strcmp(in_buf, "0")) {
		pr_debug("HC-SR04: disable trrigger timer\n");
		del_timer_sync(&hc_sr04_trigger_timer);
	} else {
		pr_debug("HC-SR04: not '0' or '1', keep timer setting");
	}

	return size;	
}

static struct file_operations hc_sr04_fops = {
    .open = hc_sr04_open,
    .release = hc_sr04_close,
    .read = hc_sr04_read,
    .write = hc_sr04_write,
};

static int hc_sr04_init(void) {
    int ret;
	pr_debug("HC-SR04: %s enter\n", __func__);

	/* Initial GPIO */
	if ((ret = gpio_request(HCSR04_TRIGGER, "HC-SR04 trigger pin"))) {
		pr_debug("HC-SR04: gpio_request for HC-SR04 trigger pin failed\n");
		return -1;
	}
	if ((ret = gpio_request(HCSR04_ECHO, "HC-SR04 echo pin"))) {
		pr_debug("HC-SR04: gpio_request for HC-SR04 echo pin failed\n");
		return -1;
	}
	if ((ret = gpio_direction_output(HCSR04_TRIGGER, 0))) {
		pr_debug("HC-SR04: gpio_direction_output for HC-SR04 trigger pin failed\n");
		return -1;
	}
	if ((ret = gpio_direction_input(HCSR04_ECHO))) {
		pr_debug("HC-SR04: gpio_direction_input for HC-SR04 echo pin failed\n");
		return -1;
	}

	/* Initial trigger timer and echo irq */	
	if (hc_sr04_trigger_timer_init()) {
		pr_debug("HC-SR04: init trigger timer failed\n");
		return -1;
	}
	irq_echo = gpiod_to_irq(gpio_to_desc(HCSR04_ECHO));
	if (irq_echo < 0) {
		pr_debug("HC-SR04: gpio_to_irq for HC-SR04 echo pin failed\n");
		return -1;
	}
	if ((ret = request_irq(irq_echo, hc_sr04_echo_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						   "hc-sr04.trigger", NULL))) {
		pr_debug("HC-SR04: init echo irq failed, it returns %d\n", ret);
		return -1;
	}

    /* Register character device */
    ret = register_chrdev(DEV_MAJOR, DEV_NAME, &hc_sr04_fops);
    if (ret < 0) {
        pr_debug("HC-SR04: Failed to register character device\n");
        return ret;
    }
	dev_class = class_create(THIS_MODULE, "dev_class");
	device_create(dev_class, NULL, hc_sr04_dev.no, &(hc_sr04_dev.dev), DEV_NAME);        
    
    return 0;
}

static void hc_sr04_exit(void) {
	pr_debug("HC-SR04: %s enter\n", __func__);

	/* Release gpio, timer and irq resource */
	del_timer_sync(&hc_sr04_trigger_timer);
	free_irq(irq_echo, NULL);
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
