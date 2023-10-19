/**
 * *****************************************************************************
 * @file        socket_buf.h
 * @brief       
 * @author  ()
 * @date        2023-09-27
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _SOCKET_BUF_H_
#define _SOCKET_BUF_H_

#include <string>
#include <queue>
#include <utility>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>

#include "./msg_queue.h"
#include "./api.h"

#include "../include/logging.h"



extern std::unordered_map<int, std::unique_ptr<std::mutex>> fdsMutex;

/**
 * @brief       Get the fd mutex object
 * 
 * @param       fd 
 * @return      std::mutex& 
 */
std::mutex& GetFdMutex(int fd);

/**
 * @brief       Get the Mutex Size object
 * 
 * @return      int 
 */
int GetMutexSize();


// Message length is the first 4 bytes of the message and does not contain these 4 bytes 
class SocketBuf
{
public:
    int fd;
    uint64_t portAndIp;

private:
    std::string _cache;
	std::mutex _mutexForRead;

    std::string _sendCache;
    std::mutex _mutexForSend;
	std::atomic<bool> _isSending;

private:
    /**
     * @brief       
     * 
     * @param       sendData 
     * @return      true 
     * @return      false 
     */
    bool _SendPkToMessQueue(const std::string& sendData);

public:
	SocketBuf() 
    : fd(0)
    , portAndIp(0)
    , _isSending(false) 
    {};
    // ~SocketBuf()
    // {
    //     std::lock_guard<std::mutex> lck(_mutexForRead);
    //     if (!this->cache.empty())
    //     {
    //         INFOLOG("SocketBuf is not empty!!");
    //         INFOLOG("SocketBuf.fd: {}", fd);
    //         INFOLOG("SocketBuf.portAndIp: {}", portAndIp);
    //         INFOLOG("SocketBuf.cache: {}", cache.c_str());
    //     }
    //     if(!this->_sendCache.empty())
    //         DEBUGLOG("_sendCache: {}", this->_sendCache.c_str());
    // };
/*API*/

    /**
     * @brief       
     * 
     * @param       data 
     * @param       len 
     * @return      true 
     * @return      false 
     */
    bool AddDataToReadBuf(char *data, size_t len);

    /**
     * @brief       
     * 
     */
    void PrintfCache();

    /**
     * @brief       Get the Send Msg object
     * 
     * @return      std::string 
     */
    std::string GetSendMsg();

    /**
     * @brief       
     * 
     * @param       n 
     */
    void PopNSendMsg(int n);

    /**
     * @brief       
     * 
     * @param       data 
     */
    void PushSendMsg(const std::string& data);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool IsSendCacheEmpty();

    /**
     * @brief       
     * 
     * @param       currMsgLen 
     */

    /**
     * @brief       Verify the buffer and fix if there are errors

     * 
     * @param       currMsgLen 
     */
    void VerifyCache(size_t currMsgLen);      

    /**
     * @brief       Fix buffers
     * 
     */
    void CorrectCache();                       //
};


class BufferCrol
{
public:
    BufferCrol() = default;
    ~BufferCrol() = default;

private:
    friend std::string PrintCache(int where);
	std::mutex _mutex;
    std::map<uint64_t,std::shared_ptr<SocketBuf>> _BufferMap;

public:

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @param       buf 
     * @param       len 
     * @return      true 
     * @return      false 
     */
    bool AddReadBufferQueue(uint32_t ip, uint16_t port, char* buf, socklen_t len);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       buf 
     * @param       len 
     * @return      true 
     * @return      false 
     */
    bool AddReadBufferQueue(uint64_t portAndIp, char* buf, socklen_t len);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool AddBuffer(uint32_t ip, uint16_t port,const int fd);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool AddBuffer(uint64_t portAndIp,const int fd);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
    bool DeleteBuffer(uint32_t ip, uint16_t port);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @return      true 
     * @return      false 
     */
    bool DeleteBuffer(uint64_t portAndIp);

    /**
     * @brief       
     * 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool DeleteBuffer(const int fd);

    /**
     * @brief       Get the Write Buffer Queue object
     * 
     * @param       portAndIp 
     * @return      std::string 
     */
    std::string GetWriteBufferQueue(uint64_t portAndIp);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       n 
     */
    void PopNWriteBufferQueue(uint64_t portAndIp, int n);
	
    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @param       ios_msg 
     * @return      true 
     * @return      false 
     */
    bool AddWritePack(uint64_t portAndIp, const std::string ios_msg);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @param       ios_msg 
     * @return      true 
     * @return      false 
     */
	bool AddWritePack(uint32_t ip, uint16_t port, const std::string ios_msg);

    /**
     * @brief       
     * 
     * @param       portAndIp 
     * @return      true 
     * @return      false 
     */
    bool IsExists(uint64_t portAndIp);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
	bool IsExists(std::string ip, uint16_t port);

    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
	bool IsExists(uint32_t ip, uint16_t port);

    /**
     * @brief       Get the Socket Buf object
     * 
     * @param       ip 
     * @param       port 
     * @return      std::shared_ptr<SocketBuf> 
     */
    std::shared_ptr<SocketBuf> GetSocketBuf(uint32_t ip, uint16_t port);
    
    /**
     * @brief       
     * 
     * @param       ip 
     * @param       port 
     * @return      true 
     * @return      false 
     */
    bool IsCacheEmpty(uint32_t ip, uint16_t port);

   
    /**
     * @brief       *test api*
     * 
     */
    void PrintBufferes();
};


#endif
