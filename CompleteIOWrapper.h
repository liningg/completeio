/*************************************************
  File name:  CCompleteIOWrapper.h
  Author:    ����
  Date:     2016-07-28
  Description:  ��ɶ˿ڹ���
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

#pragma comment(lib, "Ws2_32.lib")      // Socket������õĶ�̬���ӿ�
#pragma comment(lib, "Kernel32.lib")    // IOCP��Ҫ�õ��Ķ�̬���ӿ�

namespace Communication
{


#define SEND 0
#define RECV 1
#define ACCEPT 2
typedef void ( * _stdcall CallCompleteIO)(char * chRecvBuff,int nLen,InforParam_t client);
typedef void ( * CallAddClient)(InforParam_t client);
typedef void ( * CallDelClient)(InforParam_t client);


/**
* �ṹ�����ƣ�PER_IO_DATA
* �ṹ�幦�ܣ��ص�I/O��Ҫ�õ��Ľṹ�壬��ʱ��¼IO����
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
* �ṹ�����ƣ�PER_HANDLE_DATA
* �ṹ��洢����¼�����׽��ֵ����ݣ��������׽��ֵı������׽��ֵĶ�Ӧ�Ŀͻ��˵ĵ�ַ��
* �ṹ�����ã��������������Ͽͻ���ʱ����Ϣ�洢���ýṹ���У�֪���ͻ��˵ĵ�ַ�Ա��ڻطá�
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
	* ��ʼ��
    */
	bool initComplete(string strIp,int nPort,int nThreadCnts = -1);
	/*
	* ��ʼ��ɶ˿ڷ��� 
	*/
	bool startComplete();
	/*
	* ��ͣ��ɶ˿ڷ���
	*/
	bool suspendComplete();
	/*
	* ������ɶ˿ڷ���
	*/
    
	bool resumeComplete();
    /*
	*ֹͣ��ɶ˿ڷ���
	*/
	bool stopComplete();
	/*
	* ��ȡ������������״̬
	*/
	bool isRunning();
	/*
	*���ٷ���
    */
	void destroyComplete();
    /*
    *   ���ûص�����
    */
    void setCallCompleteIO(CallCompleteIO fn);
    void setCallAddClient(CallAddClient fn);
    void setCallDelClient(CallDelClient fn);
    /*
    *   ��������
    */
    int sendData(SOCKET s,char * sendData,int nSendLen);

protected:
	/*
	*	Ͷ��accept�¼�
	*/
	bool addAcceptEvent();
    void addSockLink(PER_IO_DATA* pHandle);
    void delSockLink(PER_IO_DATA* pHandle);
    PER_IO_DATA * getSockLink(string strIp);
 
	/*
	* ��������߳�
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
	std::vector < PER_IO_DATA*>     m_vecClientGroup;        // ��¼�ͻ��˵�������
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