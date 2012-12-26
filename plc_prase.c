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
#include <errno.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>

#include <dev/board.h>
#include <dev/gpio.h>
#include <cfg/arch/avr.h>
#include <dev/nvmem.h>
#include <time.h>
#include <sys/mutex.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "sysdef.h"
#include "bin_command_def.h"
#include "bsp.h"
#include "time_handle.h"
#include "io_time_ctl.h"
#include <dev/relaycontrol.h>
#include "sys_var.h"
#include "io_out.h"
#include "debug.h"

#include "plc_command_def.h"
#include "plc_prase.h"
#include "compiler.h"
//#include "sysdef.h"
//#include "bsp.h"
//#include "io_out.h"
//#include "debug.h"

#define THIS_INFO  0
#define THIS_ERROR 0


//100ms��ʱ���Ŀ������ݽṹ
typedef struct _TIM100MS_ARRAYS_T
{
    WORD  counter[TIMING100MS_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  enable_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  event_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  holding_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
} TIM100MS_ARRAYS_T;

TIM100MS_ARRAYS_T  tim100ms_arrys;



//1s��ʱ���Ŀ������ݽṹ
typedef struct _TIM1S_ARRAYS_T
{
    WORD  counter[TIMING1S_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  enable_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  event_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  holding_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
} TIM1S_ARRAYS_T;

TIM1S_ARRAYS_T    tim1s_arrys;

//�������Ŀ������ݽṹ
typedef struct _COUNTER_ARRAYS_T
{
    WORD  counter[COUNTER_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];		
	BYTE  event_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(COUNTER_EVENT_COUNT)];
	BYTE  last_trig_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];
} COUNTER_ARRAYS_T;

COUNTER_ARRAYS_T counter_arrys;

//�����
unsigned char inputs_new[BITS_TO_BS(IO_INPUT_COUNT)];
unsigned char inputs_last[BITS_TO_BS(IO_INPUT_COUNT)];
//����̵���
unsigned char output_last[BITS_TO_BS(IO_OUTPUT_COUNT)];
unsigned char output_new[BITS_TO_BS(IO_OUTPUT_COUNT)];
//�����̵���
unsigned char auxi_relays[BITS_TO_BS(AUXI_RELAY_COUNT)];
unsigned char auxi_relays_last[BITS_TO_BS(AUXI_RELAY_COUNT)];
//�ֽڱ�����ͨ�ñ���
unsigned char general_reg[REG_COUNT];   //��ͨ����
//RTCʵʱʱ��Ӱ��Ĵ�����û�����һ���Զ��仯
unsigned char rtc_reg[REG_RTC_COUNT];
//�¶ȼĴ�����1�����һ��
unsigned char temp_reg[REG_TEMP_COUNT];
//��ʱ�����壬�Զ����ڲ���ʱ��������м���
volatile unsigned int  time100ms_come_flag;
volatile unsigned int  time1s_come_flag;
//�������ļĴ���
#define  BIT_STACK_LEVEL     32
unsigned char bit_acc;
unsigned char bit_stack[BITS_TO_BS(BIT_STACK_LEVEL)];   //���ض�ջ��PLC��λ������ѹջ������ܹ���32��ջ
unsigned char bit_stack_sp;   //���ض�ջ��ָ��

//ָ�����
unsigned int  plc_command_index;     //��ǰָ��������
unsigned char plc_command_array[32]; //��ǰָ���ֽڱ���
#define       PLC_CODE     (plc_command_array[0])

//������״̬
unsigned char plc_cpu_stop;
//ͨ��Ԫ������
unsigned int  net_communication_count = 0;
unsigned int  net_global_send_index = 0; //�������ƣ�ָʾ��һ�������͵�����

/*************************************************
 * ������˽�е�ʵ��
 */
//�ڲ���ϵͳ������
static uint32_t last_tick;
static uint32_t last_tick1s;

void sys_time_tick_init(void)
{
	last_tick = NutGetMillis();
	last_tick1s = NutGetMillis();
}

void plc_rtc_tick_process(void)
{
#if 0
	//��ʱ�����
	unsigned int TP_temp = ReadTemperatureXX_XC();
	DS1302_VAL tval;
	sys_lock();
	temp_reg[1] = TP_temp / 100;  //��
	temp_reg[0] = TP_temp % 100;  //С����
	sys_unlock();
	ReadRTC(&tval);
	BCD2Hex(&tval,sizeof(tval));
	//���°��մ�С�������򣬷ֱ�Ϊ���룬�֣�ʱ���գ��£��꣬���������
	sys_lock();
	rtc_reg[5] = tval.YEAR;
	rtc_reg[4] = tval.MONTH;
	rtc_reg[3] = tval.DATE;
	rtc_reg[2] = tval.HR;
	rtc_reg[1] = tval.MIN;
	rtc_reg[0] = tval.SEC;
	rtc_reg[6] = tval.DAY;
	sys_unlock();
#endif
}

void plc_timing_tick_process(void)
{
	uint32_t curr = NutGetMillis();
	if((curr - last_tick) >= 100) {  //100ms
		sys_lock();
		time100ms_come_flag++;
		sys_unlock();
		last_tick = curr;
		//if(THIS_INFO)putrsUART((ROM char*)"time tick 100ms");
	}
	if((curr - last_tick1s) >= 1000) {
		sys_lock();
		time1s_come_flag++;
		sys_unlock();
		last_tick1s = curr;
		plc_rtc_tick_process();
		//if(THIS_INFO)putrsUART((ROM char*)"time tick 1s");
	}
}




