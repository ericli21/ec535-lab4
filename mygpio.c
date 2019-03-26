#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include <asm/hardware.h> //GPIO function here
#include <asm/arch/pxa-regs.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h> //IRQ here
#include <linux/jiffies.h>
#include <asm/arch/gpio.h>
#include <linux/timer.h> //need for kernel timer

//Function declarations
static void LED_helper(int num);
static irq_handler_t button0_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs);
static irq_handler_t button1_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs);
static void timer_handler(unsigned long data);

//initial count is 1, direction is 0 (subtract), state is 0 (hold value)
static int counter = 1;
static int direction = 0;
static int state = 0;

//initial timer frequency is 1HZ (1 second, 4*250ms), can be fractional
static int frequency = 4;
static struct timer_list * my_timer_ptr;
static struct timer_list my_timer;

// define LED and button pins here
#define LED_PIN_0 28
#define LED_PIN_1 29
#define LED_PIN_2 30
#define LED_PIN_3 31

#define BUTTON_PIN_0 17
#define BUTTON_PIN_1 101

//function declarations
static ssize_t gpio_read(struct file * filp, char * buff, size_t count, loff_t * f_pos);
static ssize_t gpio_write(struct file * filp, char * buff, size_t count, loff_t * f_pos);
static int gpio_open(struct inode * inode, struct file * filp);
static int gpio_release(struct inode * inode, struct file * filp);
static void gpio_exit(void);
static int __init gpio_init(void);

//struct for read and write
struct file_operations my_gpio_fops = {
	read: gpio_read,
	write: gpio_write,
	open: gpio_open,
	release: gpio_release
};

static int my_gpio_major = 61;

module_init(gpio_init);
module_exit(gpio_exit);
MODULE_LICENSE("Dual BSD/GPL");

static ssize_t gpio_write(struct file * filp, char * buff, size_t count, loff_t * f_pos)
{
	//extra functionality: writing f(1-9) changes timer frequency to n/4
	//extra functionality: writing v(hex) sets count value to given hex value

	char input[count + 1];

	if (copy_from_user(input, buff, count))
		return -EFAULT;

	//Check if valid input
	if(sizeof(input) == 4){
		if(input[0] == 'v'){
			if(input[1] >= 'A' && input[1] <= 'F'){
				counter = input[1] - 55;
			}
			else if(input[1] >= 'a' && input[1] <= 'f'){
				counter = input[1] - 87;
			}
			else if(input[1] >= '1' && input[1] <= '9'){
				counter = input[1] - '0';
			}
			else{
				printk(KERN_INFO "Invalid input format\n");
			}
		}
		else if(input[0] == 'f'){
			if(input[1] >= '1' && input[1] <= '9'){
				frequency = input[1] - '0';
			}
			else{
				printk(KERN_INFO "Invalid input format\n");
			}
		}
		else{
			printk(KERN_INFO "Invalid input format\n");
		}
	}
	else{
		printk(KERN_INFO "Invalid input format\n");
	}

	return count;
}

static ssize_t gpio_read(struct file * filp, char * buff, size_t count, loff_t * f_pos)
{

	//extra functionality: reading from /dev/mygpio returns:
	//counter value, counter frequency, counter direction, counter state
	char * page;
	int len = sprintf(page,"Count: %d\tFrequency: %d/4\tDirection: %d\tState: %d\n", counter, frequency, direction, state);

	if(*f_pos >= sizeof(page)){
		return 0;
	}

	if(copy_to_user(buff, page, sizeof(page))){		
		return -EFAULT;
	}

	*f_pos += count;
	return count;
}

static int gpio_open(struct inode * inode, struct file * filp){	
	return 0;
}

static int gpio_release(struct inode * inode, struct file * filp){
	return 0;
}

