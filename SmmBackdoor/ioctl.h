#ifndef __IOCTL_H__
#define __IOCTL_H__

#define API_BACKDOOR_CALL 137

#define BACKDOOR_WAKEUP           0x01

#define BACKDOOR_HELLO_WORLD_TEST 0x10

#define BACKDOOR_MAGIC1 0x4141414141414141
#define BACKDOOR_MAGIC2 0x4242424242424242

struct BackdoorParams {
    UINT64 r[6];
};

#endif
