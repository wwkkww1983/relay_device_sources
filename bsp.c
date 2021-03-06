#include <cfg/os.h>

#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>

#include <dev/board.h>
#include <dev/urom.h>
#include <dev/nplmmc.h>
#include <dev/sbimmc.h>
#include <fs/phatfs.h>

#include <sys/version.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/heap.h>
#include <sys/confnet.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/route.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>

#include <dev/watchdog.h>
#include <dev/board.h>
#include <dev/gpio.h>
#include <cfg/arch/avr.h>
#include <dev/nvmem.h>
#include <sys/atom.h>

//#include <arch/avr.h>

#include "bin_command_def.h"
#include "regtable.h"

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "bsp.h"

#define THISINFO           0
#define THISERROR          1


//
#define DEBUG_LED1     4
#define DEBUG_LED2     3
#define DEBUG_LED_PORT AVRPORTE
//

#define  IO_CONFIG0      6
#define  IO_CONFIG1      7


uint32_t group_arry4_to_uint32(const uint8_t arry[4])
{
	uint32_t out = arry[3];
	out <<= 8;
	out |= arry[2];
	out <<= 8;
	out |= arry[1];
	out <<= 8;
	out |= arry[0];
	return out;
}


void IoPortInit(void)
{
	//Debug LED pin
	GpioPinSetHigh(DEBUG_LED_PORT, DEBUG_LED1);
	GpioPinSetHigh(DEBUG_LED_PORT, DEBUG_LED2);
	GpioPinConfigSet(DEBUG_LED_PORT, DEBUG_LED1, GPIO_CFG_OUTPUT);
	GpioPinConfigSet(DEBUG_LED_PORT, DEBUG_LED2, GPIO_CFG_OUTPUT);
	//Config Pin
	GpioPinSetHigh(AVRPORTF, IO_CONFIG0);
	GpioPinSetHigh(AVRPORTF, IO_CONFIG1);
	GpioPinConfigSet(AVRPORTF, IO_CONFIG0, GPIO_CFG_PULLUP);
	GpioPinConfigSet(AVRPORTF, IO_CONFIG1, GPIO_CFG_PULLUP);

}

void BspDebugLedSet(uint8_t ledmsk)
{
	GpioPinSet(DEBUG_LED_PORT,DEBUG_LED1,(ledmsk&(1<<0))?0:1);
	GpioPinSet(DEBUG_LED_PORT,DEBUG_LED2,(ledmsk&(1<<1))?0:1);
}

unsigned char gconfig = 0;

uint8_t IoGetConfig(void)
{
	uint8_t io = 0;
	if(GpioPinGet(AVRPORTF,IO_CONFIG0)) {
		io |= (1<<0);
	}
#ifdef SWAP_CONFIG_PIN2_MODE
	if(!GpioPinGet(AVRPORTF,IO_CONFIG1)) {
		io |= (1<<1);
	}
#else
	if(GpioPinGet(AVRPORTF,IO_CONFIG1)) {
		io |= (1<<1);
	}
#endif
	gconfig = io;
	return io;
}

void IoPortOut(uint32_t out)
{
	out = out;
}

uint8_t IoPortIn(void)
{
	return 0;
}

//------------------------------------------------

#ifdef ONE_273_INPUT_MODE


void bsp_set_output_value(uint32_t out)
{
	out = out;
}

uint32_t bsp_get_input_value(void)
{
	return 0;
}
void clock_out_clock_in(void)
{
}

unsigned char clock_out_clock_in2(unsigned char out)
{
	return 0;
}

void bsp_input_output_init(void)
{
}

#endif

//------------------------------------------------



