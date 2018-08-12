
#include <linux/module.h>
//#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "owtherm.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jp, 2015");
MODULE_DESCRIPTION("ds18b20 therm sensor, onewire robust design");

static unsigned int gpio_in = 31; //P9-13, UART4_TXD
module_param(gpio_in, uint, (S_IRUSR | S_IRGRP | S_IROTH));
MODULE_PARM_DESC(gpio_in, "gpio in");

static unsigned int gpio_out = 1*32+17; //P9-23
module_param(gpio_out, uint, (S_IRUSR | S_IRGRP | S_IROTH));
MODULE_PARM_DESC(gpio_out, "gpio out");

static unsigned int gpio_strong_power = 1*32+16; //P9-15
module_param(gpio_strong_power, uint, (S_IRUSR | S_IRGRP | S_IROTH));
MODULE_PARM_DESC(gpio_strong_power, "gpio strong power");

static unsigned long owflag = 0;
#define OWFLAG_MODULE_USED    1
#define OWFLAG_STRONG_PWR_REQ 2
#define OWTHERM_MAX_IO_SIZE 16

static void write_bus(int v)
{
	gpio_set_value(gpio_out, !v);
}

static int read_bus(void)
{
	return gpio_get_value(gpio_in) ? 0 : 1;
}

static void strong_power(int v)
{
	gpio_set_value(gpio_strong_power, v);
}

static void write_bit(int bit, int strong_pwr)
{
	unsigned long flags;
        const unsigned lowtime = bit ? 9 : 60;
	local_irq_save(flags);

	write_bus(0);
	udelay(lowtime);
	write_bus(1);
	if(strong_pwr)
		strong_power(1);
	udelay(80 - lowtime );

	local_irq_restore(flags);
}

static int read_bit(void)
{
	unsigned long flags;
	local_irq_save(flags);

	write_bus(0);
	udelay(9);
	write_bus(1);
	udelay(9);
	int bit = read_bus();

	local_irq_restore(flags);
	udelay(62);
	return bit;
}

static int search_triplet(int param)
{
	int a;

	strong_power(0);

	a = read_bit();
	a |= read_bit() << 1;

	if(a==3)
		return a;

	if(a)
		param = a == 1;

	write_bit(param, 0);

	return a;
}

static int bus_reset_pulse(void)
{
	unsigned long flags;

	if(!read_bus()){
		return OWTHERM_ERR_BUS_SHORT;
	}

	strong_power(0);

	local_irq_save(flags);
	write_bus(0);
	udelay(490);
	if(read_bus()){
		goto out_bus_invalid;
	}
	write_bus(1);
	udelay(80);
	int presence = !read_bus();
	local_irq_restore(flags);
        
	unsigned timeout = 240;
	while(!read_bus()){
		if(--timeout == 0){
			return OWTHERM_ERR_BUS_SHORT;
		}
		udelay(1);
	}
	udelay(80);

	return presence ? 0 : OWTHERM_ERR_NO_PRESENCE;

out_bus_invalid:
	write_bus(1);
	local_irq_restore(flags);
	return OWTHERM_ERR_BUS_INVALID;
}

static int owtherm_open(struct inode *inode, struct file *file)
{
	if(!test_and_set_bit(OWFLAG_MODULE_USED, &owflag)){
		printk(KERN_INFO "ow open\n");
		return 0;
	}
	return -EBUSY;
}

static int owtherm_close(struct inode *i, struct file *f)
{
	clear_bit(OWFLAG_MODULE_USED, &owflag);
	return 0;
}

static long owtherm_ioctl(struct file *f, unsigned int req, unsigned long param)
{
	int tmp;
	switch(req)
	{
		case OWTHERM_IOCTL_PRESENCE: //reset pulse
			tmp = bus_reset_pulse();
			if(copy_to_user((void*)param, &tmp, sizeof(tmp))) return -EFAULT;
			break;
		case OWTHERM_IOCTL_STRONG_PWR:
			if(copy_from_user(&tmp, (void*)param, sizeof(tmp)))	return -EFAULT;
			if(tmp)
				set_bit(OWFLAG_STRONG_PWR_REQ, &owflag);
			else
				strong_power(0);
			break;
		case OWTHERM_IOCTL_SERCH_TRIPLET:
			if(copy_from_user(&tmp, (void*)param, sizeof(tmp)))	return -EFAULT;
			tmp = search_triplet(tmp);
			if(copy_to_user((void*)param, &tmp, sizeof(tmp))) return -EFAULT;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static ssize_t owtherm_read(struct file *f, char __user *u, size_t size, loff_t *off)
{
	if(size > OWTHERM_MAX_IO_SIZE)
		return -EINVAL;

	strong_power(0);

	u8 tmp[size];
	memset(tmp, '\0', size);
	for(int i=0; i < size*8; ++i){
		int byte = i/8, bit = i%8;
		tmp[byte] |= read_bit() << bit;
	}

	if(copy_to_user(u, tmp, size))
		return -EFAULT;

	return size;
}

static ssize_t owtherm_write(struct file *f, const char __user *u, size_t size, loff_t *off)
{
	if(size > OWTHERM_MAX_IO_SIZE)
		return -EINVAL;

	u8 tmp[size];
	if(copy_from_user(tmp, u, size))
		return -EFAULT;

	strong_power(0);

	int strong_pwr_req_tmp = 0;
	if(test_and_clear_bit(OWFLAG_STRONG_PWR_REQ, &owflag))
		strong_pwr_req_tmp = 1;

	for(int i=0; i < size*8; ++i){
		int byte = i/8, mask = 1 << i%8;
		write_bit( tmp[byte] & mask,
				   i == size*8-1 ? strong_pwr_req_tmp : 0);
	}

	return size;
}

static struct file_operations owtherm_fops = {
    .owner = THIS_MODULE,
	.open = &owtherm_open,
	.release = &owtherm_close,
	.unlocked_ioctl = &owtherm_ioctl,
	.compat_ioctl = &owtherm_ioctl,
	.read = &owtherm_read,
	.write = &owtherm_write,
    .llseek = &noop_llseek,
};

static struct miscdevice owtherm_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "owtherm",
    .fops = &owtherm_fops
};

static int __init ow_init(void)
{
	printk(KERN_INFO "owtherm\n");

	const struct gpio gpio_set[] = {
			{ gpio_in, 	GPIOF_DIR_IN, "in" },
			{ gpio_out, GPIOF_INIT_LOW, "out" },
			{ gpio_strong_power, GPIOF_INIT_LOW, "strong5V" },
	};

	int err = gpio_request_array(gpio_set, 3);
	if(!err){
		err = misc_register(&owtherm_misc_device);
		if(!err){
			return 0;
		}
		else{
			gpio_free_array(gpio_set, 3);
		}
	}
	printk(KERN_ERR "owtherm init failed\n");
	return err;
}

static void __exit ow_fini(void)
{

	misc_deregister(&owtherm_misc_device);

	const struct gpio gpio_set[] = {
			{ gpio_in, 	GPIOF_DIR_IN, "in" },
			{ gpio_out, GPIOF_INIT_LOW, "out" },
			{ gpio_strong_power, GPIOF_INIT_LOW, "strong5V" },
	};
	gpio_free_array(gpio_set, 3);

	printk(KERN_INFO "owtherm removed\n");
}

module_init(ow_init);
module_exit(ow_fini);

