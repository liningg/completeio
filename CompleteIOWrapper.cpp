
#include "CompleteIOWrapper.h"
#include "MSTcpIP.h"
using namespace std;
namespace Communication
{


typedef unsigned long ULONG_PTR;

LPFN_ACCEPTEX lpfnAcceptEx = NULL;//AcceptEx����ָ��
GUID guidAcceptEx = WSAID_ACCEPTEX;
GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs = NULL;
vector<unsigned int> vecSockLink;
CCompleteIOWrapper::CCompleteIOWrapper( void )
{
	m_bRuning = false;
	m_nThreadCnts = 0;
	m_strIp = "";
	m_nPort = 9500;
    m_nAcceptCnts = 0;
    m_CallAddClient = NULL;
    m_CallCompleteIO = NULL;
    m_CallDelClient = NULL;
	m_nCreateTheadCnts = -1;
    InitializeCriticalSection(&m_cs);
	
}

CCompleteIOWrapper::~CCompleteIOWrapper( void )
{
	destroyComplete();
    DeleteCriticalSection(&m_cs);
	
}
typedef struct TCP_KEEPALIVE_ {
    int u_longonoff;
    int u_longkeepalivetime;
    int u_longkeepaliveinterval;
} tcp_keepalive;
bool CCompleteIOWrapper::startComplete()
{
	if(m_bRuning){
		return false;
	}
    vecSockLink.clear();
	m_bRuning = true;

	 // ����IOCP���ں˶���
    /**
    * ��Ҫ�õ��ĺ�����ԭ�ͣ�
    * HANDLE WINAPI CreateIoCompletionPort(
    *    __in   HANDLE FileHandle,      // �Ѿ��򿪵��ļ�������߿վ����һ���ǿͻ��˵ľ��
    *    __in   HANDLE ExistingCompletionPort,  // �Ѿ����ڵ�IOCP���
    *    __in   ULONG_PTR CompletionKey,    // ��ɼ���������ָ��I/O��ɰ���ָ���ļ�
    *    __in   DWORD NumberOfConcurrentThreads // ��������ͬʱִ������߳�����һ���ƽ���CPU������*2
    * );
    **/
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (NULL == m_hCompletionPort) {   // ����IO�ں˶���ʧ��
        cerr << "CreateIoCompletionPort failed. Error:" << GetLastError() << endl;
        //system("pause");
        return false;
    }

    // ����IOCP�߳�--�߳����洴���̳߳�

    // ȷ���������ĺ�������
    SYSTEM_INFO mySysInfo;
    GetSystemInfo(&mySysInfo);
    int nThreadNums =( m_nCreateTheadCnts != -1 )?m_nCreateTheadCnts:mySysInfo.dwNumberOfProcessors * 2;

    // ���ڴ������ĺ������������߳�
    for (DWORD i = 0; i < (nThreadNums); ++i) {
        // �����������������̣߳�������ɶ˿ڴ��ݵ����߳�
        HANDLE ThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ServerWorkThread, this, 0, NULL);//��һNULL����Ĭ�ϰ�ȫѡ���һ��0�������߳�ռ����Դ��С���ڶ���0�������̴߳���������ִ��
        if (NULL == ThreadHandle) {
            cerr << "Create Thread Handle failed. Error:" << GetLastError() << endl;
            //system("pause");
            return false;
        }
        m_hThread[i] = ThreadHandle;
        ++m_nThreadCnts;
    }

    // ������ʽ�׽���
    m_srvSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);


    // Associate SOCKET with IOCP  
    if (NULL == CreateIoCompletionPort((HANDLE)m_srvSocket, m_hCompletionPort, NULL, 0))
    {
        cout << "CreateIoCompletionPort failed with error code: " << WSAGetLastError() << endl;
        if (INVALID_SOCKET != m_srvSocket)
        {
            closesocket(m_srvSocket);
            m_srvSocket = INVALID_SOCKET;
        }
        return false;
    }

    // ��SOCKET������
    SOCKADDR_IN srvAddr;
    srvAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(m_nPort);

	BOOL bDontLinger = FALSE;
	setsockopt (m_srvSocket,SOL_SOCKET,SO_DONTLINGER,(const char*)&bDontLinger,sizeof(BOOL));
    int bindResult = bind(m_srvSocket, (SOCKADDR*)&srvAddr, sizeof(SOCKADDR));
    if (SOCKET_ERROR == bindResult) {
        cerr << "Bind failed. Error:" << GetLastError() << endl;
        //system("pause");
        return false;
    }

    // ��SOCKET����Ϊ����ģʽ
    int listenResult = listen(m_srvSocket, SOMAXCONN);
    if (SOCKET_ERROR == listenResult) {
        cerr << "Listen failed. Error: " << GetLastError() << endl;
        //system("pause");
        return false;
    }
    

    // ��ʼ����IO����
   // cout << "����������׼�����������ڵȴ��ͻ��˵Ľ���...\n";

    //// �������ڷ������ݵ��߳�
    //m_hThread[m_nThreadCnts++] = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE)keepAlivaThread, this, 0, NULL);//�ڶ���0������ص���������Ϊ0


	
    for (int i = 0; i <1000; ++i)
    {
		if(!addAcceptEvent()){
			return false;
		}
    }
    return true;
}

