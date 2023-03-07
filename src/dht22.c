/**
 * Raspberry Pi kernel module for dht22 humidity and temperature sensor.
 *
 *  Copyright (C) 2022 by Jakob Hofer <jakob.refoh@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/gpio.h>
#include <linux/string.h> /* memset */
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/bug.h>


MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Jakob Hofer <jakob.refoh@gmail.com>");
MODULE_DESCRIPTION("Character device driver for dht22 sensor");


#define DHT22_DEVICE_NAME "dht22"
#define DHT22_CLASS_NAME "dht22"
#define DHT22_MODULE_NAME KBUILD_MODNAME


#if defined(pr_fmt)
    #undef pr_fmt
#endif

#define pr_fmt(fmt) DHT22_MODULE_NAME ": " fmt


/**
 * Workaround 'unknown register name "r2"':
 * Issue: <https://github.com/microsoft/vscode-cpptools/issues/4382> 
 * Intellisense has its problems with recognizing arm registers.
 * NOTE: This re-define doesn't change the logic seen by the compiler.
 */
#if defined(__INTELLISENSE__)
    #undef put_user
    #define put_user(x, ptr) (-1)
#endif


static int irq_number;
static int dht22_major;
static struct cdev dht22_cdev;
static struct device* dht22_device;
static struct class* dht22_class;


