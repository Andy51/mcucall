
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <termios.h>
#include <assert.h>
#include <fcntl.h>

#define THREAD_SUCCESS  NULL
#define THREAD_FAILURE  ((void*)1)

typedef enum
{
    UR_NONE = 0,
    UR_GET_VERSION,
    UR_READ_MEM
} user_request_t;

pthread_t g_reader_thread;
int g_fd;

int scanFrameSync(uint8_t *buf)
{
    int res, i;

    while(1)
    {
        for(i = 0; i <= 1; i++)
        {       
            res = read(g_fd, &buf[i], 1);
            if (res < 0)
                return res;
            if (i == 0 && buf[i] != 0xF0)
                break;
            else if (buf[i] != 0xF1)
                break;
            return 0;
        }
    }
    return res;
}

#pragma pack(push,1)
typedef struct
{
    uint8_t   size;
    uint16_t  seq;
    uint8_t   flag;

} frame_header_t;

typedef struct
{
    frame_header_t frame_header;
    uint8_t        protocol;
    uint8_t        cmd;
} cmd_header_t;
#pragma pack(pop)

#define PROTOCOL_LIST() \
    PROTOCOL_ITEM(0x00, POWER) \
    PROTOCOL_ITEM(0x01, VERSION) \
    PROTOCOL_ITEM(0x02, KEY) \
    PROTOCOL_ITEM(0x03, VIDEO) \
    PROTOCOL_ITEM(0x04, AUDIO) \
    PROTOCOL_ITEM(0x05, RADIO) \
    PROTOCOL_ITEM(0x06, DVD) \
    PROTOCOL_ITEM(0x07, SWITCH) \
    PROTOCOL_ITEM(0x08, IPOD) \
    PROTOCOL_ITEM(0x09, AUX_IN) \
    PROTOCOL_ITEM(0x0A, VIDEO_SIGNAL) \
    PROTOCOL_ITEM(0x0B, TIME) \
    PROTOCOL_ITEM(0x0C, TV) \
    PROTOCOL_ITEM(0xE0, KEYLIGHT) \
    PROTOCOL_ITEM(0xF0, TPMS) \
    PROTOCOL_ITEM(0xFF, OTHER)


#define PROTOCOL_ITEM(id, name) \
    PROTO_##name = id,
typedef enum
{
    PROTOCOL_LIST()
} protocol_id_t;
#undef PROTOCOL_ITEM

typedef struct
{
    protocol_id_t id;
    const char *name;
} protocol_info_t;

