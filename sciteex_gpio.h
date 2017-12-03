/*
 * Moduł na zajęcia z SOI.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <scsi/scsi.h>

MODULE_AUTHOR("Pawel Gajewski, SciTeeX Company");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Linux driver to gathering information from Onion IoT pin (or another Linux devices)");
MODULE_VERSION("0.2");
MODULE_INFO(intree, "Y");

///////////////////////////////////Char device data//////////////////////////////
//Name of device in /proc
#define DEVICE_NAME "sciteex_gpio"
static int major;

////////////////////////////////////Structures///////////////////////////////////
////////////////////////////////////Time list////////////////////////////////////
// Time 
struct cycle_time
{
    time_t time;
    struct list_head list;
};

static struct cycle_time time_list;

//////////////////////////////////Character device///////////////////////////////
////////////////////////////////////IOCTL codes//////////////////////////////////
#define IOCTL_RESET 1
static int sciteex_open(struct inode *inode, struct file *file);
static int sciteex_release(struct inode *inode, struct file *file);
static ssize_t sciteex_read(struct file *filp,char *buffer, size_t length, loff_t *offset);
static ssize_t sciteex_write(struct file *filp, const char *buff, size_t len, loff_t *off);

static int sciteex_ioctl(struct inode *inode, struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);

static struct file_operations sciteex_oper = {
  .read = sciteex_read,
  .write = sciteex_write,
  .open = sciteex_open,
  .release = sciteex_release
};

////////////////////////////////////Module parameters////////////////////////////

//Module log path.
static char* log_path = "/var/log/SciTeeXGPIO";

// GPIO number.
static unsigned int module_gpio = 17;

MODULE_PARM_DESC(module_gpio, "GPIO to control by module");

//Confirmation GPIO number.Repeat value from input/output declared in "module_gpio" variable.
static unsigned int confirm_gpio = 27;

MODULE_PARM_DESC(confirm_gpio, "GPIO to confirm value of GPIO controlled in this module");

// GPIO type
static bool gpio_is_input = true;

MODULE_PARM_DESC(gpio_is_input, "Flag declare input/output character of GPIO");

//Frequency
static unsigned int frequency = 100;

MODULE_PARM_DESC(frequency, "Frequency of binding gpio");

//Input optional debounce.
static unsigned int debounce = 200;

MODULE_PARM_DESC(debounce, "Debounce of gpio in input mode (in miliseconds).");

//Declare module parameters.
module_param(frequency, uint, S_IRUGO);
module_param(module_gpio, uint, S_IRUGO);
module_param(confirm_gpio, uint, S_IRUGO);
module_param(debounce, uint, S_IRUGO);
module_param(gpio_is_input, bool, S_IRUGO);

/////////////////////////////////Module logic variables///////////////////////////

//Synchronization and lock.
//Mutex of this module.
static DEFINE_MUTEX(mut);

//Logic variable of GPIO state.
static bool gpio_value = 0;

//Name of gpio in file sys.
static char gpio_name[7] = "gpioXX";
static char confirm_gpio_name[7] = "gpioXX";

//IRQ number for interrupting input.
static unsigned int irq_number;

//Number of presses on input mode.
static unsigned int number_presses = 0;

/*******Time variable********/

//Epoch of rising input/open output.
static time_t begin_time = 0;

//Epoch of falling input/close output.
static time_t end_time = 0;

//Total process time.
static time_t total_time = 0;

///////////////////////////////////Show functions///////////////////////////////////
static ssize_t total_time_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

static ssize_t confirm_gpio_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

static ssize_t number_presses_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

static ssize_t gpio_value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

//////////////////////////////Driver attributes (sysfs)/////////////////////////////
static struct kobj_attribute gpio_value_attr = __ATTR_RO(gpio_value);
static struct kobj_attribute confirm_gpio_attr = __ATTR_RO(confirm_gpio_name);
static struct kobj_attribute total_time_attr = __ATTR_RO(total_time);
static struct kobj_attribute number_presses_attr  = __ATTR_RO(number_presses);
// 
static struct attribute *sciteex_attrs[] = {

      &gpio_value_attr.attr,
      &confirm_gpio_attr.attr,
      &total_time_attr.attr,                 
      &number_presses_attr.attr,
      NULL
};

//Driver kobject
static struct kobject *sciteex_kobj;

static struct attribute_group attr_group = {

      .name  = gpio_name,
      .attrs = sciteex_attrs
};

///////////////////////////////////////////////Help functions/////////////////////////////////////////////////
//
static void reset(void);

/////////////////////////////////////////////////Handlers/////////////////////////////////////////////////////

/**
 * Handlers for GPIO in input mode.
 * */

//Handler for rising input.
static irq_handler_t sciteex_gpio_rising_handler(void);

//Handler for falling input.
static irq_handler_t sciteex_gpio_falling_handler(void);

static irq_handler_t sciteex_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);
////////////////////////////////////////Init and clean functions/////////////////////////////////

static int sciteex_init(void);

static void sciteex_cleanup(void);