/*********************************************
 * ϵͳ��ʼ��
 */

void PlcInit(void)
{
	bit_acc = 0;
	memset(bit_stack,0,sizeof(bit_stack));
	bit_stack_sp = 0;
	io_in_get_bits(0,inputs_new,IO_INPUT_COUNT);
	memcpy(inputs_last,inputs_new,sizeof(inputs_new));
    memset(auxi_relays,0,sizeof(auxi_relays));
    memset(auxi_relays_last,0,sizeof(auxi_relays_last));
	plc_command_index = 0;
	memset(output_new,0,sizeof(output_new));
	memset(output_last,0,sizeof(output_last));
    io_out_get_bits(0,output_last,IO_OUTPUT_COUNT);
	sys_time_tick_init();
	time100ms_come_flag = 0;
	time1s_come_flag = 0;
	memset(&tim100ms_arrys,0,sizeof(tim100ms_arrys));
	memset(&tim1s_arrys,0,sizeof(tim1s_arrys));
    memset(&counter_arrys,0,sizeof(counter_arrys));
	memset(general_reg,0,sizeof(general_reg));
	sys_time_tick_init();
	plc_rtc_tick_process();
}


/*
    ָ��                      ��ַ         ����
51 00 00 00 00 05 00  +   34 00 01 00  + 00  //0
51 00 00 00 00 05 00  +   35 00 01 00  + FF  //end  
51 00 00 00 00 07 00  +   36 00 03 00  + 01 00 00   //ld 0x00,0x00
51 00 00 00 00 07 00  +   39 00 03 00  + 04 01 00   //ld 0x00,0x00
51 00 00 00 00 05 00  +   42 00 01 00  + 14
51 00 00 00 00 07 00  +   43 00 03 00  + 04 01 01   //ld 0x00,0x00
51 00 00 00 00 05 00  +   46 00 01 00  + FF    //end

/-------------------+-----------+-- -- --
51 00 00 00 00 05 00 34 00 01 00 00 
51 00 00 00 00 05 00 35 00 01 00 FF 
51 00 00 00 00 05 00 35 00 01 00 00 

51 00 00 00 00 07 00 36 00 03 00 03 00 00
51 00 00 00 00 07 00 39 00 03 00 04 01 00
51 00 00 00 00 05 00 42 00 01 00 14
51 00 00 00 00 07 00 43 00 03 00 04 01 01
51 00 00 00 00 05 00 46 00 01 00 FF

/-------------------+-----------+-- -- --
50 00 00 00 00 05 00 34 00 14 00

50 00 00 01 00 18 00 34 00 14 00 00 03 00 00 04 01 00 14 04 01 01 ff 00 00 00 00 00 00 00 00 

03 08 00 
16 08 00 00 0A 
01 08 00 
23 01 00 
14 
23 01 01 
FF

*/


const unsigned char plc_test_flash[128] =
{
	0,
	PLC_BCMP, 21,    0x10,0x08,  0x02,0x00,
	PLC_BCMP, 22,    0x10,0x08,  0x02,0x03,
	PLC_BCMP, 23,    0x10,0x08,  0x02,0x06,
	PLC_BCMP, 24,    0x10,0x08,  0x02,0x09,
	PLC_BCMP, 25,    0x10,0x08,  0x02,0x0C,
	PLC_BCMP, 26,    0x10,0x08,  0x02,0x0F,
	PLC_BCMP, 27,    0x10,0x08,  0x02,0x12,
	PLC_BCMP, 28,    0x10,0x08,  0x02,0x15,

	PLC_LD,   0x02,0x01,
	PLC_OUT,  0x01,0x00,

	PLC_LD,   0x02,0x04,
	PLC_OUT,  0x01,0x01,

	PLC_LD,   0x02,0x07,
	PLC_OUT,  0x01,0x02,

	PLC_LD,   0x02,0x0A,
	PLC_OUT,  0x01,0x03,

	PLC_LD,   0x02,0x0D,
	PLC_OUT,  0x01,0x04,

	PLC_LD,   0x02,0x10,
	PLC_OUT,  0x01,0x05,

	PLC_LD,   0x02,0x13,
	PLC_OUT,  0x01,0x06,

	PLC_LD,   0x02,0x16,
	PLC_OUT,  0x01,0x07,

	PLC_END
};

int compare_rom(const unsigned char * dat,unsigned int len)
{
#if 0
	unsigned int i;
	unsigned int base = GET_OFFSET_MEM_OF_STRUCT(My_APP_Info_Struct,plc_programer);

	
	for(i=0;i<sizeof(plc_test_flash);i++) {
		XEEBeginRead(base+i);
		if(XEERead() != plc_test_flash[i]) {
			XEEEndRead();
			PrintStringNum("error addr:",base+i);
			return -1;
		}
		XEEEndRead();
	}
#endif
	return 0;
}

/**********************************************
 *  ��ȡ��һ��ָ���ָ����
 *  Ҳ���Ǵ�EEPROM�ж�ȡ�ĳ���ű�
 *  ����һ���Զ�ȡ��һ��ָ�����Ϊ�ָ���
 */
void plc_code_test_init(void)
{
#if 0
	unsigned int i;
	unsigned int base = GET_OFFSET_MEM_OF_STRUCT(My_APP_Info_Struct,plc_programer);


	for(i=0;i<sizeof(plc_test_flash);i++) {
		XEEBeginWrite(base+i);
		XEEWrite(plc_test_flash[i]);
		XEEEndWrite();
	}
	

	if(!compare_rom(plc_test_flash,sizeof(plc_test_flash))) {
		PrintStringNum("\r\ncompare successful!",0);
	} else {
		PrintStringNum("\r\ncompare failed.",0);
	}


	PrintStringNum("\r\noffset of programmer :",base);
	PrintStringNum("\r\nsize of programmer :",sizeof(My_APP_Info_Struct) - base);
	PrintStringNum("\r\nsize of My_APP_Info_Struct :",sizeof(My_APP_Info_Struct));
#endif
}