#define PROTOCOL_ITEM(id, name) \
    { id, #name },
const protocol_info_t proto_list[] = 
{
    PROTOCOL_LIST()
};
#undef PROTOCOL_ITEM

#define PROTOCOLS_COUNT (sizeof(proto_list) / sizeof(*proto_list))

const char* getProtoName(protocol_id_t id)
{
    for(int i = 0; i < PROTOCOLS_COUNT; i++)
    {
        if(proto_list[i].id == id)
            return proto_list[i].name;
    }
    return "UNKNOWN";
}

typedef enum
{
    VERSION_CMD_MODEL = 0,
    VERSION_CMD_MODEL_REPLY,
    VERSION_CMD_VERSION = 2,
    VERSION_CMD_VERSION_REPLY,
    VERSION_CMD_SERIAL = 4,
    VERSION_CMD_SERIAL_REPLY,
    VERSION_CMD_TIME = 6,
    VERSION_CMD_TIME_REPLY,
    VERSION_CMD_UPDATE = 8,
    VERSION_CMD_UPDATE_REPLY
} version_cmd_t;

void protocolVersion(uint8_t *frame)
{
    cmd_header_t *header = (cmd_header_t*)frame;

    switch(header->cmd)
    {
        case VERSION_CMD_MODEL_REPLY:
            printf("MODEL REPL: [%s]\n", (const char*)(header+1));
            break;
        case VERSION_CMD_VERSION_REPLY:
            printf("VERSION REPL: [%s]\n", (const char*)(header+1));
            break;
        case VERSION_CMD_SERIAL_REPLY:
            printf("SERIAL REPL: [%s]\n", (const char*)(header+1));
            break;
        case VERSION_CMD_TIME_REPLY:
            printf("TIME REPL: [%s]\n", (const char*)(header+1));
            break;
    }
}

typedef enum
{
    IPOD_CMD_UNKNOWN0 = 1,
    IPOD_CMD_UNKNOWN0_REPLY,
    IPOD_CMD_CHARGING_REPLY
} ipod_cmd_t;

void protocolIpod(uint8_t *frame)
{
    cmd_header_t *header = (cmd_header_t*)frame;

    switch(header->cmd)
    {
        case IPOD_CMD_UNKNOWN0_REPLY:
            printf("UNKNOWN0 REPL: [%X]\n", *(uint8_t*)(header+1));
            break;
    }
}

void processFrame(uint8_t *frame)
{
    cmd_header_t *header = (cmd_header_t*)frame;

    printf("PROTOCOL: %s (%X) CMD: %X\n", getProtoName(header->protocol), header->protocol, header->cmd);

    switch(header->protocol)
    {
        case PROTO_VERSION:
            protocolVersion(frame);
            break;
        case PROTO_IPOD:
            protocolIpod(frame);
            break;
    }
}

static const uint8_t crc8Table[] =
{
    0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83,
    0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
    0x9D, 0xC3, 0x21, 0x7F, 0xFC, 0xA2, 0x40, 0x1E,
    0x5F, 0x01, 0xE3, 0xBD, 0x3E, 0x60, 0x82, 0xDC,
    0x23, 0x7D, 0x9F, 0xC1, 0x42, 0x1C, 0xFE, 0xA0,
    0xE1, 0xBF, 0x5D, 0x03, 0x80, 0xDE, 0x3C, 0x62,
    0xBE, 0xE0, 0x02, 0x5C, 0xDF, 0x81, 0x63, 0x3D,
    0x7C, 0x22, 0xC0, 0x9E, 0x1D, 0x43, 0xA1, 0xFF,
    0x46, 0x18, 0xFA, 0xA4, 0x27, 0x79, 0x9B, 0xC5,
    0x84, 0xDA, 0x38, 0x66, 0xE5, 0xBB, 0x59, 0x07,
    0xDB, 0x85, 0x67, 0x39, 0xBA, 0xE4, 0x06, 0x58,
    0x19, 0x47, 0xA5, 0xFB, 0x78, 0x26, 0xC4, 0x9A,
    0x65, 0x3B, 0xD9, 0x87, 0x04, 0x5A, 0xB8, 0xE6,
    0xA7, 0xF9, 0x1B, 0x45, 0xC6, 0x98, 0x7A, 0x24,
    0xF8, 0xA6, 0x44, 0x1A, 0x99, 0xC7, 0x25, 0x7B,
    0x3A, 0x64, 0x86, 0xD8, 0x5B, 0x05, 0xE7, 0xB9,
    0x8C, 0xD2, 0x30, 0x6E, 0xED, 0xB3, 0x51, 0x0F,
    0x4E, 0x10, 0xF2, 0xAC, 0x2F, 0x71, 0x93, 0xCD,
    0x11, 0x4F, 0xAD, 0xF3, 0x70, 0x2E, 0xCC, 0x92,
    0xD3, 0x8D, 0x6F, 0x31, 0xB2, 0xEC, 0x0E, 0x50,
    0xAF, 0xF1, 0x13, 0x4D, 0xCE, 0x90, 0x72, 0x2C,
    0x6D, 0x33, 0xD1, 0x8F, 0x0C, 0x52, 0xB0, 0xEE,
    0x32, 0x6C, 0x8E, 0xD0, 0x53, 0x0D, 0xEF, 0xB1,
    0xF0, 0xAE, 0x4C, 0x12, 0x91, 0xCF, 0x2D, 0x73,
    0xCA, 0x94, 0x76, 0x28, 0xAB, 0xF5, 0x17, 0x49,
    0x08, 0x56, 0xB4, 0xEA, 0x69, 0x37, 0xD5, 0x8B,
    0x57, 0x09, 0xEB, 0xB5, 0x36, 0x68, 0x8A, 0xD4,
    0x95, 0xCB, 0x29, 0x77, 0xF4, 0xAA, 0x48, 0x16,
    0xE9, 0xB7, 0x55, 0x0B, 0x88, 0xD6, 0x34, 0x6A,
    0x2B, 0x75, 0x97, 0xC9, 0x4A, 0x14, 0xF6, 0xA8,
    0x74, 0x2A, 0xC8, 0x96, 0x15, 0x4B, 0xA9, 0xF7,
    0xB6, 0xE8, 0x0A, 0x54, 0xD7, 0x89, 0x6B, 0x35
};

uint8_t crc8(uint8_t crc, uint8_t *data, size_t len)
{
    while (len--)
        crc = crc8Table[crc ^ *data++];
    return crc;
}

void* readerThread(void *arg)
{
    int res;
    uint8_t crc = 0;
    uint8_t frame[256];
    frame_header_t *header = (frame_header_t*)&frame[0];
    while(1)
    {
        res = scanFrameSync(frame);
        if (res < 0)
            return THREAD_FAILURE;
        res = read(g_fd, &frame[2], 1);
        if (res < 0)
        {
            fprintf (stderr, "Failed reading the frame header: %d\n", res);
            return THREAD_FAILURE;
        }
        res = read(g_fd, &frame[3], header->size);
        if (res < 0 || res != header->size)
        {
            fprintf (stderr, "Failed reading the frame data: %d\n", res);
            return THREAD_FAILURE;
        }
        crc = frame[header->size + 2];
        if (crc != crc8(0, frame, header->size + 2))
        {
            fprintf (stderr, "CRC mismatch for packet #%d\n", header->seq);
            return THREAD_FAILURE;
        }

        printf("RX FRAME %d: sz = %d crc = %X flag = %d\n", header->seq, header->size, crc, header->flag);

        processFrame(frame);
    }

    return NULL;
}


int set_speed(int fd, int speed)
{
    struct termios tty;
    
    tcgetattr(fd, &tty);
    tcflush(fd, 2);

    cfmakeraw(&tty);

    cfsetospeed(&tty, speed);
    if (tcsetattr(fd, TCSANOW, &tty) == -1)
    {
        fprintf (stderr, "Could not set the serial speed\n");
        return -1;
    }
    tcflush(fd, 2);
    return 0;
}

int set_parity(int fd, int bits, int parity, int stop_bits)
{
    struct termios tty;
    int csize = 0;
    
    tcgetattr(fd, &tty);

    tty.c_cflag &= ~CSIZE;
    assert(bits == 8);
    tty.c_cflag |= CS8;
    
    assert(parity == 78);
    tty.c_cflag &= ~PARENB;
    tty.c_iflag &= ~INPCK;
    
    assert(stop_bits == 1);
    tty.c_cflag &= ~CSTOPB;
    
    tty.c_iflag |= INPCK;
    
    tcflush(fd, 0);
    if (tcsetattr(fd, TCSANOW, &tty) == -1)
    {
        fprintf (stderr, "Could not change parity settings\n");
        return -1;
    }
    return 0;
}

int startReader(void)
{
    int res;
    g_fd = open("/dev/ttyS1", 0x102);
    if (g_fd < 0)
    {
        fprintf (stderr, "Could not open the serial device\n");
        return 1;
    }
    
    res = set_speed(g_fd, B38400);
    if (res != 0) return res;
    res = set_parity(g_fd, 8, 78, 1);
    if (res != 0) return res;

    pthread_create(&g_reader_thread, NULL, readerThread, NULL);
}

int main(int argc, char **argv)
{
    int c;
    void *readerResult;
    uint32_t addr = 0;
    user_request_t request = UR_NONE;

    while ((c = getopt (argc, argv, "vr:")) != -1)
    {
        switch (c)
        {
        case 'v':
            request = UR_GET_VERSION;
            break;
        case 'r':
            addr = strtoul(optarg, NULL, 16);
            request = UR_READ_MEM;
            break;
        case '?':
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            return 1;
        }
    }
    
    if (request == UR_NONE)
    {
        fprintf (stderr, "Please specify an option\n");
        return 1;
    }
    
    if (startReader() != 0)
    {
        fprintf (stderr, "startReader failure\n");
        return 1;
    }
    
    switch(request)
    {
        case UR_GET_VERSION:
            break;
    }
    
    pthread_join(g_reader_thread, &readerResult);
    if (readerResult != NULL)
    {
        fprintf (stderr, "reader failure detected\n");
        return 1;
    }

    return 0;
}