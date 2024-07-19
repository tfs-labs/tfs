#ifndef TFS_DB_REDIS_H_
#define TFS_DB_REDIS_H_

#include "utils/timer.hpp"
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <shared_mutex>
struct BonusAddrInfo
{
    bool dirty{true}; 
    uint64_t amount;//Amount invested
    std::set<std::string> utxos;//utxos invested
    uint64_t time;//Qualified investment time
};

class BonusAddrCache
{
public:
    /**
     * @brief       
     *
     * @param       bonusAddr:
     * @param       amount:
     * @return      int
     */
    int getAmount(const std::string& bonusAddr, uint64_t& amount);
    /**
     * @brief       
     *
     * @param       bonusAddr:
     * @return      uint64_t
     */
    uint64_t getTime(const std::string& bonusAddr);
    /**
     * @brief       
     *
     * @param       bonusAddr:
     * @param       dirty
     */
    void isDirty(const std::string& bonusAddr, bool dirty = true);
private:
    mutable std::shared_mutex BonusAddr_mutex;
    std::map<std::string, BonusAddrInfo> bonus_addr_;
};

#endif