void read_next_plc_code(void)
{
#if 0
	unsigned int i;
	unsigned int base = GET_OFFSET_MEM_OF_STRUCT(My_APP_Info_Struct,plc_programer);
	unsigned int size = sizeof(My_APP_Info_Struct) - base;
	if((plc_command_index + sizeof(plc_command_array)) > size) {
		//������������ڴ�
		plc_command_index = 0;
	}
	base = plc_command_index + base + 1;
	XEEBeginRead(base);
	for(i=0;i<sizeof(plc_command_array);i++) {
		plc_command_array[i] = XEERead();
	}
	XEEEndRead();
	//dumpstrhex("CMD:",plc_command_array,3);
#else
	memcpy(plc_command_array,&plc_test_flash[plc_command_index+1],sizeof(plc_command_array));
	//strncpypgm2ram(plc_command_array,&plc_test_flash[plc_command_index+1],sizeof(plc_command_array));
#endif
}

void handle_plc_command_error(void)
{
	//��ʾ�ڼ���ָ�����
	//Ȼ��λ����ֹͣ����
	plc_cpu_stop = 1;
	//if(THIS_ERROR)putrsUART((ROM char*)"\r\nhandle_plc_command_error!!!!!!\r\n");
}

unsigned char get_bitval(unsigned int index)
{
	unsigned char bitval = 0;
	if(index >= IO_INPUT_BASE && index < (IO_INPUT_BASE+IO_INPUT_COUNT)) {
		index -= IO_INPUT_BASE;
        bitval = BIT_IS_SET(inputs_new,index);
	} else if(index >= IO_OUTPUT_BASE && index < (IO_OUTPUT_BASE+IO_OUTPUT_COUNT)) {
		index -= IO_OUTPUT_BASE;
        bitval = BIT_IS_SET(output_new,index);
    } else if(index >= AUXI_RELAY_BASE && index < (AUXI_RELAY_BASE + AUXI_RELAY_COUNT)) {
		index -= AUXI_RELAY_BASE;
        bitval = BIT_IS_SET(auxi_relays,index);
    } else if(index >= AUXI_HOLDRELAY_BASE && index < (AUXI_HOLDRELAY_BASE + AUXI_HOLDRELAY_COUNT)) {
#if 0
		unsigned char B,b,reg;
		index -= AUXI_HOLDRELAY_BASE;
		B = index / 8;
		b = index % 8;
		RtcRamRead(B,&reg,1);
		bitval = BIT_IS_SET(&reg,b);
#endif
	} else if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
		index -= TIMING100MS_EVENT_BASE;
        bitval = BIT_IS_SET(tim100ms_arrys.event_bits,index);
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE + TIMING1S_EVENT_COUNT)) {
		index -= TIMING1S_EVENT_BASE;
        bitval = BIT_IS_SET(tim1s_arrys.event_bits,index);
	} else if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
        bitval = BIT_IS_SET(counter_arrys.event_bits,index);
	} else {
		//handle_plc_command_error();
		bitval = 0;
	}
	return bitval;
}

static unsigned char get_last_bitval(unsigned int index)
{
	unsigned char bitval = 0;
	if(index >= IO_INPUT_BASE && index < (IO_INPUT_BASE+IO_INPUT_COUNT)) {
		index -= IO_INPUT_BASE;
        bitval = BIT_IS_SET(inputs_last,index);
	} else if(index >= IO_OUTPUT_BASE && index < (IO_OUTPUT_BASE+IO_OUTPUT_COUNT)) {
		index -= IO_OUTPUT_BASE;
        bitval = BIT_IS_SET(output_last,index);
	} else if(index >= AUXI_RELAY_BASE && index < (AUXI_RELAY_BASE + AUXI_RELAY_COUNT)) {
		index -= AUXI_RELAY_BASE;
        bitval = BIT_IS_SET(auxi_relays_last,index);
    } else if(index >= AUXI_HOLDRELAY_BASE && index < (AUXI_HOLDRELAY_BASE + AUXI_HOLDRELAY_COUNT)) {
#if 0
		unsigned char B,b,reg;
		index -= AUXI_HOLDRELAY_BASE;
		index += AUXI_HOLDRELAY_COUNT / 8; //����һ��������һ�ε�
		B = index / 8;
		b = index % 8;
		RtcRamRead(B,&reg,1);
		bitval = BIT_IS_SET(&reg,b);
#endif
	} else if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
		index -= TIMING100MS_EVENT_BASE;
        bitval = BIT_IS_SET(tim100ms_arrys.event_bits_last,index);
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE + TIMING1S_EVENT_COUNT)) {
		index -= TIMING1S_EVENT_BASE;
        bitval = BIT_IS_SET(tim1s_arrys.event_bits_last,index);
	} else if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
        bitval = BIT_IS_SET(counter_arrys.event_bits_last,index);
	} else {
		//handle_plc_command_error();
		bitval = 0;
	}
	return bitval;
}

