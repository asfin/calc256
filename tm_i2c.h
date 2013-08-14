/* - Version 1.0 - */

#define TM_ADDR         0xC0

#define TM_GET_TEMP     0x10
#define TM_GET_CORE0    0x11
#define TM_GET_CORE1    0x12

#define TM_SET_OE       0x20

static int tm_i2c_fd;

typedef struct {
	unsigned char cmd;
	unsigned char data_lsb;
	unsigned char data_msb;
} tm_struct;

int tm_i2c_init();
void tm_i2c_close();
unsigned int tm_i2c_req(int fd, unsigned char addr, unsigned char cmd, unsigned int data);
float tm_i2c_Data2Temp(unsigned int ans);
float tm_i2c_Data2Core(unsigned int ans);
float tm_i2c_gettemp(unsigned char slot);
float tm_i2c_getcore0(unsigned char slot);
float tm_i2c_getcore1(unsigned char slot);
void tm_i2c_set_oe(unsigned char slot);
void tm_i2c_clear_oe(unsigned char slot);
int tm_i2c_detect(unsigned char slot);