int BspReadIpConfig(CmdIpConfigData * cid)
{
	return NutNvMemLoad(BSP_IP_ADDR_OFFSET,cid,sizeof(CmdIpConfigData));
}
int BspWriteIpConfig(CmdIpConfigData * cid)
{
	int ret = 0;
#if 0
	if(IoGetConfig()&(1<<0)) {
		return NutNvMemSave(BSP_IP_ADDR_OFFSET,cid,sizeof(CmdIpConfigData));
	} else {
		return -1;
	}
#endif
	ret = NutNvMemSave(BSP_IP_ADDR_OFFSET,cid,sizeof(CmdIpConfigData));
	if(!ret) {
		//如果不是SET模式，则启动系统
#ifdef CHANGE_IP_REBOOT_SYSTEM
		if(!(IoGetConfig()&(1<<0))) {
		    RequestSystemReboot();
		}
#endif
	}
	return ret;
}

extern unsigned char input_trig_mode[INPUT_CHANNEL_NUM];

int BspManualCtlModeInit(void)
{
	return NutNvMemLoad(BSP_MODE_INDEX_OFFSET,input_trig_mode,INPUT_CHANNEL_NUM);
}

#include "bsp.h"

#if 0
int BspReadManualCtlModeIndex(unsigned char index,unsigned char * mode)
{
	int ret  = 0;
	if(index >= INPUT_CHANNEL_NUM) {
		return -1;
	}
	ret = NutNvMemLoad(BSP_MODE_INDEX_OFFSET+index,&input_trig_mode[index],1);
	*mode = input_trig_mode[index];
	return ret;
}

int BspWriteManualCtlModeIndex(unsigned char index,unsigned char mode)
{
	int ret = 0;
	if(index >= INPUT_CHANNEL_NUM) {
		return -1;
	}
	ret = NutNvMemSave(BSP_MODE_INDEX_OFFSET+index,&mode,1);
	if(ret == 0) {
	    input_trig_mode[index] = mode;
	}
	return ret;
}
#else  //根据新控制协议，改装旧的接口
int BspReadManualCtlModeIndex(unsigned char index,unsigned char * mode)
{
	int ret = -1;
	CmdInputControl ctl;
	uint16_t offset = BSP_MODE_CTL_OFFSET + index * sizeof(CmdInputControl);
	if(index >= INPUT_CHANNEL_NUM) {
		return ret;
	}
	ret = NutNvMemLoad(offset,&ctl,sizeof(CmdInputControl));
	if(ret == 0) {
		*mode = ctl.mode;
	}
	return ret;
}

int BspWriteManualCtlModeIndex(unsigned char index,unsigned char mode)
{
	int ret = -1;
	CmdInputControl ctl;
	uint16_t offset = BSP_MODE_CTL_OFFSET + index * sizeof(CmdInputControl);
	if(index >= INPUT_CHANNEL_NUM) {
		return ret;
	}
	ret = NutNvMemLoad(offset,&ctl,sizeof(CmdInputControl));
	ctl.mode = mode;
	ret = NutNvMemSave(offset,&ctl,sizeof(CmdInputControl));
	if(ret == 0) {
		input_trig_mode[index] = mode;
	}
	return ret;
}
#endif


int BspAvrResetType(void)
{
	uint8_t reg = inb(MCUCSR);
	outb(MCUCSR,0);
	return reg;
}

void dumpdata(void * _buffer,int len)
{
	int i;
	unsigned char * buf = (unsigned char *)_buffer;
	if(1)for(i=0;i<len;i++) {
		printf("%02X ",buf[i]);
	}
	if(1)printf("\r\n");
}



uint32_t toheip(u8_t ip[])
{
	uint32_t ipaddr = 0;
	ipaddr = ip[3];
	ipaddr <<= 8;
	ipaddr |= ip[2];
	ipaddr <<= 8;
	ipaddr |= ip[1];
	ipaddr <<= 8;
	ipaddr |= ip[0];
	return ipaddr;
}


//
int BspReadBoardInfo(CmdBoardInfo * info)
{
	return NutNvMemLoad(BSP_BOARD_INFO_OFFSET,info,sizeof(CmdBoardInfo));
}
int BspWriteBoardInfo(CmdBoardInfo * info)
{
	return NutNvMemSave(BSP_BOARD_INFO_OFFSET,info,sizeof(CmdBoardInfo));
}
//