void set_bitval(unsigned int index,unsigned char bitval)
{
	if(index >= IO_INPUT_BASE && index < (IO_INPUT_BASE+IO_INPUT_COUNT)) {
		//����ֵ�����޸�
	} else if(index >= IO_OUTPUT_BASE && index < (IO_OUTPUT_BASE+IO_OUTPUT_COUNT)) {
		index -= IO_OUTPUT_BASE;
        SET_BIT(output_new,index,bitval);
	} else if(index >= AUXI_RELAY_BASE && index < (AUXI_RELAY_BASE + AUXI_RELAY_COUNT)) {
		index -= AUXI_RELAY_BASE;
        SET_BIT(auxi_relays,index,bitval);
    } else if(index >= AUXI_HOLDRELAY_BASE && index < (AUXI_HOLDRELAY_BASE + AUXI_HOLDRELAY_COUNT)) {
#if 0
		unsigned char B,b,reg;
		index -= AUXI_HOLDRELAY_BASE;
		B = index / 8;
		b = index % 8;
		RtcRamRead(B,&reg,1);
		SET_BIT(&reg,b,bitval);
		RtcRamWrite(B,&reg,1);
#endif
	} else if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    //��������ֵ��������λ,ֻ���Ը�λ
		if(!bitval) {
		    index -= COUNTER_EVENT_BASE;
            counter_arrys.counter[index] = 0;
            SET_BIT(counter_arrys.event_bits,index,0);
		}
	} else {
		//handle_plc_command_error();
	}
}

unsigned char get_byte_val(unsigned int index)
{
	unsigned char reg;
	if(index >= REG_BASE && index < (REG_BASE+REG_COUNT)) {
		//��ͨ�ֽڱ���
		reg = general_reg[index-REG_BASE];
	} else if(index >=REG_RTC_BASE && index <(REG_RTC_BASE+REG_RTC_COUNT)) {
		//��ȡRTCʱ��,��ַ�ֱ��������գ�ʱ���룬����
		reg = rtc_reg[index-REG_RTC_BASE];
	} else if(index >=REG_TEMP_BASE && index <(REG_TEMP_BASE+REG_TEMP_COUNT)) {
		reg = temp_reg[index-REG_TEMP_BASE];
	} else {
		reg = 0;
	}
	return reg;
}

void set_byte_val(unsigned int index,unsigned char val)
{
	if(index >= REG_BASE && index < (REG_BASE+REG_COUNT)) {
		//��ͨ�ֽڱ���
		general_reg[index-REG_BASE] = val;
	} else if(index >=REG_RTC_BASE && index <(REG_RTC_BASE+REG_RTC_COUNT)) {
		//��ȡRTCʱ��,��ַ�ֱ��������գ�ʱ���룬����
		//rtc_reg[index-REG_RTC_BASE] = val;��ʱ�����޸�ʱ��
	} else if(index >=REG_TEMP_BASE && index <(REG_TEMP_BASE+REG_TEMP_COUNT)) {
	}
}

/**********************************************
 * ���������Լ�ʱ����������
 * ���ʱ�䵽�ˣ��򴥷��¼�
 * ���ʱ����δ�����������ʱ
 */
void timing_cell_prcess(void)
{
	unsigned int  i;
	unsigned int  counter;
	sys_lock();
	counter = time100ms_come_flag;
	time100ms_come_flag = 0;
	sys_unlock();
    {
	    TIM100MS_ARRAYS_T * ptiming = &tim100ms_arrys;
	    for(i=0;i<GET_ARRRYS_NUM(tim100ms_arrys.counter);i++) {
		    if(BIT_IS_SET(ptiming->enable_bits,i)) { //���������ʱ
			    if(counter > 0 && !BIT_IS_SET(ptiming->event_bits,i)) {  //���ʱ���¼�δ����
				    if(ptiming->counter[i] > counter) {
					    ptiming->counter[i] -= counter;
					} else {
					    ptiming->counter[i] = 0;
					}
					if(ptiming->counter[i] == 0) {
					    SET_BIT(ptiming->event_bits,i,1);
						//if(THIS_INFO)putrsUART((ROM char*)"timing100ms event come.");
					}
				}
			} else {
			    if(BIT_IS_SET(ptiming->holding_bits,i)) {
				    //���ֶ�ʱ��
				} else {
				    //������
					ptiming->counter[i] = 0;
					SET_BIT(ptiming->event_bits,i,0);
					//if(THIS_INFO)putrsUART((ROM char*)"timing100ms end.");
				}
			}
	    }
	}
 	sys_lock();
	counter = time1s_come_flag;
	time1s_come_flag = 0;
	sys_unlock();
	{
	    TIM1S_ARRAYS_T * ptiming = &tim1s_arrys;
	    for(i=0;i<GET_ARRRYS_NUM(tim1s_arrys.counter);i++) {
		    if(BIT_IS_SET(ptiming->enable_bits,i)) { //���������ʱ
			    if(counter > 0 && !BIT_IS_SET(ptiming->event_bits,i)) {  //���ʱ���¼�δ����
				    if(ptiming->counter[i] > counter) {
					    ptiming->counter[i] -= counter;
					} else {
					    ptiming->counter[i] = 0;
						//if(THIS_INFO)putrsUART((ROM char*)"1s come.");
					}
					if(ptiming->counter[i] == 0) {
					    SET_BIT(ptiming->event_bits,i,1);
						//if(THIS_INFO)putrsUART((ROM char*)"timing1s event [come].");
					}
				}
			} else {
			    if(BIT_IS_SET(ptiming->holding_bits,i)) {
				    //���ֶ�ʱ��
				} else {
				    //������
					ptiming->counter[i] = 0;
					SET_BIT(ptiming->event_bits,i,0);
					//if(THIS_INFO)putrsUART((ROM char*)"timing1s end.");
				}
			}
	    }
	}
}

