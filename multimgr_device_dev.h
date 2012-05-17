#ifndef __MULTIMGR_DEVICE_DEV_H__
#define __MULTIMGR_DEVICE_DEV_H__

#define CMD_GET_DEVICE_INFO     0
#define CMD_SET_DEVICE_INFO     1
#define CMD_MODBUSPACK_SEND     2
#define CMD_RESET_DEVICE        3



typedef struct __device_info_st
{
	unsigned char command;
	unsigned char command_len;
	char          to_host;
	char          host_name[64];
	unsigned char mac[6];
	char          group_name1[32];
	char          group_name2[32];
	char          change_password;
    char          password[20];   //ͨ���õĵ�ַ
	char          cncryption_mode;
	unsigned char device_time[6];
	unsigned char work_port[2];   //���ص�UDPͨ�Ŷ˿ں�
	char          remote_host_addr[64]; //Զ��������IP��ַ
	unsigned char remote_host_port[2]; //Զ������UDP�˿ں�
	//�����ͺ�
	unsigned char device_model;
	//ѡ��Ƿ�ر���������
	unsigned char system_fun_option;
	//��������
	unsigned char broadcast_time;  //�㲥ʱ�䣬��λs,0���㲥,1���Ϲ㲥
	//������Ϣ
	unsigned char local_ip[4];
	unsigned char net_mask[4];
	unsigned char gateway[4];
	unsigned char dns[4];
	//�����ϵ����ݽ���CRCУ��
	unsigned char crc[2];
} device_info_st;


typedef struct __modbus_command_st
{
	unsigned char command;
    unsigned char crc[2]; //���������ݽ���CRCУ��
    unsigned char command_len;
} modbus_command_st;

#define  GET_MODBUS_COMMAND_DATA(ptr)   ((void *)(((unsigned char *)(ptr))+sizeof(modbus_command_st)))


typedef struct __reset_device_st
{
	unsigned char command;
	unsigned char command_len;
	unsigned char pad[64];
	unsigned char crc[2];
} reset_device_st;


#endif

