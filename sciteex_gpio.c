/*
*Modul kernelowy na zajecia z SOI
*/
#include "sciteex_gpio.h"
///////////////////////////////////Char device functions////////////////////////////



// Called when a process tries to open the device file, like "cat /dev/mycharfile"


static int sciteex_open(struct inode *inode, struct file *file)

{

    //Try to allocate mutex.
   if(!mutex_trylock(&mut)){

      printk(KERN_ALERT "SciTeeX_GPIO: Device in use by another process");

      return -EBUSY;

   }
   
   
   //Deprecated.
   //MOD_INC_USE_COUNT;



   return SUCCESS;

}



// Called when a process closes the device file.
static int sciteex_release(struct inode *inode, struct file *file)

{

    mutex_unlock(&mut);
    
    //MOD_DEC_USE_COUNT;

    return 0;
}





// Called when a process, which already opened the dev file, attempts to read from it.

static ssize_t sciteex_read(struct file *filp,char *buffer, size_t length, loff_t *offset)
{

    //If state of driver is "output", return EINVAL.
    if(!gpio_is_input)
    {
        printk(KERN_ALERT "SciTeeX_GPIO: GPIO in output mode. Cannot read value.");
        return -EINVAL;
    }


	if (list_empty(&time_list.list))
    {
        printk(KERN_INFO "SciTeeX_GPIO: Nothing to read.");
        return 0;
    }


    //Read value from list.
    struct list_head *node;
    struct cycle_time *time_entry = list_entry(node, struct cycle_time, list);
    time_t temp = time_entry->time;
    
    //Delete node.
    list_del(node);
    
   return sprintf(buffer, "%d\n", temp);
}





//  Called when a process writes to dev file: echo "hi" > /dev/hello.
static ssize_t sciteex_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{

    //If state of driver is "input", return EINVAL.
    if(gpio_is_input)
    {
        printk(KERN_ALERT "SciTeeX_GPIO: GPIO in input mode. Cannot write value.");
        return -EINVAL;
    }
    
    //Validate size of buffor.
    if(len != 1)
    {
        printk(KERN_ALERT "SciTeeX_GPIO: GPIO invalid argument!");
        return -EINVAL;
    }        
    
    //If argument is correct, set outputs.
    gpio_set_value(module_gpio, gpio_value);
    gpio_set_value(confirm_gpio, gpio_value);
    
    return 1;
}

static int sciteex_ioctl(struct inode *inode, struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	switch(ioctl_num)
	{
		case IOCTL_RESET: reset(); break;
	}
}


///////////////////////////////////Show functions///////////////////////////////////
static ssize_t total_time_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", total_time);

}

static ssize_t confirm_gpio_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", total_time);

}
   
static ssize_t number_presses_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", number_presses);

}


static ssize_t gpio_value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){

   return sprintf(buf, "%d\n", gpio_value);

}

///////////////////////////////////////////////Help functions/////////////////////////////////////////////////
//
//Function to reset timers, number of presses and total time.
static void reset(void)
{
	//Reset attributes.
	total_time = 0;
	number_presses = 0;
	
	//Delete times list.
    struct cycle_time *time_entry, *tmp;
    list_for_each_entry_safe(time_entry, tmp, &time_list.list, list)
    {
		list_del(&time_entry->list);
	}
}

/////////////////////////////////////////////////Handlers/////////////////////////////////////////////////////

/**
 * Handlers for GPIO in input mode.
 */

//Handler for rising input.
static irq_handler_t sciteex_gpio_rising_handler(void){

    //Start counting time.
    struct timespec now;
    getnstimeofday(&now);
    
    begin_time = now.tv_sec * 1000 + (now.tv_nsec / 1000000);
    
    //Increase number of presses.
    ++number_presses;

    return (irq_handler_t) IRQ_HANDLED;

}

