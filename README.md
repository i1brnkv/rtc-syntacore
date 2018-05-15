# fake rtc linux kernel module

This module creates additional /dev/rtcN device with reduced features.
It supports :
 * Reading/setting time via hwclock utility, or RTC_RD_TIME/RTC_SET_TIME ioctls
 * Basic /sys/class/rtc/rtcN/ interface
 * Adjusting "time speed" via /proc/rtc-syntacore/speed.
   1.0 means tick per second, just like real clock. Biger or fewer than 1.0
   means faster or slower passing of time. This coefficient supports up to
   6 digits in fractional part, and speed * 1000000 is limited to unsigned int.
 * Random mode. If 1 is set in /proc/rtc-syntacore/rand, time speed coefficient
   updates on every time reading to random number from 0 to value from
   /proc/rtc-syntacore/speed.
