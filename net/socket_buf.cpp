#include "socket_buf.h"

#include "../net/global.h"
#include "../utils/util.h"
#include "../utils/console.h"

std::unordered_map<int, std::unique_ptr<std::mutex>> fdsMutex;
std::mutex mu;
std::mutex& GetFdMutex(int fd)
{
	std::lock_guard<std::mutex> lck(mu);    
    auto search = fdsMutex.find(fd);
    if (search != fdsMutex.end()) 
    {
        return *(search->second);
    } 
    else 
    {
        fdsMutex.insert(std::pair<int,std::unique_ptr<std::mutex>>(fd, std::unique_ptr<std::mutex>(new std::mutex)));
        return *(fdsMutex[fd]);
    }
}

int GetMutexSize()
{
    return fdsMutex.size();
}



std::string g_emptystr;

void SocketBuf::VerifyCache(size_t currMsgLen)
{
    if (currMsgLen > 100*1000*1000)
    {
        SocketBuf::CorrectCache();
    }
}

void SocketBuf::CorrectCache()
{  
    if(this->_cache.size() < sizeof(uint32_t))
    {
        this->_cache.clear();
        return;
    }
    const char * ptr = this->_cache.data();
    bool find = false;
    size_t i;
    for(i = 0 ; i <= this->_cache.size() - sizeof(uint32_t); i++)
    {
        int32_t tmpFlag = *((uint32_t*)(ptr + i));
        if(tmpFlag == END_FLAG)
        {
            find = true;
            break;
        }
    }
    if(find)
    {
        this->_cache.erase(0, i+4);
    }
    else
    {
        this->_cache.clear();
    }
}

bool SocketBuf::AddDataToReadBuf(char *data, size_t len)
{
	std::lock_guard<std::mutex> lck(_mutexForRead);
    if (data == NULL || len == 0)
    {
        ERRORLOG("AddDataToReadBuf error: data == NULL or len == 0");
        return false;
    }

    std::string tmp(data, len);
    this->_cache += tmp;
	size_t currMsgLen = 0;
    if (this->_cache.size() >= 4)
    {
        memcpy(&currMsgLen, this->_cache.data(), 4);  //The total length of the current message
        
        SocketBuf::VerifyCache(currMsgLen);

        while (this->_cache.size() >= (size_t)(4 + currMsgLen))
        {
            this->_cache.erase(0, 4);  //this leak! 
            std::string readData(this->_cache.begin(), this->_cache.begin() + currMsgLen);

            if (readData.size() < sizeof(uint32_t) * 3)
            {
                SocketBuf::CorrectCache();
                return false;
            }
            uint32_t checkSum = Util::adler32((unsigned char *)readData.data(), readData.size() - sizeof(uint32_t) * 3);
            uint32_t packCheckSum = *((uint32_t *)(this->_cache.data() + readData.size() - sizeof(uint32_t) * 3));
            
            if(checkSum != packCheckSum)
            {
                CorrectCache();
                return false;    
            }
            this->_cache.erase(0, currMsgLen);

            this->_SendPkToMessQueue(readData);

            if (this->_cache.size() < 4)
                break;
            memcpy(&currMsgLen, this->_cache.data(), 4);
            SocketBuf::VerifyCache(currMsgLen);
        }
        
        if(this->_cache.capacity() > this->_cache.size() * 20)
        {
            this->_cache.shrink_to_fit();
        }
    }
    return true;
}

bool SocketBuf::_SendPkToMessQueue(const std::string& data)
{
    MsgData sendData;
    
    std::pair<uint16_t, uint32_t> portAndIpI = net_data::DataPackPortAndIpToInt(this->portAndIp);
    sendData.type = E_WORK;
    sendData.fd = this->fd;
    sendData.ip = portAndIpI.second;
    sendData.port = portAndIpI.first;
    sendData.data = data;
    Pack::ApartPack(sendData.pack, data.data(), data.size());
    return global::g_queueWork.Push(sendData);
}