//Handler for falling input.
static irq_handler_t sciteex_gpio_falling_handler(void){

    //Stop counting time.
    struct timespec now;
    getnstimeofday(&now);
    
    //Create new list object.
    struct cycle_time* new_time;
    new_time = kmalloc(sizeof(*new_time), GFP_KERNEL);


    
    end_time = now.tv_sec * 1000 + (now.tv_nsec / 1000000);
        
    //Count and add time to total time and time table.
    time_t process_time = end_time - begin_time;
    new_time->time = process_time;
    total_time += process_time;
    
    //Add a list node.
    list_add_tail(&new_time->list, &time_list.list);

    //Print times
    printk(KERN_INFO "SciTeeX GPIO: Last operation time: %d msec. Total time: %d msec", process_time, total_time);
    
    return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly

}

static irq_handler_t sciteex_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
    
     //Set confirm gpio value.
    gpio_value = gpio_get_value(module_gpio);
    gpio_set_value(confirm_gpio, gpio_value);
    
    //Get IRQ flag
    printk(KERN_INFO "SciTeeX_GPIO: Interrupt on %d! Value: %d", module_gpio, gpio_value);
    irq_handler_t result;
    
    switch(gpio_get_value(module_gpio))
    {
        case 0: result = sciteex_gpio_falling_handler(); break;
        case 1: result = sciteex_gpio_rising_handler(); break;
    }
    return result;
    
}
////////////////////////////////////////Init and clean functions/////////////////////////////////

static int sciteex_init(void)
{
    int result = 0;
    
	printk(KERN_INFO "SciTeeX_GPIO: Init GPIO module for GPIO %d. \n", module_gpio);
    printk(KERN_INFO "SciTeeX_GPIO: Confirm GPIO is %d. \n", confirm_gpio);
    printk(KERN_INFO "The process is \"%s\" (pid %i)\n", current->comm, current->pid);

    //Check valid use of gpio.
    if (!gpio_is_valid(module_gpio) || !gpio_is_valid(confirm_gpio)){

      return -ENODEV;

    }
    
     major = register_chrdev(0, DEVICE_NAME, &sciteex_oper);



   if (major < 0) {

        printk (KERN_INFO "SciTeeX_GPIO: Registering the character device failed with %d\n", major);

     return major;

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
        gpio_set_debounce(module_gpio, debounce);        
                
    }
    else
    {        
        //Set confirm_gpio.
        gpio_direction_output(module_gpio, 0);
    }
    
        //Set IRQ handler on input.
        //Map GPIO number to IRQ number.
        irq_number = gpio_to_irq(module_gpio);

        printk(KERN_INFO "SciTeeX_GPIO: Input is mapped to IRQ: %d\n", irq_number);
        
        //Reset old handlers.
        //free_irq(irq_number, 0);
        
        //Set handlers.
        result = request_irq(irq_number,                                // The interrupt number requested
                    (irq_handler_t) sciteex_gpio_irq_handler,           // The pointer to the handler function below
                    IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,         // Interrupt on rising edge (input active)
                    "sciteex_gpio_rising_handler",                      // Used in /proc/interrupts to identify the owner
                    sciteex_kobj);                                      // Use kobject pointer as dev_id for shared interrupt lines
        
        //Check adding handler error.
        if(result)
            return result;

    //Init time list.
    INIT_LIST_HEAD(&time_list.list);
    
    //Set confirm_gpio.
    gpio_value = gpio_get_value(module_gpio);
    gpio_direction_output(confirm_gpio, gpio_value);
    
    //Make GPIO appear in /sys/gpio
    gpio_export(module_gpio, false);
    gpio_export(confirm_gpio, false);
    
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
    
    // Unregister the device.
    unregister_chrdev(major, DEVICE_NAME);

	printk(KERN_INFO "SciTeeX_GPIO:Error in unregister device for GPIO %u.\n", module_gpio);
        
    //Delete KObject from sysfs.
    kobject_put(sciteex_kobj); 
    
    //Destroy inner list.
    struct cycle_time *time_entry, *tmp;
    list_for_each_entry_safe(time_entry, tmp, &time_list.list, list)
    {
		list_del(&time_entry->list);
	}
    
    //Destroy mutex.
    mutex_destroy(&mut);
    
    printk(KERN_INFO "SciTeeX_GPIO: Clean GPIO module for GPIO %u.\n", module_gpio);
}

module_init(sciteex_init);
module_exit(sciteex_cleanup);
