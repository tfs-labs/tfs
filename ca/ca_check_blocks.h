#ifndef _CA_CHECK_BLOCKS_H
#define _CA_CHECK_BLOCKS_H

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include "ca_sync_block.h"


class CheckBlocks
{
public:

    CheckBlocks();  


    void StartTimer();

	void StopTimer() { m_timer.Cancel(); }

	int GetPledgeAddr(DBReadWriter& db_reader, std::vector<std::string>& pledge_addr);

	int ByzantineSumHash(const std::vector<std::string> &send_node_ids, const std::vector<uint64_t>& send_heights,std::vector<uint64_t>& need_sync_heights);

	int DoNewSync(std::vector<std::string> send_node_ids, std::vector<std::string>& pledge_addr ,uint64_t self_node_height);

	std::pair<uint64_t, std::string> GetTempTopData();
private:
	void init();

    int ToCheck();

	void SetTempTopData(uint64_t height, std::string hash);

private:
    uint64_t 	m_top_block_height;		
    std::string m_top_block_hash;		
    CTimer 		m_timer;		        
	std::pair<uint64_t, std::string> m_temp_top_date; 
	std::mutex m_temp_top_date_mutex;	
	std::atomic<bool> m_check_runing;
};


void SendGetCheckSumHashReq(const std::string &node_id, const std::string &msg_id, uint64_t height);
void SendGetCheckSumHashAck(const std::string &node_id, const std::string &msg_id, uint64_t height);
int  HandleGetCheckSumHashReq(const std::shared_ptr<GetCheckSumHashReq> &msg, const MsgData &msgdata);
int  HandleGetCheckSumHashAck(const std::shared_ptr<GetCheckSumHashAck> &msg, const MsgData &msgdata);

#endif