#ifdef APP_CGI_ON

//读写网页密码
int BspReadWebPassword(char *password)
{
	int ret;
	if(IoGetConfig()&(1<<0)) {
		gpassword[0] = 'a';
		gpassword[1] = 'd';
		gpassword[2] = 'm';
		gpassword[3] = 'i';
		gpassword[4] = 'n';
		gpassword[5] = 0;
		ret = 0;
	} else {
	    ret = NutNvMemLoad(BSP_KEY_OFFSET,password,sizeof(gpassword));
	    gpassword[31] = 0;
	}
	return ret;
}
int BspWriteWebPassword(char *password)
{
	return NutNvMemSave(BSP_KEY_OFFSET,password,sizeof(gpassword));
}
//
#endif

int BspReadHostAddress(host_address * info)
{
	return NutNvMemLoad(HOST_ADDRESS_CONFIG,info,sizeof(host_address));
}
int BspWriteHostAddress(host_address * info)
{
	return NutNvMemSave(HOST_ADDRESS_CONFIG,info,sizeof(host_address));
}
//
int BspReadIoName(uint8_t addr[2],CmdIoName * io)
{
	uint16_t offset = BSP_IO_NAME_OFFSET;
	if(addr[1] == 0) {
		offset += addr[0]*sizeof(CmdIoName);
	} else if(addr[1] == 1) {
		offset += OUTPUT_NUM*sizeof(CmdIoName) + addr[0]*sizeof(CmdIoName);
	} else {
		memset(io->io_name,0,sizeof(io->io_name));
		return -1;
	}
	return NutNvMemLoad(offset,io,sizeof(CmdIoName));
}
int BspWriteIoName(uint8_t addr[2],CmdIoName * io)
{
	uint16_t offset = BSP_IO_NAME_OFFSET;
	if(addr[1] == 0) {
		offset += addr[0]*sizeof(CmdIoName);
	} else if(addr[1] == 1) {
		offset += OUTPUT_NUM*sizeof(CmdIoName) + addr[0]*sizeof(CmdIoName);
	} else {
		memset(io->io_name,0,sizeof(io->io_name));
		return -1;
	}
	if(1) {
		return NutNvMemSave(offset,io,sizeof(CmdIoName));
	}
	return -1;
}
//------------------------------------------------------------
#ifdef APP_TIMEIMG_ON

int BspReadIoTimingInfo(timint_info * info)
{
	int ret;
	uint16_t offset = BSP_IO_TIMING_OFFSET;
	uint8_t  buffer[sizeof(timint_info)+2];
	memset(info,0,sizeof(timint_info));
	memset(buffer,0,sizeof(buffer));
	ret = NutNvMemLoad(offset,buffer,sizeof(buffer));
	if(!ret) {
#if THISINFO
		int i;
		printf("read timing info:");
		for(i=0;i<sizeof(buffer);i++) {
			printf("0x%x,",buffer[i]);
		}
		printf("\r\n");
		printf("buffer[0]=0x%x,buffer[%d]=0x%x\r\n",buffer[0],sizeof(buffer)-1,buffer[sizeof(buffer)-1]);
#endif
		ret = -1;
		if(buffer[0] == 0x55 && buffer[sizeof(timint_info)+1] == 0xAA) {
			memcpy(info,&buffer[1],sizeof(timint_info));
			ret = 0;
		} else {
			if(THISERROR)printf("ERROR:read timing info data value not valid!\r\n");
		}
	} else {
		if(THISERROR)printf("ERROR:Nv Mem load failed!\r\n");
	}
	return ret;
}
int BspReadIoTiming(uint16_t index,timing_node_eeprom * time)
{
	uint16_t offset = BSP_IO_TIMING_OFFSET+1+sizeof(timint_info)+1;
	if(index >= SYS_TIMING_COUNT_MAX) {
		return -1;
	}
	offset += sizeof(timing_node_eeprom)*index;
	return NutNvMemLoad(offset,time,sizeof(timing_node_eeprom));
}
//----
uint8_t  time_last_head[sizeof(timint_info)+2];

