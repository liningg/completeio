/*************************************************
  File name:  CommUtils.h
  Author:    ����
  Date:     2016-07-28
  Description:  �����ļ�����
*************************************************/
#pragma  once
#include <string>
#include <vector>

using std::vector;
using std::string;
#pragma comment(lib,"WS2_32.lib")
#pragma comment(lib,"wpcap.lib")
#pragma comment(lib,"Packet.lib")
#include <pcap.h>
#include <remote-ext.h>
#include <stdio.h>
namespace Communication
{


/* 4�ֽڵ�IP��ַ */
typedef struct ip_address{
    u_char byte1;
    u_char byte2;
    u_char byte3;
    u_char byte4;
}ip_address;

/* IPv4 �ײ� */
typedef struct ip_header{
    u_char  ver_ihl;        // �汾 (4 bits) + �ײ����� (4 bits)
    u_char  tos;            // ��������(Type of service) 
    u_short tlen;           // �ܳ�(Total length) 
    u_short identification; // ��ʶ(Identification)
    u_short flags_fo;       // ��־λ(Flags) (3 bits) + ��ƫ����(Fragment offset) (13 bits)
    u_char  ttl;            // ���ʱ��(Time to live)
    u_char  proto;          // Э��(Protocol)
    u_short crc;            // �ײ�У���(Header checksum)
    ip_address  saddr;      // Դ��ַ(Source address)
    ip_address  daddr;      // Ŀ�ĵ�ַ(Destination address)
    u_int   op_pad;         // ѡ�������(Option + Padding)
}ip_header;

/* UDP �ײ�*/
typedef struct udp_header{
    u_short sport;          // Դ�˿�(Source port)
    u_short dport;          // Ŀ�Ķ˿�(Destination port)
    u_short len;            // UDP���ݰ�����(Datagram length)
    u_short crc;            // У���(Checksum)
}udp_header;

typedef struct header_tcp
{
    u_short src_port;
    u_short dst_port;
    u_int seq;
    u_int ack_seq;
    u_short doff:4,hlen:4,fin:1,syn:1,rst:1,psh:1,ack:1,urg:1,ece:1,cwr:1;
    u_short window;
    u_short check;
    u_short urg_ptr;
}tcp_header;
//���ؽ������
typedef struct InforParam
{
	string strMac;           //mac��ַ
	string strIp;            //ip��ַ
	string strCode;          //Ψһ��
	UINT   nSock;            //socket ֵ
	int    nPackets;         //�������
	int	   nDevId;			// �豸ID
}InforParam_t;



}