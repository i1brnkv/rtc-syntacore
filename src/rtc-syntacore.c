#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/random.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Bornyakov");

static struct platform_device *pdev = NULL;
/* start time to calculate seconds passed since last time update */
static struct timespec start_time;
/* time stamp to be updated on set_time, module probe and time speed chenge */
static struct timespec time_stamp;
/* directory in /proc */
static struct proc_dir_entry *proc_dir = NULL;
/* file in /proc to store time speed */
static struct proc_dir_entry *proc_spd = NULL;
/* file in /proc to store random time speed flag */
static struct proc_dir_entry *proc_rand = NULL;
/* time speed coefficient
 * Since floating in kernel is BAD, store coefficient multiplied
 * by 1000000, last 6 digits will be fractional part. */
static unsigned int time_mega_speed = 1000000;
/* random time speed coefficient
 * Same, as above, but random from 0 to time_mega_speed. */
static unsigned int time_mega_speed_rand = 1000000;
/* time speed is random flag */
static bool is_spd_rand = false;
static char msg[80] = { 0 };

static unsigned long syntacore_gettimeofday(void) {
	struct timespec now, time_left;
	unsigned long res;

	getrawmonotonic(&now);
	time_left = timespec_sub(now, start_time);
	if (is_spd_rand) {
		res = time_stamp.tv_sec +
		      time_left.tv_sec * time_mega_speed_rand / 1000000;
	} else
		res = time_stamp.tv_sec +
		      time_left.tv_sec * time_mega_speed / 1000000;

	return res;
}

static void syntacore_upd_rand_speed(void) {
	/* update start_time and time_stamp to avoid time tearing  */
	time_stamp.tv_sec = syntacore_gettimeofday();
	getrawmonotonic(&start_time);

	/* update time speed coefficient */
	get_random_bytes(&time_mega_speed_rand, sizeof(unsigned int));
	time_mega_speed_rand %= time_mega_speed;
}

static ssize_t proc_read_spd(struct file *filep, char *buff, size_t len,
			     loff_t *offset)
{
	int err_count = 0;

	int msg_len = snprintf(msg, sizeof(msg), "%u.%06u\n",
			time_mega_speed / 1000000, time_mega_speed % 1000000);
	if ((msg_len + 1) > sizeof(msg)) {
		printk(KERN_ERR "SYNTACORE RTC buffer(%ld) is too small to store coefficient of length %d\n",
				sizeof(msg), msg_len);

		return -EFAULT;
	}

	/* reading position is behind the end of string to show */
	if (*offset >= msg_len)
		return 0;

	/* reading position is good, but overall length stands
	 * outside the end of string to show, so truncate the length */
	if (*offset + len > msg_len)
		len = msg_len - *offset;

	err_count = copy_to_user(buff, msg + *offset, len);
	if (err_count) {
		printk(KERN_ERR "SYNTACORE RTC failed to copy %d chars to user\n",
				err_count);

		return -EFAULT;
	}

	/* update reading position */
	*offset += len;

	return len;
}

static ssize_t proc_write_spd(struct file *filep, const char *buff, size_t len,
			      loff_t *offset)
{
	int i, ret, err_count = 0;
	char *msg_tmp, *msg_tail, *mega_speed, *speed_match;
	unsigned int tmp_mega_speed = 0;

	/* clear msg before writing to it */
	memset(msg, 0, sizeof(msg));

	/* if user wants to write insane length, truncate it */
	if (len > (sizeof(msg) - 1))
		len = sizeof(msg) - 1;

	err_count = copy_from_user(msg, buff, len);
	if (err_count) {
		printk(KERN_ERR "SYNTACORE RTC failed to copy %d chars to user\n",
				err_count);

		return -EFAULT;

	}

	/* copy msg string to use strsep() on it */
	msg_tmp = kzalloc(strlen(msg) + 1, GFP_KERNEL);
	if (!msg_tmp)
		goto nomem0;
	strcpy(msg_tmp, msg);

	/* delete trailing '\n', it ruins everything */
	msg_tail = strchrnul(msg_tmp, '\n');
	*msg_tail = '\0';

	/* allocate string for result coefficient times 1000000 */
	mega_speed = kzalloc(strlen(msg) + 7, GFP_KERNEL);
	if (!mega_speed)
		goto nomem1;

	/* search for '.' */
	speed_match = strsep(&msg_tmp, ".");
	strcat(mega_speed, speed_match);

	/* if strsep() above finds ".", msg_tmp will point to
	 * fractional part, otherwise NULL */
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

	ret = kstrtouint(mega_speed, 10, &tmp_mega_speed);
	if (ret)
		goto out;

	/* update start_time and time_stamp to avoid time tearing  */
	time_stamp.tv_sec = syntacore_gettimeofday();
	getrawmonotonic(&start_time);

	/* update time speed coefficient */
	time_mega_speed = tmp_mega_speed;

	/* update random time speed coefficient */
	if (is_spd_rand)
		syntacore_upd_rand_speed();

	ret = len;
out:
	kfree(speed_match);
	kfree(mega_speed);

	return ret;
nomem1:
	kfree(msg_tmp);
nomem0:
	return -ENOMEM;
}