static int gpio_pin = 4;
module_param(gpio_pin, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(gpio_pin, "GPIO pin used for communication (default 4)");


static struct
{
    size_t num_edges;
    ktime_t last_edge;
    uint8_t bytes[5]; /* Raw data received from sensor */

#if defined(DEBUG)
    /**
     *  Handshake sequence: 1 host + 1 response
     *  Sensor data: 5 bytes * 8 bits
    */
    int64_t diffs[42];
#endif

} dht22_detail;

static DEFINE_SPINLOCK(dht22_detail_spinlock);


static struct
{
    int16_t humidity;
    int16_t temperature;
    ktime_t last_read;
} dht22_sensor;

static DEFINE_MUTEX(dht22_sensor_mutex);


#if defined(DEBUG)
    static void print_timing_info(void);

    #define DEBUG_PRINT_TIMING_INFO() print_timing_info()
    #define DEBUG_STORE_PULSE_WIDTH(now) \
    ({ \
        if (dht22_detail.num_edges && (dht22_detail.num_edges <= 42)) \
            dht22_detail.diffs[dht22_detail.num_edges - 1] = ktime_to_us(now - dht22_detail.last_edge); \
    })

#else
    #define DEBUG_PRINT_TIMING_INFO() {}
    #define DEBUG_STORE_PULSE_WIDTH(now) {}
#endif


static int dht22_open(struct inode* inode, struct file* filp);
static int dht22_release(struct inode* inode, struct file* filp);
static ssize_t dht22_read(struct file* filp, char __user*  buf, size_t nbytes, loff_t* off);


static struct file_operations const dht22_fops = {
    .owner = THIS_MODULE,
    .open = dht22_open,
    .release = dht22_release,
    .read = dht22_read
};


static int _trigger_sensor(void)
{
    if (gpio_direction_output(gpio_pin, 0)) 
    {
        pr_alert("Setting GPIO pin %d to output mode failed: Value 0\n", gpio_pin);
        return -EIO;
    }

    udelay(1500);
    gpio_set_value(gpio_pin, 1);

    if (gpio_direction_input(gpio_pin)) 
    {
        pr_alert("Setting GPIO pin %d to input mode failed\n", gpio_pin);
        return -EIO;
    }

    return 0;
}


static inline void _clear_dht22_detail(void)
{
    unsigned long flags;
    spin_lock_irqsave(&dht22_detail_spinlock, flags);
    memset(&dht22_detail, 0, sizeof(dht22_detail));
    spin_unlock_irqrestore(&dht22_detail_spinlock, flags); 
}


static inline int _check_parity(void)
{
    return (((dht22_detail.bytes[0] + dht22_detail.bytes[1] +
            dht22_detail.bytes[2] + dht22_detail.bytes[3]) & 0xFF) == dht22_detail.bytes[4]);
}


static int _decode_sensor_data(void)
{
    unsigned long flags;
    spin_lock_irqsave(&dht22_detail_spinlock, flags);

    if (!_check_parity())
    {
        spin_unlock_irqrestore(&dht22_detail_spinlock, flags);
        pr_debug("Discard sensor data: invalid checksum\n");
        return -ERANGE;
    }

    dht22_sensor.humidity = (dht22_detail.bytes[0] << 8) + dht22_detail.bytes[1];
    dht22_sensor.temperature = ((dht22_detail.bytes[2] & 0x7F) << 8) + dht22_detail.bytes[3];

    if (dht22_detail.bytes[2] & 0x80)
    {
        dht22_sensor.temperature = -1 * dht22_sensor.temperature;	
    }

    spin_unlock_irqrestore(&dht22_detail_spinlock, flags);
    return 0;
}


static ssize_t _copy_data_to_user(char __user* buf, size_t nbytes)
{
    char private_buf[16]; 
    char* ptr = &private_buf[0];
    ssize_t bytes_read = 0;
    int err = 0;

    snprintf(private_buf, 13, "%d.%d,%d.%d",
        dht22_sensor.humidity / 10, dht22_sensor.humidity % 10,
        dht22_sensor.temperature / 10, dht22_sensor.temperature % 10
    );

    pr_debug("Data to user: %s\n", private_buf);

    while (nbytes && *ptr)
    {
        err = put_user(*(ptr++), buf++);
        if (err < 0) return err;
        --nbytes;
        ++bytes_read;
    }

    return bytes_read;
}


static ssize_t dht22_read(struct file* filp, char __user* buf, size_t nbytes, loff_t* off)
{	
    ktime_t const now = ktime_get();
    int64_t diff;
    int err = 0;

    diff = ktime_to_ms(now - dht22_sensor.last_read);

    if ((diff < 2000) && (dht22_sensor.last_read > 0LL))
    {	
        pr_debug("Re-read sensor too soon (within 2 seconds)\n");
        return 0;
    }

    pr_debug("Read sensor\n");

    _clear_dht22_detail();

    err = _trigger_sensor();
    if (err < 0) return err;

    msleep(10);  /* Read cycle takes less than 6ms */

    DEBUG_PRINT_TIMING_INFO();

    err = _decode_sensor_data();
    if (err < 0) return err;

    /* Set timestamp of last read to now only after a successful read */
    dht22_sensor.last_read = now;
    return _copy_data_to_user(buf, nbytes);
}


static int dht22_open(struct inode* inode, struct file* filp)
{
    if (mutex_trylock(&dht22_sensor_mutex) == 0) return -EBUSY;
    pr_debug("Device opened\n");
    return 0;
}


static int dht22_release(struct inode* inode, struct file* filp)
{
    mutex_unlock(&dht22_sensor_mutex);
    pr_debug("Device closed\n");
    return 0;
}


static irqreturn_t isr_handle_edge(int irq, void* dev_id)
{	
    ktime_t const now = ktime_get();
    unsigned long flags;
    int64_t diff;
    int i = 0;

    BUILD_BUG_ON(sizeof(dht22_detail.bytes) != 5UL);
    spin_lock_irqsave(&dht22_detail_spinlock, flags);
    DEBUG_STORE_PULSE_WIDTH(now);

    ++dht22_detail.num_edges;

    if (dht22_detail.num_edges < 4) goto edge_handled;
    if (dht22_detail.num_edges > 43) goto edge_handled;

    diff = ktime_to_us(now - dht22_detail.last_edge);

    /**
     * NOTE: I set the byte array to zero before reading from sensor, 
     * so I only have to update the bits that become a 1.
     */
    if (diff > 110)
    {
        i = dht22_detail.num_edges - 4;
        dht22_detail.bytes[i >> 3] |= 1 << (7 - (i & 0x7));
    }

edge_handled:
    dht22_detail.last_edge = now;
    spin_unlock_irqrestore(&dht22_detail_spinlock, flags);
    return IRQ_HANDLED;
}


static int _init_gpio(void)
{
    int err = -1;

    if (!gpio_is_valid(gpio_pin))
    {
        pr_alert("Invalid GPIO pin: %d\n", gpio_pin);
        return -1;
    }

    if (gpio_request(gpio_pin, DHT22_MODULE_NAME) < 0) 
    {
        pr_alert("Requesting GPIO pin %d failed\n", gpio_pin);
        return -1;
    }

    pr_debug("Using GPIO pin %d\n", gpio_pin);

    if (gpio_direction_input(gpio_pin)) 
    {
        pr_alert("Setting GPIO pin %d to input mode failed\n", gpio_pin);
        goto init_gpio_failed;
    }

    irq_number = gpio_to_irq(gpio_pin);
                                                                                                                    
    if (irq_number < 0) 
    {
        pr_alert("Requesting irq number of GPIO pin %d failed\n", gpio_pin);               
        goto init_gpio_failed;
    }

    pr_debug("Irq number of GPIO pin %d is %d\n", gpio_pin, irq_number);

    /* Register interrupt handler */
    err = request_irq(
        irq_number, isr_handle_edge, 
        IRQF_TRIGGER_FALLING, DHT22_MODULE_NAME, NULL
    );

    if (err) 
    {
        pr_alert("Registering ISR failed\n");
        goto init_gpio_failed;
    }

    return 0;

init_gpio_failed:
    gpio_free(gpio_pin);
    return err;
}


static int _init_cdev(void)
{
    int err = 0;
    dev_t dev_no;

    err = alloc_chrdev_region(&dev_no, 0, 1, DHT22_MODULE_NAME);
    if (err) 
    {
        pr_alert("Allocating major number failed\n");
        goto alloc_chrdev_region_failed;
    }

    dht22_class = class_create(THIS_MODULE, DHT22_CLASS_NAME);
    if (IS_ERR(dht22_class)) 
    {
        pr_alert("Creating device class failed\n");
        err = PTR_ERR(dht22_class);
        goto class_create_failed;
    }

    dht22_major = MAJOR(dev_no);
    cdev_init(&dht22_cdev, &dht22_fops);
    dht22_cdev.owner = THIS_MODULE;

    err = cdev_add(&dht22_cdev, dev_no, 1);
    if (err) 
    {
        pr_alert("Adding cdev to system failed\n");
        goto cdev_add_failed;
    }

    dht22_device = device_create(
        dht22_class, NULL, MKDEV(dht22_major, 0), NULL, DHT22_DEVICE_NAME
    );
    if (IS_ERR(dht22_device)) 
    {
        pr_alert("Creating device failed\n");
        err = PTR_ERR(dht22_device);
        goto device_create_failed;
    }

    return 0;

device_create_failed:
    cdev_del(&dht22_cdev);
cdev_add_failed:
    class_destroy(dht22_class);
class_create_failed:
    unregister_chrdev_region(MKDEV(dht22_major, 0), 1);
alloc_chrdev_region_failed:
    free_irq(irq_number, NULL);
    gpio_free(gpio_pin);
    return err;
}


static int __init dht22_init(void)
{
    int err = 0;

    err = _init_gpio();
    if (err) return err;

    err = _init_cdev();
    if (err) return err;

    pr_info("Initialization successful\n");
    return 0;
}


static void __exit dht22_exit(void)
{
    device_destroy(dht22_class, MKDEV(dht22_major, 0));
    cdev_del(&dht22_cdev);
    class_destroy(dht22_class);
    unregister_chrdev_region(MKDEV(dht22_major, 0), 1);
    free_irq(irq_number, NULL);
    gpio_free(gpio_pin);
    pr_info("Unload successful\n");
}


module_init(dht22_init);
module_exit(dht22_exit);


#if defined(DEBUG)
static void print_timing_info(void)
{
    int i;
    unsigned long flags;
    int64_t duration = 0;

    BUILD_BUG_ON(ARRAY_SIZE(dht22_detail.diffs) != 42UL);

    spin_lock_irqsave(&dht22_detail_spinlock, flags);
    pr_debug("Detected %d falling edges\n", dht22_detail.num_edges);
    pr_debug("Time between falling edges in µs (2 handshake, 40 data): ");

    for (i = 0; i < 41; ++i) 
    {
        pr_cont("%lld,", dht22_detail.diffs[i]);
        duration += dht22_detail.diffs[i];
    }

    duration += dht22_detail.diffs[41];
    pr_cont("%lld\n", dht22_detail.diffs[41]);

    pr_debug("Complete read cycle duration = %lldµs\n", duration);
    pr_debug("Interpreted timing info (5 bytes): ");

    for (i = 2; i < 42; ++i) 
    {
        if (((i - 2) & 0x07) == 0) pr_cont(" "); /* Print a space between bytes */
        pr_cont("%d", (dht22_detail.diffs[i] > 110) ? 1 : 0);
    }

    pr_cont("\n");
    spin_unlock_irqrestore(&dht22_detail_spinlock, flags);
}
#endif