bool CCompleteIOWrapper::suspendComplete()
{
	return true;
}

bool CCompleteIOWrapper::resumeComplete()
{
	return true;
}

bool CCompleteIOWrapper::stopComplete()
{
	for (int i = 0;i<m_nThreadCnts;i++)
	{
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
    }
	
	try
	{
		vector<PER_IO_DATA*>::iterator it = m_vecClientGroup.begin();
		while (it != m_vecClientGroup.end())
		{
			int ret = closesocket((*it)->client);
			it++;
		}
	}
	catch (...)
	{
	}
	
    m_bRuning = false;
    DWORD ret;
    for (int i = 0;i<m_nThreadCnts;i++)
    {
        ret = WaitForSingleObject(m_hThread[i],2000);
        if(ret == WAIT_TIMEOUT){
            DWORD code;
            GetExitCodeThread(m_hThread[i],&code);
            TerminateThread(m_hThread[i],code);
        }
    }
    ret =  closesocket(m_srvSocket);
	WSACleanup();
    
	return true;
}

bool CCompleteIOWrapper::isRunning()
{
	return m_bRuning;
}

void CCompleteIOWrapper::destroyComplete()
{
	
}

void CCompleteIOWrapper::setCallCompleteIO( CallCompleteIO fn )
{
    m_CallCompleteIO = fn;
}



void CCompleteIOWrapper::setCallAddClient( CallAddClient fn )
{
    m_CallAddClient = fn;
}

void CCompleteIOWrapper::setCallDelClient( CallDelClient fn )
{
    m_CallDelClient = fn;
}

int CCompleteIOWrapper::sendData(SOCKET s,char * sendData,int nSendLen )
{
    //����IP��Ӧ��socket��Ȼ����
//     PER_IO_DATA *  pData = getSockLink(strIp);
 	int ret = 0;
// 	if(pData != NULL){
// 		SOCKET client = pData->client;
// 		ret = send(client,sendData,nSendLen,0);
// 	}
	ret = send(s,sendData,nSendLen,0);	
	return ret;
}

void CCompleteIOWrapper::addSockLink(PER_IO_DATA* pHandle)
{
    EnterCriticalSection(&m_cs);
    m_vecClientGroup.push_back(pHandle);
    InforParam_t client;
    client.nSock = pHandle->client;
    client.strIp = pHandle->strIp;
    if(m_CallAddClient){
        m_CallAddClient(client);
    }
    
    LeaveCriticalSection(&m_cs);
}
void CCompleteIOWrapper::delSockLink(PER_IO_DATA* pHandle)
{
    EnterCriticalSection(&m_cs);
    vector<PER_IO_DATA*>::iterator it = m_vecClientGroup.begin();
    while (it != m_vecClientGroup.end())
    {
        if((*it)->client == pHandle->client){
            m_vecClientGroup.erase(it);
            break;
        }
        it++;
    }

    InforParam_t client;
    client.nSock = pHandle->client;
    client.strIp = pHandle->strIp;
    if(m_CallDelClient){
        m_CallDelClient(client);
    }
    LeaveCriticalSection(&m_cs);
}

PER_IO_DATA * CCompleteIOWrapper::getSockLink( string strIp )
{
    EnterCriticalSection(&m_cs);
    vector<PER_IO_DATA*>::iterator it = m_vecClientGroup.begin();
    while (it != m_vecClientGroup.end())
    {
        if((*it)->strIp == strIp){
			LeaveCriticalSection(&m_cs);
			return *it;
        }
        it++;
    }
    LeaveCriticalSection(&m_cs);
    return NULL;
}

