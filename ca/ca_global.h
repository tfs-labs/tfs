#ifndef __CA_GLOBAL_H__
#define __CA_GLOBAL_H__
#include <unordered_set>

#include "common/global.h"
#include "proto/ca_protomsg.pb.h"
#include "utils/CTimer.hpp"


namespace global{

    namespace ca{


        extern const std::string kInitAccountBase58Addr;
        extern const std::string kGenesisBlockRaw;
        extern const uint64_t kGenesisTime;
        extern const std::string kConfigJson;


        // consensus
        extern const int kConsensus;
        extern const int KRandomNodeGroup;


        // timer
        extern CTimer kBlockPoolTimer;
        extern CTimer kSeekBlockTimer;
        extern CTimer kDataBaseTimer;
        // mutex
        extern std::mutex kBonusMutex;
        extern std::mutex kInvestMutex;
        extern std::mutex kBurnMutex;

        // ca
        extern const uint64_t kDecimalNum ;
        extern const double   kFixDoubleMinPrecision ;
        extern const uint64_t kM2 ;
        extern const uint64_t kMinStakeAmt ;
        extern const uint64_t kMinInvestAmt ;
        extern const std::string kGenesisSign;
        extern const std::string kTxSign;
        extern const std::string kVirtualStakeAddr;
        extern const std::string kVirtualInvestAddr ;
        extern const std::string kVirtualBurnGasAddr;
        extern const std::string kStakeTypeNet;
        extern const std::string kInvestTypeNormal;
        extern const uint64_t kMinUnstakeHeight;
        extern const std::string kVirtualDeployContractAddr;

        extern const int KSign_node_threshold;
        extern const int kNeed_node_threshold ;

        extern const int TxTimeoutMin;

        extern const uint64_t KPackNodeThreshold;

        extern const double KBonusPumping;

        extern const uint32_t KVerifyFailThreshold;

        enum class StakeType
        {
            kStakeType_Unknown = 0,
            kStakeType_Node = 1
        };
        
        // Transacatione Type
        enum class TxType
        {
            kTxTypeGenesis = -1,
            kTxTypeUnknown, // unknown
            kTxTypeTx, //normal transaction
            kTxTypeStake, //stake
            kTxTypeUnstake, //unstake
            kTxTypeInvest, //invest
            kTxTypeDisinvest, //disinvest
            kTxTypeDeclaration, //declaration
            kTxTypeDeployContract,
            kTxTypeCallContract,
            kTxTypeBonus = 99//bonus
        };

        // Sync
        enum class SaveType
        {
            SyncNormal,
            SyncFromZero,
            Broadcast,
            Unknow
        };

        enum class BlockObtainMean
        {
            Normal,
            ByPreHash,
            ByUtxo
        };

        extern const uint64_t sum_hash_range;

        namespace DoubleSpend {
            const int SingleBlock = -66;
            const int DoubleBlock = -99;
        };

        // contract
        enum VmType {
            EVM,
            WASM
        };
        //test
        extern std::atomic<uint64_t> TxNumber;
    }
}


#endif