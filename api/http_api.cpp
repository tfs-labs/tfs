#include "api/http_api.h"

#include <netdb.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cctype>
#include <ctime>
#include <exception>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "api/interface/rsa_text.h"
#include "api/interface/base64.h"
#include "api/interface/evm.h"
#include "api/interface/rpc_error.h"
#include "api/interface/sig.h"
#include "api/interface/tx.h"
#include "api/rpc_utils.h"
#include "api/rpc_utils.h"
#include <boost/math/constants/constants.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include "ca/ca.h"
#include "ca/advanced_menu.h"
#include "ca/global.h"
#include "ca/transaction.h"
#include "ca/txhelper.h"
#include "common/global.h"
#include "db/cache.h"
#include "db/db_api.h"
#include "interface.pb.h"
#include "logging.h"
#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/tmp_log.h"
#include "net/test.hpp"
#include "ca/test.h"
#include <utils/contract_utils.h>
#include <google/protobuf/util/json_util.h>
#include "google/protobuf/stubs/status.h"
#include "include/scope_guard.h"
#include "net/interface.h"
#include "net/global.h"
#include "net/httplib.h"
#include "net/api.h"
#include "net/peer_node.h"
#include "utils/json.hpp"
#include "utils/string_util.h"
#include "net/unregister_node.h"
#include "ca/algorithm.h"
#include "interface/tx.h"
#include "block.pb.h"
#include "ca_protomsg.pb.h"
#include "transaction.pb.h"
#include "utils/envelop.h"
#include "ca/interface.h"

#define CHECK_VALUE(value)\
        std::regex pattern("(^[1-9]\\d*\\.\\d+$|^0\\.\\d+$|^[1-9]\\d*$|^0$)");\
        if (!std::regex_match(value, pattern))\
        {\
            ack_t.ErrorCode="-3004";\
            ack_t.ErrorMessage=Sutil::Format("input value error:",value);\
            res.set_content(ack_t.paseToString(), "application/json");\
            return;\
        }

#define CHECK_PASE_REQ_T\
    std::string PaseRet = req_t.paseFromJson(req.body);\
    if(PaseRet != "OK") {\
        errorL("bad error pase fail");\
        ack_t.ErrorCode="9090";\
        ack_t.ErrorMessage=Sutil::Format("pase fail:%s",PaseRet);\
        res.set_content(ack_t.paseToString(), "application/json");\
        return ;\
    }

#define CHECK_PASE(target)\
    std::string PaseRet = target.paseFromJson(req.body);\
    if(PaseRet != "OK") {\
        errorL("bad error pase fail");\
        ack_t.ErrorCode="9090";\
        ack_t.ErrorMessage=Sutil::Format("pase fail:%s",PaseRet);\
        res.set_content(ack_t.paseToString(), "application/json");\
        return ;\
    }




#define CHECK_VALUE(value)\
        std::regex pattern("(^[1-9]\\d*\\.\\d+$|^0\\.\\d+$|^[1-9]\\d*$|^0$)");\
        if (!std::regex_match(value, pattern))\
        {\
            ack_t.ErrorCode="-3004";\
            ack_t.ErrorMessage=Sutil::Format("input value error:",value);\
            res.set_content(ack_t.paseToString(), "application/json");\
            return;\
        }

#define CHECK_PASE_REQ_T\
    std::string PaseRet = req_t.paseFromJson(req.body);\
    if(PaseRet != "OK") {\
        errorL("bad error pase fail");\
        ack_t.ErrorCode="9090";\
        ack_t.ErrorMessage=Sutil::Format("pase fail:%s",PaseRet);\
        res.set_content(ack_t.paseToString(), "application/json");\
        return ;\
    }

#define CHECK_PASE(target)\
    std::string PaseRet = target.paseFromJson(req.body);\
    if(PaseRet != "OK") {\
        errorL("bad error pase fail");\
        ack_t.ErrorCode="9090";\
        ack_t.ErrorMessage=Sutil::Format("pase fail:%s",PaseRet);\
        res.set_content(ack_t.paseToString(), "application/json");\
        return ;\
    }





void CaRegisterHttpCallbacks() {
#ifndef NDEBUG // The debug build compiles these functions
    HttpServer::RegisterCallback("/", ApiJsonRpc);
    HttpServer::RegisterCallback("/block", ApiPrintBlock);
    HttpServer::RegisterCallback("/info", ApiInfo);
    HttpServer::RegisterCallback("/get_block", ApiGetBlock);
    HttpServer::RegisterCallback("/log", ApiGetLogLine);
    HttpServer::RegisterCallback("/system_info", GetAllSystemInfo);
    HttpServer::RegisterCallback("/bandwidth", ApiGetRealBandwidth);
    HttpServer::RegisterCallback("/block_info", ApiGetBlockInfo);
    HttpServer::RegisterCallback("/get_tx_info", ApiGetTxInfo);

    HttpServer::RegisterCallback("/pub", ApiPub);
    HttpServer::RegisterCallback("/startautotx", ApiStartAutoTx);
    HttpServer::RegisterCallback("/endautotx", ApiEndAutotx);
    HttpServer::RegisterCallback("/autotxstatus", ApiStatusAutoTx);
    HttpServer::RegisterCallback("/filterheight", ApiFilterHeight);
    HttpServer::RegisterCallback("/echo", ApiTestEcho);
    HttpServer::RegisterCallback("/printblock", ApiPrintAllBlocks);
    HttpServer::RegisterCallback("/printCalcHash", ApiPrintCalc1000SumHash);
    HttpServer::RegisterCallback("/setCalcTopHeight", ApiSetCalc1000TopHeight);
    HttpServer::RegisterCallback("/blockc", ApiPrintContractBlock);
    HttpServer::RegisterCallback("/printHundredHash", ApiPrintHundredSumHash);


#endif // #ifndef NDEBUG
    HttpServer::RegisterCallback("/ip", ApiIp);

    HttpServer::RegisterCallback("/get_height", JsonRpcGetHeight);
    HttpServer::RegisterCallback("/get_balance", JsonrpcGetBalance);

    
    HttpServer::RegisterCallback("/deploy_contract_req", DeployContract);
    HttpServer::RegisterCallback("/call_contract_req",CallContract);

    HttpServer::RegisterCallback("/get_transaction_req", GetTransaction);
    HttpServer::RegisterCallback("/get_stakeutxo_req", GetStakeUtxo);
    HttpServer::RegisterCallback("/get_disinvestutxo_req", GetDisinvestUtxo);
    HttpServer::RegisterCallback("/get_stake_req", GetStake);
    HttpServer::RegisterCallback("/get_unstake_req", GetUnstake);
    HttpServer::RegisterCallback("/get_invest_req", GetInvest);
    HttpServer::RegisterCallback("/get_disinvest_req", GetDisinvest);
    HttpServer::RegisterCallback("/get_rates_info", ApiGetRatesInfo);

    HttpServer::RegisterCallback("/get_bonus_req", GetBonus);
    //SendContractMessage
    HttpServer::RegisterCallback("/SendContractMessage", SendContractMessage);
    HttpServer::RegisterCallback("/SendMessage", SendMessage);

    HttpServer::RegisterCallback("/get_rsa_pub_req", GetRsaPub);

    HttpServer::RegisterCallback("/GetIsOnChain", OldGetIsOnChain);

    HttpServer::RegisterCallback("/confirm_transaction", ConfirmTransaction);

    HttpServer::RegisterCallback("/deployers_req", GetDeployer);

    HttpServer::RegisterCallback("/deploy_utxo_req", GetDeployerUtxo);

    HttpServer::RegisterCallback("/get_restinverst_req", GetRestInvest);
    HttpServer::RegisterCallback("/GetAllStakeNodeListAck",GetAllStakeNodeListAcknowledge);

//    HttpServer::RegisterCallback("/tfscrpc",TfsRpcParse);

    HttpServer::Start();
}

static bool flag = true;
static bool autoTxFlag = true;
#define BUFFER_SIZE 1024

//  void get long line
static std::vector<std::string> SearchAfterTime(const string &fileName,
                                                  const string &timeStr,
                                                  int numLines) 
{

    std::vector<std::string> lines;
    regex pattern(timeStr);
    std::string cmd =
        "grep -A " + to_string(numLines) + " \"" + timeStr + "\" " + fileName;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            lines.push_back(buffer);
        }
        pclose(pipe);
    }
    return lines;
}

static std::vector<std::string> entryFileSystem(string path) 
{
    std::vector<std::string> FileNames;
    for (const auto &entry : std::filesystem::directory_iterator(path)) 
    {
        if (entry.is_regular_file()) 
        {
            FileNames.push_back(entry.path().filename().string());
        }
    }

    return FileNames;
}

static time_t ConvertStringToTime(std::string str) 
{
    std::regex re("\\d{4}-\\d{2}-\\d{2}");
    std::smatch match;
    time_t timestamp;
    if (std::regex_search(str, match, re)) 
    {
        std::tm date = {};
        std::stringstream ss(match.str());
        ss >> std::get_time(&date, "%Y-%m-%d");
        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&date));
        timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        tp.time_since_epoch())
                        .count();
    }
    return timestamp;
}

static string IsExistLog(std::vector<std::string> log, string time) 
{
    std::string logFile;
    auto thisTime = ConvertStringToTime(time);
    for (const auto &str : log) {
        if (thisTime == ConvertStringToTime(str)) {
            logFile = str;
        }
    }
    return logFile;
}

// void get cpu info
struct CPUStat 
{
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
};

static std::vector<CPUStat> GetCpuStats() {
    std::vector<CPUStat> cpuStats;
    std::ifstream statFile("/proc/stat");

    std::string line;
    while (std::getline(statFile, line)) 
    {
        if (line.compare(0, 3, "cpu") == 0) 
        {
            std::istringstream ss(line);

            std::string cpuLabel;
            CPUStat stat;
            ss >> cpuLabel >> stat.user >> stat.nice >> stat.system >>
                stat.idle >> stat.iowait >> stat.irq >> stat.softirq;

            cpuStats.push_back(stat);
        }
    }

    return cpuStats;
}

static double CalculateCpuUsage(const CPUStat &prev, const CPUStat &curr) 
{
    auto prevTotal = prev.user + prev.nice + prev.system + prev.idle +
                      prev.iowait + prev.irq + prev.softirq;
    auto currTotal = curr.user + curr.nice + curr.system + curr.idle +
                      curr.iowait + curr.irq + curr.softirq;

    auto totalDiff = currTotal - prevTotal;
    auto idleDiff = curr.idle - prev.idle;

    return (totalDiff - idleDiff) * 100.0 / totalDiff;
}

static std::string DoubleToStringWithPrecision(double value, int precision) 
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

struct ProcessInfo 
{
    std::string name;
    int openFiles;
    std::vector<std::string> fileList;
};

static std::map<int, ProcessInfo> GetAllProcessesInfo() 
{
    std::map<int, ProcessInfo> processesInfo;

    DIR *procDir = opendir("/proc");
    if (!procDir) {

        return processesInfo;
    }

    struct dirent *entry;
    while ((entry = readdir(procDir))) 
    {
        int pid = atoi(entry->d_name);
        if (pid > 0) 
        {
            std::string pidPath = "/proc/" + std::to_string(pid);
            std::string cmdlinePath = pidPath + "/cmdline";
            std::string fdDirPath = pidPath + "/fd";

            std::ifstream cmdlineFile(cmdlinePath);
            std::string processName;
            std::getline(cmdlineFile, processName);
            cmdlineFile.close();

            ProcessInfo processInfo;
            processInfo.name = processName;

            DIR *fdDir = opendir(fdDirPath.c_str());
            if (fdDir) {
                struct dirent *fdEntry;
                while ((fdEntry = readdir(fdDir))) {
                    if (fdEntry->d_type == DT_LNK) {
                        char buf[1024];
                        std::string fdPath =
                            fdDirPath + "/" + fdEntry->d_name;
                        ssize_t len =
                            readlink(fdPath.c_str(), buf, sizeof(buf) - 1);
                        if (len != -1) 
                        {
                            buf[len] = '\0';
                            processInfo.fileList.push_back(std::string(buf));
                        }
                    }
                }
                closedir(fdDir);
            }

            processInfo.openFiles = processInfo.fileList.size();
            processesInfo[pid] = processInfo;
        }
    }

    closedir(procDir);
    return processesInfo;
}

