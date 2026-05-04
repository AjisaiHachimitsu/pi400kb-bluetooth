#include "pi400.h"

// #include "gadget-hid.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>

#include "btferret/btlib.h"

#ifndef KEYBOARD_DEV
#define KEYBOARD_DEV "/dev/input/by-id/usb-_Raspberry_Pi_Internal_Keyboard-event-kbd"
#endif
#ifndef MOUSE_DEV
#define MOUSE_DEV "/dev/input/by-id/usb-PixArt_USB_Optical_Mouse-event-mouse"
#endif

// NOTE the size of reportmap (99 in this case) must appear in keymouse.txt as
// follows:
//   LECHAR=Report Map      SIZE=99 Permit=02  UUID=2A4B
unsigned char reportmap[99] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xE0,
    0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
    0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01, 0x05, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02,
    0x81, 0x06, 0xC0, 0xC0};

// NOTE the size of report (8 in this case) must appear in keymouse.txt as
// follows:
//   LECHAR=Key report         SIZE=8  Permit=92  UUID=2A4D
unsigned char report[8] = {0, 0, 0, 0, 0, 0, 0, 0};

unsigned char *name = "HID";
unsigned char appear[2] = {
    0xC1, 0x03}; // 03C1 = keyboard icon appears on connecting device
unsigned char pnpinfo[7] = {0x02, 0x6B, 0x1D, 0x46, 0x02, 0x37, 0x05};
unsigned char protocolmode[1] = {0x01};
unsigned char hidinfo[4] = {0x01, 0x11, 0x00, 0x02};
unsigned char battery[1] = {100};

int ble_keyboard_report_index = -1;
int ble_mouse_report_index = -1;

int ble_callback(int clientnode, int op, int cticn) {
  if (op == LE_CONNECT) {
    printf("BLE connected\n");
  }
  if (op == LE_DISCONNECT) {
    return SERVER_EXIT;
  }
  return SERVER_CONTINUE;
}

void send_ble_keyboard_report(unsigned char *data) {
  if (ble_keyboard_report_index < 0)
    return;
  write_ctic(localnode(), ble_keyboard_report_index, data, 0);
}

void send_ble_mouse_report(unsigned char *data) {
  unsigned char buf[3];
  if (ble_mouse_report_index < 0)
    return;
  buf[0] = data[0];
  buf[1] = data[1];
  buf[2] = data[2];
  write_ctic(localnode(), ble_mouse_report_index, buf, 0);
}

void send_empty_ble_reports_both() {
  unsigned char buf8[8] = {0};
  unsigned char buf3[3] = {0};

  if (ble_keyboard_report_index >= 0)
    write_ctic(localnode(), ble_keyboard_report_index, buf8, 0);
  if (ble_mouse_report_index >= 0)
    write_ctic(localnode(), ble_mouse_report_index, buf3, 0);
}

int bt_init() {
  unsigned char uuid[2], randadd[6];

  if (init_blue("keymouse.txt") == 0)
    return (0);

  if (localnode() != 1) {
    printf("ERROR - Edit keymouse.txt to set ADDRESS = %s\n",
           device_address(localnode()));
    return (0);
  }

  // Write data to local characteristics
  uuid[0] = 0x2A;
  uuid[1] = 0x00;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid), name, 3);

  uuid[0] = 0x2A;
  uuid[1] = 0x01;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid), appear,
             0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4E;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid),
             protocolmode, 0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4A;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid), hidinfo,
             0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4B;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid), reportmap,
             0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4D;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid), report,
             0);

  uuid[0] = 0x2A;
  uuid[1] = 0x50;
  write_ctic(localnode(), find_ctic_index(localnode(), UUID_2, uuid), pnpinfo,
             0);

  /**** battery level ******/
  //  uuid[0] = 0x2A;
  //  uuid[1] = 0x19;
  //  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),battery,1);
  /************************/

  // Set unchanging random address by hard-coding a fixed value.
  // If connection produces an "Attempting Classic connection"
  // error then choose a different address.
  // If set_le_random_address() is not called, the system will set a
  // new and different random address every time this code is run.

  // Choose the following 6 numbers
  randadd[0] = 0xD3; // 2 hi bits must be 1
  randadd[1] = 0x56;
  randadd[2] = 0xD6;
  randadd[3] = 0x74;
  randadd[4] = 0x33;
  randadd[5] = 0x04;
  set_le_random_address(randadd);

  set_le_wait(20000);
  le_pair(localnode(), JUST_WORKS, 0);
  set_flags(FAST_TIMER, FLAG_ON);
  le_server(ble_callback, 20);
  return (1);
}

#define EVIOC_GRAB 1
#define EVIOC_UNGRAB 0

volatile int running = 0;
volatile int grabbed = 0;

int ret;
int keyboard_fd;
int mouse_fd;
int uinput_keyboard_fd;
int uinput_mouse_fd;
int keyboard_report_index = -1;
int mouse_report_index = -1;
struct hid_buf keyboard_buf;
struct hid_buf mouse_buf;

void signal_handler(int dummy) { running = 0; }

bool modprobe_libcomposite() {
  pid_t pid;

  pid = fork();

  if (pid < 0)
    return false;
  if (pid == 0) {
    char *const argv[] = {"modprobe", "libcomposite", NULL};
    execv("/usr/sbin/modprobe", argv);
    exit(0);
  }
  waitpid(pid, NULL, 0);
}

bool trigger_hook() {
  char buf[4096];
  snprintf(buf, sizeof(buf), "%s %u", HOOK_PATH, grabbed ? 1u : 0u);
  system(buf);
}