/**********************************************
 * �򿪶�ʱ�������趨����ʱ������ֵ
 * ����Ѿ���ʼ��ʱ���������ʱ�����û�п�ʼ����ʼ
 */
static void timing_cell_start(unsigned int index,unsigned int event_count,unsigned char upordown,unsigned char holding)
{
	if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
	    TIM100MS_ARRAYS_T * ptiming = &tim100ms_arrys;
		index -= TIMING100MS_EVENT_BASE;
		if(!BIT_IS_SET(ptiming->enable_bits,index)) {
		    ptiming->counter[index]    = event_count;
			SET_BIT(ptiming->enable_bits,  index,1);
			SET_BIT(ptiming->upordown_bits,index,upordown);
			SET_BIT(ptiming->holding_bits, index,holding);
			SET_BIT(ptiming->event_bits,   index,0);
			//if(THIS_INFO)putrsUART((ROM char*)"timing100ms start.");
		}
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE+TIMING1S_EVENT_COUNT)) {
	    TIM1S_ARRAYS_T * ptiming = &tim1s_arrys;
		index -= TIMING1S_EVENT_BASE;
		if(!BIT_IS_SET(ptiming->enable_bits,index)) {
		    ptiming->counter[index]    = event_count;
			SET_BIT(ptiming->enable_bits,  index,1);
			SET_BIT(ptiming->upordown_bits,index,upordown);
			SET_BIT(ptiming->holding_bits, index,holding);
			SET_BIT(ptiming->event_bits,   index,0);
			//if(THIS_INFO)putrsUART((ROM char*)"timing1s start.");
		}
	} else {
		handle_plc_command_error();
	}
}

/**********************************************
 * �رն�ʱ������ȡ�������¼�
 */
static void timing_cell_stop(unsigned int index)
{
	if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
	    TIM100MS_ARRAYS_T * ptiming = &tim100ms_arrys;
		index -= TIMING100MS_EVENT_BASE;
		if(BIT_IS_SET(ptiming->enable_bits,index)) {
		    SET_BIT(ptiming->enable_bits,  index,0);
		}
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE+TIMING1S_EVENT_COUNT)) {
	    TIM1S_ARRAYS_T * ptiming = &tim1s_arrys;
		index -= TIMING1S_EVENT_BASE;
		if(BIT_IS_SET(ptiming->enable_bits,index)) {
		    //if(THIS_INFO)putrsUART((ROM char*)"timing1ms stop.");
		    SET_BIT(ptiming->enable_bits,  index,0);
		}
	} else {
		//if(THIS_ERROR)putrsUART((ROM char*)"timing stop error");
	    handle_plc_command_error();
	}
}

/**********************************************
 * ��������˿ڵ�����ֵ
 */
void handle_plc_ld(void)
{
	bit_acc = get_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]));
	if(PLC_CODE == PLC_LDI) {
		bit_acc = !bit_acc;
	}
	plc_command_index += 3;
}
/**********************************************
 * ��λ����Ľ�����������˿���
 */
void handle_plc_out(void)
{
	set_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]),bit_acc);
	plc_command_index += 3;
}


/**********************************************
 * ����������
 */
void handle_plc_and_ani(void)
{
	unsigned char bittmp = get_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]));
	if(PLC_CODE == PLC_AND) {
	    bit_acc = bit_acc && bittmp;
	} else {
		bit_acc = bit_acc && (!bittmp);
	}
	plc_command_index += 3;
}

/**********************************************
 * ���������
 */
void handle_plc_or_ori(void)
{
	unsigned char  bittmp = get_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]));
	if(PLC_CODE == PLC_OR) {
	    bit_acc = bit_acc || bittmp;
	} else if(PLC_CODE == PLC_ORI) {
		bit_acc = bit_acc || (!bittmp);
	}
	plc_command_index += 3;
}

/**********************************************
 * ��������������ػ��½���
 */
void handle_plc_ldp_ldf(void)
{
	unsigned char  reg;
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg  = get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_LDP) {
		if(reg == 0x02) { //ֻ�������أ�����������Ǽٵ�
			bit_acc = 1;
		} else {
		    bit_acc = 0;
		}
	} else if(PLC_CODE == PLC_LDF) {
		if(reg == 0x01) {//ֻ���½��أ�����������Ǽٵ�
		    bit_acc = 1;
		} else {
		    bit_acc = 0;
		}
	}
	plc_command_index += 3;
}


/**********************************************
 * �������ػ��½���
 */
void handle_plc_andp_andf(void)
{
    unsigned char  reg;
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg =  get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_ANDP) {
		if(reg == 0x02) {
			bit_acc = bit_acc && 1;
		} else {
			bit_acc = 0;
		}
	} else if(PLC_CODE == PLC_ANDF) {
		if(reg == 0x01) {
		    bit_acc = bit_acc && 1;
		} else {
			bit_acc = 0;
		}
	}
	plc_command_index += 3;
}
/**********************************************
 * �������ػ��½���
 */
void handle_plc_orp_orf(void)
{
	unsigned char  reg;
	unsigned int   bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg =  get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_ORP) {
		if(reg == 0x02) {
			bit_acc = bit_acc || 1;
		} else {
			bit_acc = bit_acc || 0;
		}
	} else if(PLC_CODE == PLC_ORF) {
		if(reg == 0x01) {
		    bit_acc = bit_acc || 1;
		} else {
			bit_acc = bit_acc || 0;
		}
	}
	plc_command_index += 3;
}




