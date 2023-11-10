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
    static const std::string kNetVersion = "33";
    static const std::string kLinuxCompatible = "0.33.0";
    static const std::string kWindowsCompatible = "0.33.0";
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
    static const int kCaThreadNumber = 15;
    static const int kNetThreadNumber = 15;
    static const int kBroadcastThreadNumber = 10;
    static const int kTxThreadNumber = 50;
    static const int kSyncBlockThreadNumber = 25;
    static const int kSaveBlockThreadNumber = 50;

    static const int kBlockThreadNumber = 50;
    static const int kWorkThreadNumber = 50;
}

#endif // !_GLOBAL_H
