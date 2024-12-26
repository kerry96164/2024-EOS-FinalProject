#include <linux/kernel.h> 
#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/kdev_t.h> 
#include <linux/fs.h> 
#include <linux/cdev.h> 
#include <linux/device.h> 
#include <linux/delay.h> 
#include <linux/uaccess.h>  //copy_to/from_user() 
#include <linux/gpio.h>     //GPIO
#include <linux/pwm.h>      //PWM
 
#define COUNT 4
#define RIGHT_BACKWARD_PIN  (23) // 右 後退
#define RIGHT_FORWARD_PIN  (24) // 右 前進
#define LEFT_BACKWARD_PIN  (17) // 左 後退
#define LEFT_FORWARD_PIN  (27) // 左 前進
#define PWM_LEFT_CHANNEL 0    // 樹莓派的 PWM 0 GPIO18 PIN12
#define PWM_RIGHT_CHANNEL 1   // 樹莓派的 PWM 1 GPIO19 PIN35

  
dev_t dev = 0; 
static int gpios[COUNT] = {RIGHT_BACKWARD_PIN , RIGHT_FORWARD_PIN, LEFT_BACKWARD_PIN , LEFT_FORWARD_PIN }; // Pin 16 18 11 13
static struct class *dev_class; 
static struct cdev etx_cdev;
static struct pwm_device *pwm_left = NULL;   // PWM 設備（左）
static struct pwm_device *pwm_right = NULL;  // PWM 設備（右）
  
static int __init etx_driver_init(void); 
static void __exit etx_driver_exit(void);

void set_left_gpio(int lb, int lf){
    gpio_set_value(LEFT_BACKWARD_PIN, lb);
    gpio_set_value(LEFT_FORWARD_PIN, lf);
}
void set_right_gpio(int rb, int rf){
    gpio_set_value(RIGHT_BACKWARD_PIN, rb);
    gpio_set_value(RIGHT_FORWARD_PIN, rf);
}

void set_pwm_duty_cycle(struct pwm_device *pwm, int duty_cycle){
    struct pwm_state state;
    pwm_get_state(pwm, &state);
    if (state.period == 0) {
        state.period = 20000000; // 默認 20ms 週期
    }

    state.duty_cycle = state.period * duty_cycle / 100; // 設置占空比
    // 禁用 PWM
    if (pwm_is_enabled(pwm)) {
        pwm_disable(pwm);
    }
    pwm_apply_state(pwm, &state);                      // 應用設置
    pwm_enable(pwm);                       // 啟用 PWM
}

  
/*************** Driver functions **********************/ 
static int     etx_open(struct inode *inode, struct file *file); 
static int     etx_release(struct inode *inode, struct file *file); 
static ssize_t etx_read(struct file *filp,  
                char __user *buf, size_t len,loff_t * off); 
static ssize_t etx_write(struct file *filp,  
                const char *buf, size_t len, loff_t * off); 
/******************************************************/ 
  
//File operation structure  
static struct file_operations fops = 
{ 
    .owner          = THIS_MODULE, 
    .read           = etx_read, 
    .write          = etx_write, 
    .open           = etx_open, 
    .release        = etx_release, 
}; 
 
/* 
** This function will be called when we open the Device file 
*/  
static int etx_open(struct inode *inode, struct file *file) 
{ 
    pr_info("Device File Opened...!!!\n"); 
    return 0; 
} 
 
/* 
** This function will be called when we close the Device file 
*/ 
static int etx_release(struct inode *inode, struct file *file) 
{ 
    pr_info("Device File Closed...!!!\n"); 
    return 0; 
} 
 
/* 
** This function will be called when we read the Device file 
*/  
static ssize_t etx_read(struct file *filp,  
                char __user *buf, size_t len, loff_t *off) 
{ 
    uint gpio_states[COUNT] = {0};
    
    //reading GPIO value 
    for(int i = 0; i < COUNT; i++){
      gpio_states[i] = gpio_get_value(gpios[i]);
    }

    //write to user 
    len = COUNT * sizeof(uint); 
    if( copy_to_user(buf, gpio_states, len) != 0) { 
      pr_err("ERROR: Not all the bytes have been copied to user\n"); 
      return -EFAULT; 
    } 
    
    pr_info("Read function : GPIO states = [%d, %d, %d, %d] \n", gpio_states[0],gpio_states[1], gpio_states[2], gpio_states[3]); 
    
    return 0; 
} 
 