/**********************************************
 * ѹջ����ջ����ջ
 */
void handle_plc_mps_mrd_mpp(void)
{
	unsigned char  B,b;
	if(PLC_CODE == PLC_MPS) {
		if(bit_stack_sp >= BIT_STACK_LEVEL) {
		    if(THIS_ERROR)printf("��ջ���\r\n");
		    handle_plc_command_error();
			return ;
		}
	    B = bit_stack_sp / 8;
	    b = bit_stack_sp % 8;
		if(bit_acc) {
			bit_stack[B] |=  code_msk[b];
		} else {
			bit_stack[B] &= ~code_msk[b];
		}
		bit_stack_sp++;
	} else if(PLC_CODE == PLC_MRD) {
	    unsigned char last_sp = bit_stack_sp - 1;
	    B = last_sp / 8;
	    b = last_sp % 8;
		bit_acc = (bit_stack[B] & code_msk[b])?1:0;
	} else if(PLC_CODE == PLC_MPP) {
	    bit_stack_sp--;
	    B = bit_stack_sp / 8;
	    b = bit_stack_sp % 8;
		bit_acc = (bit_stack[B] & code_msk[b])?1:0;
	}
	plc_command_index += 1;
}
/**********************************************
 * �������ָ��
 */
void handle_plc_set_rst(void)
{
	unsigned int bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	if(PLC_CODE == PLC_SET) {
	    if(bit_acc) {
			set_bitval(bit_index,1);
	    }
	} else if(PLC_CODE == PLC_RST) {
	    if(bit_acc) {
			set_bitval(bit_index,0);
	    }
	}
	plc_command_index += 3;
}
/**********************************************
 * ������ȥ����û���룬���ı�
 */
void handle_plc_seti(void)
{
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	if(bit_acc) {
        set_bitval(bit_index,!get_bitval(bit_index));
	}
	plc_command_index += 3;
}
/**********************************************
 * ȡ�����
 */
void handle_plc_inv(void)
{
	bit_acc = !bit_acc;
	plc_command_index += 1;
}

/**********************************************
 * �������ʱ��
 * ���ݱ�ţ����������100ms��ʱ����Ҳ���������1s��ʱ��
 */
void handle_plc_out_t(void)
{
	unsigned int  kval;
	unsigned int  time_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	if(bit_acc) {
	    kval = HSB_BYTES_TO_WORD(&plc_command_array[3]);
	    timing_cell_start(time_index,kval,1,0);
	} else {
		timing_cell_stop(time_index);
	}
	plc_command_index += 5;
}
/**********************************************
 * �����������
 */
void handle_plc_out_c(void)
{
	unsigned int  kval = HSB_BYTES_TO_WORD(&plc_command_array[3]);
	unsigned int  index = HSB_BYTES_TO_WORD(&plc_command_array[1]);

	//�ж������Ƿ���Ч
    if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
	} else {
	    if(THIS_ERROR)printf("�������������ֵ�д���!\r\n");
		handle_plc_command_error();
	    return ;
	}
	//�������ڲ�ά����һ�εĴ�����ƽ
	//������������ʱ�򣬰���εĴ�����ƽ������������
	if(bit_acc) {
	    if(index < GET_ARRRYS_NUM(counter_arrys.counter)) {
	        if(!BIT_IS_SET(counter_arrys.last_trig_bits,index)) {
		        //��һ���ǵ͵�ƽ������Ǹߵ�ƽ�����Դ�������
			    if(counter_arrys.counter[index] < kval) {
			        if(++counter_arrys.counter[index] == kval) {
				        SET_BIT(counter_arrys.event_bits,index,1);
				    }
			    }
		    }
		}
	}
	//�ѵ�ƽ��¼��������ȥ
	if(index < GET_ARRRYS_NUM(counter_arrys.counter)) {
	    SET_BIT(counter_arrys.last_trig_bits,index,bit_acc);
	}
	plc_command_index += 5;
}

/**********************************************
 * ���临λָ������Y0 - Y7 ��λ
 */
void handle_plc_zrst(void)
{
	typedef struct _zrst_command
	{
		unsigned char cmd;
		unsigned char start_hi;
		unsigned char start_lo;
		unsigned char end_hi;
		unsigned char end_lo;
	} zrst_command;
	zrst_command * plc = (zrst_command *)plc_command_array;
	unsigned int start = HSB_BYTES_TO_WORD(&plc->start_hi);
	unsigned int end = HSB_BYTES_TO_WORD(&plc->end_hi);
	unsigned int i;
	for(i=start;i<end;i++) {
		set_bitval(i,0);
	}
	plc_command_index += sizeof(zrst_command);
}
/**********************************************
 * �Ƚ�ָ��
 * BCMP   K   B   M0
 * ��ָ��Ϊ�ֽڱȽ�ָ����ȽϵĽ��<,=,>���ֽ���ֱ��֪��M0��M1��M2��
 */
void handle_plc_bcmp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char kval;
		unsigned char reg_hi;
		unsigned char reg_lo;
		unsigned char out_hi;
		unsigned char out_lo;
	} plc_command;
	plc_command * plc = (plc_command *)plc_command_array;
	unsigned char reg = get_byte_val(HSB_BYTES_TO_WORD(&plc->reg_hi));
	unsigned int  out = HSB_BYTES_TO_WORD(&plc->out_hi);
	if(plc->kval < reg) {
		set_bitval(out,1);
		set_bitval(out+1,0);
		set_bitval(out+2,0);
	} else if(plc->kval == reg) {
		set_bitval(out,0);
		set_bitval(out+1,1);
		set_bitval(out+2,0);
	} else if(plc->kval > reg) {
		set_bitval(out,0);
		set_bitval(out+1,0);
		set_bitval(out+2,1);
	}
	plc_command_index += sizeof(plc_command);
}
/**********************************************
 * �ֽ�����Ƚ�ָ��
 * BZCP   K1  K2  B  M0
 * ��ָ��Ϊ�ֽڱȽ�ָ����ȽϵĽ��<,�м�,>���ֽ���ֱ��֪��M0��M1��M2��
 */
