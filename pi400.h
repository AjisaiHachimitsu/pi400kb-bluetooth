#include <pthread.h>

#ifndef HOOK_PATH
#define HOOK_PATH "/home/pi/pi400kb/hook.sh"
#endif

#define KEYBOARD_HID_REPORT_SIZE 8
#define MOUSE_HID_REPORT_SIZE    4

int initUSB();
int main();
void sendHIDReport();

struct hid_buf {
    unsigned char report_id;
    unsigned char data[64];
}  __attribute__ ((aligned (1)));