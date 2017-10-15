#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/input/mt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include "hall.h"
#include <linux/interrupt.h>

struct input_dev *hall_input = NULL;
struct hall_data *hall = NULL;
static struct class *hall_class = NULL;
static struct device *hall_dev = NULL;
static unsigned int gpio_code_reported = 0;
//extern void ft5x06_disable_leather_cover(void);
//extern void ft5x06_enable_leather_cover(void);

void report_hall_boot_state(void)
{
	unsigned int gpio_code = 0;
	unsigned int gpio_code_main = 0;
	unsigned int gpio_code_sub = 0;

	gpio_code_main = gpio_get_value(hall->irq_gpio)? KEY_UNLOCK_COVER : KEY_LOCK_LED_COVER;
	if (hall->irq_gpio_sub >= 0)
	{
		gpio_code_sub = gpio_get_value(hall->irq_gpio_sub)? KEY_UNLOCK_COVER : KEY_LOCK_LED_COVER;
		if ((gpio_code_main == KEY_UNLOCK_COVER)&&(gpio_code_sub == KEY_UNLOCK_COVER))
			gpio_code = KEY_UNLOCK_COVER;
		else
			gpio_code = KEY_LOCK_LED_COVER;
	}
	else
	{
		if (gpio_code_main == KEY_UNLOCK_COVER)
			gpio_code = KEY_UNLOCK_COVER;
		else
			gpio_code = KEY_LOCK_LED_COVER;
	}
	//if(gpio_code == KEY_LOCK_LED_COVER)
	{
	input_report_key(hall_input, gpio_code, 1);
	input_sync(hall_input);
	input_report_key(hall_input, gpio_code, 0);
	input_sync(hall_input);

	gpio_code_reported = gpio_code;
	}
}


static int hall_power_set(struct hall_data *data, bool on)
{
	int rc = 0;

	if (!on && data->power_enabled) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->pdev->dev,"Regulator vdd disable failed rc=%d\n", rc);
			goto out;
		}
		data->power_enabled = false;
		return rc;
	} else if (on && !data->power_enabled) {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->pdev->dev,"Regulator vdd enable failed rc=%d\n", rc);
			goto out;
		}

		data->power_enabled = true;

		usleep(40000);
		return rc;
	} else {
		dev_warn(&data->pdev->dev,"Power on=%d. enabled=%d\n",on, data->power_enabled);
		return rc;
	}

out:
	return rc;
}

static int hall_power_init(struct hall_data *data, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0,
				HALL_VDD_MAX_UV);

		regulator_put(data->vdd);
	} else {
		data->vdd = regulator_get(&data->pdev->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->pdev->dev,"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd,
				HALL_VDD_MIN_UV, HALL_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->pdev->dev,"Regulator set failed vdd rc=%d\n",rc);
				goto reg_vdd_put;
			}
		}
	}

	return 0;

reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static void do_hall_work(struct work_struct *work)
{
	unsigned int gpio_code = 0;
	gpio_code = gpio_get_value(hall->irq_gpio)? KEY_UNLOCK_COVER : KEY_LOCK_LED_COVER;
	input_report_key(hall_input, gpio_code, 1);
	input_sync(hall_input);
	input_report_key(hall_input, gpio_code, 0);
	input_sync(hall_input);

	gpio_code_reported = gpio_code;
/*
	if (gpio_code == KEY_UNLOCK_COVER)	//furong add 2015.03.31. 
		ft5x06_disable_leather_cover();
	if (gpio_code == KEY_LOCK_LED_COVER)
		ft5x06_enable_leather_cover();
*/
}
static void do_hall_work_sub(struct work_struct *work)
{
	unsigned int gpio_code = 0;
	gpio_code = gpio_get_value(hall->irq_gpio_sub)? KEY_UNLOCK_COVER : KEY_LOCK_LED_COVER;
	input_report_key(hall_input, gpio_code, 1);
	input_sync(hall_input);
	input_report_key(hall_input, gpio_code, 0);
	input_sync(hall_input);

	gpio_code_reported = gpio_code;
/*
	if (gpio_code == KEY_UNLOCK_COVER)	//furong add 2015.03.31. 
		ft5x06_disable_leather_cover();
	if (gpio_code == KEY_LOCK_LED_COVER)
		ft5x06_enable_leather_cover();
*/
}