int BspTimingStartWrite(void)
{
	uint16_t offset = BSP_IO_TIMING_OFFSET;
	uint8_t  buffer[sizeof(timint_info)+2];
	memset(buffer,0,sizeof(buffer));
	buffer[0] = 0x55;
	NutNvMemLoad(offset,time_last_head,sizeof(time_last_head));
	return NutNvMemSave(offset,buffer,sizeof(buffer));
}
int BspWriteIoTimingInfo(timint_info * info)
{
	uint16_t offset = BSP_IO_TIMING_OFFSET+1;
	if(info->time_max_count > SYS_TIMING_COUNT_MAX) {
		//还原
		NutNvMemSave(BSP_IO_TIMING_OFFSET,time_last_head,sizeof(time_last_head));
		return -1;
	}
	return NutNvMemSave(offset,info,sizeof(timint_info));
}
int BspTimingDoneWrite(void)
{
	uint16_t offset = BSP_IO_TIMING_OFFSET+1+sizeof(timint_info);
	uint8_t ox = 0xAA;
	return NutNvMemSave(offset,&ox,sizeof(ox));
}

int BspWriteIoTiming(uint16_t index,timing_node_eeprom * time)
{
	uint16_t offset = BSP_IO_TIMING_OFFSET+1+sizeof(timint_info)+1;
	if(index >= SYS_TIMING_COUNT_MAX) {
		return -1;
	}
	offset += sizeof(timing_node_eeprom)*index;
	return NutNvMemSave(offset,time,sizeof(timing_node_eeprom));
}
#endif

void BspWriteEepromSerialAddress(uint16_t address)
{
	uint16_t offset = BSP_IO_SERIAL_ADDR_OFFSET;
	if(IoGetConfig()&(1<<0)) {  //配置为1的时候，才能写数据
		uint16_t tmp;
		NutNvMemLoad(offset,&tmp,sizeof(tmp));
		if(tmp != address) {
			NutNvMemSave(offset,&address,sizeof(address));
		}
	}
}

uint16_t BspReadEepromSerialAddress(void)
{
	uint16_t tmp = 0;
	uint16_t offset = BSP_IO_SERIAL_ADDR_OFFSET;
	NutNvMemLoad(offset,&tmp,sizeof(tmp)); //内部串口地址，不管是否配置模式
	return tmp;
}

#ifdef APP_MULTI_MANGER_FRAME
void BspLoadmultimgr_info(device_info_st * info)
{
	uint16_t offset = BSP_MULTI_MANGER_DATA_OFFSET;
	NutNvMemLoad(offset,info,sizeof(device_info_st));
}
void BspSavemultimgr_info(device_info_st * info)
{
	uint16_t offset = BSP_MULTI_MANGER_DATA_OFFSET;
	NutNvMemSave(offset,info,sizeof(device_info_st));
}
#endif

char BspReadFactoryOut(void)
{
	char ch;
	uint16_t offset = BSP_FACTORY_OUT_OFFSET;
	NutNvMemLoad(offset,&ch,sizeof(char));
	return ch;
}
void BspWriteFactoryOut(char ch)
{
	uint16_t offset = BSP_FACTORY_OUT_OFFSET;
	NutNvMemSave(offset,&ch,sizeof(char));
}

#ifdef APP_HTTP_PROTOTOL_CLIENT 
int load_relay_info(ethernet_relay_info * info)
{
	uint16_t offset = BSP_HTTP_CLIENT_INFO_OFFSET;
	NutNvMemLoad(offset,info,sizeof(ethernet_relay_info));
}
int save_relay_info(ethernet_relay_info * info)
{
	uint16_t offset = BSP_HTTP_CLIENT_INFO_OFFSET;
	NutNvMemSave(offset,info,sizeof(ethernet_relay_info));
}
#endif