int find_hidraw_device(char *device_type, int16_t vid, int16_t pid) {
  int fd;
  int ret;
  struct hidraw_devinfo hidinfo;
  char path[20];

  for (int x = 0; x < 16; x++) {
    sprintf(path, "/dev/hidraw%d", x);

    if ((fd = open(path, O_RDWR | O_NONBLOCK)) == -1) {
      continue;
    }

    ret = ioctl(fd, HIDIOCGRAWINFO, &hidinfo);

    if (hidinfo.vendor == vid && hidinfo.product == pid) {
      printf("Found %s at: %s\n", device_type, path);
      return fd;
    }

    close(fd);
  }

  return -1;
}

int grab(char *dev) {
  printf("Grabbing: %s\n", dev);
  int fd = open(dev, O_RDONLY);
  ioctl(fd, EVIOCGRAB, EVIOC_UNGRAB);
  usleep(500000);
  ioctl(fd, EVIOCGRAB, EVIOC_GRAB);
  return fd;
}

void ungrab(int fd) {
  ioctl(fd, EVIOCGRAB, EVIOC_UNGRAB);
  close(fd);
}

void printhex(unsigned char *buf, size_t len) {
  for (int x = 0; x < len; x++) {
    printf("%x ", buf[x]);
  }
  printf("\n");
}

void ungrab_both() {
  printf("Releasing Keyboard and/or Mouse\n");

  if (uinput_keyboard_fd > -1) {
    ungrab(uinput_keyboard_fd);
  }

  if (uinput_mouse_fd > -1) {
    ungrab(uinput_mouse_fd);
  }

  grabbed = 0;

  trigger_hook();
}

void grab_both() {
  printf("Grabbing Keyboard and/or Mouse\n");

  if (keyboard_fd > -1) {
    uinput_keyboard_fd = grab(KEYBOARD_DEV);
  }

  if (mouse_fd > -1) {
    uinput_mouse_fd = grab(MOUSE_DEV);
  }

  if (uinput_keyboard_fd > -1 || uinput_mouse_fd > -1) {
    grabbed = 1;
  }

  trigger_hook();
}

void send_empty_hid_reports_both() {
  (void)keyboard_fd;
  (void)mouse_fd;
}

int main() {
  if (!bt_init())
    return 1;

  modprobe_libcomposite();

  keyboard_buf.report_id = 1;
  mouse_buf.report_id = 2;

  unsigned char uuid[2];
  uuid[0] = 0x2A;
  uuid[1] = 0x4D;
  keyboard_report_index = find_ctic_index(localnode(), UUID_2, uuid);
  if (keyboard_report_index < 0) {
    printf("Failed to find BLE keyboard report characteristic\n");
    return 1;
  }

  keyboard_fd = find_hidraw_device("keyboard", KEYBOARD_VID, KEYBOARD_PID);
  if (keyboard_fd == -1) {
    printf("Failed to open keyboard device\n");
  }

  mouse_fd = find_hidraw_device("mouse", MOUSE_VID, MOUSE_PID);
  if (mouse_fd == -1) {
    printf("Failed to open mouse device\n");
  }

  if (mouse_fd > -1)
    mouse_report_index = keyboard_report_index + 1;

  mouse_fd = find_hidraw_device("mouse", MOUSE_VID, MOUSE_PID);
  if (mouse_fd == -1) {
    printf("Failed to open mouse device\n");
  }

  if (mouse_fd == -1 && keyboard_fd == -1) {
    printf("No devices to forward, bailing out!\n");
    return 1;
  }

  grab_both();

  printf("Running...\n");
  running = 1;
  signal(SIGINT, signal_handler);

  struct pollfd pollFd[2];
  int nfds = 0;
  if (keyboard_fd > -1) {
    pollFd[nfds].fd = keyboard_fd;
    pollFd[nfds].events = POLLIN;
    nfds++;
  }
  if (mouse_fd > -1) {
    pollFd[nfds].fd = mouse_fd;
    pollFd[nfds].events = POLLIN;
    nfds++;
  }

  if (nfds == 0) {
    printf("No hidraw devices available for polling\n");
    return 1;
  }

  while (running) {
    poll(pollFd, nfds, -1);
    if (keyboard_fd > -1) {
      int c = read(keyboard_fd, keyboard_buf.data, KEYBOARD_HID_REPORT_SIZE);

      if (c == KEYBOARD_HID_REPORT_SIZE) {
        printf("K:");
        printhex(keyboard_buf.data, KEYBOARD_HID_REPORT_SIZE);

        // Trap Ctrl + Raspberry and toggle capture on/off
        if (keyboard_buf.data[0] == 0x09) {
          if (grabbed) {
            ungrab_both();
            send_empty_ble_reports_both();
          } else {
            grab_both();
          }
        }
        // Trap Ctrl + Shift + Raspberry and exit
        if (keyboard_buf.data[0] == 0x0b) {
          running = 0;
          break;
        }

        if (grabbed) {
          send_ble_keyboard_report(keyboard_buf.data);
          usleep(1000);
        }
      }
    }
    if (mouse_fd > -1) {
      int c = read(mouse_fd, mouse_buf.data, MOUSE_HID_REPORT_SIZE);

      if (c == MOUSE_HID_REPORT_SIZE) {
        printf("M:");
        printhex(mouse_buf.data, MOUSE_HID_REPORT_SIZE);

        if (grabbed) {
          send_ble_mouse_report(mouse_buf.data);
          usleep(1000);
        }
      }
    }
  }

  ungrab_both();
  send_empty_ble_reports_both();

  return 0;
}
