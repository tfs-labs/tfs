#ifndef _SOCKET_BUF_H_
#define _SOCKET_BUF_H_

#include <string>
#include <queue>
#include <utility>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>


#include "../include/logging.h"
#include "./msg_queue.h"
#include "./net_api.h"


extern std::unordered_map<int, std::unique_ptr<std::mutex>> fds_mutex;
std::mutex& get_fd_mutex(int fd);



/* Message length is the first 4 bytes of the message and does not contain these 4 bytes */
class SocketBuf
{
public:
    int fd;
    uint64_t port_and_ip;

private:
    std::string cache;
	std::mutex mutex_for_read_;

    std::string send_cache;
    std::mutex mutex_for_send_;
	std::atomic<bool> is_sending_;

private:
    bool send_pk_to_mess_queue(const std::string& send_data);

public:
	SocketBuf() : fd(0), port_and_ip(0), is_sending_(false) {};
    ~SocketBuf()
    {
        std::lock_guard<std::mutex> lck(mutex_for_read_);
        if (!this->cache.empty())
        {
            INFOLOG("SocketBuf is not empty!!");
            INFOLOG("SocketBuf.fd: {}", fd);
            INFOLOG("SocketBuf.port_and_ip: {}", port_and_ip);
            INFOLOG("SocketBuf.cache: {}", cache.c_str());
        }
        if(!this->send_cache.empty())
            DEBUGLOG("send_cache: {}", this->send_cache.c_str());
    };
/*API*/
    bool add_data_to_read_buf(char *data, size_t len);
    void printf_cache();
    std::string get_send_msg();
    void pop_n_send_msg(int n);
    void push_send_msg(const std::string& data);
    bool is_send_cache_empty();

    void verify_cache(size_t curr_msg_len);      //Verify the buffer and fix if there are errors
    void correct_cache();                       //Fix buffers
};


class BufferCrol
{
public:
    BufferCrol() = default;
    ~BufferCrol() = default;

private:
	std::mutex mutex_;
    std::map<uint64_t,std::shared_ptr<SocketBuf>> BufferMap;

public:
    bool add_read_buffer_queue(uint32_t ip, uint16_t port, char* buf, socklen_t len);
    bool add_read_buffer_queue(uint64_t port_and_ip, char* buf, socklen_t len);
    bool add_buffer(uint32_t ip, uint16_t port,const int fd);
    bool add_buffer(uint64_t port_and_ip,const int fd);
    bool delete_buffer(uint32_t ip, uint16_t port);
    bool delete_buffer(uint64_t port_and_ip);
    bool delete_buffer(const int fd);
    std::string get_write_buffer_queue(uint64_t port_and_ip);
    void pop_n_write_buffer_queue(uint64_t port_and_ip, int n);
	
    bool add_write_pack(uint64_t port_and_ip, const std::string ios_msg);
	bool add_write_pack(uint32_t ip, uint16_t port, const std::string ios_msg);

    bool is_exists(uint64_t port_and_ip);
	bool is_exists(std::string ip, uint16_t port);
	bool is_exists(uint32_t ip, uint16_t port);

    std::shared_ptr<SocketBuf> get_socket_buf(uint32_t ip, uint16_t port);
    
    bool is_cache_empty(uint32_t ip, uint16_t port);

    /*test api*/
    void print_bufferes();
};


#endif