static struct file_operations spd_proc_fops = {
	.read  = proc_read_spd,
	.write = proc_write_spd,
};

static ssize_t proc_read_rand(struct file *filep, char *buff, size_t len,
			      loff_t *offset)
{
	int err_count = 0;

	if (is_spd_rand)
		strcpy(msg, "1\n");
	else
		strcpy(msg, "0\n");

	/* reading position is behind the end of string to show */
	if (*offset >= 2)
		return 0;

	/* reading position is good, but overall length stands
	 * outside the end of string to show, so truncate the length */
	if (*offset + len > 2)
		len = 2 - *offset;

	err_count = copy_to_user(buff, msg + *offset, len);
	if (err_count) {
		printk(KERN_ERR "SYNTACORE RTC failed to copy %d chars to user\n",
				err_count);

		return -EFAULT;
	}

	/* update reading position */
	*offset += len;

	return len;
}

static ssize_t proc_write_rand(struct file *filep, const char *buff, size_t len,
			       loff_t *offset)
{
	int err_count = 0;

	/* clear msg before writing to it */
	memset(msg, 0, sizeof(msg));

	/* support writing max 2 symbols: '0' or '1' plus '\n' */
	if (len > 2)
		len = 2;

	err_count = copy_from_user(msg, buff, len);
	if (err_count) {
		printk(KERN_ERR "SYNTACORE RTC failed to copy %d chars from user",
				err_count);

		return -EFAULT;
	}

	switch (msg[0]) {
	case '1':
		if (!is_spd_rand)
			syntacore_upd_rand_speed();
		is_spd_rand = true;
		break;
	case '0':
		is_spd_rand = false;
		break;
	default:
		return -EINVAL;
	}

	return len;
}

static struct file_operations rand_proc_fops = {
	.read  = proc_read_rand,
	.write = proc_write_rand,
};

static int syntacore_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long cur_time = syntacore_gettimeofday();
	rtc_time_to_tm(cur_time, tm);

	/* update random time speed coefficient on every read */
	if (is_spd_rand)
		syntacore_upd_rand_speed();

	return rtc_valid_tm(tm);
}

static int syntacore_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long t;

	rtc_tm_to_time(tm, &t);
	time_stamp.tv_sec = t;
	getrawmonotonic(&start_time);

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

	proc_dir = proc_mkdir("rtc-syntacore", NULL);
	if (proc_dir == NULL) {
		err = -ENOMEM;
		goto err_proc_dir;
	}

	proc_spd = proc_create("speed", 0, proc_dir, &spd_proc_fops);
	if (proc_spd == NULL) {
		err = -ENOMEM;
		goto err_proc_spd;
	}

	proc_rand = proc_create("rand", 0, proc_dir, &rand_proc_fops);
	if (proc_rand == NULL) {
		err = -ENOMEM;
		goto err_proc_rand;
	}

	err = platform_driver_register(&syntacore_driver);
	if (err)
		goto err_plat_drv;

	pdev = platform_device_alloc("rtc-syntacore", 0);
	if (pdev == NULL) {
		err = -ENOMEM;
		goto err_plat_dev_alloc;
	}

	err = platform_device_add(pdev);
	if (err)
		goto err_plat_dev_add;

	getnstimeofday(&time_stamp);
	getrawmonotonic(&start_time);

	return 0;

err_plat_dev_add:
	platform_device_put(pdev);
err_plat_dev_alloc:
	platform_driver_unregister(&syntacore_driver);
err_plat_drv:
	proc_remove(proc_rand);
err_proc_rand:
	proc_remove(proc_spd);
err_proc_spd:
	proc_remove(proc_dir);
err_proc_dir:
	return err;
}

static void __exit syntacore_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&syntacore_driver);
	proc_remove(proc_rand);
	proc_remove(proc_spd);
	proc_remove(proc_dir);
}


module_init(syntacore_init);
module_exit(syntacore_exit);