bool CCompleteIOWrapper::initComplete(string strIp,int nPort,int nThreadCnts)
{
	// ����socket��̬���ӿ�
	WORD wVersionRequested = MAKEWORD(2, 2); // ����2.2�汾��WinSock��
	WSADATA wsaData;    // ����Windows Socket�Ľṹ��Ϣ
	DWORD err = WSAStartup(wVersionRequested, &wsaData);

	if (0 != err) { // ����׽��ֿ��Ƿ�����ɹ�
		cerr << "Request Windows Socket Library Error!\n";
		return false;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {// ����Ƿ�����������汾���׽��ֿ�
		WSACleanup();
		cerr << "Request Windows Socket Version 2.2 Error!\n";
		return false;
	}	
    m_strIp = strIp;
    m_nPort = nPort;
    m_nCreateTheadCnts = nThreadCnts;
	m_hMutex = CreateMutex(NULL, FALSE, NULL);
	return true;
}

bool CCompleteIOWrapper::addAcceptEvent()
{
	DWORD dwBytes = 0;
	PER_HANDLE_DATA * PerHandleData = NULL;
	SOCKET acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == acceptSocket)
	{
		cerr << "WSASocket failed with error code: %d/n" << WSAGetLastError() << endl;
		return false;
	}
	// ����KeepAlive

	BOOL bKeepAlive = TRUE;
	int nRet = ::setsockopt(acceptSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&bKeepAlive, sizeof(bKeepAlive));
	if (nRet == SOCKET_ERROR)
	{
		return FALSE;
	}
	// ����KeepAlive����
	tcp_keepalive alive_in               = {0};
	tcp_keepalive alive_out              = {0};
	alive_in.u_longkeepalivetime         = 5000;                // ��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��
	alive_in.u_longkeepaliveinterval     = 5000;                // ����KeepAlive̽����ʱ����
	alive_in.u_longonoff                 = TRUE;
	unsigned long ulBytesReturn = 0;
	nRet = WSAIoctl(acceptSocket, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL);
	if (nRet == SOCKET_ERROR)
	{
		return FALSE;
	}
	// ��ʼ�ڽ����׽����ϴ���I/Oʹ���ص�I/O����
	// ���½����׽�����Ͷ��һ�������첽
	// WSARecv��WSASend������ЩI/O������ɺ󣬹������̻߳�ΪI/O�����ṩ����    
	// ��I/O��������(I/O�ص�)
	LPPER_IO_OPERATION_DATA PerIoData = NULL;
	PerIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATEION_DATA));
	ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED));
	PerIoData->databuff.len = DataBuffSize;
	PerIoData->databuff.buf = PerIoData->buffer;
	PerIoData->operationType = ACCEPT;  // read
	PerIoData->client = acceptSocket;

	if (SOCKET_ERROR == WSAIoctl(m_srvSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx), &lpfnAcceptEx,
		sizeof(lpfnAcceptEx), &dwBytes, NULL, NULL))
	{
		cerr << "WSAIoctl failed with error code: " << WSAGetLastError() << endl;
		if (INVALID_SOCKET != m_srvSocket)
		{
			closesocket(m_srvSocket);
			m_srvSocket = INVALID_SOCKET;
		}
		//goto EXIT_CODE;
		return false;
	}

	if (SOCKET_ERROR == WSAIoctl(m_srvSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs), &lpfnGetAcceptExSockAddrs, sizeof(lpfnGetAcceptExSockAddrs),
		&dwBytes, NULL, NULL))
	{
		cerr << "WSAIoctl failed with error code: " << WSAGetLastError() << endl;
		if (INVALID_SOCKET != m_srvSocket)
		{
			closesocket(m_srvSocket);
			m_srvSocket = INVALID_SOCKET;
		}
		//goto EXIT_CODE;
		return false;
	}

	if (FALSE == lpfnAcceptEx(m_srvSocket, PerIoData->client, PerIoData->databuff.buf, /*PerIoData->databuff.len - ((sizeof(SOCKADDR_IN) + 16) * 2)*/0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &dwBytes, &(PerIoData->overlapped)))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			cerr << "lpfnAcceptEx failed with error code: " << WSAGetLastError() << endl;

			return false;
		}
	}
	return true;
}