void SocketBuf::PrintfCache()
{
	std::lock_guard<std::mutex> lck(_mutexForRead);

    DEBUGLOG("fd: {}", this->fd);
    DEBUGLOG("portAndIp: {}", this->portAndIp);
    DEBUGLOG("_transactionCache: {}", this->_cache.c_str());
    DEBUGLOG("sendCache: {}", this->_sendCache.c_str());
}

std::string SocketBuf::GetSendMsg()
{
    std::lock_guard<std::mutex> lck(_mutexForSend);
    return this->_sendCache;
}

bool SocketBuf::IsSendCacheEmpty()
{
    std::lock_guard<std::mutex> lck(_mutexForSend);
    return this->_sendCache.size() == 0;
}

void SocketBuf::PushSendMsg(const std::string& data)
{
	std::lock_guard<std::mutex> lck(_mutexForSend);
    this->_sendCache += data;
}

void SocketBuf::PopNSendMsg(int n)
{
	std::lock_guard<std::mutex> lck(_mutexForSend);
    if((int)_sendCache.size() >= n)
    {
        this->_sendCache.erase(0 ,n);
    }
    else
    {
        this->_sendCache.erase(0 ,_sendCache.size());
    }
}

std::string BufferCrol::GetWriteBufferQueue(uint64_t portAndIp)
{
    auto itr = this->_BufferMap.find(portAndIp);
    if(itr == this->_BufferMap.end())
    {
        return g_emptystr;
    }
    return itr->second->GetSendMsg();
}

bool BufferCrol::AddReadBufferQueue(uint64_t portAndIp, char *buf, socklen_t len)
{
    auto port_ip = net_data::DataPackPortAndIpToString(portAndIp);
    if (buf == NULL || len == 0)
    {
        ERRORLOG("AddReadBufferQueue error buf == NULL or len == 0");
        return false;
    }

    std::map<uint64_t,std::shared_ptr<SocketBuf>>::iterator itr;
    {
        std::lock_guard<std::mutex> lck(_mutex);

        itr = this->_BufferMap.find(portAndIp);
        if (itr == this->_BufferMap.end())
        {
            DEBUGLOG("no key portAndIp is {}", portAndIp);
            return false;
        }
    }
	
    return itr->second->AddDataToReadBuf(buf, len);
}

bool BufferCrol::AddReadBufferQueue(uint32_t ip, uint16_t port, char *buf, socklen_t len)
{
    uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
    return this->AddReadBufferQueue(portAndIp, buf, len);
}

bool BufferCrol::AddBuffer(uint64_t portAndIp, const int fd)
{

    if (portAndIp == 0 || fd <= 0)
    {
        return false;
    }

	std::lock_guard<std::mutex> lck(_mutex);

    auto itr = this->_BufferMap.find(portAndIp);
    if (itr != this->_BufferMap.end())
    {
        return false;
    }
    std::shared_ptr<SocketBuf> tmp(new SocketBuf());
    tmp->fd = fd;
    tmp->portAndIp = portAndIp;
    this->_BufferMap[portAndIp] = tmp;

    return true;
}

bool BufferCrol::AddBuffer(uint32_t ip, uint16_t port, const int fd)
{
    uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
    return this->AddBuffer(portAndIp, fd);
}

void BufferCrol::PrintBufferes()
{
	std::lock_guard<std::mutex> lck(_mutex);
    DEBUGLOG("************************** PrintBufferes b **************************");
    for(auto i : this->_BufferMap)
    { 
        i.second->PrintfCache();
    }
    DEBUGLOG("************************** PrintBufferes e **************************");
}

