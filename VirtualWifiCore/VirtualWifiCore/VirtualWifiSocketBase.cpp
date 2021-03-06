#include "stdafx.h"
#include <process.h>

VirtualWifiSocketBase::VirtualWifiSocketBase()
{   
}
void VirtualWifiSocketBase::CloseSocket(SOCKET& socket)
{
	if (socket)
	{
		closesocket(socket);
	}
}
void VirtualWifiSocketBase::OnAccept(SOCKET& socket)
{
	v_log->write_log_come_to("VirtualWifiSocketBase::OnAccept(SOCKET& socket)");
	AddClient(socket);
}
bool VirtualWifiSocketBase::AddClient(SOCKET& socket)
{
    if (m_nClientTotal>WSA_MAXIMUM_WAIT_EVENTS)
    {
		return false;
    }
	WSAEVENT event = WSACreateEvent();
	WSAEventSelect(socket, event, FD_READ|FD_CLOSE);
	m_eventArray[m_nClientTotal] = event;
	m_socketArrary[m_nClientTotal] = socket;
	m_nClientTotal++;
	return true;
}
void VirtualWifiSocketBase::OnRead(SOCKET& socket)
{
   
}
void VirtualWifiSocketBase::OnClose(SOCKET& socket,int nIndex)
{
   CloseSocket(socket);
   for (int j=nIndex;j<m_nClientTotal-1;++j)
   {
	   m_socketArrary[j]=m_socketArrary[j+1];
	   m_eventArray[j]=m_eventArray[j+1];
   }
   m_nClientTotal--;
}
//启动监听线程,等待socket连接
//注意:不要使用这个类的局部变量,因为非阻塞,出函数体外,内存会销毁
bool VirtualWifiSocketBase::Start()
{
	WSADATA wsaData;
	int err,ret;
	WORD wVersionRequested = MAKEWORD( 2, 2 );
	err = WSAStartup( wVersionRequested, &wsaData);
	if ( err != 0 ) {
		return false;
	}

	m_acceptSocket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (m_acceptSocket==INVALID_SOCKET)
	{
		return false;
	}
	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr.S_un.S_addr=htonl(INADDR_ANY);
	addrSrv.sin_family=AF_INET;
	addrSrv.sin_port=htons(m_softinfo->sPort);

	ret=bind(m_acceptSocket,(SOCKADDR*)&addrSrv,sizeof(SOCKADDR));// 绑定端口
	if (SOCKET_ERROR==ret)
	{
		//10048错误,端口占用
		int err=WSAGetLastError();
		v_log->write_log_error_code("bind ",err);
		return false;
	}
	ret=listen(m_acceptSocket,5);
   
	m_nClientTotal=0;

	_beginthread(CreateSocket,0,this);

	DWORD derr=GetLastError();
	return true;
}
void VirtualWifiSocketBase::CreateSocket(LPVOID param)
{
	v_log->write_log_come_to("create socket thread");
	VirtualWifiSocketBase* VWS=(VirtualWifiSocketBase*)param;

    //事件句柄和套接字句柄
    WSAEVENT* eventArray=VWS->GetEventArrary();
    SOCKET*  socketArrary=VWS->GetSocketArrary();
    int& nEventTotal = VWS->GetClientTotal();
 
	int ret=0,err=0;
    
	SOCKET acceptSocket=VWS->GetAcceptSocket();

    WSAEVENT event =WSACreateEvent();

    ret=WSAEventSelect(acceptSocket,event,FD_ACCEPT);
    err=WSAGetLastError();

    eventArray[nEventTotal] = event;
    socketArrary[nEventTotal] = acceptSocket;
    nEventTotal++;
   
	v_log->write_log_success("init socket");
	while(nEventTotal>0)
	{
	int nIndex = WSAWaitForMultipleEvents(nEventTotal, eventArray, FALSE, 360, FALSE);

	nIndex = nIndex - WSA_WAIT_EVENT_0;

	for(int i=nIndex;i<nEventTotal;i++)
	{
	nIndex = WSAWaitForMultipleEvents(1, &eventArray[i], TRUE, 0, FALSE);
	if(nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
	{
	     continue;
	}
	else 
	{
		WSANETWORKEVENTS event;
		WSAEnumNetworkEvents(socketArrary[i],eventArray[i],&event);
		if(event.lNetworkEvents&FD_ACCEPT)// 处理FD_ACCEPT事件
		{   
			v_log->write_log_run_to("socket accept");
			struct sockaddr_in sa; 
			int naddrlen = sizeof(sa);
			SOCKET newsocket = accept(socketArrary[i],(struct sockaddr FAR *)&sa,&naddrlen);
			VWS->SetSocketInfo(sa);
		    VWS->OnAccept(newsocket);
		}
		else if(event.lNetworkEvents&FD_READ)// 处理FD_READ事件
		{
			VWS->OnRead(socketArrary[i]);
		}
		else if(event.lNetworkEvents&FD_CLOSE)
		{
			VWS->OnClose(socketArrary[i],i);
		}
	}
}
}
}

const char* VirtualWifiSocketBase::GetAcceptIp()
{
	return inet_ntoa(m_sockaddr_in->sin_addr);
}
void VirtualWifiSocketBase::SetSocketInfo(sockaddr_in& sa)
{
	this->m_sockaddr_in=&sa;
}
SOCKET& VirtualWifiSocketBase::GetAcceptSocket()
{
	return m_acceptSocket;
}
SOCKET* VirtualWifiSocketBase::GetSocketArrary()
{
	return m_socketArrary;
}

WSAEVENT* VirtualWifiSocketBase::GetEventArrary()
{
	return m_eventArray;
}
int& VirtualWifiSocketBase::GetClientTotal()
{
	return m_nClientTotal;
}

//MS这样停止,不安全
void VirtualWifiSocketBase::Stop()
{
   m_nClientTotal=-1;
   WSACleanup();
}
VirtualWifiSocketBase::~VirtualWifiSocketBase()
{
	
}