UINT WINAPI ServerWorkThread(LPVOID pvParam)
{
    CCompleteIOWrapper * pComplete = (CCompleteIOWrapper * )pvParam;
	HANDLE CompletionPort = pComplete->m_hCompletionPort;
	DWORD BytesTransferred;
	LPOVERLAPPED IpOverlapped;
	LPPER_HANDLE_DATA PerHandleData = NULL;
	LPPER_IO_DATA PerIoData = NULL;
	DWORD RecvBytes = 0;
	DWORD Flags = 0;
	BOOL bRet = false;

	while (true) {
		bRet = GetQueuedCompletionStatus(CompletionPort, &BytesTransferred, (PULONG_PTR)&PerHandleData, (LPOVERLAPPED*)&IpOverlapped, INFINITE);
		if (bRet == 0) {
			 /** 
             *  ERROR_NETNAME_DELETED 
             *  ��ʾ�ͻ�������ʧЧ�ˣ���Ҫ�Ͽ������������������ͻ����ǿ���
             */
			if( (GetLastError() ==WAIT_TIMEOUT) || (GetLastError() == ERROR_NETNAME_DELETED) )
			{
				PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);
				//�����������ǣ�����һ���ṹ��ʵ���еĳ�Ա�ĵ�ַ��ȡ�������ṹ��ʵ���ĵ�ַ
				//PER_IO_DATA�ĳ�Աoverlapped�ĵ�ַΪ&IpOverlapped������Ϳ��Ի��PER_IO_DATA�ĵ�ַ
				//��ɶ˿ڹر�
				if(PerIoData == NULL)
				{
					break;
				}
				if(PerHandleData == NULL)
				{
					continue;
				}
                pComplete->delSockLink(PerIoData);
                closesocket(PerHandleData->socket);
                GlobalFree(PerHandleData);
                GlobalFree(PerIoData); 
				pComplete->addAcceptEvent();
				continue;
			}
            //��ɶ˿�ʧ��
            //break ;
		}
		PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);
		//�����������ǣ�����һ���ṹ��ʵ���еĳ�Ա�ĵ�ַ��ȡ�������ṹ��ʵ���ĵ�ַ
		//PER_IO_DATA�ĳ�Աoverlapped�ĵ�ַΪ&IpOverlapped������Ϳ��Ի��PER_IO_DATA�ĵ�ַ
        //��ɶ˿ڹر�
        if(PerIoData == NULL){
            break;
        }


		// ������׽������Ƿ��д�����
		if (0 == BytesTransferred && (PerIoData->operationType == RECV || PerIoData->operationType == SEND))
		{
			// ��ʼ���ݴ����������Կͻ��˵�����
            pComplete->delSockLink(PerIoData);
			closesocket(PerHandleData->socket);
			GlobalFree(PerHandleData);
			GlobalFree(PerIoData);   
			pComplete->addAcceptEvent();
			continue;
		}
		

		switch (PerIoData->operationType)
		{
		case ACCEPT:
			{
				SOCKADDR_IN* remote = NULL;
				SOCKADDR_IN* local = NULL;
				int remoteLen = sizeof(SOCKADDR_IN);
				int localLen = sizeof(SOCKADDR_IN);
				lpfnGetAcceptExSockAddrs(PerIoData->databuff.buf, /*PerIoData->databuff.len - ((sizeof(SOCKADDR_IN) + 16) * 2)*/0,
					sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&local, &localLen, (LPSOCKADDR*)&remote, &remoteLen);

                //inet_ntoa(remote->sin_addr)

				//ʹ��GetAcceptExSockaddrs���� ��þ���ĸ�����ַ����.
				/*if (setsockopt(PerIoData->client, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
					(char*)&(PerHandleData->socket), sizeof(PerHandleData->socket)) == SOCKET_ERROR)
					cout << "setsockopt..." << endl;*/
				// �����������׽��ֹ����ĵ����������Ϣ�ṹ
				PerHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));  // �ڶ���Ϊ���PerHandleData����ָ����С���ڴ�
				PerHandleData->socket = PerIoData->client;
               

				//memcpy(&(perHandleData->clientAddr),raddr,sizeof(raddr));
				//���µĿͻ��׽�������ɶ˿�����
				CreateIoCompletionPort((HANDLE)PerHandleData->socket,
					CompletionPort, (ULONG_PTR)PerHandleData, 0);

				memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
				PerIoData->operationType = RECV;        //��״̬���óɽ���
				//����WSABUF�ṹ
				PerIoData->databuff.buf = PerIoData->buffer;
				PerIoData->databuff.len = PerIoData->BufferLen = 8*1024;
                PerIoData->strIp = inet_ntoa(remote->sin_addr);

				cout << "wait for data arrive(Accept)..." << endl;
				Flags = 0;
                //
				if (WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1,
					&RecvBytes, &Flags, &(PerIoData->overlapped), NULL) == SOCKET_ERROR)
					if (WSAGetLastError() == WSA_IO_PENDING)
						cout << "WSARecv Pending..." << endl;

                pComplete->addSockLink(PerIoData);
				continue;
			}
			break;
		case RECV:
            {
                // ��ʼ���ݴ����������Կͻ��˵�����
                WaitForSingleObject(pComplete->m_hMutex,INFINITE);
				//�����������0 ���ʾ�����ɹ�
				InforParam_t infor;
				infor.nSock = PerIoData->client;
				infor.strIp = PerIoData->strIp;
				pComplete->m_CallCompleteIO(PerIoData->databuff.buf,BytesTransferred,infor);
				// Ϊ��һ���ص����ý�����I/O��������
				ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED)); // ����ڴ�
				PerIoData->databuff.len = DataBuffSize;
				PerIoData->databuff.buf = PerIoData->buffer;//buf�Ǹ�ָ�룬��һ���̻����buffer������
				PerIoData->operationType = RECV;    // read
				WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);
				
                ReleaseMutex(pComplete->m_hMutex);
            }
            break;
        case SEND:
            break;
		default:
			break;
		}
	}

	return 0;
}
}