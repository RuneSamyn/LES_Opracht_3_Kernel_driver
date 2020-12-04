#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>

static long int     toggleSpeed	= 1000;
static int 			outputPins[2] 	= { -1, -1 };
static int          inputPin = -1;


/*
 * Struct defining pins, direction and inital state 
 */
static struct gpio leds[] = {
		{  4, GPIOF_OUT_INIT_HIGH, "LED 1" },
		{ 25, GPIOF_OUT_INIT_HIGH, "LED 2" },
		{ 24, GPIOF_OUT_INIT_HIGH, "LED 3" },
};