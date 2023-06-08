#ifndef _GLOBAL_H
#define _GLOBAL_H
#include <string>
#include <atomic>
namespace global{

    enum class BuildType
    {
        kBuildType_Primary,
        kBuildType_Test,
        kBuildType_Dev
    };
    
    // data
    #ifdef PRIMARYCHAIN
        const BuildType kBuildType = BuildType::kBuildType_Primary;
    #elif TESTCHAIN
        const BuildType kBuildType = BuildType::kBuildType_Test;
    #else // DEVCHAIN
        static const BuildType kBuildType = BuildType::kBuildType_Dev;
    #endif

    // version
    static const std::string kNetVersion = "27";
    static const std::string kLinuxCompatible = "0.27.0";
    static const std::string kWindowsCompatible = "0.27.0";
    static const std::string kIOSCompatible = "4.0.4";
    static const std::string kAndroidCompatible = "3.1.0";

    #if WINDOWS
        static const std::string kSystem = "2";
        static const std::string kCompatibleVersion = kWindowsCompatible;
    #else
        static const std::string kSystem = "1";
        static const std::string kCompatibleVersion = kLinuxCompatible;
    #endif 

    #ifdef PRIMARYCHAIN
        static const std::string kVersion = kSystem + "_" + kCompatibleVersion + "_p";
    #elif TESTCHAIN
        static const std::string kVersion = kSystem + "_" + kCompatibleVersion + "_t";
    #else // DEVCHAIN
        static const std::string kVersion = kSystem + "_" + kCompatibleVersion + "_d";
    #endif

    //thread pool 
    static const int ca_thread_number = 50;
    static const int net_thread_number = 50;
    static const int broadcast_thread_number = 18;
    static const int tx_thread_number = 512;
    static const int syncBlock_thread_number = 100;
    static const int saveBlock_thread_number = 512;

    static const int block_thread_number = 100;
    static const int work_thread_number = 200;
}

#endif // !_GLOBAL_H
