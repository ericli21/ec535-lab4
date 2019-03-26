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

static void LED_helper(int num);
static irq_handler_t button0_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs);
static irq_handler_t button1_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs);
static void timer_handler(unsigned long data);

//initial count is 1, direction is 0 (down), state is 0 (hold value)
static int counter = 1;
static int direction = 1;
static int state = 0;

//initial timer frequency is 1HZ (1 second), can be fractional
static int frequency = 1;
static struct timer_list * my_timer_ptr;
static struct timer_list my_timer;

// define LED and button pins here
#define LED_PIN_0 28
#define LED_PIN_1 29
#define LED_PIN_2 30
#define LED_PIN_3 31

#define BUTTON_PIN_0 17
#define BUTTON_PIN_1 101

static struct proc_dir_entry *proc_gpio_parent;
static struct proc_dir_entry *proc_gpios[PXA_LAST_GPIO + 1];

typedef struct
{
	int	gpio;
	char    name[32];
} gpio_summary_type;

static gpio_summary_type gpio_summaries[PXA_LAST_GPIO + 1];

static int proc_gpio_write(struct file *file, const char __user *buf,
                           unsigned long count, void *data)
{

	//extra functionality: writing f(1-9) changes timer frequency to n/4
	//extra functionality: writing v(hex) sets count value to given hex value

	//add checks for null input after an f or a v 

	//if count > 1

	//if buffer[0] == f
		//number = atoi(buffer[1])
		//if number < 10 && number >= 1
			//frequency = number/4.0;

	//if buffer[0] == v
		//get next char
		//A is 65, F is 70 in ASCII
		//if(buffer[1] >= '0' && buffer[1] <= '9' && (buffer[2] is NULL or a newline))
			//counter = atoi(buffer[1]);
		//else if(buffer[1] >= 'A' && buffer[1] <= 'F' && (buffer[2] is NULL or a newline))
			//counter = atoi(buffer[1] - 55);

	//We probably don't need anything beyond this point
/*

	char *cur, lbuf[count + 1];
	gpio_summary_type *summary = data;
	u32 altfn, direction, setclear, gafr;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	memset(lbuf, 0, count + 1);

	if (copy_from_user(lbuf, buf, count))
		return -EFAULT;

	cur = lbuf;

	// Initialize to current state
	altfn = ((GAFR(summary->gpio) >> ((summary->gpio & 0x0f) << 0x01)) & 0x03);
	direction = GPDR(summary->gpio) & GPIO_bit(summary->gpio);
	setclear = GPLR(summary->gpio) & GPIO_bit(summary->gpio);
	while(1)
	{
		// We accept options: {GPIO|AF1|AF2|AF3}, {set|clear}, {in|out}
		// Anything else is an error
		while(cur[0] && (isspace(cur[0]) || ispunct(cur[0]))) cur = &(cur[1]);

		if('\0' == cur[0]) break;

		// Ok, so now we're pointing at the start of something
		switch(cur[0])
		{
			case 'G':
				// Check that next is "PIO" -- '\0' will cause safe short-circuit if end of buf
				if(!(cur[1] == 'P' && cur[2] == 'I' && cur[3] == 'O')) goto parse_error;
				// Ok, so set this GPIO to GPIO (non-ALT) function
				altfn = 0;
				cur = &(cur[4]);
				break;
			case 'A':
				if(!(cur[1] == 'F' && cur[2] >= '1' && cur[2] <= '3')) goto parse_error;
				altfn = cur[2] - '0';
				cur = &(cur[3]);
				break;
			case 's':
				if(!(cur[1] == 'e' && cur[2] == 't')) goto parse_error;
				setclear = 1;
				cur = &(cur[3]);
				break;
			case 'c':
				if(!(cur[1] == 'l' && cur[2] == 'e' && cur[3] == 'a' && cur[4] == 'r')) goto parse_error;
				setclear = 0;
				cur = &(cur[5]);
				break;
			case 'i':
				if(!(cur[1] == 'n')) goto parse_error;
				direction = 0;
				cur = &(cur[2]);
				break;
			case 'o':
				if(!(cur[1] == 'u' && cur[2] == 't')) goto parse_error;
				direction = 1;
				cur = &(cur[3]);
				break;
			default: goto parse_error;
		}
	}
	// Ok, now set gpio mode and value
	if(direction)
		GPDR(summary->gpio) |= GPIO_bit(summary->gpio);
	else
		GPDR(summary->gpio) &= ~GPIO_bit(summary->gpio);

	gafr = GAFR(summary->gpio) & ~(0x3 << (((summary->gpio) & 0xf)*2));
	GAFR(summary->gpio) = gafr |  (altfn  << (((summary->gpio) & 0xf)*2));

	if(direction && !altfn)
	{
		if(setclear) GPSR(summary->gpio) = GPIO_bit(summary->gpio);
		else GPCR(summary->gpio) = GPIO_bit(summary->gpio);
	}

#ifdef CONFIG_PROC_GPIO_DEBUG
	printk(KERN_INFO "Set (%s,%s,%s) via /proc/gpio/%s\n",altfn ? (altfn == 1 ? "AF1" : (altfn == 2 ? "AF2" : "AF3")) : "GPIO",
				direction ? "out" : "in",
				setclear ? "set" : "clear",
				summary->name);
#endif
*/
	return count;

parse_error:
	printk(KERN_CRIT "Parse error: Expect \"[GPIO|AF1|AF2|AF3]|[set|clear]|[in|out] ...\"\n");
	return -EINVAL;
}

