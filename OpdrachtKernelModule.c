#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/interrupt.h> 
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h> 

#include "OpdrachtKernelModule.h"

#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static struct gpio outputs[2];
static struct gpio input;

static int          toggleSpeed	= 1000;
static int 			outputPins[2] = { -1, -1 };
static int          inputPin = -1;
static int 			arr_argc = 0;
static struct       timer_list toggle_timer;
static long         outputState=1;
static int          input_irq = -1;
static long         input_edge_counter = 0;

module_param(toggleSpeed, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(toggleSpeed, "toggle speed of output pins in sec");
module_param(inputPin, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(inputPin, "1 input gpio pin");
module_param_array(outputPins, int, &arr_argc, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(outputPins, "2 output gpio pins");


static void toggle_timer_func(struct timer_list* t)
{
    gpio_set_value(outputs[0].gpio, outputState);
    gpio_set_value(outputs[1].gpio, outputState);
	outputState=!outputState;
	printk(KERN_INFO "number of input edges: %ld \n", input_edge_counter);
	/* schedule next execution */
	toggle_timer.expires = jiffies + (toggleSpeed * HZ); 		// 1 sec.
	add_timer(&toggle_timer);
}

/*
 * The interrupt service routine called on button presses
 */
static irqreturn_t input_isr(int irq, void *data)
{
	if(irq == input_irq){
		input_edge_counter++;
	}
	return IRQ_HANDLED;
}

static int my_open(struct inode *i, struct file *f)
{
    return 0;
}
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
    query_arg_t q;
 
    switch (cmd)
    {
        case GET_EDGES:
            q.edges = input_edge_counter;
            if (copy_to_user((query_arg_t *)arg, &q, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            break;
        case CLEAR_EDGES:
            input_edge_counter = 0;
            break;
        case SET_TOGGLE_SPEED:
            if (copy_from_user(&q, (query_arg_t *)arg, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            toggleSpeed = q.toggleSpeed;
            break;
        default:
            return -EINVAL;
    }
 
    return 0;
}
 
static struct file_operations opdrachtKM_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};

static int ioctl_init(void)
{
	
    int ret;
    struct device *dev_ret;
 
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "opdrachtKM_ioctl")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &opdrachtKM_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "opdrachtKM")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
 
    return 0;
}

/*
 * Module init function
 */
static int __init opdrachtKM_init(void)
{
	int i;
	int ret = 0;
	printk(KERN_INFO "%s\n=============\n", __func__);
	printk(KERN_INFO "Toggle speed set to %d sec\n", toggleSpeed);
	printk(KERN_INFO "Input pin: %d\n", inputPin);
    // add arguments to gpio array
	for (i = 0; i < (sizeof outputPins / sizeof (int)); i++)
	{
        if(outputPins[i] > 0) {
            struct gpio g = {outputPins[i], GPIOF_OUT_INIT_LOW, "Output pin" };
            outputs[i] = g;
        }
		printk(KERN_INFO "Output pin %d = %d\n", i + 1, outputPins[i]);
	}
    if(inputPin > 0) {
		struct gpio g = {inputPin, GPIOF_IN, "Input pin"};
        input = g;
    }
	// register outputs
	ret = gpio_request_array(outputs, ARRAY_SIZE(outputs));
	if(ret) {
		printk(KERN_ERR "Unable to request GPIOs for OUTPUTs: %d\n", ret);
	}

	// register INPUT gpio
	ret = gpio_request(input.gpio, "Input pin");
	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs for INPUT: %d\n", ret);
	}
	ret = gpio_to_irq(input.gpio);
	if(ret < 0) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
	}
	input_irq = ret;
	printk(KERN_INFO "Successfully requested INPUT IRQ # %d\n", input_irq);
	
	ret = request_irq(input_irq, input_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING /*| IRQF_DISABLED*/, "gpiomod#input", NULL);

	printk(KERN_INFO "after IRQ request\n");
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
	}
	/* init timer, add timer function */
	timer_setup(&toggle_timer, toggle_timer_func, 0);
	toggle_timer.function = toggle_timer_func;
	toggle_timer.expires = jiffies + (toggleSpeed * HZ); 		// 1 sec.
	add_timer(&toggle_timer);

	// init ioctl
	ret = ioctl_init();
	if(ret) {
		printk(KERN_ERR "Error init ioctl %d\n", ret);
	}

	return ret;
}

/*
 * Exit function
 */
static void __exit opdrachtKM_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	
	// free irqs
	free_irq(input_irq, NULL);

	printk(KERN_INFO "after free_irq");

	// deactivate timer if running
	del_timer_sync(&toggle_timer);

	// turn all GPIOs off
	int i = 0;
	for(i = 0; i < ARRAY_SIZE(outputs); i++) {
		gpio_set_value(outputs[i].gpio, 0); 
	}
	printk(KERN_INFO "after turn gpio off");
	
	// unregister all GPIOs
	gpio_free_array(outputs, ARRAY_SIZE(outputs));

	// destroy ioctl
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rune Samyn");
MODULE_DESCRIPTION("Linux kernel module: toggles gpio pins and read edges of inputs");

module_init(opdrachtKM_init);
module_exit(opdrachtKM_exit);