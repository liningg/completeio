/*************************************************
  File name:  CCompleteIOWrapper.h
  Author:    李宁
  Date:     2016-07-28
  Description:  完成端口工具
*************************************************/
#pragma once
#include <string>
#include <WinSock2.h>
#include <vector>
#include <iostream>
#include <mswsock.h>
#include "../CommUtilsDef.h"
using std::string;
#define MAX_THREADCNT    50

#pragma comment(lib, "Ws2_32.lib")      // Socket编程需用的动态链接库
#pragma comment(lib, "Kernel32.lib")    // IOCP需要用到的动态链接库

namespace Communication
{


#define SEND 0
#define RECV 1
#define ACCEPT 2
typedef void ( * _stdcall CallCompleteIO)(char * chRecvBuff,int nLen,InforParam_t client);
typedef void ( * CallAddClient)(InforParam_t client);
typedef void ( * CallDelClient)(InforParam_t client);


/**
* 结构体名称：PER_IO_DATA
* 结构体功能：重叠I/O需要用到的结构体，临时记录IO数据
**/
const int DataBuffSize = 8 * 1024;
typedef struct
{
	OVERLAPPED overlapped;
	WSABUF databuff;
	char buffer[DataBuffSize];
	int BufferLen;
	int operationType;
	SOCKET client;
    string strIp;
}PER_IO_OPERATEION_DATA, *LPPER_IO_OPERATION_DATA, *LPPER_IO_DATA, PER_IO_DATA;

/**
* 结构体名称：PER_HANDLE_DATA
* 结构体存储：记录单个套接字的数据，包括了套接字的变量及套接字的对应的客户端的地址。
* 结构体作用：当服务器连接上客户端时，信息存储到该结构体中，知道客户端的地址以便于回访。
**/
typedef struct
{
	SOCKET socket;
	SOCKADDR_STORAGE ClientAddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;


class CCompleteIOWrapper
{
public:
	CCompleteIOWrapper(void);
	~CCompleteIOWrapper(void);

public:
    /*
	* 初始化
    */
	bool initComplete(string strIp,int nPort,int nThreadCnts = -1);
	/*
	* 开始完成端口服务 
	*/
	bool startComplete();
	/*
	* 暂停完成端口服务
	*/
	bool suspendComplete();
	/*
	* 重启完成端口服务
	*/
    
	bool resumeComplete();
    /*
	*停止完成端口服务
	*/
	bool stopComplete();
	/*
	* 获取服务器的运行状态
	*/
	bool isRunning();
	/*
	*销毁服务
    */
	void destroyComplete();
    /*
    *   设置回调函数
    */
    void setCallCompleteIO(CallCompleteIO fn);
    void setCallAddClient(CallAddClient fn);
    void setCallDelClient(CallDelClient fn);
    /*
    *   发送数据
    */
    int sendData(SOCKET s,char * sendData,int nSendLen);

protected:
	/*
	*	投递accept事件
	*/
	bool addAcceptEvent();
    void addSockLink(PER_IO_DATA* pHandle);
    void delSockLink(PER_IO_DATA* pHandle);
    PER_IO_DATA * getSockLink(string strIp);
 
	/*
	* 处理服务线程
    */
	friend UINT WINAPI ServerWorkThread(LPVOID pvParam);

private:                                                                                                                                                
	string							m_strIp;
	int								m_nPort;
	bool							m_bRuning;
	int								m_nThreadCnts;
	HANDLE							m_hThread[MAX_THREADCNT];
	SOCKET							m_srvSocket;
	HANDLE							m_hMutex;
	std::vector < PER_IO_DATA*>     m_vecClientGroup;        // 记录客户端的向量组
	HANDLE							m_hCompletionPort;
    int                             m_nAcceptCnts;
    CallCompleteIO                  m_CallCompleteIO;
    CallAddClient                   m_CallAddClient;
    CallDelClient                   m_CallDelClient;
    CRITICAL_SECTION                m_cs;
    HANDLE                          m_hSendThread;
    int                             m_nCreateTheadCnts;
    
};

}