irqreturn_t interrupt_hall_irq(int irq, void *dev)
{
	printk("\n\n-----%s,%d,irq_value = %d\n\n",__func__,__LINE__,gpio_get_value(hall->irq_gpio));
	queue_work(hall->hall_wq, &hall->hall_work);

	return IRQ_HANDLED;
}
irqreturn_t interrupt_hall_irq_sub(int irq, void *dev)
{
	printk("\n\n-----%s,%d,irq_value_sub = %d\n\n",__func__,__LINE__,gpio_get_value(hall->irq_gpio_sub));
	queue_work(hall->hall_wq, &hall->hall_work_sub);

	return IRQ_HANDLED;
}

static int hall_pinctrl_init(struct hall_data *hall, struct platform_device *pdev)
{
	hall->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(hall->pinctrl)) {
		dev_err(&pdev->dev, "Failed to get pinctrl\n");
		return PTR_ERR(hall->pinctrl);
	}
	hall->pin_default = pinctrl_lookup_state(hall->pinctrl, "default");
	if (IS_ERR_OR_NULL(hall->pin_default)) {
		dev_err(&pdev->dev, "Failed to look up default state\n");
		return PTR_ERR(hall->pin_default);
	}
	return 0;
}

static ssize_t hall_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned int gpio_code = 0;
	unsigned int gpio_code_main = 0;
	unsigned int gpio_code_sub = 0;
	unsigned int cnt = 0;

	gpio_code_main = gpio_get_value(hall->irq_gpio)? KEY_UNLOCK_COVER : KEY_LOCK_LED_COVER;
	if (hall->irq_gpio_sub >= 0)
	{
		gpio_code_sub = gpio_get_value(hall->irq_gpio_sub)? KEY_UNLOCK_COVER : KEY_LOCK_LED_COVER;
		if ((gpio_code_main == KEY_UNLOCK_COVER)&&(gpio_code_sub == KEY_UNLOCK_COVER))
			gpio_code = KEY_UNLOCK_COVER;
		else
			gpio_code = KEY_LOCK_LED_COVER;

	#if 1
		cnt = snprintf(buf, 256, "%d\n", gpio_code);
	#else
		cnt = snprintf(buf, 256, "hall status:0x%x, main:0x%x, sub:0x%x, reported:0x%x!\n", 
				gpio_code, gpio_code_main, gpio_code_sub, gpio_code_reported);
	#endif
	}
	else
	{
		if (gpio_code_main == KEY_UNLOCK_COVER)
			gpio_code = KEY_UNLOCK_COVER;
		else
			gpio_code = KEY_LOCK_LED_COVER;

	#if 1
		cnt = snprintf(buf, 256, "%d\n", gpio_code);
	#else
		cnt = snprintf(buf, 256, "hall status:0x%x, main:0x%x, reported:0x%d!\n", 
				gpio_code, gpio_code_main, gpio_code_reported);
	#endif
	}

	return cnt;
}

static ssize_t hall_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	static unsigned int hall_init_ok = 0;

	printk("\n-----%s-----\n\n", __func__);
	if(!hall_init_ok)
	{
		report_hall_boot_state();
		hall_init_ok = 1;
	}

	return size;
}

static DEVICE_ATTR(status, 0664, hall_status_show, hall_status_store);

