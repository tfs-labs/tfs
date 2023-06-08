#include "epoll_mode.h"
#include "./global.h"
#include "utils/console.h"

EpollMode::EpollMode()
{
    this->listen_thread = NULL;
}
EpollMode::~EpollMode()
{
    if (NULL != this->listen_thread)
    {
        delete this->listen_thread;
        this->listen_thread = NULL;
    }
}

bool EpollMode::init_listen()
{
    this->epoll_fd = epoll_create(MAXEPOLLSIZE);
    if (this->epoll_fd < 0)
    {
        ERRORLOG("EpollMode error");
        return false;
    }
    this->fd_ser_main = net_tcp::listen_server_init(SERVERMAINPORT, 1000);
    this->add_epoll_event(this->fd_ser_main, EPOLLIN | EPOLLOUT | EPOLLET);
    return true;
}

void EpollMode::work(EpollMode *epmd)
{
    epmd->init_listen();
    global::listen_thread_inited = true;
    global::cond_listen_thread.notify_all();
    epmd->epoll_loop();
}

bool EpollMode::start()
{
    this->listen_thread = new thread(EpollMode::work, this);
    this->listen_thread->detach();
    return true;
}

int EpollMode::epoll_loop()
{
    INFOLOG("EpollMode working");
    int nfds, n;
    struct sockaddr_in cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event events[MAXEPOLLSIZE];
    struct rlimit rt;
    u32 u32_ip;
    u16 u16_port;
    int e_fd;
    MsgData data;
    //Sets the maximum number of files allowed to be opened per process
    rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
    if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
    {
        ERRORLOG("setrlimit error");
    }
    INFOLOG("epoll loop start success!");
    while (1)
    {

        //Wait for something to happen
        nfds = epoll_wait(this->epoll_fd, events, MAXEPOLLSIZE, 1 * 1000);
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
                e_fd = events[n].data.fd;
                status = getsockopt(e_fd, SOL_SOCKET, SO_ERROR, &err, &len);
                //Connection failed
                if (status == 0)
                {
                    MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(e_fd);
                }
            }
            //Handle primary connection listening
            if (events[n].data.fd == this->fd_ser_main)
            {
                int connfd = 0;
                int lis_fd = events[n].data.fd;
       
                while ((connfd = net_tcp::Accept(lis_fd, (struct sockaddr *)&cliaddr, &socklen)) > 0)
                {
                    net_tcp::set_fd_noblocking(connfd);
                    //Turn off all signals
                    int value = 1;
                    setsockopt(connfd, SOL_SOCKET, MSG_NOSIGNAL, &value, sizeof(value));

                    u32_ip = IpPort::ipnum(inet_ntoa(cliaddr.sin_addr));
                    u16_port = htons(cliaddr.sin_port);
                    auto self = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
                    DEBUGLOG(YELLOW "u32_ip({}),u16_port({}),self.public_ip({}),self.local_ip({})" RESET, IpPort::ipsz(u32_ip), u16_port, IpPort::ipsz(self.public_ip), IpPort::ipsz(self.listen_ip));

                    MagicSingleton<BufferCrol>::GetInstance()->add_buffer(u32_ip, u16_port, connfd);
                    MagicSingleton<EpollMode>::GetInstance()->add_epoll_event(connfd, EPOLLIN | EPOLLOUT | EPOLLET);
                }
                if (connfd == -1)
                {
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
                        ERRORLOG("accept");
                }
                continue;
            }
            if (events[n].events & EPOLLIN)
            {
                e_fd = events[n].data.fd;
                this->delete_epoll_event(e_fd);
                u32_ip = IpPort::get_peer_nip(e_fd);
                u16_port = IpPort::get_peer_port(e_fd);
                if(u16_port == SERVERMAINPORT)
                {
                     u16_port = IpPort::get_connect_port(e_fd);
                }

                data.ip = u32_ip;
                data.port = u16_port;
                data.type = E_READ;
                data.fd = e_fd;
                global::queue_read.push(data);
            }
            if (events[n].events & EPOLLOUT)
            {
                e_fd = events[n].data.fd;
                u32_ip = IpPort::get_peer_nip(e_fd);
                u16_port = IpPort::get_peer_port(e_fd);
                if(u16_port == SERVERMAINPORT)
                {
                     u16_port = IpPort::get_connect_port(e_fd);
                }
                
                bool isempty = MagicSingleton<BufferCrol>::GetInstance()->is_cache_empty(u32_ip, u16_port);
                if(isempty){
                    continue;
                }
                MsgData send;
                send.type = E_WRITE;
                send.fd = e_fd;
                send.ip = u32_ip;
                send.port = u16_port;
                global::queue_write.push(send);
            }
        }
    }
    return 0;
}

bool EpollMode::add_epoll_event(int fd, int state)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = state;
    ev.data.fd = fd;

    int ret = epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, fd, &ev);

    if (0 != ret)
    {
        return false;
    }
    return true;
}

bool EpollMode::delete_epoll_event(int fd)
{
    int ret = epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (0 != ret)
    {
        return false;
    }
    return true;
}