void handle_plc_bzcp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char klow;
		unsigned char khig;
		unsigned char reg_hi;
		unsigned char reg_lo;
		unsigned char out_hi;
		unsigned char out_lo;
	} plc_command;
	plc_command * plc = (plc_command *)plc_command_array;
	unsigned char reg = get_byte_val(HSB_BYTES_TO_WORD(&plc->reg_hi));
	unsigned int  out = HSB_BYTES_TO_WORD(&plc->out_hi);
	if(reg < plc->klow) {
		set_bitval(out,1);
		set_bitval(out+1,0);
		set_bitval(out+2,0);
	} else if(reg >= plc->klow && reg <= plc->khig) {
		set_bitval(out,0);
		set_bitval(out+1,1);
		set_bitval(out+2,0);
	} else if(reg > plc->khig) {
		set_bitval(out,0);
		set_bitval(out+1,0);
		set_bitval(out+2,1);
	}
	plc_command_index += sizeof(plc_command);
}





/**********************************************
 * ͨ��λָ�������
 * ����ҵ����յ����ָ���������Ȼ�󷵻�һ������
 * ���ָ�����һ���������ͼ�����򽫵ȴ���λ����λдָ��
 * ���ҳ���Ҳ����ѯ��ָ����Զ������ID���Ǵ�ID��������
 * ��ID����������ͨ�������������㲥��������ָ��IP������
 */
//�����ָ��


void handle_plc_net_rb(void)
{
  //����ͨ��ָ��
  typedef struct _NetRdOptT
  {
    unsigned char op;
    unsigned char net_index;  //����ָ����������Ϊ������Ҫ�����ѯ������һ���ͣ�����������ڴ����
    unsigned char remote_device_addr;
    unsigned char remote_start_addr_hi;  //Զ�����ݵ���ʼ��ַ
    unsigned char remote_start_addr_lo;  //Զ�����ݵ���ʼ��ַ
    unsigned char local_start_addr_hi;  //Զ�����ݵ���ʼ��ַ
    unsigned char local_start_addr_lo;  //Զ�����ݵ���ʼ��ַ
    unsigned char data_number; //ͨ�����ݵĸ���
    //�������
    unsigned char enable_addr_hi;
    unsigned char enable_addr_lo;
    //����һ��ͨ��
    unsigned char request_addr_hi;
    unsigned char request_addr_lo;
    //ͨ�Ž����б��
    unsigned char txing_hi;
    unsigned char txing_lo;
    //��ɵ�ַ
    unsigned char done_addr_hi;
    unsigned char done_addr_lo;
    //��ʱ��ʱ������
    unsigned char timeout_addr_hi;
    unsigned char timeout_addr_lo;
    //��ʱ��ʱ,S
    unsigned char timeout_val;
  } NetRdOptT;
  //
#if 0
  NetRdOptT * p = (NetRdOptT *)plc_command_array;
  DATA_RX_PACKET_T * prx;
  //�ֵ����ͨ��ָ��ִ��ʱ����
  if(net_global_send_index == p->net_index) {  //�õ����Ƶ�
	  rx_look_up_packet(); //�õ����Ƶ��ˣ����뿴һ����յ����ݣ�Ҫ��ҪҪ˵һ����
      //�ǵģ����Է���
	  if(get_bitval(HSB_BYTES_TO_WORD(&p->enable_addr_hi))) { //���ͨ�ŵ�Ԫ��ʹ�ܵ�
        if(get_bitval(HSB_BYTES_TO_WORD(&p->txing_hi))) {
            //���ڷ����У��ж��Ƿ�ʱ
            if(get_bitval(HSB_BYTES_TO_WORD(&p->timeout_addr_hi))) {
                //��ʱ�ˣ���ô������һ�η��ͳ���
                set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),0);
                goto try_again;
            } else {
                //û�г�ʱ����ô�ȴ�Ӧ�������Ƿ���
                unsigned int i;
                for(i=0;i<RX_PACKS_MAX_NUM;i++) {
                    prx = &rx_ctl.rx_packs[i];
                    if(prx->finished) {
                        //MODBUSλָ��룬���ݷ���������ָ����λ��
                        unsigned int localbits = HSB_BYTES_TO_WORD(&p->local_start_addr_hi);
                        if(THIS_ERROR)printf("rb get one rx packet.");
						if(1) { ///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!modbus_prase_read_multi_coils_ack(p->remote_device_addr,prx->buffer,prx->index,localbits,p->data_number)) {
                             //Ӧ������OK��������ɴ˴��������󣬵ȴ���һѭ��������
                            if(THIS_ERROR)printf("rb rx ack data is ok.");
                            set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),0);
                            set_bitval(HSB_BYTES_TO_WORD(&p->timeout_addr_hi),0);
                            set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),1);
                            break; 
		    		    }
                    }
    			}
                if(prx == NULL) {
                    //û���յ�Ӧ��Ү���������ٵ�һ�Ȱɡ�����
                    //if(THIS_ERROR)printf("rb timeout,resend packet.");
    			}
	    	}
		} else {
            //��δ����
try_again:
    	 	if(get_bitval(HSB_BYTES_TO_WORD(&p->request_addr_hi))) {
                if(THIS_ERROR)printf("rb read coils send request.");
		    	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1modbus_read_multi_coils_request(HSB_BYTES_TO_WORD(&p->local_start_addr_hi),p->data_number,p->remote_device_addr);
                //Ȼ��������ʱ��
                timing_cell_stop(HSB_BYTES_TO_WORD(&p->timeout_addr_hi));
                timing_cell_start(HSB_BYTES_TO_WORD(&p->timeout_addr_hi),p->timeout_val,1,0);
                //���������
                set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),1);
	    	} else {
			    //�������ͣ���û��Ҫ������
			}
		}
	  } else {
	    //����ʹ�ܱ��رյ�
        //ֹͣ��ʱ��
        timing_cell_stop(HSB_BYTES_TO_WORD(&p->timeout_addr_hi));
        set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),0);
        set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),0);
	  }
  } else {
	  //û���õ����ƣ��Ǿ͵���һ�ΰ�
  }
