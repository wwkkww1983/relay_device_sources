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


//100ms计时器的控制数据结构
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



//1s计时器的控制数据结构
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

//计数器的控制数据结构
typedef struct _COUNTER_ARRAYS_T
{
    WORD  counter[COUNTER_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];		
	BYTE  event_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(COUNTER_EVENT_COUNT)];
	BYTE  last_trig_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];
} COUNTER_ARRAYS_T;

COUNTER_ARRAYS_T counter_arrys;

//输入口
unsigned char inputs_new[BITS_TO_BS(IO_INPUT_COUNT)];
unsigned char inputs_last[BITS_TO_BS(IO_INPUT_COUNT)];
//输出继电器
unsigned char output_last[BITS_TO_BS(IO_OUTPUT_COUNT)];
unsigned char output_new[BITS_TO_BS(IO_OUTPUT_COUNT)];
//辅助继电器
unsigned char auxi_relays[BITS_TO_BS(AUXI_RELAY_COUNT)];
unsigned char auxi_relays_last[BITS_TO_BS(AUXI_RELAY_COUNT)];
//字节变量，通用变量
unsigned char general_reg[REG_COUNT];   //普通变量
//RTC实时时间影像寄存器，没秒更新一次自动变化
unsigned char rtc_reg[REG_RTC_COUNT];
//温度寄存器，1秒更新一次
unsigned char temp_reg[REG_TEMP_COUNT];
//定时器定义，自动对内部的时钟脉冲进行计数
volatile unsigned int  time100ms_come_flag;
volatile unsigned int  time1s_come_flag;
//运算器的寄存器
#define  BIT_STACK_LEVEL     32
unsigned char bit_acc;
unsigned char bit_stack[BITS_TO_BS(BIT_STACK_LEVEL)];   //比特堆栈，PLC的位运算结果压栈在这里，总共有32层栈
unsigned char bit_stack_sp;   //比特堆栈的指针

//指令编码
unsigned int  plc_command_index;     //当前指令索引，
unsigned char plc_command_array[32]; //当前指令字节编码
#define       PLC_CODE     (plc_command_array[0])

//处理器状态
unsigned char plc_cpu_stop;
//通信元件个数
unsigned int  net_communication_count = 0;
unsigned int  net_global_send_index = 0; //发送令牌，指示下一个个发送的令牌

/*************************************************
 * 以下是私有的实现
 */
//内部用系统计数器
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
	//读时间进来
	unsigned int TP_temp = ReadTemperatureXX_XC();
	DS1302_VAL tval;
	sys_lock();
	temp_reg[1] = TP_temp / 100;  //度
	temp_reg[0] = TP_temp % 100;  //小数度
	sys_unlock();
	ReadRTC(&tval);
	BCD2Hex(&tval,sizeof(tval));
	//以下按照从小到大排序，分别为：秒，分，时，日，月，年，最后是星期
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
 * 系统初始化
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
    指令                      地址         数据
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
 *  获取下一条指令的指令码
 *  也许是从EEPROM中读取的程序脚本
 *  这里一次性读取下一个指令，长度为最长指令长度
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
		//不允许读别的内存
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
	//提示第几条指令出错
	//然后复位，或停止运行
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
		index += AUXI_HOLDRELAY_COUNT / 8; //后面一部分是上一次的
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
		//输入值不能修改
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
	    //计数器的值不可以置位,只可以复位
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
		//普通字节变量
		reg = general_reg[index-REG_BASE];
	} else if(index >=REG_RTC_BASE && index <(REG_RTC_BASE+REG_RTC_COUNT)) {
		//读取RTC时间,地址分别是年月日，时分秒，星期
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
		//普通字节变量
		general_reg[index-REG_BASE] = val;
	} else if(index >=REG_RTC_BASE && index <(REG_RTC_BASE+REG_RTC_COUNT)) {
		//读取RTC时间,地址分别是年月日，时分秒，星期
		//rtc_reg[index-REG_RTC_BASE] = val;暂时不能修改时间
	} else if(index >=REG_TEMP_BASE && index <(REG_TEMP_BASE+REG_TEMP_COUNT)) {
	}
}

