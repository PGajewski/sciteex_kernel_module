/*
*Modul kernelowy na zajecia z SOI
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/kobject.h>
//#include <sys/ioctl.h>


/*
 * Do debugowania: printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);


 */

MODULE_AUTHOR("Pawel Gajewski, SciTeeX Company");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Linux driver to gathering information from Onion IoT pin (or another Linux devices)");
MODULE_VERSION("0.1");

////////////////////////////////////Module parameters////////////////////////////

//Module log path.
static char* log_path = "/var/log/SciTeeXGPIO";

// GPIO number.
static unsigned int module_gpio = 17;

MODULE_PARM_DESC(module_gpio, "GPIO to control by module");

//Confirmation GPIO number.Repeat value from input/output declared in "module_gpio" variable.
static unsigned int confirm_gpio = 21;

MODULE_PARM_DESC(confirm_gpio, "GPIO to confirm value of GPIO controlled in this module");

// GPIO type
static bool gpio_is_input = true;

MODULE_PARM_DESC(gpio_is_input, "Flag declare input/output character of GPIO");

//Frequency
static unsigned int frequency = 100;

MODULE_PARM_DESC(frequency, "Frequency of binding gpio");

//Declare module parameters.
module_param(frequency, uint, S_IRUGO);
module_param(module_gpio, uint, S_IRUGO);
module_param(confirm_gpio, uint, S_IRUGO);
module_param(gpio_is_input, bool, S_IRUGO);

/////////////////////////////////Module logic variables///////////////////////////

//Mutex of this module.
static struct mutex mut;

//Logic variable of GPIO state.
static bool gpio_value = 0;

//Name of gpio in file sys.
static char gpio_name[7] = "gpioXX";
static char confirm_gpio_name[7] = "gpioXX";


//IRQ nunber for interrupting input.
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

//Duration of all input cycle.
static time_t* time_table;

//Time table alloc size. For begin is 10.
static unsigned int time_table_alloc_size = 10;

//Time table alloc increaser. Tells how much should grows capacity in realloc.
static unsigned int time_table_alloc_inc = 10;

//Time table real size.
static unsigned int time_table_real_size = 0;

///////////////////////////////////Show functions///////////////////////////////////
static ssize_t total_time_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", total_time);

}

static ssize_t confirm_gpio_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", total_time);

}

static ssize_t time_table_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

    if(time_table_real_size)
        return sprintf(buf, "Time table is ok.");
    else
        return sprintf(buf, "Time table is empty");
}

static ssize_t time_table_real_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", time_table_real_size);
   
}

static ssize_t number_presses_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", number_presses);

}


static ssize_t gpio_value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", gpio_value);

}

//////////////////////////////////Store functions///////////////////////////////////
/*
 * Store function works only if state of drives is output.
 */
static ssize_t gpio_value_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)

{

    //If state of driver is "input", return 0.
    if(gpio_is_input)
    {
        printk(KERN_ALERT "SciTeeX_GPIO: GPIO in input mode. Cannot store value.");
        return 0;
    }
    
    if(count != 0)
    {
        printk(KERN_ALERT "SciTeeX_GPIO: GPIO invalid argument!");
        return 0;
    }
    
    if(buf[0] == '1')
    {
        gpio_value = true;
    }
    else if(buf[0] == '0')
    {
        gpio_value = false;
    }
    else
    {
        printk(KERN_ALERT "SciTeeX_GPIO: GPIO invalid argument!");
        return 0;
    }
    

    if (mutex_lock_interruptible(&mut))
        return -ERESTARTSYS;

    
    //If argument is correct, set outputs.
    gpio_set_value(module_gpio, gpio_value);
    gpio_set_value(confirm_gpio, gpio_value);
    
out:
    mutex_unlock(&mut);
	return 1;

}

//////////////////////////////Driver attributes (sysfs)/////////////////////////////
static struct kobj_attribute gpio_value_attr = __ATTR(gpio_value, 0664, gpio_value_show, gpio_value_store); //0664 
static struct kobj_attribute confirm_gpio_attr = __ATTR_RO(confirm_gpio_name);
static struct kobj_attribute total_time_attr = __ATTR_RO(total_time);
static struct kobj_attribute time_table_attr  = __ATTR_RO(time_table);
static struct kobj_attribute time_table_size_attr  = __ATTR_RO(time_table_real_size);
static struct kobj_attribute number_presses_attr  = __ATTR_RO(number_presses);
// 
static struct attribute *sciteex_attrs[] = {

      &gpio_value_attr.attr,
      &confirm_gpio_attr.attr,
      &total_time_attr.attr,                 
      &time_table_attr.attr, 
      &time_table_size_attr.attr,
      &number_presses_attr.attr,
      NULL
};

//Driver kobject
static struct kobject *sciteex_kobj;

static struct attribute_group attr_group = {

      .name  = gpio_name,
      .attrs = sciteex_attrs
};

/////////////////////////////////////////////////Handlers/////////////////////////////////////////////////////

/**
 * Handlers for GPIO in input mode.
 * */

//Handler for rising input.
static irq_handler_t sciteex_rising_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){

    
    //Start counting time.
    struct timespec now;
    getnstimeofday(&now)
;
    begin_time = now.tv_sec;
    
    //Increase number of presses.
    ++number_presses;
    
    //Set confirm gpio value.
    gpio_value = 1;
    gpio_set_value(confirm_gpio, gpio_value);          // Set the physical LED accordingly

    printk(KERN_INFO "SciTeeX_GPIO: Interrupt! (Input state is %d)\n", gpio_get_value(module_gpio));

    return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly

}

