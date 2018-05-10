#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Bornyakov");

static struct platform_device *pdev = NULL;
static long int time_offset = 0;
/* directory in /proc */
static struct proc_dir_entry *proc_dir = NULL;
/* file in /proc to store time speed */
static struct proc_dir_entry *proc_spd = NULL;
/* time speed coeffitient */
/* since floating in kernel is BAD, store coeffitient times 1000000,
 * last 6 digits will be fractional part */
static unsigned long int time_mega_speed = 1000000;
static char msg[80] = { 0 };

static ssize_t proc_read_spd(struct file *filep, char *buff, size_t len,
			     loff_t *offset)
{
	int err_count = 0;

	int msg_len = snprintf(msg, sizeof(msg), "%lu.%06lu\n",
			time_mega_speed / 1000000, time_mega_speed % 1000000);
	if ((msg_len + 1) > sizeof(msg)) {
		printk(KERN_ERR "SYNTACORE RTC buffer(%ld) is too small to store coeffitient of length %d\n",
				sizeof(msg), msg_len);

		return -EFAULT;
	}

	/* reading position is behind the end of string to show */
	if (*offset >= msg_len)
		return 0;

	/* reading position is good, but overall length stands */
	/* outside the end of string to show, so truncate the length */
	if (*offset + len > msg_len)
		len = msg_len - *offset;

	err_count = copy_to_user(buff, msg + *offset, len);
	if (err_count == 0) {
		/* update reading position */
		*offset += len;

		return len;
	} else {
		printk(KERN_ERR "SYNTACORE RTC failed to copy %d chars to user\n",
				err_count);

		return -EFAULT;
	}

	return 0;
}

static ssize_t proc_write_spd(struct file *filep, const char *buff, size_t len,
			      loff_t *offset)
{
	int i, err_count = 0;
	char *msg_tmp, *msg_tail, *mega_speed, *speed_match;
	unsigned long int tmp_mega_speed = 0;

	/* clear msg before writing to it */
	memset(msg, 0, sizeof(msg));

	/* if user wants to write insane length, truncate it */
	if (len > (sizeof(msg) - 1))
		len = sizeof(msg) - 1;

	err_count = copy_from_user(msg, buff, len);
	if (err_count == 0) {
		/* copy msg string to use strsep() on it */
		msg_tmp = kzalloc(strlen(msg) + 1, GFP_KERNEL);
		if (!msg_tmp)
			return -ENOMEM;
		strcpy(msg_tmp, msg);

		/* delete trailing '\n', it ruins everything */
		msg_tail = strchrnul(msg_tmp, '\n');
		*msg_tail = '\0';

		/* allocate string for result coeffitient times 1000000 */
		mega_speed = kzalloc(strlen(msg) + 7, GFP_KERNEL);
		if (!mega_speed) {
			kfree(msg_tmp);

			return -ENOMEM;
		}

		/* search for '.' */
		speed_match = strsep(&msg_tmp, ".");
		strcat(mega_speed, speed_match);

		/* if strsep() above finds ".", msg_tmp will point to fractional
		 * part, otherwise NULL */
		if (msg_tmp) {
			if (strlen(msg_tmp) > 6)
				strncat(mega_speed, msg_tmp, 6);
			else {
				strcat(mega_speed, msg_tmp);
				for (i = 0; i < (6 - strlen(msg_tmp)); i++)
					strcat(mega_speed, "0");
			}
		} else
			strcat(mega_speed, "000000");

		err_count = kstrtoul(mega_speed, 10, &tmp_mega_speed);
		if (err_count) {
			kfree(speed_match);
			kfree(mega_speed);

			return err_count;
		}
		/* update time speed coeffitient */
		time_mega_speed = tmp_mega_speed;

		kfree(speed_match);
		kfree(mega_speed);

		return len;
	} else {
		printk(KERN_ERR "SYNTACORE RTC failed to copy %d chars to user\n",
				err_count);

		return -EFAULT;
	}
}

static struct file_operations spd_proc_fops = {
	.read  = proc_read_spd,
	.write = proc_write_spd,
};

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

	proc_dir = proc_mkdir("rtc-syntacode", NULL);
	if (proc_dir == NULL)
		return -ENOMEM;

	proc_spd = proc_create("speed", 0, proc_dir, &spd_proc_fops);
	if (proc_spd == NULL) {
		proc_remove(proc_dir);

		return -ENOMEM;
	}

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
	proc_remove(proc_spd);
	proc_remove(proc_dir);
}


module_init(syntacore_init);
module_exit(syntacore_exit);