/**********************************************
 * 根据条件对计时器进行增加
 * 如果时间到了，则触发事件
 * 如果时间尚未到，则继续计时
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
		    if(BIT_IS_SET(ptiming->enable_bits,i)) { //如果允许计时
			    if(counter > 0 && !BIT_IS_SET(ptiming->event_bits,i)) {  //如果时间事件未发生
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
				    //保持定时器
				} else {
				    //不保持
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
		    if(BIT_IS_SET(ptiming->enable_bits,i)) { //如果允许计时
			    if(counter > 0 && !BIT_IS_SET(ptiming->event_bits,i)) {  //如果时间事件未发生
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
				    //保持定时器
				} else {
				    //不保持
					ptiming->counter[i] = 0;
					SET_BIT(ptiming->event_bits,i,0);
					//if(THIS_INFO)putrsUART((ROM char*)"timing1s end.");
				}
			}
	    }
	}
}

/**********************************************
 * 打开定时器，并设定触发时间的最大值
 * 如果已经开始计时，则继续计时，如果没有开始，则开始
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
 * 关闭定时器，并取消触发事件
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
 * 加载输入端口的输入值
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
 * 把位运算的结果输出到输出端口中
 */
void handle_plc_out(void)
{
	set_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]),bit_acc);
	plc_command_index += 3;
}


/**********************************************
 * 与或与非运算
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
 * 或或或非运算
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
 * 加载输入端上升沿或下降沿
 */
void handle_plc_ldp_ldf(void)
{
	unsigned char  reg;
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg  = get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_LDP) {
		if(reg == 0x02) { //只管上升沿，其他情况都是假的
			bit_acc = 1;
		} else {
		    bit_acc = 0;
		}
	} else if(PLC_CODE == PLC_LDF) {
		if(reg == 0x01) {//只管下降沿，其他情况都是假的
		    bit_acc = 1;
		} else {
		    bit_acc = 0;
		}
	}
	plc_command_index += 3;
}


/**********************************************
 * 与上升沿或下降沿
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
 * 或上升沿或下降沿
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
 * 压栈、出栈、读栈
 */