//--------------------------------------------------------------------
/*
* 以下是控制模式的代码
*/
extern unsigned int  input_filter_hold_time_max[INPUT_CHANNEL_NUM];
extern unsigned int  input_trig_before_delay_max[INPUT_CHANNEL_NUM],input_trig_after_delay_max[INPUT_CHANNEL_NUM];
extern unsigned char input_trig_mode[INPUT_CHANNEL_NUM];
extern unsigned char input_trig_to_witch_io_out[INPUT_CHANNEL_NUM]; //需要触发那一路？只能映射一路


int BspReadControlMode(unsigned char index,CmdInputControl * pm)
{
	int ret = -1;
	uint16_t offset = BSP_MODE_CTL_OFFSET + index * sizeof(CmdInputControl);
	if(index >= INPUT_CHANNEL_NUM) {
		return ret;
	}
	ret = NutNvMemLoad(offset,pm,sizeof(CmdInputControl));
	if(ret == 0) {
		unsigned int tmp;
		input_trig_mode[index] = pm->mode;

		tmp = pm->input_filter_time_hi;
		tmp <<= 8;
		tmp += pm->input_filter_time_lo;
		input_filter_hold_time_max[index] = tmp;

		tmp = pm->input_trig_front_time_hi;
		tmp <<= 8;
		tmp += pm->input_trig_front_time_lo;
		input_trig_before_delay_max[index] = tmp;

		tmp = pm->input_trig_after_time_hi;
		tmp <<= 8;
		tmp += pm->input_trig_after_time_lo;
		input_trig_after_delay_max[index] = tmp;

		input_trig_to_witch_io_out[index] = pm->input_trig_io_number;
	}
	return ret;
}
int BspWriteControlMode(unsigned char index,CmdInputControl * pm)
{
	int ret = -1;
	uint16_t offset = BSP_MODE_CTL_OFFSET + index * sizeof(CmdInputControl);
	if(index >= INPUT_CHANNEL_NUM) {
		return ret;
	}
	ret = NutNvMemSave(offset,pm,sizeof(CmdInputControl));
	if(ret == 0) {
		unsigned int tmp;
		input_trig_mode[index] = pm->mode;

		tmp = pm->input_filter_time_hi;
		tmp <<= 8;
		tmp += pm->input_filter_time_lo;
		input_filter_hold_time_max[index] = tmp;

		tmp = pm->input_trig_front_time_hi;
		tmp <<= 8;
		tmp += pm->input_trig_front_time_lo;
		input_trig_before_delay_max[index] = tmp;

		tmp = pm->input_trig_after_time_hi;
		tmp <<= 8;
		tmp += pm->input_trig_after_time_lo;
		input_trig_after_delay_max[index] = tmp;

		input_trig_to_witch_io_out[index] = pm->input_trig_io_number;
	}
	return ret;
}
/*
* 以上 是控制模式的代码
*/
//--------------------------------------------------------------------




//根据不同的地址，不同的长度判断读写什么内容
//如果非法地址，或者未定义地址，则返回0，否则返回读取的长度

int device_read_register(unsigned int addr,unsigned char * buffer,unsigned int len)
{
	reg_node * node = &reg_table[0];
	while(1)
	{
		node++;
		if(node->data_len == 0 && node->count == 0) {
			break;
		}
		if((addr >= node->base_addr) && 
			(addr < (node->base_addr + (node->data_len*node->count))) && 
			((addr%node->data_len) == 0)) 
		{
			if(len == node->data_len) {
				//NutNvMemLoad(offset,info,sizeof(ethernet_relay_info));
				break;
			}
		}
	}
	return 0;
}

int device_write_register(unsigned int addr,unsigned char * buffer,unsigned int len)
{
	return 0;
}




