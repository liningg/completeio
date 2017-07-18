
#include "CompleteIOWrapper.h"
#include "MSTcpIP.h"
using namespace std;
namespace Communication
{


typedef unsigned long ULONG_PTR;

LPFN_ACCEPTEX lpfnAcceptEx = NULL;//AcceptEx函数指针
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

	 // 创建IOCP的内核对象
    /**
    * 需要用到的函数的原型：
    * HANDLE WINAPI CreateIoCompletionPort(
    *    __in   HANDLE FileHandle,      // 已经打开的文件句柄或者空句柄，一般是客户端的句柄
    *    __in   HANDLE ExistingCompletionPort,  // 已经存在的IOCP句柄
    *    __in   ULONG_PTR CompletionKey,    // 完成键，包含了指定I/O完成包的指定文件
    *    __in   DWORD NumberOfConcurrentThreads // 真正并发同时执行最大线程数，一般推介是CPU核心数*2
    * );
    **/
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (NULL == m_hCompletionPort) {   // 创建IO内核对象失败
        cerr << "CreateIoCompletionPort failed. Error:" << GetLastError() << endl;
        //system("pause");
        return false;
    }

    // 创建IOCP线程--线程里面创建线程池

    // 确定处理器的核心数量
    SYSTEM_INFO mySysInfo;
    GetSystemInfo(&mySysInfo);
    int nThreadNums =( m_nCreateTheadCnts != -1 )?m_nCreateTheadCnts:mySysInfo.dwNumberOfProcessors * 2;

    // 基于处理器的核心数量创建线程
    for (DWORD i = 0; i < (nThreadNums); ++i) {
        // 创建服务器工作器线程，并将完成端口传递到该线程
        HANDLE ThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ServerWorkThread, this, 0, NULL);//第一NULL代表默认安全选项，第一个0，代表线程占用资源大小，第二个0，代表线程创建后立即执行
        if (NULL == ThreadHandle) {
            cerr << "Create Thread Handle failed. Error:" << GetLastError() << endl;
            //system("pause");
            return false;
        }
        m_hThread[i] = ThreadHandle;
        ++m_nThreadCnts;
    }

    // 建立流式套接字
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

    // 绑定SOCKET到本机
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

    // 将SOCKET设置为监听模式
    int listenResult = listen(m_srvSocket, SOMAXCONN);
    if (SOCKET_ERROR == listenResult) {
        cerr << "Listen failed. Error: " << GetLastError() << endl;
        //system("pause");
        return false;
    }
    

    // 开始处理IO数据
   // cout << "本服务器已准备就绪，正在等待客户端的接入...\n";

    //// 创建用于发送数据的线程
    //m_hThread[m_nThreadCnts++] = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE)keepAlivaThread, this, 0, NULL);//第二个0，代表回掉函数参数为0


	
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
    //查找IP对应的socket，然后发送
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
	// 加载socket动态链接库
	WORD wVersionRequested = MAKEWORD(2, 2); // 请求2.2版本的WinSock库
	WSADATA wsaData;    // 接收Windows Socket的结构信息
	DWORD err = WSAStartup(wVersionRequested, &wsaData);

	if (0 != err) { // 检查套接字库是否申请成功
		cerr << "Request Windows Socket Library Error!\n";
		return false;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {// 检查是否申请了所需版本的套接字库
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
	// 开启KeepAlive

	BOOL bKeepAlive = TRUE;
	int nRet = ::setsockopt(acceptSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&bKeepAlive, sizeof(bKeepAlive));
	if (nRet == SOCKET_ERROR)
	{
		return FALSE;
	}
	// 设置KeepAlive参数
	tcp_keepalive alive_in               = {0};
	tcp_keepalive alive_out              = {0};
	alive_in.u_longkeepalivetime         = 5000;                // 开始首次KeepAlive探测前的TCP空闭时间
	alive_in.u_longkeepaliveinterval     = 5000;                // 两次KeepAlive探测间的时间间隔
	alive_in.u_longonoff                 = TRUE;
	unsigned long ulBytesReturn = 0;
	nRet = WSAIoctl(acceptSocket, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL);
	if (nRet == SOCKET_ERROR)
	{
		return FALSE;
	}
	// 开始在接受套接字上处理I/O使用重叠I/O机制
	// 在新建的套接字上投递一个或多个异步
	// WSARecv或WSASend请求，这些I/O请求完成后，工作者线程会为I/O请求提供服务    
	// 单I/O操作数据(I/O重叠)
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
             *  表示客户端连接失效了，需要断开重连，可能阻塞到客户端那块了
             */
			if( (GetLastError() ==WAIT_TIMEOUT) || (GetLastError() == ERROR_NETNAME_DELETED) )
			{
				PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);
				//这个宏的作用是：根据一个结构体实例中的成员的地址，取到整个结构体实例的地址
				//PER_IO_DATA的成员overlapped的地址为&IpOverlapped，结果就可以获得PER_IO_DATA的地址
				//完成端口关闭
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
            //完成端口失败
            //break ;
		}
		PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);
		//这个宏的作用是：根据一个结构体实例中的成员的地址，取到整个结构体实例的地址
		//PER_IO_DATA的成员overlapped的地址为&IpOverlapped，结果就可以获得PER_IO_DATA的地址
        //完成端口关闭
        if(PerIoData == NULL){
            break;
        }


		// 检查在套接字上是否有错误发生
		if (0 == BytesTransferred && (PerIoData->operationType == RECV || PerIoData->operationType == SEND))
		{
			// 开始数据处理，接收来自客户端的数据
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

				//使用GetAcceptExSockaddrs函数 获得具体的各个地址参数.
				/*if (setsockopt(PerIoData->client, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
					(char*)&(PerHandleData->socket), sizeof(PerHandleData->socket)) == SOCKET_ERROR)
					cout << "setsockopt..." << endl;*/
				// 创建用来和套接字关联的单句柄数据信息结构
				PerHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));  // 在堆中为这个PerHandleData申请指定大小的内存
				PerHandleData->socket = PerIoData->client;
               

				//memcpy(&(perHandleData->clientAddr),raddr,sizeof(raddr));
				//将新的客户套接字与完成端口连接
				CreateIoCompletionPort((HANDLE)PerHandleData->socket,
					CompletionPort, (ULONG_PTR)PerHandleData, 0);

				memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
				PerIoData->operationType = RECV;        //将状态设置成接收
				//设置WSABUF结构
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
                // 开始数据处理，接收来自客户端的数据
                WaitForSingleObject(pComplete->m_hMutex,INFINITE);
				//返回如果大于0 则表示解析成功
				InforParam_t infor;
				infor.nSock = PerIoData->client;
				infor.strIp = PerIoData->strIp;
				pComplete->m_CallCompleteIO(PerIoData->databuff.buf,BytesTransferred,infor);
				// 为下一个重叠调用建立单I/O操作数据
				ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED)); // 清空内存
				PerIoData->databuff.len = DataBuffSize;
				PerIoData->databuff.buf = PerIoData->buffer;//buf是个指针，这一过程会清空buffer的内容
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