std::string Exec(const char *cmd) 
{
    char buffer[128];
    std::string result = "";
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}
static std::string GetOsVersion() 
{
    struct utsname buffer;
    std::string osRelease = Exec("cat /etc/os-release");
    std::string str;
    if (uname(&buffer) != 0) {
        return "Error getting OS version";
    }
    str = std::string(buffer.sysname) + " " + std::string(buffer.release) +
          " " + std::string(buffer.version);
    str += osRelease;
    return str;
}

std::vector<std::string> Paginate(const std::string &text,
                                  size_t linesPerPage) 
{
    std::vector<std::string> pages;
    std::stringstream textStream(text);
    std::string line;
    size_t lineNumber = 0;
    std::string currentPage;

    while (std::getline(textStream, line)) 
    {
        if (lineNumber >= linesPerPage) 
        {
            pages.push_back(currentPage);
            currentPage.clear();
            lineNumber = 0;
        }
        currentPage += line + "\n";
        lineNumber++;
    }

    if (!currentPage.empty()) 
    {
        pages.push_back(currentPage);
    }

    return pages;
}

struct NetStat 
{
    unsigned long long bytesReceived;
    unsigned long long bytesSent;
};

static NetStat GetNetStat(const std::string &interface) {
    NetStat netStat = {0, 0};
    std::ifstream netDevFile("/proc/net/dev");
    std::string line;

    while (std::getline(netDevFile, line)) 
    {
        if (line.find(interface) != std::string::npos) 
        {
            std::istringstream ss(line);
            std::string iface;
            ss >> iface >> netStat.bytesReceived;

            for (int i = 0; i < 7; ++i) 
            {
                unsigned long long tmp;
                ss >> tmp;
            }

            ss >> netStat.bytesSent;
            break;
        }
    }

    return netStat;
}

static std::string formatSpeed(double speed) 
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << speed << " Mbps";
    return ss.str();
}

static std::string GetMacAddress(const std::string &interface) 
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) 
    {
        return "";
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) 
    {
        close(sock);
        return "";
    }

    close(sock);
    char macAddress[18];
    snprintf(macAddress, sizeof(macAddress), "%02x:%02x:%02x:%02x:%02x:%02x",
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[0]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[1]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[2]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[3]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[4]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[5]));

    return macAddress;
}

static std::string GetNetworkInterfaceModel(const std::string &interface) 
{
    std::string modelPath = "/sys/class/net/" + interface + "/device/modalias";
    std::ifstream modelFile(modelPath);
    if (!modelFile.is_open()) 
    {
        return "";
    }

    std::string modelInfo;
    std::getline(modelFile, modelInfo);
    modelFile.close();

    return modelInfo;
}

// get all memory and disk

