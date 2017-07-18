/*************************************************
  File name:  CommUtils.h
  Author:    李宁
  Date:     2016-07-28
  Description:  数据文件定义
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


/* 4字节的IP地址 */
typedef struct ip_address{
    u_char byte1;
    u_char byte2;
    u_char byte3;
    u_char byte4;
}ip_address;

/* IPv4 首部 */
typedef struct ip_header{
    u_char  ver_ihl;        // 版本 (4 bits) + 首部长度 (4 bits)
    u_char  tos;            // 服务类型(Type of service) 
    u_short tlen;           // 总长(Total length) 
    u_short identification; // 标识(Identification)
    u_short flags_fo;       // 标志位(Flags) (3 bits) + 段偏移量(Fragment offset) (13 bits)
    u_char  ttl;            // 存活时间(Time to live)
    u_char  proto;          // 协议(Protocol)
    u_short crc;            // 首部校验和(Header checksum)
    ip_address  saddr;      // 源地址(Source address)
    ip_address  daddr;      // 目的地址(Destination address)
    u_int   op_pad;         // 选项与填充(Option + Padding)
}ip_header;

/* UDP 首部*/
typedef struct udp_header{
    u_short sport;          // 源端口(Source port)
    u_short dport;          // 目的端口(Destination port)
    u_short len;            // UDP数据包长度(Datagram length)
    u_short crc;            // 校验和(Checksum)
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
//返回结果参数
typedef struct InforParam
{
	string strMac;           //mac地址
	string strIp;            //ip地址
	string strCode;          //唯一码
	UINT   nSock;            //socket 值
	int    nPackets;         //包的序号
	int	   nDevId;			// 设备ID
}InforParam_t;



}