static int proc_gpio_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{

	//extra functionality: reading from /dev/mygpio returns:
	//counter value, counter frequency, counter direction, counter state
	//int len = sprintf(page,"%d\t%.3f\t%d\t%d\n", counter, frequency, direction, state);

	int len;

	/*
	char *p = page;
	gpio_summary_type *summary = data;
	int len, i, af;
	i = summary->gpio;

	p += sprintf(p, "%d\t%s\t%s\t%s\n", i,
			(af = ((GAFR(i) >> ((i & 0x0f) << 0x01)) & 0x03)) ? (af == 1 ? "AF1" : (af == 2 ? "AF2" : "AF3")) : "GPIO",
			(GPDR(i) & GPIO_bit(i)) ? "out" : "in",
			(GPLR(i) & GPIO_bit(i)) ? "set" : "clear");

	len = (p - page) - off;

	if(len < 0)
	{
		len = 0;
	}

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;
	*/
	return len;
}
/*
//We probably don't need these other read functions

#ifdef CONFIG_PXA25x
static const char const *GAFR_DESC[] = { "GAFR0_L", "GAFR0_U", "GAFR1_L", "GAFR1_U", "GAFR2_L", "GAFR2_U" };
#elif defined(CONFIG_PXA27x)
static const char const *GAFR_DESC[] = { "GAFR0_L", "GAFR0_U", "GAFR1_L", "GAFR1_U", "GAFR2_L", "GAFR2_U", "GAFR3_L", "GAFR3_U" };
#endif

static int proc_gafr_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	char *p = page;
	int i, len;

	for(i=0; i<ARRAY_SIZE(GAFR_DESC); i++)
	{
		p += sprintf(p, "%s: %08x\n", GAFR_DESC[i], GAFR(i*16));
	}

	len = (p - page) - off;

	if(len < 0)
	{
		len = 0;
	}

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int proc_gpdr_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	char *p = page;
	int i, len;

	for(i=0; i<=2; i++)
	{
		p += sprintf(p, "GPDR%d: %08x\n", i, GPDR(i * 32));
	}

	len = (p - page) - off;

	if(len < 0)
	{
		len = 0;
	}

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int proc_gplr_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	char *p = page;
	int i, len;

	for(i=0; i<=2; i++)
	{
		p += sprintf(p, "GPLR%d: %08x\n", i, GPLR(i * 32));
	}

	len = (p - page) - off;

	if(len < 0)
	{
		len = 0;
	}

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}
*/
static int __init gpio_init(void)
{
	int i;

  	//initialize LED pins here, set as output
	int led0 = pxa_gpio_mode(LED_PIN_0 | GPIO_OUT);
	int led1 = pxa_gpio_mode(LED_PIN_1 | GPIO_OUT);
	int led2 = pxa_gpio_mode(LED_PIN_2 | GPIO_OUT);
	int led3 = pxa_gpio_mode(LED_PIN_3 | GPIO_OUT);

	pxa_gpio_set_value(LED_PIN_0, 0);
	pxa_gpio_set_value(LED_PIN_1, 0);
	pxa_gpio_set_value(LED_PIN_2, 0);
	pxa_gpio_set_value(LED_PIN_3, 0);

	//set up buttons as input
	pxa_gpio_mode(BUTTON_PIN_0 | GPIO_IN);
	pxa_gpio_mode(BUTTON_PIN_1 | GPIO_IN);

	int irq0 = IRQ_GPIO(BUTTON_PIN_0);
	int irq1 = IRQ_GPIO(BUTTON_PIN_1);


	//Set up interrupt
	if (request_irq(irq0, &button0_interrupt, SA_INTERRUPT | SA_TRIGGER_RISING | SA_TRIGGER_FALLING,
				"mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }else{
                printk ( "irq %d acquired successfully \n", irq0 );
	}

	if (request_irq(irq1, &button1_interrupt, SA_INTERRUPT | SA_TRIGGER_RISING | SA_TRIGGER_FALLING,
				"mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }else{
                printk ( "irq %d acquired successfully \n", irq1 );
	}

  	//Set up timer
	my_timer_ptr = &my_timer;
	//printk(KERN_ALERT "Made it\n");
	init_timer(my_timer_ptr);
	//printk(KERN_ALERT "Made it\n");
	(*my_timer_ptr).expires = jiffies + msecs_to_jiffies(frequency * 1000);
	//printk(KERN_ALERT "Made it\n");
	(*my_timer_ptr).function = timer_handler;

	add_timer(my_timer_ptr);


	LED_helper(counter);


  	//Probably don't need anything beyond this point, all proc setup



/*
  proc_gpio_parent = NULL;
  for (proc_gpio_parent = proc_root.subdir; proc_gpio_parent; proc_gpio_parent = proc_gpio_parent->next) {
    if (( proc_gpio_parent->namelen == 4 ) && ( memcmp(proc_gpio_parent->name, "gpio", 4 ) == 0 ))
      break;
  }

  // proc_gpio_parent will be non-NULL if the directory already exists

  if (!proc_gpio_parent) {
       proc_gpio_parent = proc_gpio_parent = create_proc_entry("gpio", S_IFDIR | S_IRUGO | S_IXUGO, NULL);
  }

 	if(!proc_gpio_parent) return 0;

	for(i=0; i < (PXA_LAST_GPIO+1); i++)
	{
		gpio_summaries[i].gpio = i;
		sprintf(gpio_summaries[i].name, "GPIO%d", i);
		proc_gpios[i] = create_proc_entry(gpio_summaries[i].name, 0644, proc_gpio_parent);
		if(proc_gpios[i])
		{
			proc_gpios[i]->data = &gpio_summaries[i];
			proc_gpios[i]->read_proc = proc_gpio_read;
			proc_gpios[i]->write_proc = proc_gpio_write;
		}
	}

	create_proc_read_entry("GAFR", 0444, proc_gpio_parent, proc_gafr_read, NULL);
	create_proc_read_entry("GPDR", 0444, proc_gpio_parent, proc_gpdr_read, NULL);
	create_proc_read_entry("GPLR", 0444, proc_gpio_parent, proc_gplr_read, NULL);
*/
	return 0;
}

static void gpio_exit(void)
{
	//delete timer if there is one
	//Probably don't need the rest of this because it's all proc cleanup


	int i;
	del_timer(my_timer_ptr);
	free_irq(IRQ_GPIO(BUTTON_PIN_0), NULL);
	free_irq(IRQ_GPIO(BUTTON_PIN_1), NULL);
/*
	remove_proc_entry("GAFR", proc_gpio_parent);
	remove_proc_entry("GPDR", proc_gpio_parent);
	remove_proc_entry("GPLR", proc_gpio_parent);

	for(i=0; i < (PXA_LAST_GPIO+1); i++)
	{
		if(proc_gpios[i]) remove_proc_entry(gpio_summaries[i].name, proc_gpio_parent);
	}

  if (proc_gpio_parent)
  {
    if (!proc_gpio_parent->subdir)
    {
      // Only remove /proc/gpio if it's empty.
      remove_proc_entry( "gpio", NULL );
    }
  }
*/

}

static void LED_helper(int num){

	//use bitwise AND to check if the current bit should light the LED
	printk(KERN_ALERT "%d\n", num);

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
	printk(KERN_ALERT "State Interrupt\n");	
	state = !state;
	return IRQ_HANDLED;
	

}

static irq_handler_t button1_interrupt(unsigned int irq, void *dev_id, struct pt_regs *regs){
	//button 1: released sets count direction to down, pressed sets count direction to up
	printk(KERN_ALERT "Direction Interrupt\n");	
	direction = !direction;
	return IRQ_HANDLED;

}

static void timer_handler(unsigned long data) {

	//check if we need to update counter value
	printk(KERN_ALERT "In Timer handler\n");
	if(state){

		//check counter direction
		if(direction){
			counter++;
		}
		else{
			counter--;
		}

	}

	//Update LEDs
	counter = counter % 16;
	if(counter == 0 && direction == 1)
		counter = 1;
	else if(counter == 0 && direction == 0)
		counter = 15;
	LED_helper(counter);

	//modify timer to frequency * seconds
	mod_timer(my_timer_ptr, jiffies + msecs_to_jiffies(frequency * 1000));
	return;
 
}

module_init(gpio_init);
module_exit(gpio_exit);
MODULE_LICENSE("GPL");