#endif
  plc_command_index += sizeof(NetRdOptT);
}


void handle_plc_net_wb(void)
{
}


void PlcProcess(void)
{
	//���봦��,��ȡIO�ڵ�����
	io_in_get_bits(0,inputs_new,IO_INPUT_COUNT);
	//����ͨ�ų���
	//��ʼ��ͨ������
    net_communication_count = 0; ////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!plc_test_buffer[0]; //��������������������5������
	plc_command_index = 0;
	plc_cpu_stop = 0;
 next_plc_command:
	read_next_plc_code();
	//�߼�����,����һ�Σ�����һ���û��ĳ���
	switch(PLC_CODE)
	{
	case PLC_END: //ָ���������������
		plc_command_index = 0;
		goto plc_command_finished;
	case PLC_LD: 
	case PLC_LDI:
		handle_plc_ld();
		break;
    case PLC_LDKH:
        bit_acc = 1;
        plc_command_index++;
        break;
    case PLC_LDKL:
        bit_acc = 0;
        plc_command_index++;
        break;
    case PLC_SEI:
        handle_plc_seti();
        break;
	case PLC_OUT:
		handle_plc_out();
		break;
	case PLC_AND:
	case PLC_ANI:
		handle_plc_and_ani();
		break;
	case PLC_OR:
	case PLC_ORI:
		handle_plc_or_ori();
		break;
	case PLC_LDP:
	case PLC_LDF:
		handle_plc_ldp_ldf();
		break;
	case PLC_ANDP:
	case PLC_ANDF:
		handle_plc_andp_andf();
		break;
	case PLC_ORP:
	case PLC_ORF:
		handle_plc_orp_orf();
		break;
	case PLC_MPS:
	case PLC_MRD:
	case PLC_MPP:
		handle_plc_mps_mrd_mpp();
		break;
	case PLC_SET:
	case PLC_RST:
		handle_plc_set_rst();
		break;
	case PLC_INV:
		handle_plc_inv();
		break;
	case PLC_OUTT:
		handle_plc_out_t();
		break;
	case PLC_OUTC:
		handle_plc_out_c();
		break;
	case PLC_ZRST:
		handle_plc_zrst();
		break;
    case PLC_NETRB:
        handle_plc_net_rb(); 
        break;
	case PLC_BCMP:
		handle_plc_bcmp();
		break;
	case PLC_BZCP:
		handle_plc_bzcp();
		break;
    case PLC_NETWB:
    case PLC_NETRW:
    case PLC_NETWW:
	default:
	    handle_plc_command_error();
		goto plc_command_finished;
	case PLC_NONE: //��ָ�ֱ������
        plc_command_index++;
		break;
	}
	if(plc_cpu_stop) {
		goto plc_command_finished;
	}
	goto next_plc_command;
 plc_command_finished:
	//�����������������������̵�����
	io_out_set_bits(0,output_new,IO_OUTPUT_COUNT);
	memcpy(output_last,output_new,sizeof(output_new));
	//��������
	memcpy(inputs_last,inputs_new,sizeof(inputs_new));
	//�����̵���
	memcpy(auxi_relays_last,auxi_relays,sizeof(auxi_relays));
	//
#if 0
	{//���̵ּ������ڴ����
		unsigned int i;
		unsigned char reg;
		for(i=0;i<AUXI_HOLDRELAY_COUNT/8;i++) { //RTC�ڴ��ֽ�����һ��
			RtcRamRead(i,&reg,1);
			RtcRamWrite(i+AUXI_HOLDRELAY_COUNT/8,&reg,1); //��������벿��
		}
	}
#endif
	//ϵͳʱ�䴦���������õ���ʱ���жϴ���
	memcpy(tim100ms_arrys.event_bits_last,tim100ms_arrys.event_bits,sizeof(tim100ms_arrys.event_bits));
	//
	memcpy(tim1s_arrys.event_bits_last,tim1s_arrys.event_bits,sizeof(tim1s_arrys.event_bits));
	//
	memcpy(counter_arrys.event_bits_last,counter_arrys.event_bits,sizeof(counter_arrys.event_bits));
	//��ʱ������
	timing_cell_prcess();
	//����������
    //�ѽ��յ������õ�����������
    //rx_free_useless_packet(net_communication_count); !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if(++net_global_send_index >= net_communication_count) {
        //���������������һ�飬ÿ���˶��л�����һ��ͨ�Ŷ���
        net_global_send_index = 0;
    }
}