static int __init gpio_init(void)
{
	int result;
	result = register_chrdev(my_gpio_major, "mygpio", &my_gpio_fops);
	if(result < 0){
		return result;
	}

  	//initialize LED pins here, set as output
	int led0 = pxa_gpio_mode(LED_PIN_0 | GPIO_OUT);
	int led1 = pxa_gpio_mode(LED_PIN_1 | GPIO_OUT);
	int led2 = pxa_gpio_mode(LED_PIN_2 | GPIO_OUT);
	int led3 = pxa_gpio_mode(LED_PIN_3 | GPIO_OUT);

	//Clear LEDs
	pxa_gpio_set_value(LED_PIN_0, 0);
	pxa_gpio_set_value(LED_PIN_1, 0);
	pxa_gpio_set_value(LED_PIN_2, 0);
	pxa_gpio_set_value(LED_PIN_3, 0);

	//Initialize buttons as input
	pxa_gpio_mode(BUTTON_PIN_0 | GPIO_IN);
	pxa_gpio_mode(BUTTON_PIN_1 | GPIO_IN);

	//Set up interrupts, need both rising and falling edge interrupt conditions
	int irq0 = IRQ_GPIO(BUTTON_PIN_0);
	int irq1 = IRQ_GPIO(BUTTON_PIN_1);

	if (request_irq(irq0, &button0_interrupt, SA_INTERRUPT | SA_TRIGGER_RISING | SA_TRIGGER_FALLING,
				"mygpio", NULL) != 0 ) {
                printk(KERN_ALERT "IRQ not acquired \n" );
                return -1;
        }
	if (request_irq(irq1, &button1_interrupt, SA_INTERRUPT | SA_TRIGGER_RISING | SA_TRIGGER_FALLING,
				"mygpio", NULL) != 0 ) {
                printk(KERN_ALERT "IRQ not acquired \n" );
                return -1;
        }

  	//Set up timer
	my_timer_ptr = &my_timer;
	init_timer(my_timer_ptr);
	(*my_timer_ptr).expires = jiffies + msecs_to_jiffies(frequency * 250);
	(*my_timer_ptr).function = timer_handler;
	add_timer(my_timer_ptr);

	//Call LED helper to set counter
	LED_helper(counter);

	return 0;
}

static void gpio_exit(void)
{
	//Clean up timer and IRQs
	unregister_chrdev(my_gpio_major, "mygpio");
	del_timer(my_timer_ptr);
	free_irq(IRQ_GPIO(BUTTON_PIN_0), NULL);
	free_irq(IRQ_GPIO(BUTTON_PIN_1), NULL);
}

static void LED_helper(int num){

	//use bitwise AND to check if the current bit should light the LED
	//LSB LED 0
	if(num & 0x1){
		//set LED 0 to high
		pxa_gpio_set_value(LED_PIN_0, 1);
	}
	else{
		//set LED 0 to low
		pxa_gpio_set_value(LED_PIN_0, 0);
	}

	//look at next bit
	num = num >> 1;

	//LED 1
	if(num & 0x1){
		//set LED 1 to high
		pxa_gpio_set_value(LED_PIN_1, 1);
	}
	else{
		//set LED 1 to low
		pxa_gpio_set_value(LED_PIN_1, 0);
	}

	//look at next bit
	num = num >> 1;

	//LED 2
	if(num & 0x1){
		//set LED 2 to high
		pxa_gpio_set_value(LED_PIN_2, 1);
	}
	else{
		//set LED 2 to low
		pxa_gpio_set_value(LED_PIN_2, 0);
	}

	//look at next bit
	num = num >> 1;

	//MSB LED 3
	if(num & 0x1){
		//set LED 3 to high
		pxa_gpio_set_value(LED_PIN_3, 1);
	}
	else{
		//set LED 3 to low
		pxa_gpio_set_value(LED_PIN_3, 0);
	}

}

static irq_handler_t button0_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs){
	//button 0: released holds value, pressed counts by 1
	//printk(KERN_ALERT "State Interrupt\n");	
	state = !state;
	return IRQ_HANDLED;
	

}

static irq_handler_t button1_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs){
	//button 1: released sets count direction to down, pressed sets count direction to up
	//printk(KERN_ALERT "Direction Interrupt\n");	
	direction = !direction;
	return IRQ_HANDLED;

}

static void timer_handler(unsigned long data) {

	//Check if we need to update counter value
	if(state){

		//check counter direction
		if(direction){
			counter++;
		}
		else{
			counter--;
		}

	}

	//Update LEDs, if statements to have number wrap around
	counter = counter % 16;
	if(counter == 0 && direction == 1)
		counter = 1;
	else if(counter == 0 && direction == 0)
		counter = 15;
	LED_helper(counter);

	//Reset timer
	mod_timer(my_timer_ptr, jiffies + msecs_to_jiffies(frequency * 250));
	return;
 
}