bool BufferCrol::DeleteBuffer(uint32_t ip, uint16_t port)
{
    DEBUGLOG("DeleteBuffer ip:({}),port:({})",IpPort::IpSz(ip),port);
    uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
    return this->DeleteBuffer(portAndIp);
}
bool BufferCrol::DeleteBuffer(uint64_t portAndIp)
{
    if (portAndIp == 0)
    {
        return false;
    }

	std::lock_guard<std::mutex> lck(_mutex);

    auto itr = this->_BufferMap.find(portAndIp);
    if(itr != this->_BufferMap.end())
    {
        this->_BufferMap.erase(portAndIp);
        return true;
    }
    return false;
}

bool BufferCrol::DeleteBuffer(const int fd)
{
    if (fd <= 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lck(_mutex);

    auto iter = std::find_if(_BufferMap.begin(), _BufferMap.end(), [fd](auto & i){

        return i.second->fd == fd;

    });

    if (iter == _BufferMap.end())
    {
        DEBUGLOG("DeleteBuffer(const int fd) iter == _BufferMap.end()");
        return false;
    }
    
    std::pair<uint16_t, uint32_t> pair = net_data::DataPackPortAndIpToInt(iter->second->portAndIp);
    DEBUGLOG("DeleteBuffer(const int fd) ip:({}),port:({})",IpPort::IpSz(pair.first), pair.second);
    _BufferMap.erase(iter);
    return true;
}

void BufferCrol::PopNWriteBufferQueue(uint64_t portAndIp, int n)
{
    if (portAndIp == 0)
    {
        ERRORLOG("PopNWriteBufferQueue error portAndIp == 0 portAndIp: {}", portAndIp);
    }

	std::lock_guard<std::mutex> lck(_mutex);

    auto itr = this->_BufferMap.find(portAndIp);
    if(itr != this->_BufferMap.end())
    {
        itr->second->PopNSendMsg(n);
        return ;
    }
    DEBUGLOG(" portAndIp is not exist in map, portAndIp: {}", portAndIp);    
}

bool BufferCrol::IsExists(uint64_t portAndIp)
{
	std::lock_guard<std::mutex> lck(_mutex);

	auto itr = this->_BufferMap.find(portAndIp);
	if (itr == this->_BufferMap.end())
	{
		DEBUGLOG(RED "no key portAndIp is {}" RESET , portAndIp);
		return false;
	}
	return true;
}


bool BufferCrol::IsExists(std::string ip, uint16_t port)
{
	uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
	return IsExists(portAndIp);
}

bool BufferCrol::IsExists(uint32_t ip, uint16_t port)
{
	uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
	return IsExists(portAndIp);
}

std::shared_ptr<SocketBuf> BufferCrol::GetSocketBuf(uint32_t ip, uint16_t port)
{
    std::lock_guard<std::mutex> lck(_mutex);

    uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
    auto iter = _BufferMap.find(portAndIp);
    if (iter == _BufferMap.end())
    {
        return std::shared_ptr<SocketBuf>();
    }
    
    return iter->second;
}

bool BufferCrol::AddWritePack(uint64_t portAndIp, const std::string iosMsg)
{
	if (iosMsg.size() == 0)
	{
		ERRORLOG("add_write_buffer_queue error msg.size == 0");
		return false;
	}

	std::lock_guard<std::mutex> lck(_mutex);

	auto itr = this->_BufferMap.find(portAndIp);
	if (itr == this->_BufferMap.end())
	{
		DEBUGLOG("no key portAndIp is {}", portAndIp);
		return false;
	}
	itr->second->PushSendMsg(iosMsg);
	return true;
}

bool BufferCrol::AddWritePack(uint32_t ip, uint16_t port, const std::string iosMsg)
{
	uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
	AddWritePack(portAndIp, iosMsg);
	return true;
}


bool BufferCrol::IsCacheEmpty(uint32_t ip, uint16_t port)
{
    uint64_t portAndIp = net_data::DataPackPortAndIp(port, ip);
    auto itr = this->_BufferMap.find(portAndIp);
    if(itr == this->_BufferMap.end())
    {
        return true;
    }
    return itr->second->IsSendCacheEmpty();
}