void GetMemOccupy(int lenNum, std::string &strMem) 
{
    strMem = "";
    FILE *fpMemInfo = fopen("/proc/meminfo", "r");
    if (NULL == fpMemInfo) 
    {
        strMem = "-1 meminfo fopen error";
        return;
    }

    int i = 0;
    int value = 0;
    char name[512];
    char line[512];
    int nFiledNumber = 2;
    int total = 0;
    int available = 0;
    while (fgets(line, sizeof(line) - 1, fpMemInfo)) 
    {
        if (sscanf(line, "%s%u", name, &value) != nFiledNumber) 
        {
            continue;
        }
        if (0 == strcmp(name, "MemTotal:")) 
        {
            total = value;
            strMem += "MemTotal:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "MemFree:")) 
        {
            strMem += "MemFree:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "MemAvailable:")) 
        {
            available = value;
            strMem += "MemAvailable:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "Buffers:")) 
        {
            strMem += "MemBuffers:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "Cached:")) 
        {
            strMem += "MemCached:\t" + std::to_string(value) + '\n';
        }
        else if (0 == strcmp(name, "SwapCached:")) 
        {
            strMem += "SwapCached:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "SwapTotal:")) 
        {
            strMem += "SwapTotal:\t" + std::to_string(value) + '\n';
        } 
        else if (0 == strcmp(name, "SwapFree:")) 
        {
            strMem += "SwapFree:\t" + std::to_string(value) + '\n';
        }

        if (++i == lenNum) 
        {
            break;
        }
    }
    strMem += "Memory usage:\t" +
              std::to_string(100.0 * (total - available) / total) + "%\n";
    fclose(fpMemInfo);
}

void GetDiskType(std::string &strMem) {
    ifstream rotational("/sys/block/sda/queue/rotational");
    if (rotational.is_open()) {
        int isRotational;
        rotational >> isRotational;
        strMem += "Disk type:\t";
        strMem += (isRotational ? "HDD" : "SSD");
        rotational.close();
    } 
    else 
    {
        strMem += "-1 Disk rotational open error";
    }
}

int GetFileLine() 
{
    FILE *fp;
    int flag = 0, count = 0;
    if ((fp = fopen("/proc/meminfo", "r")) == NULL)
        return -1;
    while (!feof(fp)) 
    {
        flag = fgetc(fp);
        if (flag == '\n')
            count++;
    }
    fclose(fp);
    return count;
}

void ApiPub(const Request &req, Response &res) 
{
    std::ostringstream oss;
    const int MaxInformationSize = 256;
    char buff[MaxInformationSize] = {};
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f == NULL) {
        DEBUGLOG("Failed to obtain main information ");
    } else {
        char readc;
        int i = 0;
        while (((readc = fgetc(f)) != EOF)) {
            if (readc == '\0') {
                buff[i++] = ' ';
            } else {
                buff[i++] = readc;
            }
        }
        fclose(f);
        char *fileName = strtok(buff, "\n");
        oss << "file_name:" << fileName << std::endl;
        oss << "==================================" << std::endl;
    }
    MagicSingleton<ProtobufDispatcher>::GetInstance()->TaskInfo(oss);
    oss << "g_queueRead:" << global::g_queueRead.msgQueue.size() << std::endl;
    oss << "g_queueWork:" << global::g_queueWork.msgQueue.size() << std::endl;
    oss << "g_queueWrite:" << global::g_queueWrite.msgQueue.size() << std::endl;
    oss << "\n" << std::endl;

    double total = .0f;
    uint64_t n64Count = 0;
    oss << "------------------------------------------" << std::endl;
    for (auto &item : global::g_reqCntMap) {
        total += (double)item.second.second; // data size
        oss.precision(3);                    // Keep 3 decimal places
        // Type of data		        Number of calls convert MB
        oss << item.first << ": " << item.second.first
            << " size: " << (double)item.second.second / 1024 / 1024 << " MB"
            << std::endl;
        n64Count += item.second.first;
    }
    oss << "------------------------------------------" << std::endl;
    oss << "Count: " << n64Count << "   Total: " << total / 1024 / 1024 << " MB"
        << std::endl; // Total size

    oss << std::endl;
    oss << std::endl;

    oss << "amount:" << std::endl;
    std::vector<std::string> baseList;

    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(baseList);
    for (auto &i : baseList) 
    {
        uint64_t amount = 0;
        GetBalanceByUtxo(i, amount);
        oss << i + ":" + std::to_string(amount) << std::endl;
    }

    oss << std::endl << std::endl;

    std::vector<Node> pubNodeList =
        MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    oss << "Public PeerNode size is: " << pubNodeList.size() << std::endl;
    oss << MagicSingleton<PeerNode>::GetInstance()->NodelistInfo(
        pubNodeList); //   Convert all public network node data to string for
                      //   saving
    res.set_content(oss.str(), "text/plain");
}

void ApiJsonRpc(const Request &req, Response &res) 
{
    nlohmann::json ret;
    ret["jsonrpc"] = "2.0";
    try {
        auto json = nlohmann::json::parse(req.body);

        std::string method = json["method"];

        auto p = HttpServer::rpcCbs.find(method);
        if (p == HttpServer::rpcCbs.end()) 
        {
            ret["error"]["code"] = -32601;
            ret["error"]["message"] = "Method not found";
            ret["id"] = "";
        } 
        else 
        {
            auto params = json["params"];
            ret = HttpServer::rpcCbs[method](params);
            try {
                ret["id"] = json["id"].get<int>();
            } 
            catch (const std::exception &e) 
            {
                ret["id"] = json["id"].get<std::string>();
            }
            ret["jsonrpc"] = "2.0";
        }
    } 
    catch (const std::exception &e) 
    {
        ret["error"]["code"] = -32700;
        ret["error"]["message"] = "Internal error";
        ret["id"] = "";
    }
    res.set_content(ret.dump(4), "application/json");
}

void ApiIp(const Request &req, Response &res) 
{
    std::ostringstream oss;
    std::map<uint64_t,std::map<std::string, int>> stakeResult;
    std::map<uint64_t,std::map<std::string, int>> unStakeResult;
    MagicSingleton<UnregisterNode>::GetInstance()->GetIpMap(stakeResult,unStakeResult);
    oss << "total size:" << stakeResult.size() + unStakeResult.size() << std::endl;
    oss << "stake size: " << stakeResult.size() << std::endl;
    for (auto &item : stakeResult) 
    {
        oss << "---------------------------------------------------------------"
               "----------------------"
            << std::endl;
        oss << MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(
                   item.first)
            << std::endl;
        for (auto i : item.second) 
        {
            oss << "Addr: " << i.first
                << "  Count: " << i.second << std::endl;
        }
    }

    oss << "unstake size: " << unStakeResult.size() << std::endl;
    for (auto &item : unStakeResult) 
    {
        oss << "---------------------------------------------------------------"
               "----------------------"
            << std::endl;
        oss << MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(
                   item.first)
            << std::endl;
        for (auto i : item.second) 
        {
            oss << "Addr: " << i.first
                << "  Count: " << i.second << std::endl;
        }
    }

    res.set_content(oss.str(), "text/plain");
}


void ApiPrintBlock(const Request &req, Response &res) 
{
    int num = 100;
    if (req.has_param("num")) {
        num = atol(req.get_param_value("num").c_str());
    }
    int startNum = 0;
    if (req.has_param("height")) {
        startNum = atol(req.get_param_value("height").c_str());
    }
    int hash = 0;
    if (req.has_param("hash")) {
        hash = atol(req.get_param_value("hash").c_str());
    }

    std::string str;

    if (hash) {
        str = PrintBlocksHash(num, req.has_param("pre_hash_flag"));
        res.set_content(str, "text/plain");
        return;
    }

    if (startNum == 0)
        str = PrintBlocks(num, req.has_param("pre_hash_flag"));
    else
        str = PrintRangeBlocks(startNum, num, req.has_param("pre_hash_flag"));

    res.set_content(str, "text/plain");
}
void ApiInfo(const Request &req, Response &res) 
{

    std::ostringstream oss;

    oss << "queue:" << std::endl;
    oss << "g_queueRead:" << global::g_queueRead.msgQueue.size() << std::endl;
    oss << "g_queueWork:" << global::g_queueWork.msgQueue.size() << std::endl;
    oss << "g_queueWrite:" << global::g_queueWrite.msgQueue.size() << std::endl;
    oss << "\n" << std::endl;

    oss << "amount:" << std::endl;
    std::vector<std::string> baseList;

    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(baseList);
    for (auto &i : baseList) {
        uint64_t amount = 0;
        GetBalanceByUtxo(i, amount);
        oss << i + ":" + std::to_string(amount) << std::endl;
    }
    oss << "\n" << std::endl;

    std::vector<Node> nodeList =
        MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    oss << "Public PeerNode size is: " << nodeList.size() << std::endl;
    oss << MagicSingleton<PeerNode>::GetInstance()->NodelistInfo(nodeList);

    oss << std::endl << std::endl; 

    res.set_content(oss.str(), "text/plain");
}

void ApiGetBlock(const Request &req, Response &res) {
    nlohmann::json block;
    nlohmann::json blocks;

    int top = 0;
    if (req.has_param("top")) {
        top = atol(req.get_param_value("top").c_str());
    }
    int num = 0;
    if (req.has_param("num")) {
        num = atol(req.get_param_value("num").c_str());
    }

    num = num > 500 ? 500 : num;

    if (top < 0 || num <= 0) {
        ERRORLOG("ApiGetBlock top < 0||num <= 0");
        return;
    }

    DBReader dbReader;
    uint64_t myTop = 0;
    dbReader.GetBlockTop(myTop);
    if (top > (int)myTop) {
        ERRORLOG("ApiGetBlock begin > myTop");
        return;
    }
    int k = 0;
    uint64_t countNum = top + num;
    if (countNum > myTop) {
        countNum = myTop;
    }
    for (auto i = top; i <= countNum; i++) {
        std::vector<std::string> blockHashs;

        if (dbReader.GetBlockHashsByBlockHeight(i, blockHashs) !=
            DBStatus::DB_SUCCESS) 
        {
            return;
        }

        for (auto hash : blockHashs) 
        {
            string strHeader;
            if (dbReader.GetBlockByBlockHash(hash, strHeader) !=
                DBStatus::DB_SUCCESS) 
            {
                return;
            }
            if(top <= global::ca::OldVersionSmartContractFailureHeight)
            {
                BlockInvert_V33_1(strHeader, block);
            }
            else
            {
                BlockInvert(strHeader, block);
            }
            blocks[k++] = block;
        }
    }
    std::string str = blocks.dump();
    res.set_content(str, "application/json");
}

void ApiFilterHeight(const Request &req, Response &res) 
{
    std::ostringstream oss;

    DBReader dbReader;
    uint64_t myTop = 0;
    dbReader.GetBlockTop(myTop);

    std::vector<Node> nodeList =
        MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
    std::vector<Node> filterNodes;

    for (auto &node : nodeList) 
    {
        if (node.height == myTop) {
            filterNodes.push_back(node);
        }
    }

    oss << "My Top : " << myTop << std::endl;
    oss << "Public PeerNode size is: " << filterNodes.size() << std::endl;
    oss << MagicSingleton<PeerNode>::GetInstance()->NodelistInfo(filterNodes);

    oss << std::endl << std::endl;

    res.set_content(oss.str(), "text/plain");
}

void ApiStartAutoTx(const Request &req, Response &res) {
    if (!autoTxFlag) {
        ApiEndAutoTxTest(res);
        autoTxFlag = true;
        return;
    }

    if (!flag) {
        std::cout << "flag =" << flag << std::endl;
        std::cout << "ApiStartAutoTx is going " << std::endl;
        return;
    }

    int Interval = 0;
    if (req.has_param("Interval")) 
    {
        Interval = atol(req.get_param_value("Interval").c_str());
    }
    int Interval_frequency = 0;
    if (req.has_param("Interval_frequency")) 
    {
        Interval_frequency =
            atol(req.get_param_value("Interval_frequency").c_str());
    }

    std::cout << "Interval =" << Interval << std::endl;
    std::cout << "Interval_frequency =" << Interval_frequency << std::endl;
    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::vector<std::string>::iterator it = std::find(
        addrs.begin(), addrs.end(),
        MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
    if (it != addrs.end()) 
    {
        addrs.erase(it);
    }

    flag = false;

    ThreadTest::SetStopTxFlag(flag);
    std::thread th(&ThreadTest::TestCreateTx, Interval_frequency, addrs,
                   Interval);
    th.detach();

    sleep(1);
    if (ApiStatusAutoTxTest(res)) 
    {
        autoTxFlag = false;
    }

    return;
}

void ApiStatusAutoTx(const Request &req, Response &res) 
{
    ApiStatusAutoTxTest(res);
}

void ApiEndAutotx(const Request &req, Response &res) 
{
    ApiEndAutoTxTest(res);
}

bool ApiStatusAutoTxTest(Response &res) 
{
    std::ostringstream oss;
    bool flag = false;
    bool stopTx = false;
    ThreadTest::GetStopTxFlag(flag);
    if (!flag) {
        oss << "auto tx is going :" << std::endl;
        stopTx = true;
    } else {
        oss << "auto tx is end!:" << std::endl;
    }
    res.set_content(oss.str(), "text/plain");
    return stopTx;
}

void ApiEndAutoTxTest(Response &res) {
    std::ostringstream oss;
    oss << "end auto tx:" << std::endl;

    flag = true;
    ThreadTest::SetStopTxFlag(flag);
    res.set_content(oss.str(), "text/plain");
}

// get log info
void ApiGetLogLine(const Request &req, Response &res) 
{
    std::string fileName;
    std::string str;
    std::string bigStr;
    int line = 500;
    std::string strTime = MagicSingleton<TimeUtil>::GetInstance()->FormatUTCTimestamp(MagicSingleton<TimeUtil>::GetInstance()->GetTimestamp());
    replace(strTime.begin(),strTime.end(),'/','-');
  
    std::string file1 = "tfs_";

    file1 += strTime.substr(0,10);
    file1 += ".log";
    replace(file1.begin(),file1.end(),'/','-');
   
    std::string path = "./logs";

    if (req.has_param("line")) {
        line = atol(req.get_param_value("line").c_str());
    }
 

    if (req.has_param("time")) {
        strTime = req.get_param_value("time").c_str();
        path = "./logs/" + IsExistLog(entryFileSystem(path), strTime);
    }
    else
    {
        path = "./logs/" + file1;
    }
    
    // test_time,file,and line message
    for (auto i : SearchAfterTime(path, strTime, line)) {
        bigStr += i + '\n';
    }

    res.set_content(bigStr, "text/plain");
}

// get cpu info
std::string ApiGetCpuInfo() 
{
    std::string sum;
    sum =
        "======================================================================"
        "=========";
    sum += "\n";
    sum += "get_cpu_info";
    sum += "\n";
    std::ifstream cpuinfoFile("/proc/cpuinfo");
    std::string line;
    int cpuCores = 0;
    std::string cpuModel;
    double cpuFrequency = 0;

    while (std::getline(cpuinfoFile, line)) 
    {
        if (line.compare(0, 9, "processor") == 0) 
        {
            cpuCores++;
        } else if (line.compare(0, 10, "model name") == 0) 
        {
            cpuModel = line.substr(line.find(":") + 2);
        } else if (line.compare(0, 7, "cpu MHz") == 0) 
        {
            cpuFrequency = std::stod(line.substr(line.find(":") + 2)) / 1000;
        }
    }

    auto prevStats = GetCpuStats();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto currStats = GetCpuStats();

    double totalUsage = 0;
    for (size_t i = 1; i < prevStats.size(); ++i) {
        totalUsage += CalculateCpuUsage(prevStats[i], currStats[i]);
    }

    double avgUsage = totalUsage / (prevStats.size() - 1);
    sum +=
        "CPU Usage: " + DoubleToStringWithPrecision(avgUsage, 1) + "%" + "\n";
    sum += "CPU Frequency: " + DoubleToStringWithPrecision(cpuFrequency, 3) +
           " GHZ" + "\n";
    sum += "CPU Model: " + cpuModel + "\n";
    sum += "CPU Cores: " + std::to_string(cpuCores);
    return sum;
}

std::string ApiGetSystemInfo() 
{
    std::string str;
    str =
        "======================================================================"
        "=========";
    str += "\n";
    str += "ApiGetSystemInfo";
    str += "\n";
    str += "OS Version: " + GetOsVersion() + "\n";
    return str;
}

std::string GetProcessInfo() 
{
    std::string str;
    str =
        "======================================================================"
        "=========";
    str += "\n";
    str += "GetProcessInfo";
    str += "\n";

    FILE *pipe = popen("ps -ef", "r");
    if (!pipe) {

        return "-1";
    }
    char buffer[BUFFER_SIZE];
    while (fgets(buffer, BUFFER_SIZE, pipe)) 
    {

        str += buffer;
        str += "\n";
    }
    pclose(pipe);
    return str;
}

std::string GetNetRate() 
{
    std::string interface = "eth0";
    auto prevStat = GetNetStat(interface);
    std::string str;
    str =
        "======================================================================"
        "=========";
    str += "\n";
    str += "GetNetRate";
    str += "\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto currStat = GetNetStat(interface);

    double downloadSpeed =
        (currStat.bytesReceived - prevStat.bytesReceived) * 8 / 1000.0 /
        1000.0;
    double uploadSpeed =
        (currStat.bytesSent - prevStat.bytesSent) * 8 / 1000.0 / 1000.0;

    std::string downloadSpeedStr = formatSpeed(downloadSpeed);
    std::string uploadSpeedStr = formatSpeed(uploadSpeed);

    str += "Download speed: " + downloadSpeedStr + "\n";
    str += "Upload speed: " + uploadSpeedStr + "\n";
    str += "Interface: " + interface + "\n";
    str += "MAC Address: " + GetMacAddress(interface);
    str += "Model Info: " + GetNetworkInterfaceModel(interface);
    prevStat = currStat;
    return str;
}


std::string ApiTime()
{
    std::string str;
	auto now = std::time(0);
    str += std::ctime(&now);	
   	auto now1 = std::chrono::system_clock::now();
    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now1.time_since_epoch()).count();
    auto stamp= std::to_string(nowUs) ;
    str  += stamp +"\n";

    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo *result;
    getaddrinfo(NULL, "0", &hints, &result);
    sockaddr_in *addr = (sockaddr_in *)result->ai_addr;
    // addr->sin_port htons(0);

    auto timeMs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    std::string netTime = std::to_string(timeMs) ; 
    str += netTime + "\n";

    if(timeMs >1000 + nowUs)
    {
        std::string cache = std::to_string(timeMs - nowUs); 
        str +=cache +"microsecond"+"slow" + "\n" ;
    }

    if(nowUs  > 1000 + timeMs)
    {
        std::string cache = std::to_string(timeMs - nowUs);
        str +=cache +"microsecond"+"fast" +"\n";
    }
    else 
    {
        str +="normal";
        str +="\n";
    }
    str +="time check======================" ;
    str +="\n";
    return str;
}


void GetAllSystemInfo(const Request &req, Response &res) 
{
    std::string outPut;
    std::string MemStr;
    int lenNum = GetFileLine();
    GetMemOccupy(lenNum, MemStr);
    GetDiskType(MemStr);
    outPut =
        "=================================================================="
        "=============";
    outPut += "\n";
    outPut += "GetMemOccupy";
    outPut += MemStr;
    outPut += "\n";

    outPut += ApiGetCpuInfo() + "\n";
    outPut += GetNetRate() + "\n";
    outPut += ApiGetSystemInfo() + "\n";
    outPut += ApiTime() + "\n";
    outPut += GetProcessInfo() + "\n";
    

    res.set_content(outPut, "text/plain");
}

// complete the net_real_width
void ApiGetRealBandwidth(const Request &req, Response &res) 
{
    std::string base58;
    if (req.has_param("base58")) 
    {
        base58 = req.get_param_value("base58").c_str();
    }

    if (base58.empty()) 
    {
        base58 = "base58 error";
        res.set_content(base58, "text/plain");
        return;
    }

    if (!CheckBase58Addr(base58)) 
    {
        std::string error = "Check Base58Addr failed!";
        res.set_content(error, "text/plain");
        return;
    }

    TestNetReq testReq;
    string data(10485760, '*');
    testReq.set_data(data);
    testReq.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
    testReq.set_hash(Getsha256hash(data));
    auto ms = chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch());
    testReq.set_time(ms.count());

    bool isRegister = MagicSingleton<netTest>::GetInstance()->GetSignal();
    if (!isRegister) 
    {
        std::string error = "Please wait. It's still being processed";
        res.set_content(error, "text/plain");
        return;
    }

    bool hasValue = MagicSingleton<netTest>::GetInstance()->GetFlag();
    if (hasValue) 
    {
        std::string speed;
        double time = MagicSingleton<netTest>::GetInstance()->GetTime();
        speed += "base58addresss:" + base58 + "\n" +
                 "bandwidth:" + std::to_string(time) + "MB/s";
        res.set_content(speed, "text/plain");
        return;
    }

    auto t = chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch());

    bool isSucceed = net_com::SendMessage(
        base58, testReq, net_com::Compress::kCompress_False,
        net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
    if (isSucceed == false) 
    {
        std::string error = "broadcast TestReq failed!";
        res.set_content(error, "text/plain");
        return;
    }

    MagicSingleton<netTest>::GetInstance()->NetTestRegister();
    res.set_content("message has sent: ", "text/plain");
    return;
}

void ApiGetBlockInfo(const Request &req, Response &res) 
{
    std::string blockHash;
    if (req.has_param("blockHash")) {
        blockHash = req.get_param_value("blockHash").c_str();
    }
    if (blockHash.empty()) {
        res.set_content("get blockHash error", "text/plain");
        return;
    }

    DBReader dbReader;
    std::string strHeader;

    if (DBStatus::DB_SUCCESS !=
        dbReader.GetBlockByBlockHash(blockHash, strHeader)) {
        res.set_content("blockHash error", "text/plain");
        return;
    }
    CBlock block;

    if (!block.ParseFromString(strHeader)) {
        res.set_content("block ParseFromString error", "text/plain");
        return;
    }

    std::ostringstream ss;
    PrintBlock(block, false, ss);

    res.set_content(ss.str(), "text/plain");
}

void ApiGetTxInfo(const Request &req, Response &res) 
{

    get_tx_info_req req_t;
    get_tx_info_ack ack_t;
    CHECK_PASE_REQ_T
    DBReader dbReader;
    std::string BlockHash;
    std::string strHeader;
    unsigned int BlockHeight;
    if (DBStatus::DB_SUCCESS !=
        dbReader.GetTransactionByHash(req_t.txhash, strHeader)) 
    {
        ack_t.ErrorCode = "-1";
        ack_t.ErrorMessage = "txhash error";
        res.set_content(ack_t.paseToString(), "application/json");
        return;
    }

    if (DBStatus::DB_SUCCESS !=
        dbReader.GetBlockHashByTransactionHash(req_t.txhash, BlockHash)) 
    {
        ack_t.ErrorCode = "-2";
        ack_t.ErrorMessage = "Block error";
        res.set_content(ack_t.paseToString(), "application/json");
        return;
    }

    if (DBStatus::DB_SUCCESS !=
        dbReader.GetBlockHeightByBlockHash(BlockHash, BlockHeight)) {
        ack_t.ErrorCode = "-3";
        ack_t.ErrorMessage = "Block error";
        res.set_content(ack_t.paseToString(), "application/json");
        return;
    }

    CTransaction tx;
    if (!tx.ParseFromString(strHeader)) {
        ack_t.ErrorCode = "-4";
        ack_t.ErrorMessage = "tx ParseFromString error";
        res.set_content(ack_t.paseToString(), "application/json");
        return;
    }

    std::string txStr;
    google::protobuf::util::Status status =
        google::protobuf::util::MessageToJsonString(tx, &txStr);
    ack_t.tx = txStr;
    ack_t.blockhash = BlockHash;
    ack_t.blockheight = BlockHeight;
    res.set_content(ack_t.paseToString(), "application/json");
}

bool tool_DeCode(const std::string &source, std::string &dest,
                 std::string &pubstr) {
    std::shared_ptr<Envelop> enve = MagicSingleton<Envelop>::GetInstance();
    bool bret = RSADeCode(source, enve.get(), pubstr, dest);
    if (bret == false) {
        return false;
    }
    return true;
}

void JsonRpcGetHeight(const Request &req, Response &res) {
    the_top ack_t;
    DBReader dbReader;
    uint64_t height = 0;
    dbReader.GetBlockTop(height);
    ack_t.height = std::to_string(height);
    ack_t.identity  = MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr();
    auto node       = MagicSingleton<PeerNode>::GetInstance()->GetSelfNode();
    ack_t.ip        = std::string(IpPort::IpSz(node.publicIp));
    res.set_content(ack_t.paseToString(), "application/json");
}

void JsonrpcGetBalance(const Request &req, Response &res) {
    balance_req req_t;
    balance_ack ack_t;
    CHECK_PASE_REQ_T
    std::string address = req_t.addr;

    if (!CheckBase58Addr(address)) {
        ack_t.ErrorCode = "-1";
        ack_t.ErrorMessage = "address is invalid ";
    }

    uint64_t balance = 0;
    if (GetBalanceByUtxo(address.c_str(), balance) != 0) {

        ack_t.ErrorCode = "-2";
        ack_t.ErrorMessage = "search balance failed";
    }
    ack_t.balance = std::to_string(balance);
    res.set_content(ack_t.paseToString(), "application/json");
}

void DeployContract(const Request &req, Response &res) {
    deploy_contract_req req_t;
    contract_ack ack_t;
    CHECK_PASE_REQ_T
    std::string ret;
    //add condition of height and version
	uint64_t selfNodeHeight = 0;
	DBReader dbReader;
	auto status = dbReader.GetBlockTop(selfNodeHeight);
	if (DBStatus::DB_SUCCESS != status)
	{
		ERRORLOG("Get block top error");
		return ;
	}
	if(selfNodeHeight <= global::ca::OldVersionSmartContractFailureHeight)
    {
        ret = RpcDeployContract_V33_1((void *)&req_t, &ack_t);
    }
    else
    {
        ret = RpcDeployContract((void *)&req_t, &ack_t);
    }
    
    if (ret != "0") {
        auto rpcError=GetRpcError();
        if(rpcError.first!="0"){
            ack_t.ErrorMessage = rpcError.second;
            ack_t.ErrorCode = rpcError.first;
        }else{
            ack_t.ErrorMessage = ret;
            ack_t.ErrorCode = "-1";
        }
    }
       
    ack_t.type = "deploy_contract_req";
    ack_t.ErrorCode = "0";

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetStakeUtxo(const Request &req, Response &res) {
    get_stakeutxo_ack ack_t;
    get_stakeutxo_req req_t;

    ack_t.ErrorCode = "0";
    CHECK_PASE_REQ_T

    DBReader dbReader;
    std::vector<string> utxos;
    dbReader.GetStakeAddressUtxo(req_t.fromAddr, utxos);
    std::reverse(utxos.begin(), utxos.end());
    std::map<std::string, uint64_t> outPut;

    for (auto &utxo : utxos) {
        std::string txRaw;
        dbReader.GetTransactionByHash(utxo, txRaw);
        CTransaction tx;
        tx.ParseFromString(txRaw);
        uint64_t value = 0;
        for (auto &vout : tx.utxo().vout()) {
            if (vout.addr() == global::ca::kVirtualStakeAddr) {
                value = vout.value();
            }
            outPut[utxo] = value;
        }
    }
    ack_t.utxos = outPut;
    res.set_content(ack_t.paseToString(), "application/json");
    DEBUGLOG("http_api.cpp:GetStakeUtxo ack_T.paseToString{}",ack_t.paseToString());
}

void GetDisinvestUtxo(const Request &req, Response &res) {
    get_disinvestutxo_ack ack_t;
    get_disinvestutxo_req req_t;

    ack_t.ErrorCode = "0";
    CHECK_PASE_REQ_T


    DBReader dbReader;
    std::vector<string> vecUtxos;
    dbReader.GetBonusAddrInvestUtxosByBonusAddr(req_t.toAddr, req_t.fromAddr,
                                                 vecUtxos);
    std::reverse(vecUtxos.begin(), vecUtxos.end());
    ack_t.utxos = vecUtxos;
    res.set_content(ack_t.paseToString(), "application/json");
    DEBUGLOG("http_api.cpp:GetDisinvestUtxo ack_T.paseToString{}",ack_t.paseToString());

}



void GetTransaction(const Request &req, Response &res) 
{
    tx_ack ack_t;
    tx_req req_t;
    ack_t.ErrorCode = "0";
    //req_t.paseFromJson(req.body);

    CHECK_PASE_REQ_T
   
    std::map<std::string, int64_t> toAddr;

    for (auto iter = req_t.toAddr.begin(); iter != req_t.toAddr.end(); iter++) {
       
        std::string inputValue=iter->second;
        CHECK_VALUE(inputValue);
        toAddr[iter->first] =
            (std::stod(iter->second) + global::ca::kFixDoubleMinPrecision) *
            global::ca::kDecimalNum;
    }

    std::string strError =
        TxHelper::ReplaceCreateTxTransaction(req_t.fromAddr, toAddr, &ack_t);
    if (strError != "0") 
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-5";
    }
  
    res.set_content(ack_t.paseToString(), "application/json");
}

void GetStake(const Request &req, Response &res) 
{
    get_stake_req req_t;
    tx_ack ack_t;

    CHECK_PASE_REQ_T

  

    std::string fromAddr = req_t.fromAddr;

    CHECK_VALUE(req_t.stake_amount);
    uint64_t stake_amount =
        (std::stod(req_t.stake_amount) + global::ca::kFixDoubleMinPrecision) *
        global::ca::kDecimalNum;
    int32_t pledgeType = std::stoll(req_t.PledgeType);

    std::regex bonus("^(5|6|7|8|9|1[0-9]|20)$"); // 5 - 20 
    if(!std::regex_match(req_t.pumpingPercentage,bonus))
    {
        ack_t.ErrorCode="-3005";
        ack_t.ErrorMessage=Sutil::Format("input pumping percentage error:",req_t.pumpingPercentage);
        res.set_content(ack_t.paseToString(), "application/json");
        return;
    }
    double pumpingPercentage = std::stod(req_t.pumpingPercentage) / 100;

    ack_t.type = "getStake_ack";
    ack_t.ErrorCode = "0";
    std::string strError = TxHelper::ReplaceCreateStakeTransaction(
        fromAddr, stake_amount, pledgeType, &ack_t, pumpingPercentage);
    if (strError != "0") {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetUnstake(const Request &req, Response &res) 
{
    get_unstake_req req_t;
    tx_ack ack_t;
    CHECK_PASE_REQ_T


    std::string fromAddr = req_t.fromAddr;
    std::string utxoHash = req_t.utxo_hash;

    ack_t.type = "getUnstake_ack";
    ack_t.ErrorCode = "0";

    std::string strError =
        TxHelper::ReplaceCreatUnstakeTransaction(fromAddr, utxoHash, &ack_t);
    if (strError != "0") {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetInvest(const Request &req, Response &res) {
    get_invest_req req_t;
    tx_ack ack_t;

    CHECK_PASE_REQ_T

   
    std::string fromAddr = req_t.fromAddr;
    std::string toAddr = req_t.toAddr;
    CHECK_VALUE(req_t.invest_amount);
    long double value = std::stold(req_t.invest_amount) * 10000;
    value = value * 10000;
    uint64_t investAmout =
        (std::stod(req_t.invest_amount) + global::ca::kFixDoubleMinPrecision) *
        global::ca::kDecimalNum;
    int32_t investType = std::stoll(req_t.investType);

    ack_t.type = "getInvest_ack";
    ack_t.ErrorCode = "0";

    std::string strError = TxHelper::ReplaceCreateInvestTransaction(
        fromAddr, toAddr, investAmout, investType, &ack_t);
    if (strError != "0") {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetDisinvest(const Request &req, Response &res) 
{
    get_disinvest_req req_t;
    tx_ack ack_t;

    CHECK_PASE_REQ_T    


    std::string fromAddr = req_t.fromAddr;
    std::string toAddr = req_t.toAddr;
    std::string utxoHash = req_t.utxo_hash;

    ack_t.type = "getDisInvest_ack";
    ack_t.ErrorCode = "0";

    std::string strError = TxHelper::ReplaceCreateDisinvestTransaction(
        fromAddr, toAddr, utxoHash, &ack_t);
    if (strError != "0") {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetDeclare(const Request &req, Response &res) 
{
    get_declare_req req_t;
    tx_ack ack_t;
    CHECK_PASE_REQ_T

   
    std::string fromAddr = req_t.fromAddr;
    std::string toAddr = req_t.toAddr;
    uint64_t amount = std::stoll(req_t.amount) * global::ca::kDecimalNum;

    std::string multiSignPub;
    Base64 base;
    multiSignPub = base.Decode((const char *)req_t.multiSignPub.c_str(),
                                req_t.multiSignPub.size());

    std::vector<std::string> signAddrList = req_t.signAddrList;
    uint64_t signThreshold = std::stoll(req_t.signThreshold);

    ack_t.type = "get_declare_req";
    ack_t.ErrorCode = "0";

    std::string strError = TxHelper::ReplaceCreateDeclareTransaction(
        fromAddr, toAddr, amount, multiSignPub, signAddrList, signThreshold,
        &ack_t);
    if (strError != "0") 
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetBonus(const Request &req, Response &res) 
{
    get_bonus_req req_t;
    tx_ack ack_t;
    CHECK_PASE_REQ_T

  
    std::string Addr = req_t.Addr;

    ack_t.type = "getDisInvest_ack";
    ack_t.ErrorCode = "0";

    std::string strError =
        TxHelper::ReplaceCreateBonusTransaction(Addr, &ack_t);
    if (strError != "0") {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");
}

void CallContract(const Request &req, Response &res) 
{
    RpcErrorClear();
    call_contract_req req_t;
    contract_ack ack_t;
    CHECK_PASE_REQ_T;
    
    ack_t.type = "call_contract_ack";
    ack_t.ErrorCode = "0";

    std::string ret;
    
    
    //add condition of height and version
	uint64_t selfNodeHeight = 0;
	DBReader dbReader;
	auto status = dbReader.GetBlockTop(selfNodeHeight);
	if (DBStatus::DB_SUCCESS != status)
	{
		ERRORLOG("Get block top error");
		return ;
	}
	if(selfNodeHeight <= global::ca::OldVersionSmartContractFailureHeight)
    {
        ret = RpcCallContract_V33_1((void *)&req_t, &ack_t);
    }
    else
    {
        ret = RpcCallContract((void *)&req_t, &ack_t);
    }
    

    if (ret != "0") {
        auto rpcError=GetRpcError();
        if(rpcError.first!="0"){
            ack_t.ErrorMessage = rpcError.second;
            ack_t.ErrorCode = rpcError.first;
        }else{
            ack_t.ErrorMessage = ret;
            ack_t.ErrorCode = "-1";
        }
    }
    res.set_content(ack_t.paseToString(), "application/json");
}

void CallContract_V33_1(const Request &req, Response &res) 
{
    RpcErrorClear();
    call_contract_req req_t;
    tx_ack ack_t;
    CHECK_PASE_REQ_T;

    std::string ret = RpcCallContract_V33_1((void *)&req_t, &ack_t);
    if (ret != "0") {
        auto rpcError=GetRpcError();
        if(rpcError.first!="0"){
            ack_t.ErrorMessage = rpcError.second;
            ack_t.ErrorCode = rpcError.first;
        }else{
            ack_t.ErrorMessage = ret;
            ack_t.ErrorCode = "-1";
        }
    }
    res.set_content(ack_t.paseToString(), "application/json");
}


void GetRsaPub(const Request &req, Response &res) 
{
    rsa_pubstr_ack ack_t;
    std::shared_ptr<Envelop> enve = MagicSingleton<Envelop>::GetInstance();
    std::string pubstr = enve->GetPubstr();
    Base64 base;
    ack_t.rsa_pubstr =
        base.Encode((const unsigned char *)pubstr.c_str(), pubstr.size());
    res.set_content(ack_t.paseToString(), "application/json");
}

void SendMessage(const Request &req, Response &res) 
{
    rpc_ack ack_t;
    tx_ack req_t;
    CHECK_PASE_REQ_T;

    CTransaction tx;
    Vrf info;
    int height;
    TxHelper::vrfAgentType type;
    google::protobuf::util::Status status =
        google::protobuf::util::JsonStringToMessage(req_t.txJson, &tx);
    status = google::protobuf::util::JsonStringToMessage(req_t.vrfJson, &info);
   
    height = std::stoi(req_t.height);
    type = (TxHelper::vrfAgentType)std::stoi(req_t.txType);
    std::string txHash = Getsha256hash(tx.SerializeAsString());
    ack_t.txhash = txHash;
    int ret = TxHelper::SendMessage(tx, height, info, type);
    ack_t.ErrorCode = std::to_string(ret);

    std::string back = ack_t.paseToString();

    res.set_content(back, "application/json");
}

void SendContractMessage(const Request & req,Response & res){
    contract_ack ack;

    rpc_ack ack_t;
    //debugL(req.body);
    if(ack.paseFromJson(req.body)!="OK"){
        errorL("pase fail");
        return;
    }
    ContractTxMsgReq ContractMsg;
    CTransaction tx;
    google::protobuf::util::JsonStringToMessage(ack.contractJs, &ContractMsg);
    google::protobuf::util::JsonStringToMessage(ack.txJs, &tx);

    std::string txHash = Getsha256hash(tx.SerializeAsString());
    tx.set_hash(txHash);
    
    ack_t.txhash=txHash;
    ack_t.type = "SendContractMessage";
    ack_t.ErrorCode = "0";

   TxMsgReq txReq= ContractMsg.txmsgreq();
   TxMsgInfo info=txReq.txmsginfo();
   info.set_tx(tx.SerializeAsString());
   txReq.clear_txmsginfo();
   TxMsgInfo *info_p=txReq.mutable_txmsginfo();
   info_p->CopyFrom(info);
   ContractMsg.clear_txmsgreq();
   TxMsgReq * txReq_p=ContractMsg.mutable_txmsgreq();
   txReq_p->CopyFrom(txReq);
    auto msg = make_shared<ContractTxMsgReq>(ContractMsg);
   DropCallShippingTx(msg,tx);
    res.set_content(ack_t.paseToString(), "application/json");
}
void OldGetIsOnChain(const Request &req, Response &res) 
{
    get_isonchain_req req_j;
    get_isonchain_ack ack_t;
    CHECK_PASE(req_j)

    IsOnChainAck ack;
    std::shared_ptr<IsOnChainReq> req_t = std::make_shared<IsOnChainReq>();
    req_t->add_txhash(req_j.txhash);
    req_t->set_version(global::kVersion);
    auto currentTime =
        MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    req_t->set_time(currentTime);

    int ret = 0;
    ret = OldSendCheckTxReq(req_t, ack);
    if(ret != 0)
    {
        ERRORLOG("sussize is empty{}",ret);
        ack_t.ErrorMessage = "initial node list is empty";
        ack_t.ErrorCode = std::to_string(ret);
        res.set_content(ack_t.paseToString(),"application/json");
        return;
    }
    std::string debugValue;
    google::protobuf::util::Status status =
        google::protobuf::util::MessageToJsonString(ack, &debugValue);
    DEBUGLOG("http_api.cpp:OldGetIsOnChain ack_T.paseToString {}",debugValue);

   
    auto sus = ack.percentage();
    auto susSize = sus.size();
    if(susSize == 0)
    {
        ERRORLOG("sussize is empty{}",susSize);
        ack_t.ErrorMessage = "initial node list is empty";
        ack_t.ErrorCode = "-5";
        res.set_content(ack_t.paseToString(),"application/json");
        return;
    }
    auto rate = sus.at(0);
    ack_t.txhash = rate.hash();
    ack_t.pro = std::to_string(rate.rate());
    ack_t.ErrorCode = std::to_string(ret);
    res.set_content(ack_t.paseToString(), "application/json");
}

void ConfirmTransaction(const Request &req, Response &res) 
{
    confirm_transaction_req req_j;
    confirm_transaction_ack ack_t;
     CHECK_PASE(req_j)

    uint64_t height = std::stoll(req_j.height);
    ConfirmTransactionAck ack;
    std::shared_ptr<ConfirmTransactionReq> req_t = std::make_shared<ConfirmTransactionReq>();
    req_t->add_txhash(req_j.txhash);
    req_t->set_version(global::kVersion);
    req_t->set_height(height);
    auto currentTime =
        MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
    req_t->set_time(currentTime);

    int ret = 0;
    ret = SendConfirmTransactionReq(req_t, ack);
    if(ret != 0)
    {
        ERRORLOG("sussize is empty{}",ret);
        ack_t.ErrorMessage = "initial node list is empty";
        ack_t.ErrorCode = std::to_string(ret);
        res.set_content(ack_t.paseToString(),"application/json");
        return;
    }
    std::string debugValue;
    google::protobuf::util::Status status =
        google::protobuf::util::MessageToJsonString(ack, &debugValue);
     DEBUGLOG("http_api.cpp:ConfirmTransaction ack_T.paseToString {}",debugValue);

   
    auto sus = ack.percentage();
    auto susSize = sus.size();
    if(susSize == 0)
    {
        ERRORLOG("sussize is empty{}",susSize);
        ack_t.ErrorMessage = "initial node list is empty";
        ack_t.ErrorCode = "-5";
        res.set_content(ack_t.paseToString(),"application/json");
        return;
    }
    auto rate = sus.at(0);
    ack_t.txhash = rate.hash();
    ack_t.percent = std::to_string(rate.rate());
    ack_t.ErrorCode = std::to_string(ret);
    ack_t.receivedsize = std::to_string(ack.received_size());
    ack_t.sendsize = std::to_string(ack.send_size());
    res.set_content(ack_t.paseToString(), "application/json");
}

void GetDeployer(const Request &req, Response &res) 
{
    deployers_ack ack_t;
    DBReader dataReader;
    std::vector<std::string> vecDeployers;
    dataReader.GetAllDeployerAddr(vecDeployers);
    std::cout << "=====================deployers====================="
              << std::endl;
    for (auto &deployer : vecDeployers) {
        std::cout << "deployer: " << deployer << std::endl;
    }
    ack_t.deployers = vecDeployers;

    res.set_content(ack_t.paseToString(), "application/json");
}

void GetDeployerUtxo(const Request &req, Response &res) 
{
    deploy_utxo_req req_t;
    deploy_utxo_ack ack_t;
    CHECK_PASE_REQ_T

    DBReader dataReader;
    std::vector<std::string> vecDeployUtxos;
    dataReader.GetDeployUtxoByDeployerAddr(req_t.addr, vecDeployUtxos);
    std::cout << "=====================deployed utxos====================="
              << std::endl;
    for (auto &deploy_utxo : vecDeployUtxos) {
        std::cout << "deployed utxo: " << deploy_utxo << std::endl;
    }
    std::cout << "=====================deployed utxos====================="
              << std::endl;
    ack_t.utxos = vecDeployUtxos;
    res.set_content(ack_t.paseToString(), "application/json");
}

void GetRestInvest(const Request &req, Response &res) 
{
    GetRestInvestAmountAck ack;
    get_restinverst_req req_t;
    get_restinverst_ack ack_t;
     CHECK_PASE_REQ_T
    std::shared_ptr<GetRestInvestAmountReq> req_c =
        std::make_shared<GetRestInvestAmountReq>();
    req_c->set_base58(req_t.addr);
    req_c->set_version(global::kVersion);
    int ret = GetRestInvestAmountReqImpl(req_c, ack);
   
    ack_t.addr = ack.base58();
    ack_t.amount = std::to_string(ack.amount());

    res.set_content(ack_t.paseToString(), "application/json");
}


void GetAllStakeNodeListAcknowledge(const Request & req,Response & res){
    int ret;
   GetAllStakeNodeListAck  ack;
   std::shared_ptr<GetAllStakeNodeListReq> req_t;
    ret = GetAllStakeNodeListReqImpl(req_t, ack);
    if(ret!=0){
        ack.set_code(ret);
    }
    std::string jsonstr;
    google::protobuf::util::Status status =
        google::protobuf::util::MessageToJsonString(ack, &jsonstr);
       if(!status.ok()){
            errorL("protobuff to json fail");
            jsonstr="protobuff to json fail";
       }
    res.set_content(jsonstr.c_str(),"application/json");
}


void ApiTestEcho(const Request & req, Response & res)
{
    std::string order;
    if (req.has_param("order")) 
    {
        order = req.get_param_value("order").c_str();
    }

    if(order == "send")
    {
        std::string message = std::to_string(MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp());

        EchoReq echoReq;
        echoReq.set_id(MagicSingleton<PeerNode>::GetInstance()->GetSelfId());
        echoReq.set_message(message);
        bool isSucceed = net_com::BroadCastMessage(echoReq, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
        if (isSucceed == false)
        {
            res.set_content(":broadcast EchoReq failed!", "text/plain");
            return;
        }

        res.set_content("EchoReq success!", "text/plain");
        return;
    }

    if(order == "print")
    {
        std::map<std::string, std::vector<std::string>>  echoCatch = MagicSingleton<echoTest>::GetInstance()->GetEchoCatch();
        std::ostringstream oss;
        for(const auto& echo: echoCatch)
        {

            oss << "time: " << echo.first << "\tnum: " << echo.second.size() << std::endl;
            for(const auto& ip: echo.second)
            {
                oss << "IP: " << ip << std::endl;
            }
            oss << "---------------------------------------------------------------------" << std::endl;
        }
        res.set_content(oss.str(), "text/plain");
        return;
    }

    if(order == "clear")
    {
        uint64_t time = 0;
        if (req.has_param("time")) {
            time = atol(req.get_param_value("time").c_str());
        }
        
        if(MagicSingleton<echoTest>::GetInstance()->DeleteEchoCatch(std::to_string(time)))
        {
            res.set_content("delete success", "text/plain");
            return;
        }

        res.set_content("time error", "text/plain");
        return;
    }

    if(order == "all_clear")
    {
        MagicSingleton<echoTest>::GetInstance()->AllClear();
        res.set_content("AllClear success", "text/plain");
    }

    return;
}

uint64_t GetCirculationBeforeYesterday(uint64_t curTime) {
  DBReadWriter dbWriter;
  std::vector<std::string> utxos;
  std::string strTx;
  CTransaction tx;
  {
    uint64_t Period =
        MagicSingleton<TimeUtil>::GetInstance()->GetPeriod(curTime);
    auto ret = dbWriter.GetBonusUtxoByPeriod(Period, utxos);
    if (DBStatus::DB_SUCCESS != ret && DBStatus::DB_NOT_FOUND != ret) 
    {
      return -2;
    }
  }
  uint64_t claimVoutAmount = 0;
  uint64_t totalClaimDay = 0;
  for (auto utxo = utxos.rbegin(); utxo != utxos.rend(); utxo++) 
  {
    if (dbWriter.GetTransactionByHash(*utxo, strTx) != DBStatus::DB_SUCCESS) 
    {
      return -3;
    }
    if (!tx.ParseFromString(strTx)) 
    {
      return -4;
    }
    uint64_t claimAmount = 0;
    if ((global::ca::TxType)tx.txtype() != global::ca::TxType::kTxTypeTx) 
    {
      nlohmann::json dataJson = nlohmann::json::parse(tx.data());
      nlohmann::json tx_info = dataJson["TxInfo"].get<nlohmann::json>();
      tx_info["BonusAmount"].get_to(claimAmount);
      totalClaimDay += claimAmount;
    }
  }

  return totalClaimDay;
}

void ApiGetRatesInfo(const Request &req, Response &res) 
{
  typedef boost::multiprecision::cpp_bin_float_50 cpp_bin_float;
  uint64_t curTime =
      MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
  uint64_t totalCirculationYesterday = 0;
  uint64_t totalInvestYesterday = 0;
  uint64_t totalCirculation = 0;
  DBReadWriter dbWriter;
  nlohmann::json jsObject;
  int ret = 0;

  do {
    ret = GetTotalCirculationYesterday(curTime, totalCirculationYesterday);
    if (ret < 0) {
      jsObject["Code"] = std::to_string(ret -= 100);
      jsObject["Message"] = "GetTotalCirculation error";
      break;
    }

    uint64_t totalBrunYesterday = 0;
    ret = GetTotalBurnYesterday(curTime, totalBrunYesterday);
    if (ret < 0) {
      jsObject["Code"] = std::to_string(ret -= 200);
      jsObject["Message"] = "GetTotalBurn error";
      break;
    }

    totalCirculationYesterday = totalCirculationYesterday - totalBrunYesterday;
    uint64_t claimReward = GetCirculationBeforeYesterday(curTime);
    jsObject["LastClaimReward"] = std::to_string(claimReward);
    jsObject["TotalCirculatingSupply"] = std::to_string(totalCirculationYesterday);
    jsObject["TotalBurn"] = std::to_string(totalBrunYesterday);
    ret = GetTotalInvestmentYesterday(curTime, totalInvestYesterday);
    if (ret < 0) {
      jsObject["Code"] = std::to_string(ret -= 400);
      jsObject["Message"] = "GetTotalInvestment error";
      break;
    }
    jsObject["TotalStaked"] = std::to_string(totalInvestYesterday);

    uint64_t StakeRate =
        ((double)totalInvestYesterday / totalCirculationYesterday + 0.005) *
        100;
    if (StakeRate <= 35) 
    {
      StakeRate = 35;
    }
    else if (StakeRate >= 90) 
    {
      StakeRate = 90;
    }

    jsObject["StakingRate"] = std::to_string((double)totalInvestYesterday /
                                             totalCirculationYesterday);

    double inflationRate = .0f;
    ret =
        ca_algorithm::GetInflationRate(curTime, StakeRate - 1, inflationRate);
    if (ret < 0) {
      jsObject["Code"] = std::to_string(ret -= 500);
      jsObject["Message"] = "GetInflationRate error";
      break;
    }

    std::stringstream ss;
    ss << std::setprecision(8) << inflationRate;
    std::string inflationRateStr = ss.str();
    ss.str(std::string());
    ss.clear();
    ss << std::setprecision(2) << (StakeRate / 100.0);
    std::string stakeRateStr = ss.str();
    cpp_bin_float earningRate0 =
        static_cast<cpp_bin_float>(std::to_string(global::ca::kDecimalNum)) *
        (static_cast<cpp_bin_float>(inflationRateStr) /
         static_cast<cpp_bin_float>(stakeRateStr));
    ss.str(std::string());
    ss.clear();
    ss << std::setprecision(8) << earningRate0;

    uint64_t earningRate1 = std::stoi(ss.str());

    
    double earningRate2 = (double)earningRate1 / global::ca::kDecimalNum;
    if (earningRate2 > 0.22) 
    {
      jsObject["Code"] = std::to_string(-5);
      jsObject["Message"] = "earningRate2 error";
      break;
    }

    jsObject["CurrentAPR"] = std::to_string(earningRate2);

    jsObject["Code"] = "0";
    jsObject["Message"] = "";

  } while (0);

  res.set_content(jsObject.dump(), "application/json");
}

void ApiPrintAllBlocks(const Request &req,Response &res)
{
    int startHeight = 1;
    if (req.has_param("startHeight")) {
        startHeight = atol(req.get_param_value("startHeight").c_str());
    }
    if(startHeight <= 0)
    {
        res.set_content("error startHeight <= 0", "text/plain");
        return;
    }

    int endHeight = 0;
    if (req.has_param("endHeight")) {
        endHeight = atol(req.get_param_value("endHeight").c_str());
    }

    uint64_t selfNodeHeight = 0;
    DBReader dbReader;
    auto status = dbReader.GetBlockTop(selfNodeHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        res.set_content("GetBlockTop error", "text/plain");
        return;
    }

    if(endHeight > selfNodeHeight)
    {
        res.set_content("endHeight > selfNodeHeight", "text/plain");
        return;
    }

    std::stringstream oss;
    oss << "block_hash_" << startHeight << "_" << endHeight << ".txt";
    ofstream fout(oss.str(), std::ios::out);
    for(int i = startHeight; i <= endHeight; ++i)
    {
        std::vector<std::string> selfBlockHashes;
        if (DBStatus::DB_SUCCESS != dbReader.GetBlockHashesByBlockHeight(i, i, selfBlockHashes))
        {
            res.set_content("GetBlockHashesByBlockHeight error", "text/plain");
            return;
        }
        std::sort(selfBlockHashes.begin(), selfBlockHashes.end());
        fout << "block height: " << i << "\tblock size: " << selfBlockHashes.size() << std::endl; 
        for(const auto& hash: selfBlockHashes)
        {
            std::string strHeader;
            if (DBStatus::DB_SUCCESS != dbReader.GetBlockByBlockHash(hash, strHeader)) 
            {
                res.set_content("GetBlockByBlockHash error", "text/plain");
                return;
            }

            CBlock block;
            if(!block.ParseFromString(strHeader))
            {
                res.set_content("ParseFromString error", "text/plain");
                return;
            }
            fout << block.hash() << std::endl;
        }
        fout << "==============================================\n\n";

    }

    res.set_content("print success", "text/plain");

}


#ifdef tfsrpc

bool CheckRpcField(std::vector<std::tuple<std::string, std::string,int>> cFeild,nlohmann::json & obj)
{
    std::string field;
    std::string value;
    for(auto & f:cFeild)
    {
        field=std::get<0>(f);
        value=std::get<1>(f);
        if(obj.contains(field)){
            if(obj[field]!= value){
                SetError(Sutil::Format("rpc error field:%s is not equal %s", field,value));
                SetErrorNum(std::get<2>(f));
                return false;
            }
        }else{
            SetError(Sutil::Format("rpc error field:%s is not exists",field));
            SetErrorNum(std::get<2>(f));
            return false;
        }
    }
    return true;
}

// void TfsRpcParse(const Request &req,Response &res){

//   //  debugL("comm");
//     std::string ret;
//     std::vector<std::string> fields={"tfsrpc",
//                                      "method",
//                                      "body",
//                                      "message",
//                                      "error"};
//     nlohmann::json obj;
//     nlohmann::json jsonRet=CreateJsonObj
//         <std::string,std::string,std::string,std::string,std::string>(fields,"0.0.1","unkown","","","0");
//     try{
//         obj=nlohmann::json::parse(req.body);
//     }catch(std::exception & e){
//         ret=Sutil::Format("TfsRpcParse error:%s", e.what());
//         jsonRet["message"]=ret;
//         jsonRet["error"]=std::to_string((int)RPCERROR::PASE_ERROR);
//         res.set_content(jsonRet.dump(), "application/json");
//         return ;
//     }

//    if(!CheckRpcField({{"tfsrpc","0.0.1",(int)RPCERROR::FIELD_REEOR}},obj)){
//         jsonRet["message"]=GetError();
//         jsonRet["error"]=std::to_string(GetErrorNum());
//         res.set_content(jsonRet.dump(), "application/json");
//         return ;
//    }

//     nlohmann::json params=obj["params"];
//     std::string method_string=obj["method"];
//     debugL("method:%s",method_string);

//     debugL("body :%s",req.body);

//     auto clearHash =[](CTransaction & tx) {
//             tx.clear_hash();
//             std::set<std::string> Miset;
//             Base64 base;
//             auto txUtxo = tx.mutable_utxo();
//             int index = 0;
//             auto vin = txUtxo->mutable_vin();
//             for (auto &owner : txUtxo->owner()) {

//                 Miset.insert(owner);
//                 auto vin_t = vin->Mutable(index);
//                 vin_t->clear_vinsign();
//                 index++;
//             }
//             for (auto &owner : Miset) {
//                 CTxUtxo *txUtxo = tx.mutable_utxo();
//                 CTxUtxo copyTxUtxo = *txUtxo;
//                 copyTxUtxo.clear_multisign();
//                 txUtxo->clear_multisign();
//             }
//     };

//     auto transaction_func =
//         [](const nlohmann::json &obj_func, nlohmann::json &out_json) -> bool {
        
//         tx_ack ack_t;
//         std::tuple<std::string,std::string,std::string> target=
//         GetRpcParams<std::string,std::string,std::string>(obj_func);

//         std::string FromAddr=std::get<0>(target);
//         std::string ToAddr=std::get<1>(target);
//         std::string value=std::get<2>(target);
//         std::regex pattern("(^[1-9]\\d*\\.\\d+$|^0\\.\\d+$|^[1-9]\\d*$|^0$)");\
//         if (!std::regex_match(value, pattern)){
//             out_json["message"]=Sutil::Format("field:%s is no a number", "value");
//             out_json["error"]=(int)RPCERROR::NUMBER_ERROR;
//             return false;
//         }
//         std::map<std::string, int64_t> toAddr;
//         std::vector<std::string> _fromaddr;
//         _fromaddr.push_back(FromAddr);
    
       
//         toAddr[ToAddr] =
//             (std::stod(value) + global::ca::kFixDoubleMinPrecision) *
//             global::ca::kDecimalNum;

//         std::string strError =
//             TxHelper::ReplaceCreateTxTransaction(_fromaddr, toAddr, &ack_t);
//             debugL("strError:%s",strError);
//         if (strError != "0") {
//             out_json["message"]=strError;
//             out_json["error"]=std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"] = ack_t.paseToString();
//         out_json["error"] = "0";
//         debugL("fainsh");
//         return true;
//     };

//     auto stake_func=[](const nlohmann::json & obj_func,nlohmann::json & out_json)->bool
//     {
//         debugL("stake_func");
//         tx_ack ack_t;
//        // std::tuple<std::string,std::string,std::string> params=GetRpcParams<std::string,std::string,std::string>(obj_func);
//         std::string FromAddr=obj_func[0];
//         std::string stake_amount=obj_func[1];
//         std::string strCommission = obj_func[2];
        
//         debugL("FromAddr:%s",FromAddr);
//         debugL("stake_amount:%s",stake_amount);
//         int type=0;
//         std::regex pattern("(^[1-9]\\d*\\.\\d+$|^0\\.\\d+$|^[1-9]\\d*$|^0$)");\
//         if (!std::regex_match(stake_amount, pattern)){
//             out_json["message"]=Sutil::Format("field:%s is no a number", "stake_amount");
//             out_json["error"]=(int)RPCERROR::NUMBER_ERROR;
//             debugL("fail");
//             return false;
//         }
//         uint64_t stake_amount_s =
//         (std::stod(stake_amount) + global::ca::kFixDoubleMinPrecision) *
//         global::ca::kDecimalNum;

//         double commission = std::stold(strCommission);
//         debugL(stake_amount_s);
//          std::string strError = TxHelper::ReplaceCreateStakeTransaction(FromAddr, stake_amount_s, 0, &ack_t, commission);
//          debugL("strError:%s",strError);
//         if (strError != "0") {
//             out_json["message"]=strError;
//             out_json["error"]=std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"]=ack_t.paseToString();
//         out_json["error"]="0";
//         return true;

//     };

//     auto unstake=[](const nlohmann::json & obj_func,nlohmann::json & out_json)->bool
//     {
//         tx_ack ack_t;
//         std::tuple<std::string,std::string> params=GetRpcParams<std::string,std::string>(obj_func);
//         std::string addr=std::get<0>(params);
//         std::string utxohash=std::get<1>(params);

//         std::string strError =
//         TxHelper::ReplaceCreatUnstakeTransaction(addr, utxohash, &ack_t);
//         if (strError != "0") {
//             out_json["message"]=strError;
//             out_json["error"]=std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"]=ack_t.paseToString();
//         out_json["error"]="0";
//         return true;
//     };

//     auto invest_func=[](const nlohmann::json & obj_func,nlohmann::json & out_json)->bool
//     {
//         tx_ack ack_t;
//         std::tuple<std::string,std::string,std::string> params=GetRpcParams<std::string,std::string,std::string>(obj_func);
//         std::string fAddr=std::get<0>(params);
//         std::string tAddr=std::get<1>(params);
//         std::string value=std::get<2>(params);
//         debugL("f:%s,t:%s,v:%s",fAddr,tAddr,value);

//         std::regex pattern("(^[1-9]\\d*\\.\\d+$|^0\\.\\d+$|^[1-9]\\d*$|^0$)");\
//         if (!std::regex_match(value, pattern)){
//             out_json["message"]=Sutil::Format("field:%s is no a number", "stake_amount");
//             out_json["error"]=(int)RPCERROR::NUMBER_ERROR;
//             return false;
//         }

        
//         uint64_t investAmout =
//         (std::stod(value) + global::ca::kFixDoubleMinPrecision) *
//         global::ca::kDecimalNum;
//         int32_t investType = 0;

//         std::string strError = TxHelper::ReplaceCreateInvestTransaction(fAddr,tAddr, investAmout, investType, &ack_t);
//         debugL("strError",strError);
//         if (strError != "0") {
//             out_json["message"]=strError;
//             out_json["error"]=std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"]=ack_t.paseToString();
//         out_json["error"]="0";
//         return true;
//     };

//     auto disinvest_func = [](const nlohmann::json &obj_func,
//                              nlohmann::json &out_json) -> bool {
//         tx_ack ack_t;
//         std::tuple<std::string, std::string, std::string> params =
//             GetRpcParams<std::string, std::string, std::string>(obj_func);
//         std::string fAddr = std::get<0>(params);
//         std::string tAddr = std::get<1>(params);
//         std::string utxo = std::get<2>(params);

//         std::string strError = TxHelper::ReplaceCreateDisinvestTransaction(
//             fAddr, tAddr, utxo, &ack_t);

//         if (strError != "0") {
//             out_json["message"] = strError;
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"] = ack_t.paseToString();
//         out_json["error"] = "0";
//         return true;
//     };
//     auto bonus_func = [](const nlohmann::json &obj_func,
//                     nlohmann::json &out_json) -> bool {
//         tx_ack ack_t;
//         std::tuple<std::string> params = GetRpcParams<std::string>(obj_func);
//         std::string addr = std::get<0>(params);
//         std::string strError =
//             TxHelper::ReplaceCreateBonusTransaction(addr, &ack_t);
//         if (strError != "0") {
//             out_json["message"] = strError;
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"] = ack_t.paseToString();
//         out_json["error"] = "0";
//         return true;
//     };

//     auto deploycontract_func = [clearHash](const nlohmann::json &obj_func,
//                                            nlohmann::json &out_json) -> bool {
//         tx_ack ack_t;
//         std::tuple<std::string,    // base58
//                    std::string,    // contracttype
//                    nlohmann::json, // info
//                    std::string,    // contract,
//                    std::string,    // data
//                    std::string     // pubstrbase64
//                    >
//             params=
//                 GetRpcParams<std::string, std::string, nlohmann::json,
//                              std::string, std::string, std::string>(obj_func);

//         std::string strFromAddr = std::get<0>(params);
//         std::string type_C = std::get<1>(params);
//         nlohmann::json info_t_con = std::get<2>(params);
//         std::string contract_str = std::get<3>(params);
//         std::string inputStr = std::get<4>(params);
//         std::string pubstrBase64 = std::get<5>(params);

//      //   nlohmann::json str=obj_func[2];

//         int ret = 0;
//         if (!CheckBase58Addr(strFromAddr)) {
//             out_json["message"] = DSTR "base58 error!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         DBReader dataReader;
//         uint64_t top = 0;
//         if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top)) {

//             out_json["message"] = DSTR "db get top failed!!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         uint32_t nContractType = std::stoi(type_C);

       

//         CTransaction outTx;
//         TxHelper::vrfAgentType isNeedAgent_flag;
//         Vrf info_;
//         if (nContractType == 0) {

//             std::string code = contract_str;

//             if (code.empty()) {
//                 out_json["message"] = DSTR "code is empty!";
//                 out_json["error"] =
//                     std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//                 return false;
//             }

//             std::string strInput = inputStr;

//             if (strInput == "0") {
//                 strInput.clear();
//             } else if (strInput.substr(0, 2) == "0x") {
//                 strInput = strInput.substr(2);
//                 code += strInput;
//             }
//             // Account launchAccount;
//             Base64 base;
//             std::string pubstr =
//                 base.Decode(pubstrBase64.c_str(), pubstrBase64.size());
//             std::string OwnerEvmAddr = evm_utils::GenerateEvmAddr(pubstr);

//             ret = rpc_evm::RpcCreateEvmDeployContractTransaction(
//                 strFromAddr, OwnerEvmAddr, code, top + 1, info_t_con, outTx,
//                 isNeedAgent_flag, info_);
//             if (ret != 0) {

//                 out_json["message"] =
//                     DSTR Sutil::Format("Failed to create DeployContract "
//                                        "transaction! The error code is:%s",
//                                        ret);
//                 out_json["error"] =
//                     std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//                 return false;
//             }
//         } else {
//             out_json["message"] = DSTR "unknow error";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         // tx_ack *ack_t=(tx_ack*)ack;
//         clearHash(outTx);
//         std::string txJsonString;
//         std::string vrfJsonString;
//         google::protobuf::util::Status status =
//             google::protobuf::util::MessageToJsonString(outTx, &txJsonString);
//         status =
//             google::protobuf::util::MessageToJsonString(info_, &vrfJsonString);
//         ack_t.txJson = txJsonString;
//         ack_t.vrfJson = vrfJsonString;
//         ack_t.ErrorCode = "0";
//         ack_t.height = std::to_string(top);
//         ack_t.txType = std::to_string((int)isNeedAgent_flag);

//         if (ret != 0) {
//             out_json["message"] = std::to_string(ret);
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         out_json["body"] = ack_t.paseToString();
//         out_json["error"] = "0";

//         return true;
//     };

//     auto callcontract_func = [clearHash](const nlohmann::json &obj_func,nlohmann::json &out_json) -> bool {
//         tx_ack ack_t;
//         std::string addr = obj_func[0];
//         std::string deployer = obj_func[1];
//         std::string deployutxo = obj_func[2];
//         std::string args = obj_func[3];
//         std::string pubstrBase64 = obj_func[4];
//         std::string tip = obj_func[5];
//         std::string money = obj_func[6];

//         if (!CheckBase58Addr(addr)) {

//             out_json["message"] = DSTR "base58 addr error!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         std::string strInput = args;
//         // std::string strToAddr=ret_t->deployer;

//         DBReader dataReader;

//         if (!CheckBase58Addr(deployer)) {
//             out_json["message"] = DSTR "base58 addr error!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         if (strInput.substr(0, 2) == "0x") {
//             strInput = strInput.substr(2);
//         }

//         std::string contractTipStr = tip;

//         std::regex pattern("^\\d+(\\.\\d+)?$");
//         if (!std::regex_match(contractTipStr, pattern)) {
//             out_json["message"] = DSTR "input contract tip error ! ";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         std::string contractTransferStr = money;
//         if (!std::regex_match(contractTransferStr, pattern)) {

//             out_json["message"] = DSTR "input contract transfer error ! ";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         uint64_t contractTip =
//             (std::stod(contractTipStr) + global::ca::kFixDoubleMinPrecision) *
//             global::ca::kDecimalNum;
//         uint64_t contractTransfer = (std::stod(contractTransferStr) +
//                                      global::ca::kFixDoubleMinPrecision) *
//                                     global::ca::kDecimalNum;
//         uint64_t top = 0;
//         if (DBStatus::DB_SUCCESS != dataReader.GetBlockTop(top)) {

//             out_json["message"] = DSTR "db get top failed!!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         CTransaction outTx;
//         CTransaction tx;
//         std::string tx_raw;
//         if (DBStatus::DB_SUCCESS !=
//             dataReader.GetTransactionByHash(deployutxo, tx_raw)) {
//             out_json["message"] = DSTR "get contract transaction failed!!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }
//         if (!tx.ParseFromString(tx_raw)) {

//             out_json["message"] = DSTR "contract transaction parse failed!!";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         nlohmann::json dataJson = nlohmann::json::parse(tx.data());
//         nlohmann::json tx_info = dataJson["TxInfo"].get<nlohmann::json>();
//         int vm_type = tx_info["VmType"].get<int>();

//         int ret = 0;
//         TxHelper::vrfAgentType isNeedAgent_flag;
//         Vrf info_;
//         if (vm_type == global::ca::VmType::EVM) {
//             Base64 base;
//             std::string pubstr_ =
//                 base.Decode(pubstrBase64.c_str(), pubstrBase64.size());
//             std::string OwnerEvmAddr = evm_utils::GenerateEvmAddr(pubstr_);
//             ret = rpc_evm::RpcCreateEvmCallContractTransaction(
//                 addr, deployer, deployutxo, strInput, OwnerEvmAddr, top + 1,
//                 outTx, isNeedAgent_flag, info_, contractTip, contractTransfer,true);
//             if (ret != 0) {
//                 out_json["message"] = DSTR Sutil::Format(
//                     "Create call contract transaction failed! ret: %s", ret);
//                 out_json["error"] =
//                     std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//                 return false;
//             }
//         } else {

//             out_json["message"] = DSTR "not EVM";
//             out_json["error"] =
//                 std::to_string((int)RPCERROR::TRANSACTION_ERROR);
//             return false;
//         }

//         // errorL("VVVVVVVRRRRFDATA:%s",info_.data());
//         clearHash(outTx);
//         std::string txJsonString;
//         std::string vrfJsonString;
//         google::protobuf::util::Status status =
//             google::protobuf::util::MessageToJsonString(outTx, &txJsonString);
//         status =
//             google::protobuf::util::MessageToJsonString(info_, &vrfJsonString);
//         ack_t.txJson = txJsonString;
//         ack_t.vrfJson = vrfJsonString;
//         ack_t.ErrorCode = "0";
//         ack_t.height = std::to_string(top);
//         ack_t.txType = std::to_string((int)isNeedAgent_flag);
//         out_json["body"] = ack_t.paseToString();
//         out_json["error"] = "0";

//         return true;
//     };

    
//     auto get_height=[](const nlohmann::json &obj_func,nlohmann::json &out_json) {
//         the_top ack_t;
//         DBReader dbReader;
//         uint64_t top = 0;
//         dbReader.GetBlockTop(top);
//         ack_t.top = std::to_string(top);
//         out_json["height"]=top;
//         out_json["message"]="";
//         return true;
//     };

//     auto getbalance_func=[](const nlohmann::json &obj_func,nlohmann::json &out_json)->bool
//     {
//         std::string addr=obj_func[0];
//         if (!CheckBase58Addr(addr)) {
//             out_json["message"]="check base error";
//             out_json["error"]=(int)RPCERROR::NUMBER_ERROR;
//             return false;
//         }

//         uint64_t balance = 0;
//         if (GetBalanceByUtxo(addr.c_str(), balance) != 0) {
//             out_json["message"]="find addr error";
//             out_json["error"]=(int)RPCERROR::NUMBER_ERROR;
//             return false;
        
//         }
//         out_json["balance"]=std::to_string(balance);
//         return true;
//     };

//     auto getstakeutxo_func=[](const nlohmann::json &obj_func,nlohmann::json &out_json)->bool
//     {
//          std::string addr=obj_func[0];
//         DBReader dbReader;
//         std::vector<string> utxos;
//         dbReader.GetStakeAddressUtxo(addr, utxos);
//         std::reverse(utxos.begin(), utxos.end());
//         std::map<std::string, uint64_t> outPut;

//         for (auto &utxo : utxos) {
//             std::string txRaw;
//             dbReader.GetTransactionByHash(utxo, txRaw);
//             CTransaction tx;
//             tx.ParseFromString(txRaw);
//             uint64_t value = 0;
//             for (auto &vout : tx.utxo().vout()) {
//                 if (vout.addr() == global::ca::kVirtualStakeAddr) {
//                     value = vout.value();
//                 }
//                 outPut[utxo] = value;
//             }
//         }
//         nlohmann::json utxos_json;
//         for(auto & n:outPut){
//             nlohmann::json utxo_json;
//             utxo_json["utxo"]=n.first;
//             utxo_json["value"]=n.second;
//             utxos_json.push_back(utxo_json);
//         }
//         out_json["utxos"]=utxos_json;
//         return true;

    
//     };

//     auto getdeployutxo_func=[](const nlohmann::json &obj_func,nlohmann::json &out_json)->bool
//     {
//         DBReader dataReader;
//         std::string addr=obj_func[0];
//         std::vector<std::string> vecDeployUtxos;
//         dataReader.GetDeployUtxoByDeployerAddr(addr, vecDeployUtxos);
//         std::cout << "=====================deployed utxos====================="
//               << std::endl;
//         for (auto &deploy_utxo : vecDeployUtxos) {
//             std::cout << "deployed utxo: " << deploy_utxo << std::endl;
//         }
//         std::cout << "=====================deployed utxos====================="
//               << std::endl;
//         //ack_t.utxos = vecDeployUtxos;
//          nlohmann::json utxos_json;
//         for(auto & n:vecDeployUtxos){
//             nlohmann::json utxo_json;
//             utxo_json["utxo"]=n;
//             utxos_json.push_back(utxo_json);
//         }
//         out_json["utxos"]=utxos_json;
//         return true;

//     };

//     auto getinvestutxo_func=[](const nlohmann::json &obj_func,nlohmann::json &out_json)->bool
//     {
//        std::string faddr=obj_func[0];
//        std::string taddr=obj_func[1];

        


//         DBReader dbReader;
//         std::vector<string> vecUtxos;
//         dbReader.GetBonusAddrInvestUtxosByBonusAddr(taddr, faddr,
//                                                  vecUtxos);
//         std::reverse(vecUtxos.begin(), vecUtxos.end());


//           nlohmann::json utxos_json;
//         for(auto & n:vecUtxos){
//             nlohmann::json utxo_json;
//             utxo_json["utxo"]=n;
//             utxos_json.push_back(utxo_json);
//         }
//         out_json["utxos"]=utxos_json;
//         return true;

//     };
//     auto sendtransation_func=[](const nlohmann::json &obj_func,nlohmann::json &out_json)->bool
//     {
//         rpc_ack ack_back;
//         tx_ack ack_t;
//         ack_t.paseFromJson(obj_func[0]);
//         std::string vlaue=obj_func[0];
//         debugL("vlaue:%s------------->",vlaue);
//         CTransaction tx;
//         Vrf info;
//         int height;
//         TxHelper::vrfAgentType type;
//         google::protobuf::util::Status status =
//         google::protobuf::util::JsonStringToMessage(ack_t.txJson, &tx);
//         status = google::protobuf::util::JsonStringToMessage(ack_t.vrfJson, &info);
   

//         height = std::stoi(ack_t.height);
//         type = (TxHelper::vrfAgentType)std::stoi(ack_t.txType);
//         std::string txHash = Getsha256hash(tx.SerializeAsString());
//         ack_back.txhash = txHash;
//         int ret = TxHelper::SendMessage(tx, height, info, type);
//         ack_back.ErrorCode = std::to_string(ret);

//         std::string back = ack_back.paseToString();
//         out_json["message"]=back;
//         out_json["error"]="0";
//         return true;
//     };

//     static std::map<std::string ,std::function<bool(const nlohmann::json &,nlohmann::json &)>> method_table=
//     {
//         {"transaction",transaction_func},
//         {"stake",stake_func},
//         {"unstake",unstake},
//         {"invest",invest_func},
//         {"disinvest",disinvest_func},
//         {"bonus",bonus_func},
//         {"deploycontract",deploycontract_func},
//         {"callcontract",callcontract_func},
//         {"sendtransation",sendtransation_func},
//         {"getinvestutxo",getinvestutxo_func},
//         {"getdeployutxo",getdeployutxo_func},
//         {"getstakeutxo",getstakeutxo_func}
//         };

//     method_table[method_string](params,jsonRet);

//     res.set_content(jsonRet.dump(), "application/json");

// }

#endif

void ApiPrintCalc1000SumHash(const Request &req,Response &res)
{
    int startHeight = 1000;
    if (req.has_param("startHeight")) {
        startHeight = atol(req.get_param_value("startHeight").c_str());
    }
    if(startHeight <= 0 || startHeight % 1000 != 0)
    {
        res.set_content("startHeight error", "text/plain");
        return;
    }

    int endHeight = 0;
    if (req.has_param("endHeight")) {
        endHeight = atol(req.get_param_value("endHeight").c_str());
    }

    if(endHeight < startHeight || endHeight % 1000 != 0)
    {
        res.set_content("endHeight error", "text/plain");
        return;
    }

    uint64_t max_height = 0;
    DBReader dbReader;
    if(DBStatus::DB_SUCCESS != dbReader.GetBlockComHashHeight(max_height))
    {
        res.set_content("GetBlockComHashHeight error", "text/plain");
        return;
    }

    std::ostringstream oss;
    if(max_height < endHeight)
    {
        endHeight = max_height;
        oss << "max_height = " << max_height << std::endl;
    }


    for(int i = startHeight; i <= endHeight; i += 1000)
    {
        std::string sumHash;
        auto ret = dbReader.GetCheckBlockHashsByBlockHeight(i, sumHash);
        if(ret == DBStatus::DB_SUCCESS)
        {
            oss << "blockHeight: " << i << "\t sumHash: " << sumHash << std::endl;
        }
        else 
        {
            oss << "GetCheckBlockHashsByBlockHeight error" << std::endl;
        }
        
    }


    res.set_content(oss.str(), "text/plain");

}


void ApiSetCalc1000TopHeight(const Request &req,Response &res)
{
    int topHeight = 0;
    if (req.has_param("topHeight")) {
        topHeight = atol(req.get_param_value("topHeight").c_str());
    }

    DBReadWriter db_reader_write;

    uint64_t selfNodeHeight = 0;
    auto status = db_reader_write.GetBlockTop(selfNodeHeight);
    if (DBStatus::DB_SUCCESS != status)
    {
        res.set_content("GetBlockTop error", "text/plain");
        return;
    }

    if(topHeight % 1000 != 0 || topHeight < 0 || topHeight > selfNodeHeight)
    {
        res.set_content("topHeight error", "text/plain");
        return;
    }

    if (DBStatus::DB_SUCCESS != db_reader_write.SetBlockComHashHeight(topHeight))
    {
        res.set_content("SetBlockComHashHeight error", "text/plain");
        return;
    }

    if (DBStatus::DB_SUCCESS != db_reader_write.TransactionCommit())
    {
        res.set_content("TransactionCommit error", "text/plain");
        return;
    }

    res.set_content("SetBlockComHashHeight success", "text/plain");

}

void ApiPrintContractBlock(const Request & req, Response & res)
{
    {
    int num = 100;
    if (req.has_param("num")) {
        num = atol(req.get_param_value("num").c_str());
    }
    int startNum = 0;
    if (req.has_param("height")) {
        startNum = atol(req.get_param_value("height").c_str());
    }
    int hash = 0;
    if (req.has_param("hash")) {
        hash = atol(req.get_param_value("hash").c_str());
    }

    std::string str;

    if (hash) {
        str = PrintBlocksHash(num, req.has_param("pre_hash_flag"));
        res.set_content(str, "text/plain");
        return;
    }

    if (startNum == 0)
        str = PrintContractBlocks(num, req.has_param("pre_hash_flag"));
    else
        str = PrintRangeContractBlocks(startNum, num, req.has_param("pre_hash_flag"));

    res.set_content(str, "text/plain");
}
}


void ApiPrintHundredSumHash(const Request & req, Response & res)
{
    int startHeight = 100;
    if (req.has_param("startHeight")) {
        startHeight = atol(req.get_param_value("startHeight").c_str());
    }
    if(startHeight <= 0 || startHeight % 100 != 0)
    {
        res.set_content("startHeight error", "text/plain");
        return;
    }

    int endHeight = 0;
    if (req.has_param("endHeight")) {
        endHeight = atol(req.get_param_value("endHeight").c_str());
    }

    if(endHeight < startHeight || endHeight % 100 != 0)
    {
        res.set_content("endHeight error", "text/plain");
        return;
    }

    DBReader dbReader;
    std::ostringstream oss;

    for(int i = startHeight; i <= endHeight; i += 100)
    {
        std::string sumHash;
        auto ret = dbReader.GetSumHashByHeight(i, sumHash);
        if(ret == DBStatus::DB_SUCCESS)
        {
            oss << "blockHeight: " << i << "\t sumHash: " << sumHash << std::endl;
        }
        else 
        {
            oss << "GetSumHashByHeight error, error height: " << i << std::endl;
        }
        
    }
    
    res.set_content(oss.str(), "text/plain");
}