//Handler for falling input.
static irq_handler_t sciteex_falling_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){


    //Stop counting time.
    struct timespec now;
    getnstimeofday(&now)
;
    end_time = now.tv_sec;
    
    //Set confirm gpio value.
    gpio_value = 0;
    
    //Count and add time to total time and time table.
    time_t process_time = end_time - begin_time;
    total_time += process_time;
    
    
    //Realloc time table, if needed.
    if(time_table_real_size == time_table_alloc_size)
    {
        //Count new alloc size.
        time_table_alloc_size += time_table_alloc_inc;
        
        //Alloc new space.
        time_t* temp = (time_t*) kmalloc(sizeof(time_t) * time_table_alloc_size, GFP_KERNEL);
        
        //Rewrite time table.
        int i;
        for(i = 0; i <time_table_real_size; ++i)
        {
            temp[i] = time_table[i];
        }
        
        //Free old table.
        kfree(time_table);
        
        //Set new pointer.
        time_table = temp;
        
    }
    
    time_table[time_table_real_size++] = process_time;
    
    gpio_set_value(confirm_gpio, gpio_value);

    printk(KERN_INFO "SciTeeX_GPIO: Interrupt! (Last process time in seconds: %d)\n", process_time);

    return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly

}

////////////////////////////////////////Init and clean functions/////////////////////////////////

static int sciteex_init(void)
{
    int result;
    
	printk(KERN_INFO "SciTeeX_GPIO: Init GPIO module for GPIO %d. \n", module_gpio);
    printk(KERN_INFO "SciTeeX_GPIO: Confirm GPIO is %d. \n", confirm_gpio);
    printk(KERN_INFO "The process is \"%s\" (pid %i)\n", current->comm, current->pid);

    //Check valid use of gpio.
    if (!gpio_is_valid(module_gpio) || !gpio_is_valid(confirm_gpio)){

      return -ENODEV;

    }
    
    // Create the gpio115 name for /sys/sciteex/gpioXXX
    sprintf(gpio_name, "gpio%d", module_gpio);
    sprintf(confirm_gpio_name, "gpio%d", confirm_gpio);


    // create the kobject sysfs entry at /sys/sciteex -- probably not an ideal location!

    sciteex_kobj = kobject_create_and_add("sciteex", kernel_kobj->parent); // kernel_kobj points to /sys/kernel

    if(!sciteex_kobj){

      printk(KERN_ALERT "SciTeeX_GPIO: failed to create kobject mapping\n");

      return -ENOMEM;

    }

    // add the attributes to /sys/sciteex/
    result = sysfs_create_group(sciteex_kobj, &attr_group);

    if(result) {

      printk(KERN_ALERT "SciTeeX_GPIO: failed to create sysfs group\n");

      //Remove the kobject sysfs entry
      kobject_put(sciteex_kobj);             
      return result;

    }

    //request hardoce of GPIO.
    gpio_request(module_gpio, "sysfs");
    gpio_request(confirm_gpio, "sysfs");
    
    //Set direction of GPIO.
    if (gpio_is_input)
    {
        gpio_direction_input(module_gpio);
        
        //Set IRQ handler on input.
        //Map GPIO number to IRQ number.
        irq_number = gpio_to_irq(module_gpio);

        printk(KERN_INFO "SciTeeX_GPIO: Input is mapped to IRQ: %d\n", irq_number);
        
        //Alloc space for time table.
        time_table = (time_t*) kmalloc(sizeof(time_t) * time_table_alloc_size, GFP_KERNEL);
    
        printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);
        
        //Set handlers.
        result = request_irq(irq_number,                                 // The interrupt number requested
                    (irq_handler_t) sciteex_rising_gpio_irq_handler,    // The pointer to the handler function below
                    IRQF_TRIGGER_RISING | IRQF_SHARED,                                // Interrupt on rising edge (input active)
                    "sciteex_gpio_rising_handler",                      // Used in /proc/interrupts to identify the owner
                    sciteex_kobj);                                              // Use kobject pointer as dev_id for shared interrupt lines
        
        //Check adding handler error.
        if(result)
            return result;
        
        printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);
        
        result = request_irq(irq_number,                                         
                    (irq_handler_t) sciteex_falling_gpio_irq_handler,  
                    IRQF_TRIGGER_FALLING | IRQF_SHARED,                               // Interrupt on falling edge (input release)
                    "sciteex_gpio_falling_handler",
                    sciteex_kobj);
        
        //Check adding handler error.
        if(result)
            return result;
    
    }
    else
    {
        //Set confirm_gpio.
        gpio_direction_output(confirm_gpio, gpio_value);
    }
    printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);

    //Make GPIO appear in /sys/gpio
    gpio_export(module_gpio, false);
    gpio_export(confirm_gpio, false);
    

    //Set confirm GPIO variable.
    gpio_set_value(confirm_gpio, gpio_value);
    
    printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);

    
    //Init semathore.
    mutex_init(&mut);
    
	return 0;
}

static void sciteex_cleanup(void)
{
    //Free GPIO.
    gpio_unexport(module_gpio);               
    gpio_free(module_gpio);
    
    gpio_unexport(confirm_gpio);               
    gpio_free(confirm_gpio);
    
    //Remove GPIO from irq, if GPIO is input.
    if(gpio_is_input)
        free_irq(irq_number, sciteex_kobj);
    
    //Free time table.
    kfree(time_table);
    
    //Delete KObject from sysfs.
    kobject_put(sciteex_kobj); 
    
	printk(KERN_INFO "SciTeeX_GPIO: Clean GPIO module for GPIO $u.\n", module_gpio);
}


module_init(sciteex_init);

module_exit(sciteex_cleanup);