/* 
** This function will be called when we write the Device file 
*/  
static ssize_t etx_write(struct file *filp,  
                const char __user *buf, size_t len, loff_t *off) 
{ 
    if (pwm_left == NULL || pwm_right == NULL) {
        pwm_left = pwm_request(PWM_LEFT_CHANNEL, "pwm_left");
        pwm_right = pwm_request(PWM_RIGHT_CHANNEL, "pwm_right");
    }
    char rec_buf[20] = {0};
    char command[5] = {0};      // 指令
    int duty_cycle_left = 100;  // 預設左馬達速度（百分比）
    int duty_cycle_right = 100; // 預設右馬達速度（百分比）
    
    if (len >= sizeof(rec_buf)) {
        len = sizeof(rec_buf) - 1;
    }
    if (copy_from_user(rec_buf, buf, len) != 0) {
        pr_err("ERROR: Not all the bytes have been copied from user\n");
        return -EFAULT;
    }
    rec_buf[len] = '\0';
    
    // 解析指令格式，例如 "w 80 60"
    sscanf(rec_buf, "%s %d %d", command, &duty_cycle_left, &duty_cycle_right);
    pr_info("Write Function : %s %d %d\n", command, duty_cycle_left, duty_cycle_right);
    
    if (strcmp(command, "rf") == 0) { // 右前進
        set_right_gpio(0, 1);
    } else if (strcmp(command, "rb") == 0) { // 右後退
        set_right_gpio(1, 0);
    } else if (strcmp(command, "lf") == 0) { // 左前進
        set_left_gpio(0, 1);
    } else if (strcmp(command, "lb") == 0) { // 左後退
        set_left_gpio(1, 0);
    } else if (strcmp(command, "stop") == 0) { // 停止
        set_left_gpio(0, 0);
        set_right_gpio(0, 0);
    } else if (strcmp(command, "pwm") == 0) { // 設置 PWM
        set_pwm_duty_cycle(pwm_left, duty_cycle_left);
        set_pwm_duty_cycle(pwm_right, duty_cycle_right);
    } else {
        return -EINVAL;
    }

    // 設置左右馬達速度

    
    return len; 
} 
 

/* 
** Module Init function 
*/  
static int __init etx_driver_init(void) 
{ 
  /*Allocating Major number*/ 
  if((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) <0){ 
    pr_err("Cannot allocate major number\n"); 
    goto r_unreg; 
  } 
  pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev)); 
  
  /*Creating cdev structure*/ 
  cdev_init(&etx_cdev,&fops); 
  
  /*Adding character device to the system*/ 
  /*Such as command "mknod" */
  /*
  For example:
  open(/dev/etx_device) equals to open("sys/class/gpio/gpio21")
  */
  if((cdev_add(&etx_cdev,dev,1)) < 0){ 
    pr_err("Cannot add the device to the system\n"); 
    goto r_del; 
  } 
  
  /*Creating struct class*/ 
  if((dev_class = class_create(THIS_MODULE,"etx_class")) == NULL){ 
    pr_err("Cannot create the struct class\n"); 
    goto r_class; 
  } 
  
  /*Creating device*/ 
  if((device_create(dev_class,NULL,dev,NULL,"car_device")) == NULL){ 
    pr_err( "Cannot create the Device \n"); 
    goto r_device; 
  } 
   
  //Checking the GPIO is valid or not 
  /* Verify */
  for (int i = 0; i < COUNT; i++) {
    if (gpio_is_valid(gpios[i]) == false) {
        pr_err("GPIO %d is not valid\n", gpios[i]);
        goto r_device;
    }
  }
   
  //Requesting the GPIO 
  /* Label */
 char label[10];  // safe label string

 for (int i = 0; i < COUNT; i++) {
    sprintf(label, "GPIO_%d", gpios[i]);  // generate labels
    if (gpio_request(gpios[i], label) < 0) {
        goto r_device;
    }
    gpio_direction_output(gpios[i], 0);   //configure the GPIO as output 
 }


  /* Using this call the GPIO 21 will be visible in /sys/class/gpio/ 
  ** Now you can change the gpio values by using below commands also. 
  ** echo 1 > /sys/class/gpio/gpio21/value  (turn ON the LED) 
  ** echo 0 > /sys/class/gpio/gpio21/value  (turn OFF the LED) 
  ** cat /sys/class/gpio/gpio21/value  (read the value LED) 
  **  
  ** the second argument prevents the direction from being changed. 
  */ 
  for (int i = 0; i < COUNT; i++){
    gpio_export(gpios[i], false);
  }

    // 初始化 PWM
    pwm_left = pwm_request(PWM_LEFT_CHANNEL, "pwm_left");
    pwm_right = pwm_request(PWM_RIGHT_CHANNEL, "pwm_right");

    if (pwm_left == NULL || pwm_right == NULL) {
        printk(KERN_ERR "PWM initialization failed\n");
        goto r_gpio;
    }
    
    pr_info("Device Driver Insert...Done!!!\n"); 
    return 0; 
    
  r_gpio: 
      for (int i = 0; i < COUNT; i++) {
        gpio_free(gpios[i]);
      }
  r_device: 
      device_destroy(dev_class,dev); 
  r_class: 
      class_destroy(dev_class); 
  r_del: 
      cdev_del(&etx_cdev); 
  r_unreg: 
      unregister_chrdev_region(dev,1); 
    
    return -1; 
} 
 
/* 
** Module exit function 
*/  
static void __exit etx_driver_exit(void) 
{ 
    for(int i = 0; i< COUNT ; i++){
        gpio_unexport(gpios[i]); 
        gpio_free(gpios[i]); 
    }
    if (pwm_left != NULL) {  
        pwm_disable(pwm_left);
        pwm_free(pwm_left); 
        pwm_left = NULL;
    }
    if (pwm_right != NULL) {  
        pwm_disable(pwm_right);
        pwm_free(pwm_right);
        pwm_right = NULL; 
    }
    device_destroy(dev_class,dev); 
    class_destroy(dev_class); 
    cdev_del(&etx_cdev); 
    unregister_chrdev_region(dev, 1); 
    pr_info("Device Driver Remove...Done!!\n"); 
} 
  
module_init(etx_driver_init); 
module_exit(etx_driver_exit); 
  
MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("YuKai Lu <yukai.ee12@nycu.edu.tw>");
MODULE_DESCRIPTION("PWM Motor Driver"); 
MODULE_VERSION("2.0"); 