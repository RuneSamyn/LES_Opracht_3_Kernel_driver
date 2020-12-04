#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/init.h>

static int          toggleSpeed	= 1000;
static int 			outputPins[2] = { -1, -1 };
static int          inputPin = -1;
static int 			arr_argc = 0;
static struct       timer_list toggle_timer;
static long         outputState=1;
/*
 * Struct defining pins, direction and inital state 
 */
static struct gpio outputs[2];

module_param(toggleSpeed, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(toggleSpeed, "toggle speed of output pins in sec");
module_param(inputPin, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(inputPin, "1 input gpio pin");
module_param_array(outputPins, int, &arr_argc, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(outputPins, "2 output gpio pins");


static void toggle_timer_func(struct timer_list* t)
{
    int i;
	for (i = 0; i < (sizeof outputs / sizeof (int)); i++)
	{
        gpio_set_value(outputs[i].gpio, outputState);
	}
	outputState=!outputState;
	/* schedule next execution */
	toggle_timer.expires = jiffies + (toggleSpeed * HZ); 		// 1 sec.
	add_timer(&toggle_timer);
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
	// register LED gpios, turn gpios on
	ret = gpio_request_array(outputs, ARRAY_SIZE(outputs));
	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
	}
	/* init timer, add timer function */
	timer_setup(&toggle_timer, toggle_timer_func, 0);
	toggle_timer.function = toggle_timer_func;
	toggle_timer.expires = jiffies + (toggleSpeed * HZ); 		// 1 sec.
	add_timer(&toggle_timer);

	return ret;
}

/*
 * Exit function
 */
static void __exit opdrachtKM_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
    
	// deactivate timer if running
	del_timer_sync(&toggle_timer);

	// turn all LEDs off
    int i;
	for(i = 0; i < ARRAY_SIZE(outputs); i++) {
		gpio_set_value(outputs[i].gpio, 0); 
	}
	
	// unregister all GPIOs
	gpio_free_array(outputs, ARRAY_SIZE(outputs));
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rune Samyn");
MODULE_DESCRIPTION("Linux kernel module: toggles gpio pins and read edges of inputs");

module_init(opdrachtKM_init);
module_exit(opdrachtKM_exit);