void handle_plc_mps_mrd_mpp(void)
{
	unsigned char  B,b;
	if(PLC_CODE == PLC_MPS) {
		if(bit_stack_sp >= BIT_STACK_LEVEL) {
		    if(THIS_ERROR)printf("堆栈溢出\r\n");
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
 * 锁或解锁指令
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
 * 有输入去翻，没输入，不改变
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
 * 取反结果
 */
void handle_plc_inv(void)
{
	bit_acc = !bit_acc;
	plc_command_index += 1;
}

/**********************************************
 * 输出到定时器
 * 根据编号，可能输出到100ms定时器，也可能输出到1s定时器
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
 * 输出到计数器
 */
void handle_plc_out_c(void)
{
	unsigned int  kval = HSB_BYTES_TO_WORD(&plc_command_array[3]);
	unsigned int  index = HSB_BYTES_TO_WORD(&plc_command_array[1]);

	//判断索引是否有效
    if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
	} else {
	    if(THIS_ERROR)printf("输出计数器索引值有错误!\r\n");
		handle_plc_command_error();
	    return ;
	}
	//计数器内部维持上一次的触发电平
	//触发计数器的时候，把这次的触发电平触发进计数器
	if(bit_acc) {
	    if(index < GET_ARRRYS_NUM(counter_arrys.counter)) {
	        if(!BIT_IS_SET(counter_arrys.last_trig_bits,index)) {
		        //上一次是低电平，这次是高电平，可以触发计数
			    if(counter_arrys.counter[index] < kval) {
			        if(++counter_arrys.counter[index] == kval) {
				        SET_BIT(counter_arrys.event_bits,index,1);
				    }
			    }
		    }
		}
	}
	//把电平记录到计数器去
	if(index < GET_ARRRYS_NUM(counter_arrys.counter)) {
	    SET_BIT(counter_arrys.last_trig_bits,index,bit_acc);
	}
	plc_command_index += 5;
}

/**********************************************
 * 区间复位指令，比如把Y0 - Y7 复位
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
 * 比较指令
 * BCMP   K   B   M0
 * 该指令为字节比较指令，将比较的结果<,=,>三种结果分别告知给M0，M1，M2。
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
 * 字节区间比较指令
 * BZCP   K1  K2  B  M0
 * 该指令为字节比较指令，将比较的结果<,中间,>三种结果分别告知给M0，M1，M2。
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
 * 通信位指令处理程序
 * 如果找到接收到这个指令，则处理它，然后返回一个数据
 * 这个指令可以一定允许，和激活，程序将等待上位机的位写指令
 * 并且程序也定期询问指定的远程主机ID，非此ID，不接受
 * 此ID主机可能是通用任意主机，广播主机，或指定IP的主机
 */
//网络读指令


void handle_plc_net_rb(void)
{
  //定义通信指令
  typedef struct _NetRdOptT
  {
    unsigned char op;
    unsigned char net_index;  //发送指令索引，因为发送需要间隔轮询，不能一起发送，这样会造成内存紧张
    unsigned char remote_device_addr;
    unsigned char remote_start_addr_hi;  //远端数据的起始地址
    unsigned char remote_start_addr_lo;  //远端数据的起始地址
    unsigned char local_start_addr_hi;  //远端数据的起始地址
    unsigned char local_start_addr_lo;  //远端数据的起始地址
    unsigned char data_number; //通信数据的个数
    //输入变量
    unsigned char enable_addr_hi;
    unsigned char enable_addr_lo;
    //激活一次通信
    unsigned char request_addr_hi;
    unsigned char request_addr_lo;
    //通信进行中标记
    unsigned char txing_hi;
    unsigned char txing_lo;
    //完成地址
    unsigned char done_addr_hi;
    unsigned char done_addr_lo;
    //超时定时器索引
    unsigned char timeout_addr_hi;
    unsigned char timeout_addr_lo;
    //定时超时,S
    unsigned char timeout_val;
  } NetRdOptT;
  //
#if 0
  NetRdOptT * p = (NetRdOptT *)plc_command_array;
  DATA_RX_PACKET_T * prx;
  //轮到这个通信指令执行时间了
  if(net_global_send_index == p->net_index) {  //拿到令牌的
	  rx_look_up_packet(); //拿到令牌的人，必须看一遍接收的数据，要不要要说一声。
      //是的，可以发送
	  if(get_bitval(HSB_BYTES_TO_WORD(&p->enable_addr_hi))) { //这个通信单元被使能的
        if(get_bitval(HSB_BYTES_TO_WORD(&p->txing_hi))) {
            //正在发送中，判断是否超时
            if(get_bitval(HSB_BYTES_TO_WORD(&p->timeout_addr_hi))) {
                //超时了，那么，重启一次发送程序
                set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),0);
                goto try_again;
            } else {
                //没有超时，那么等待应答数据是否到了
                unsigned int i;
                for(i=0;i<RX_PACKS_MAX_NUM;i++) {
                    prx = &rx_ctl.rx_packs[i];
                    if(prx->finished) {
                        //MODBUS位指令翻译，根据翻译结果设置指定的位置
                        unsigned int localbits = HSB_BYTES_TO_WORD(&p->local_start_addr_hi);
                        if(THIS_ERROR)printf("rb get one rx packet.");
						if(1) { ///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!modbus_prase_read_multi_coils_ack(p->remote_device_addr,prx->buffer,prx->index,localbits,p->data_number)) {
                             //应答数据OK，可以完成此次数据请求，等待下一循环的请求
                            if(THIS_ERROR)printf("rb rx ack data is ok.");
                            set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),0);
                            set_bitval(HSB_BYTES_TO_WORD(&p->timeout_addr_hi),0);
                            set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),1);
                            break; 
		    		    }
                    }
    			}
                if(prx == NULL) {
                    //没有收到应答耶，那我们再等一等吧。。。
                    //if(THIS_ERROR)printf("rb timeout,resend packet.");
    			}
	    	}
		} else {
            //尚未发送
try_again:
    	 	if(get_bitval(HSB_BYTES_TO_WORD(&p->request_addr_hi))) {
                if(THIS_ERROR)printf("rb read coils send request.");
		    	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1modbus_read_multi_coils_request(HSB_BYTES_TO_WORD(&p->local_start_addr_hi),p->data_number,p->remote_device_addr);
                //然后启动定时器
                timing_cell_stop(HSB_BYTES_TO_WORD(&p->timeout_addr_hi));
                timing_cell_start(HSB_BYTES_TO_WORD(&p->timeout_addr_hi),p->timeout_val,1,0);
                //置启动标记
                set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),1);
	    	} else {
			    //允许发送，还没需要发送呢
			}
		}
	  } else {
	    //发送使能被关闭的
        //停止定时器
        timing_cell_stop(HSB_BYTES_TO_WORD(&p->timeout_addr_hi));
        set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),0);
        set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),0);
	  }
  } else {
	  //没有拿到令牌，那就等下一次吧
  }
