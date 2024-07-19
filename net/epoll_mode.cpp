#include "epoll_mode.h"
#include "./global.h"
#include "../utils/console.h"

EpollMode::EpollMode()
{
    this->_listenThread = NULL;
}

EpollMode::~EpollMode()
{
    if (NULL != this->_listenThread)
    {
        delete this->_listenThread;
        this->_listenThread = NULL;
    }
}

bool EpollMode::InitListen()
{
    this->epollFd = epoll_create(MAXEPOLLSIZE);
    if (this->epollFd < 0)
    {
        ERRORLOG("EpollMode error");
        return false;
    }
    this->_fdSerMain = net_tcp::ListenServerInit(SERVERMAINPORT, 1000);
    this->EpollLoop(this->_fdSerMain, EPOLLIN | EPOLLOUT | EPOLLET);
    return true;
}

void EpollMode::EpollWork(EpollMode *epmd)
{
    epmd->InitListen();
    global::g_ListenThreadInited = true;
    global::g_condListenThread.notify_all();
    epmd->EpollLoop();
}

bool EpollMode::EpoolModeStart()
{
    this->_listenThread = new std::thread(EpollMode::EpollWork, this);
    this->_listenThread->detach();
    return true;
}

int EpollMode::EpollLoop()
{
    INFOLOG("EpollMode working");
    int nfds, n;
    struct sockaddr_in cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event events[MAXEPOLLSIZE];
    struct rlimit rt;
    u32 u32_ip;
    u16 u16_port;
    int eFd;
    MsgData data;
    //Sets the maximum number of files allowed to be opened per process
    rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
    if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
    {
        ERRORLOG("setrlimit error");
    }
    INFOLOG("epoll loop EpoolModeStart success!");
    while (_haltListening)
    {

        //Wait for something to happen
        nfds = epoll_wait(this->epollFd, events, MAXEPOLLSIZE, 1 * 1000);
        if (nfds == -1)
        {
            continue;
        }
        //Handle all events
        for (n = 0; n < nfds; ++n)
        {
            if (events[n].events & EPOLLERR)
            {
                int status, err;
                socklen_t len;
                err = 0;
                len = sizeof(err);
                eFd = events[n].data.fd;
                status = getsockopt(eFd, SOL_SOCKET, SO_ERROR, &err, &len);
                //Connection failed
                if (status == 0)
                {
                    MagicSingleton<PeerNode>::GetInstance()->DeleteByFd(eFd);
                }
            }
            //Handle primary connection listening
            if (events[n].data.fd == this->_fdSerMain)
            {
                int connFd = 0;
                int lisFd = events[n].data.fd;
       
                while ((connFd = net_tcp::Accept(lisFd, (struct sockaddr *)&cliaddr, &socklen)) > 0)
                {
                    net_tcp::SetFdNoBlocking(connFd);
                    //Turn off all signals
                    int value = 1;
                    setsockopt(connFd, SOL_SOCKET, MSG_NOSIGNAL, &value, sizeof(value));

                    u32_ip = IpPort::IpNum(inet_ntoa(cliaddr.sin_addr));
                    u16_port = htons(cliaddr.sin_port);
                    auto self = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
                    DEBUGLOG(YELLOW "u32_ip({}),u16_port({}),self.publicIp({}),self.local_ip({})" RESET, IpPort::IpSz(u32_ip), u16_port, IpPort::IpSz(self.publicIp), IpPort::IpSz(self.listenIp));

                    MagicSingleton<BufferCrol>::GetInstance()->AddBuffer(u32_ip, u16_port, connFd);
                    MagicSingleton<EpollMode>::GetInstance()->EpollLoop(connFd, EPOLLIN | EPOLLOUT | EPOLLET);
                }
                if (connFd == -1)
                {
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
                        ERRORLOG("accept");
                }
                continue;
            }
            if (events[n].events & EPOLLIN)
            {
                eFd = events[n].data.fd;
                this->DeleteEpollEvent(eFd);
                u32_ip = IpPort::GetPeerNip(eFd);
                u16_port = IpPort::GetPeerPort(eFd);
                if(u16_port == SERVERMAINPORT)
                {
                     u16_port = IpPort::GetConnectPort(eFd);
                }

                data.ip = u32_ip;
                data.port = u16_port;
                data.type = E_READ;
                data.fd = eFd;
                global::g_queueRead.Push(data);
            }
            if (events[n].events & EPOLLOUT)
            {
                eFd = events[n].data.fd;
                u32_ip = IpPort::GetPeerNip(eFd);
                u16_port = IpPort::GetPeerPort(eFd);
                if(u16_port == SERVERMAINPORT)
                {
                     u16_port = IpPort::GetConnectPort(eFd);
                }
                
                bool isempty = MagicSingleton<BufferCrol>::GetInstance()->IsCacheEmpty(u32_ip, u16_port);
                if(isempty){
                    continue;
                }
                MsgData send;
                send.type = E_WRITE;
                send.fd = eFd;
                send.ip = u32_ip;
                send.port = u16_port;
                global::g_queueWrite.Push(send);
            }
        }
    }
    return 0;
}

bool EpollMode::EpollLoop(int fd, int state)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = state;
    ev.data.fd = fd;

    int ret = epoll_ctl(this->epollFd, EPOLL_CTL_ADD, fd, &ev);

    if (0 != ret)
    {
        return false;
    }
    return true;
}

bool EpollMode::DeleteEpollEvent(int fd)
{
    int ret = epoll_ctl(this->epollFd, EPOLL_CTL_DEL, fd, NULL);
    if (0 != ret)
    {
        return false;
    }
    return true;
}
