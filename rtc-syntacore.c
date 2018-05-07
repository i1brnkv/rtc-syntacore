#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Bornyakov");

static struct platform_device *pdev = NULL;
static long int time_offset = 0;

static int syntacore_read_time(struct device *dev, struct rtc_time *tm)
{
	struct timeval now;

	do_gettimeofday(&now);
	rtc_time_to_tm(now.tv_sec + time_offset, tm);

	return rtc_valid_tm(tm);
}

static int syntacore_set_time(struct device *dev, struct rtc_time *tm)
{
	struct timeval now;

	do_gettimeofday(&now);
	time_offset = rtc_tm_to_time64(tm) - now.tv_sec;

	return 0;
}

static const struct rtc_class_ops syntacore_ops = {
	.read_time = syntacore_read_time,
	.set_time  = syntacore_set_time,
};

static int syntacore_probe(struct platform_device *plat_dev)
{
	struct rtc_device *rtc;

	rtc = rtc_device_register(pdev->name, &plat_dev->dev, &syntacore_ops,
				  THIS_MODULE);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);

	platform_set_drvdata(plat_dev, rtc);

	return 0;
}

static int syntacore_remove(struct platform_device *plat_dev)
{
	rtc_device_unregister(platform_get_drvdata(plat_dev));

	return 0;
}

static struct platform_driver syntacore_driver = {
	.probe  = syntacore_probe,
	.remove = syntacore_remove,
	.driver = {
		.name = "rtc-syntacore"
	},
};

static int __init syntacore_init(void)
{
	int err;

	err = platform_driver_register(&syntacore_driver);
	if (err)
		return err;

	pdev = platform_device_alloc("rtc-syntacore", 0);
	if (pdev == NULL) {
		platform_driver_unregister(&syntacore_driver);

		return -ENOMEM;
	}

	err = platform_device_add(pdev);
	if (err) {
		platform_device_put(pdev);
		platform_driver_unregister(&syntacore_driver);

		return err;
	}

	return 0;
}

static void __exit syntacore_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&syntacore_driver);
}

module_init(syntacore_init);
module_exit(syntacore_exit);