#endif
  plc_command_index += sizeof(NetRdOptT);
}


void handle_plc_net_wb(void)
{
}


void PlcProcess(void)
{
	//输入处理,读取IO口的输入
	io_in_get_bits(0,inputs_new,IO_INPUT_COUNT);
	//处理通信程序
	//初始化通信令牌
    net_communication_count = 0; ////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!plc_test_buffer[0]; //打个比喻，代码里面存在5处发送
	plc_command_index = 0;
	plc_cpu_stop = 0;
 next_plc_command:
	read_next_plc_code();
	//逻辑运算,调用一次，运行一次用户的程序
	switch(PLC_CODE)
	{
	case PLC_END: //指令结束，处理后事
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
	case PLC_NONE: //空指令，直接跳过
        plc_command_index++;
		break;
	}
	if(plc_cpu_stop) {
		goto plc_command_finished;
	}
	goto next_plc_command;
 plc_command_finished:
	//输出处理，把运算结果输出到继电器中
	io_out_set_bits(0,output_new,IO_OUTPUT_COUNT);
	memcpy(output_last,output_new,sizeof(output_new));
	//后续处理
	memcpy(inputs_last,inputs_new,sizeof(inputs_new));
	//辅助继电器
	memcpy(auxi_relays_last,auxi_relays,sizeof(auxi_relays));
	//
#if 0
	{//保持继电器的内存搬移
		unsigned int i;
		unsigned char reg;
		for(i=0;i<AUXI_HOLDRELAY_COUNT/8;i++) { //RTC内存字节数的一半
			RtcRamRead(i,&reg,1);
			RtcRamWrite(i+AUXI_HOLDRELAY_COUNT/8,&reg,1); //拷贝到后半部分
		}
	}
#endif
	//系统时间处理，可以拿到定时器中断处理
	memcpy(tim100ms_arrys.event_bits_last,tim100ms_arrys.event_bits,sizeof(tim100ms_arrys.event_bits));
	//
	memcpy(tim1s_arrys.event_bits_last,tim1s_arrys.event_bits,sizeof(tim1s_arrys.event_bits));
	//
	memcpy(counter_arrys.event_bits_last,counter_arrys.event_bits,sizeof(counter_arrys.event_bits));
	//定时器处理
	timing_cell_prcess();
	//计数器处理
    //把接收到的无用的数据清理掉
    //rx_free_useless_packet(net_communication_count); !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if(++net_global_send_index >= net_communication_count) {
        //令牌溢出，重新来一遍，每个人都有机会做一次通信动作
        net_global_send_index = 0;
    }
}