int hall_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device_node *node = pdev->dev.of_node;

	hall = devm_kzalloc(&pdev->dev, sizeof(struct hall_data),
				 GFP_KERNEL);
	if (hall == NULL)
	{
		dev_err(&pdev->dev, "%s:%d Unable to allocate memory\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	if (!hall_pinctrl_init(hall, pdev)) {
		rc = pinctrl_select_state(hall->pinctrl, hall->pin_default);
		if (rc) {
			dev_err(&pdev->dev, "Can't select pinctrl state\n");
			goto error;
		}
	}

	hall->irq_gpio = of_get_named_gpio(node, "interrupt-gpios", 0);
	hall->irq_gpio_sub = of_get_named_gpio(node, "interrupt-gpios", 1);
	
	if (hall->irq_gpio < 0)
	{
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed. rc =  %d\n",
			"interrupt-gpios", node->full_name, hall->irq_gpio);
		goto error;
	}
	else
	{
		rc = gpio_request(hall->irq_gpio, "HALL_INTERRUPT");
		if (rc)
		{
			dev_err(&pdev->dev,
				"%s: Failed to request gpio %d,rc = %d\n",
				__func__, hall->irq_gpio, rc);

			goto error;
		}
		rc = gpio_direction_input(hall->irq_gpio);
		if (rc)
		{
			dev_err(&pdev->dev, "Unable to set direction for irq gpio [%d]\n",
				hall->irq_gpio);
			gpio_free(hall->irq_gpio);
			goto error;
		}
	}

	if (hall->irq_gpio_sub >= 0)
	{
		rc = gpio_request(hall->irq_gpio_sub, "HALL_INTERRUPT_SUB");
		if (rc)
		{
			dev_err(&pdev->dev,
				"%s: Failed to request gpio %d,rc = %d\n",
				__func__, hall->irq_gpio_sub, rc);

			goto error;
		}
		rc = gpio_direction_input(hall->irq_gpio_sub);
		if (rc)
		{
			dev_err(&pdev->dev, "Unable to set direction for irq gpio [%d]\n",
				hall->irq_gpio_sub);
			gpio_free(hall->irq_gpio_sub);
			goto error;
		}
	}
	
	rc = request_irq(gpio_to_irq(hall->irq_gpio), interrupt_hall_irq, IRQ_TYPE_EDGE_BOTH , "hall_work", &pdev->dev);
	if(rc < 0)
	{
		goto exit_free_irq;
	}

	if (hall->irq_gpio_sub >= 0)
	{
		rc = request_irq(gpio_to_irq(hall->irq_gpio_sub), interrupt_hall_irq_sub, IRQ_TYPE_EDGE_BOTH , "hall_work_sub", &pdev->dev);
		if(rc < 0)
		{
			goto exit_free_irq_sub;
		}
	}

	hall->pdev = pdev;
	dev_set_drvdata(&pdev->dev, hall);

	hall_power_init(hall, 1);
	hall_power_set(hall, 1);

	hall_input = input_allocate_device();
	if (!hall_input)
	{
		dev_err(&pdev->dev, "hall.c: Not enough memory\n");
		return -ENOMEM;
	}

	hall_input->name = "hall";
	input_set_capability(hall_input, EV_KEY, KEY_UNLOCK_COVER);
	input_set_capability(hall_input, EV_KEY, KEY_LOCK_LED_COVER);

	rc = input_register_device(hall_input);
	if (rc)
	{
		dev_err(&pdev->dev, "hall.c: Failed to register device\n");
		return rc;
	}

	hall->hall_wq = create_singlethread_workqueue("hall_wq");
	INIT_WORK(&hall->hall_work, do_hall_work);
	enable_irq_wake(gpio_to_irq(hall->irq_gpio));

	if (hall->irq_gpio_sub >= 0)
	{
	//	hall->hall_wq = create_singlethread_workqueue("hall_wq_sub");
		INIT_WORK(&hall->hall_work_sub, do_hall_work_sub);
		enable_irq_wake(gpio_to_irq(hall->irq_gpio_sub));
	}
	
	hall_class= class_create(THIS_MODULE, "hall");
	hall_dev = device_create(hall_class, NULL, 0, NULL, "window_cover");
	if (IS_ERR(hall_dev))
        dev_err(&pdev->dev, "Failed to create device(hall_dev)!\n");

	rc = device_create_file(hall_dev, &dev_attr_status);
	if (rc) {
		dev_err(&pdev->dev, "sys file creation failed\n");
		return rc;
	}

	printk("\n\n ***##xx##*** hall probe completed \n\n");
	return 0;

exit_free_irq_sub:
	free_irq(gpio_to_irq(hall->irq_gpio_sub),NULL);

exit_free_irq:
	free_irq(gpio_to_irq(hall->irq_gpio),NULL);

error:
	devm_kfree(&pdev->dev, hall);

	return rc;
}

int hall_remove(struct platform_device *pdev)
{
	hall = platform_get_drvdata(pdev);

	free_irq(gpio_to_irq(hall->irq_gpio),NULL);

	input_unregister_device(hall_input);
	if (hall_input)
	{
		input_free_device(hall_input);
		hall_input = NULL;
	}

	if (gpio_is_valid(hall->irq_gpio))
		gpio_free(hall->irq_gpio);

	return 0;
}

static struct of_device_id hall_of_match[] = {
	{.compatible = "qcom,hall",},
	{},
};

static struct platform_driver hall_driver = {
	.probe = hall_probe,
	.remove = hall_remove,
	.driver = {
		   .name = "qcom,hall",
		   .owner = THIS_MODULE,
		   .of_match_table = hall_of_match,
	}
};

static int __init hall_init(void)
{
	return platform_driver_register(&hall_driver);
}

static void __init hall_exit(void)
{
	platform_driver_unregister(&hall_driver);
}

module_init(hall_init);
module_exit(hall_exit);
MODULE_LICENSE("GPL");

