#include "api/http_api.h"
#include "ca_global.h"
#include "ca_transaction.h"
#include "utils/MagicSingleton.h"
#include <algorithm>
#include <string>
#include "ca_txhelper.h"
#include <sstream>
#include "ca/ca_AdvancedMenu.h"
#include "db/db_api.h"
#include "db/cache.h"
#include "utils/AccountManager.h"
#include "api/interface/tx.h"
#include "api/interface/sig.h"
#include "api/interface/RSA_TEXT.h"
#include "api/interface/evm.h"
#include "api/interface/base64.h"
#include "utils/tmplog.h"
#include "ca/ca.h"







#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <vector>

#include <chrono>
#include <ctime>

#include <iomanip>

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sstream>
#include <map>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
static bool flag = true;
static bool autotx_flag = true;
#define BUFFER_SIZE 1024

//  void get long line 
static std::vector<std::string> search_after_time(const string &filename, const string &time_str, int num_lines)
{

    std::vector<std::string> lines;
    regex pattern(time_str);
    std::string cmd = "grep -A " + to_string(num_lines) + " \"" + time_str + "\" " + filename;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe)
    {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        {
            lines.push_back(buffer);
        }
        pclose(pipe);
    }
    return lines;
}

static std::vector<std::string> entry_filesystem(string path)
{
    std::vector<std::string> file_names;
    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            file_names.push_back(entry.path().filename().string());
        }
    }

    return file_names;
}

static time_t convert_string_to_time(std::string str)
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
        timestamp = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    }
    return timestamp;
}

static string is_exist_log(std::vector<std::string> log, string time)
{

    std::string log_file;
    auto this_time = convert_string_to_time(time);
    for (const auto &str : log)
    {
        if (this_time == convert_string_to_time(str))
        {
            log_file = str;
        }
    }
    return log_file;
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

static std::vector<CPUStat> get_cpu_stats()
{
    std::vector<CPUStat> cpu_stats;
    std::ifstream stat_file("/proc/stat");

    std::string line;
    while (std::getline(stat_file, line))
    {
        if (line.compare(0, 3, "cpu") == 0)
        {
            std::istringstream ss(line);

            std::string cpu_label;
            CPUStat stat;
            ss >> cpu_label >> stat.user >> stat.nice >> stat.system >> stat.idle >> stat.iowait >> stat.irq >> stat.softirq;

            cpu_stats.push_back(stat);
        }
    }

    return cpu_stats;
}

static double calculate_cpu_usage(const CPUStat &prev, const CPUStat &curr)
{
    auto prev_total = prev.user + prev.nice + prev.system + prev.idle + prev.iowait + prev.irq + prev.softirq;
    auto curr_total = curr.user + curr.nice + curr.system + curr.idle + curr.iowait + curr.irq + curr.softirq;

    auto total_diff = curr_total - prev_total;
    auto idle_diff = curr.idle - prev.idle;

    return (total_diff - idle_diff) * 100.0 / total_diff;
}

static std::string doubleToStringWithPrecision(double value, int precision)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

struct ProcessInfo
{
    std::string name;
    int open_files;
    std::vector<std::string> file_list;
};

static std::map<int, ProcessInfo> get_all_processes_info()
{
    std::map<int, ProcessInfo> processes_info;

    DIR *proc_dir = opendir("/proc");
    if (!proc_dir)
    {
        std::cerr << "Error opening /proc directory." << std::endl;
        return processes_info;
    }

    struct dirent *entry;
    while ((entry = readdir(proc_dir)))
    {
        int pid = atoi(entry->d_name);
        if (pid > 0)
        {
            std::string pid_path = "/proc/" + std::to_string(pid);
            std::string cmdline_path = pid_path + "/cmdline";
            std::string fd_dir_path = pid_path + "/fd";

            std::ifstream cmdline_file(cmdline_path);
            std::string process_name;
            std::getline(cmdline_file, process_name);
            cmdline_file.close();

            ProcessInfo process_info;
            process_info.name = process_name;

            DIR *fd_dir = opendir(fd_dir_path.c_str());
            if (fd_dir)
            {
                struct dirent *fd_entry;
                while ((fd_entry = readdir(fd_dir)))
                {
                    if (fd_entry->d_type == DT_LNK)
                    {
                        char buf[1024];
                        std::string fd_path = fd_dir_path + "/" + fd_entry->d_name;
                        ssize_t len = readlink(fd_path.c_str(), buf, sizeof(buf) - 1);
                        if (len != -1)
                        {
                            buf[len] = '\0';
                            process_info.file_list.push_back(std::string(buf));
                        }
                    }
                }
                closedir(fd_dir);
            }

            process_info.open_files = process_info.file_list.size();
            processes_info[pid] = process_info;
        }
    }

    closedir(proc_dir);
    return processes_info;
}

std::string exec(const char* cmd)
{
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try
    {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
    } catch (...) 
    {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}
static std::string get_os_version()
{
    struct utsname buffer;
    std::string os_release = exec("cat /etc/os-release");
    std::string str;
    if (uname(&buffer) != 0)
    {
        return "Error getting OS version";
    }
    str = std::string(buffer.sysname) + " " + std::string(buffer.release) + " " + std::string(buffer.version) ;
    str +=os_release;
    return str;
}

std::vector<std::string> paginate(const std::string &text, size_t linesPerPage)
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
    unsigned long long bytes_received;
    unsigned long long bytes_sent;
};

static NetStat get_net_stat(const std::string &interface)
{
    NetStat net_stat = {0, 0};
    std::ifstream net_dev_file("/proc/net/dev");
    std::string line;

    while (std::getline(net_dev_file, line))
    {
        if (line.find(interface) != std::string::npos)
        {
            std::istringstream ss(line);
            std::string iface;
            ss >> iface >> net_stat.bytes_received;

            for (int i = 0; i < 7; ++i)
            {
                unsigned long long tmp;
                ss >> tmp;
            }

            ss >> net_stat.bytes_sent;
            break;
        }
    }

    return net_stat;
}

static std::string format_speed(double speed)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << speed << " Mbps";
    return ss.str();
}

static std::string get_mac_address(const std::string &interface)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        return "";
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        std::cerr << "Error getting MAC address" << std::endl;
        close(sock);
        return "";
    }

    close(sock);
    char mac_address[18];
    snprintf(mac_address, sizeof(mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[0]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[1]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[2]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[3]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[4]),
             static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[5]));

    return mac_address;
}

static std::string get_network_interface_model(const std::string &interface)
{
    std::string model_path = "/sys/class/net/" + interface + "/device/modalias";
    std::ifstream model_file(model_path);
    if (!model_file.is_open())
    {
        std::cerr << "Error opening model file: " << model_path << std::endl;
        return "";
    }

    std::string model_info;
    std::getline(model_file, model_info);
    model_file.close();

    return model_info;
}

// get all memory and disk

void get_mem_occupy(int lenNum, std::string &strMem)
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
    strMem += "Memory usage:\t" + std::to_string(100.0 * (total - available) / total) + "%\n";
    fclose(fpMemInfo);
}

void getDisktype(std::string &strMem)
{
    ifstream rotational("/sys/block/sda/queue/rotational");
    if (rotational.is_open())
    {
        int is_rotational;
        rotational >> is_rotational;
        strMem += "Disk type:\t";
        strMem += (is_rotational ? "HDD" : "SSD");
        rotational.close();
    }
    else
    {
        strMem += "-1 Disk rotational open error";
    }
}

int getFileLine()
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

void ca_register_http_callbacks()
{
    HttpServer::registerCallback("/", api_jsonrpc);
    HttpServer::registerCallback("/block", api_print_block);
    HttpServer::registerCallback("/info", api_info);
    HttpServer::registerCallback("/get_block", api_get_block);
    HttpServer::registerCallback("/log", api_get_log_line);
    HttpServer::registerCallback("/system_info",get_all_system_info);
    HttpServer::registerCallback("/bandwidth",api_get_real_bandwidth);
    HttpServer::registerCallback("/block_info",api_get_block_info);
    HttpServer::registerCallback("/get_tx_info",api_get_tx_info);
    

    HttpServer::registerCallback("/pub", api_pub);
    HttpServer::registerCallback("/startautotx", api_start_autotx);
    HttpServer::registerCallback("/endautotx", api_end_autotx);
    HttpServer::registerCallback("/autotxstatus", api_status_autotx);
    HttpServer::registerCallback("/filterheight", api_filter_height);


    HttpServer::registerJsonRpcCallback("get_height", jsonrpc_get_height);
    HttpServer::registerJsonRpcCallback("get_balance", jsonrpc_get_balance);


    HttpServer::registerCallback("/deploy_contract_req", deploy_contract);


    HttpServer::registerJsonRpcCallback("get_height", jsonrpc_get_height);
    HttpServer::registerJsonRpcCallback("get_balance", jsonrpc_get_balance);

    HttpServer::registerCallback("/getTxTransaction_req",jsonrpc_get_TxTransaction);
    HttpServer::registerCallback("/getStakeUtxo_req",jsonrpc_get_StakeUtxos);
    HttpServer::registerCallback("/getDisInvestUtxo_req",jsonrpc_get_DisinvestUtxos);
    HttpServer::registerCallback("/getStake_req",jsonrpc_get_StakeTransaction);
    HttpServer::registerCallback("/getUnstake_req",jsonrpc_get_UnstakeTransaction);
    HttpServer::registerCallback("/getInvest_req",jsonrpc_get_InvestTransaction);
    HttpServer::registerCallback("/getDisInvest_req",jSonrpc_get_DisinvestTransaction);
    HttpServer::registerCallback("/getDeclare_req",jSonrpc_get_DeclareTransaction);
    HttpServer::registerCallback("/getBonus_req",jSonrpc_get_BonusTransaction);

    HttpServer::registerCallback("/get_rsa_req",jSonrpc_get_RSA_PUBSTR);



    //Start the http service
    HttpServer::start();
}

void api_pub(const Request &req, Response &res)
{
   std::ostringstream oss;
   MagicSingleton<ProtobufDispatcher>::GetInstance()->task_info(oss);
   oss << "queue_read:" << global::queue_read.msg_queue_.size() << std::endl;
   oss << "queue_work:" << global::queue_work.msg_queue_.size() << std::endl;
   oss << "queue_write:" << global::queue_write.msg_queue_.size() << std::endl;
   oss << "\n"
       << std::endl;

   double total = .0f;
   uint64_t n64Count = 0;
   oss << "------------------------------------------" << std::endl;
   for (auto &item : global::reqCntMap)
   {
       total += (double) item.second.second;// data size
       oss.precision(3);//Keep 3 decimal places
       // Type of data		        Number of calls							 convert MB
       oss << item.first << ": " << item.second.first << " size: " << (double) item.second.second / 1024 / 1024
           << " MB" << std::endl;
       n64Count += item.second.first;
   }
   oss << "------------------------------------------" << std::endl;
   oss << "Count: " << n64Count << "   Total: " << total / 1024 / 1024 << " MB" << std::endl;//Total size

   oss << std::endl;
   oss << std::endl;

   oss << "amount:" << std::endl;
   std::vector<std::string> baselist;

   MagicSingleton<AccountManager>::GetInstance()->GetAccountList(baselist);
   for (auto &i : baselist)
   {
       uint64_t amount = 0;
       GetBalanceByUtxo(i, amount);
       oss << i + ":" + std::to_string(amount) << std::endl;
   }

   oss << std::endl << std::endl;

   std::vector<Node> pubNodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
   oss << "Public PeerNode size is: " << pubNodeList.size() << std::endl;
   oss << MagicSingleton<PeerNode>::GetInstance()->nodelist_info(pubNodeList);//   Convert all public network node data to string for saving
   res.set_content(oss.str(), "text/plain");
}


void api_jsonrpc(const Request &req, Response &res)
{
   nlohmann::json ret;
   ret["jsonrpc"] = "2.0";
   try
   {
       auto json = nlohmann::json::parse(req.body);

       std::string method = json["method"];

       auto p = HttpServer::rpc_cbs.find(method);
       if (p == HttpServer::rpc_cbs.end())
       {
           ret["error"]["code"] = -32601;
           ret["error"]["message"] = "Method not found";
           ret["id"] = "";
       } else
       {
           auto params = json["params"];
           ret = HttpServer::rpc_cbs[method](params);
           try
           {
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

void api_print_block(const Request &req, Response &res)
{
   int num = 100;
   if (req.has_param("num"))
   {
       num = atol(req.get_param_value("num").c_str());
   }
   int startNum = 0;
   if(req.has_param("height"))
   {
       startNum = atol(req.get_param_value("height").c_str());
   }
   int hash = 0;
   if(req.has_param("hash"))
   {
       hash = atol(req.get_param_value("hash").c_str());
   }

   std::string str;

    if(hash)
    {
        str = printBlocksHash(num, req.has_param("pre_hash_flag"));
        res.set_content(str, "text/plain");
        return;
    }


   if(startNum == 0)
       str = printBlocks(num, req.has_param("pre_hash_flag"));
   else
       str = printRangeBlocks(startNum, num, req.has_param("pre_hash_flag"));

       
   res.set_content(str, "text/plain");
}
void api_info(const Request &req, Response &res)
{

   std::ostringstream oss;

   oss << "queue:" << std::endl;
   oss << "queue_read:" << global::queue_read.msg_queue_.size() << std::endl;
   oss << "queue_work:" << global::queue_work.msg_queue_.size() << std::endl;
   oss << "queue_write:" << global::queue_write.msg_queue_.size() << std::endl;
   oss << "\n"
       << std::endl;

   oss << "amount:" << std::endl;
   std::vector<std::string> baselist;
   
   MagicSingleton<AccountManager>::GetInstance()->GetAccountList(baselist);
   for (auto &i : baselist)
   {
       uint64_t amount = 0;
       GetBalanceByUtxo(i, amount);
       oss << i + ":" + std::to_string(amount) << std::endl;
   }
   oss << "\n"
       << std::endl;

   std::vector<Node> nodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
   oss << "Public PeerNode size is: " << nodeList.size() << std::endl;
   oss << MagicSingleton<PeerNode>::GetInstance()->nodelist_info(nodeList);

   oss << std::endl << std::endl;

   res.set_content(oss.str(), "text/plain");
}

void api_get_block(const Request &req, Response &res)
{
    nlohmann::json block;
    nlohmann::json blocks;

    int top = 0;
    if (req.has_param("top"))
    {
        top = atol(req.get_param_value("top").c_str());
    }
    int num = 0;
    if (req.has_param("num"))
    {
        num = atol(req.get_param_value("num").c_str());
    }

    num = num > 500 ? 500 : num;

    if (top < 0 || num <= 0)
    {
        ERRORLOG("api_get_block top < 0||num <= 0");
        return;
    }

	DBReader db_reader;
    uint64_t mytop = 0;
    db_reader.GetBlockTop(mytop);
    if (top > (int) mytop)
    {
        ERRORLOG("api_get_block begin > mytop");
        return;
    }
    int k = 0;
    uint64_t countNum = top + num;
    if(countNum > mytop)
    {
        countNum = mytop;
    }
    for (auto i = top; i <= countNum; i++)
    {
        std::vector<std::string> vBlockHashs;

        if(db_reader.GetBlockHashsByBlockHeight(i, vBlockHashs) != DBStatus::DB_SUCCESS)
        {
            return;
        }

        for (auto hash : vBlockHashs)
        {
            string strHeader;
            if(db_reader.GetBlockByBlockHash(hash, strHeader) != DBStatus::DB_SUCCESS)
            {
                return;
            }
            BlockInvert(strHeader, block);
            blocks[k++] = block;
        }
    }
    std::string str = blocks.dump();
   res.set_content(str, "application/json");
}

void api_filter_height(const Request &req, Response &res)
{
    std::ostringstream oss;

    DBReader db_reader;
    uint64_t myTop = 0;
    db_reader.GetBlockTop(myTop);

    std::vector<Node> nodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    std::vector<Node> filterNodes;

    for(auto & node : nodeList)
    {
        if(node.height == myTop)
        {
            filterNodes.push_back(node);
        }
    }

    oss << "My Top : " << myTop << std::endl;
    oss << "Public PeerNode size is: " << filterNodes.size() << std::endl;
    oss << MagicSingleton<PeerNode>::GetInstance()->nodelist_info(filterNodes);

    oss << std::endl << std::endl;

   res.set_content(oss.str(), "text/plain");

}

void api_start_autotx(const Request &req, Response &res)
{
    if(!autotx_flag)
    {
        api_end_autotx_test(res);
        autotx_flag = true;
        return;
    }

    if(!flag)
    {
        std::cout<<"flag ="<<flag<<std::endl;
        std::cout<<"api_start_autotx is going "<<std::endl;
        return ;
    }

    int Interval = 0;
    if (req.has_param("Interval"))
    {
        Interval = atol(req.get_param_value("Interval").c_str());
    }
    int Interval_frequency = 0;
    if (req.has_param("Interval_frequency"))
    {
        Interval_frequency = atol(req.get_param_value("Interval_frequency").c_str());
    }

   
    std::cout<<"Interval ="<<Interval<<std::endl;
    std::cout<<"Interval_frequency ="<<Interval_frequency<<std::endl;
    std::vector<std::string> addrs;

    MagicSingleton<AccountManager>::GetInstance()->GetAccountList(addrs);

    std::vector<std::string>::iterator it = std::find(addrs.begin(),addrs.end(),MagicSingleton<AccountManager>::GetInstance()->GetDefaultBase58Addr());
    if(it != addrs.end())
    {
        addrs.erase(it);
    }

    flag = false;
    
    ThreadTest::set_StopTx_flag(flag);
    std::thread th(&ThreadTest::test_createTx,Interval_frequency, addrs, Interval);
    th.detach();

    sleep(1);
    if(api_status_autotx_test(res))
    {
        autotx_flag = false;
    }

    return;
}


void api_status_autotx(const Request &req, Response &res)
{
    api_status_autotx_test(res);
}

void api_end_autotx(const Request &req, Response &res)
{
    api_end_autotx_test(res);
}

bool api_status_autotx_test(Response & res)
{
    std::ostringstream oss; 
    bool flag = false;
    bool stop_tx = false;
    ThreadTest::get_StopTx_flag(flag);
    if(!flag)
    {
        oss << "auto tx is going :" << std::endl;
        stop_tx = true;
    }
    else
    {
         oss << "auto tx is end!:" << std::endl;
    }
    res.set_content(oss.str(), "text/plain");
    return stop_tx;
}

void api_end_autotx_test(Response & res)
{
    std::ostringstream oss; 
    oss << "end auto tx:" << std::endl;
  
    flag = true;
    ThreadTest::set_StopTx_flag(flag);
    res.set_content(oss.str(), "text/plain");
}

nlohmann::json jsonrpc_get_height(const nlohmann::json &param)
{
    DBReader db_reader;
    uint64_t top = 0;
    db_reader.GetBlockTop(top);

   nlohmann::json ret;
   ret["result"]["height"] = std::to_string(top);
   return ret;
}

nlohmann::json jsonrpc_get_balance(const nlohmann::json &param)
{
   nlohmann::json ret;
   std::string address;
   try
   {
       if (param.find("address") != param.end())
       {
           address = param["address"].get<std::string>();
       }
       else
       {
           throw std::exception();
       }
   }
   catch (const std::exception &e)
   {
       ret["error"]["code"] = -32602;
       ret["error"]["message"] = "Invalid params";
       return ret;
   }

   if (!CheckBase58Addr(address))
   {
       ret["error"]["code"] = -1;
       ret["error"]["message"] = "address is invalid ";
       return ret;
   }

   uint64_t balance = 0;
   if (GetBalanceByUtxo(address.c_str(), balance) != 0)
   {
       ret["error"]["code"] = -2;
       ret["error"]["message"] = "search balance failed";
       return ret;
   }
   ret["result"]["balance"] = balance;
   return ret;
}

//get log info
void api_get_log_line(const Request &req, Response &res)
{

    std::string filename;
    std::string str;
    std::string big_str;
    int line = 500;
 
    std::string path = "./logs";

    if (req.has_param("line"))
    {
        line = atol(req.get_param_value("line").c_str());
    }
    std::string str_time;

    if (req.has_param("time"))
    {
        str_time = req.get_param_value("time").c_str();
    }
    path = "./logs/" + is_exist_log(entry_filesystem(path), str_time);

    // test_time,file,and line message
    for (auto i : search_after_time(path, str_time, line))
    {
        big_str += i + '\n';
    }

  
    res.set_content(big_str, "text/plain");
}

// get cpu info
std::string api_get_cpu_info()
{
    std::string sum;
    sum = "===============================================================================";
    sum += "\n";
    sum += "get_cpu_info";
    sum += "\n";
    std::ifstream cpuinfo_file("/proc/cpuinfo");
    std::string line;
    int cpu_cores = 0;
    std::string cpu_model;
    double cpu_frequency = 0;

    while (std::getline(cpuinfo_file, line))
    {
        if (line.compare(0, 9, "processor") == 0)
        {
            cpu_cores++;
        }
        else if (line.compare(0, 10, "model name") == 0)
        {
            cpu_model = line.substr(line.find(":") + 2);
        }
        else if (line.compare(0, 7, "cpu MHz") == 0)
        {
            cpu_frequency = std::stod(line.substr(line.find(":") + 2)) / 1000;
        }
    }


    auto prev_stats = get_cpu_stats();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto curr_stats = get_cpu_stats();

    double total_usage = 0;
    for (size_t i = 1; i < prev_stats.size(); ++i)
    {
        total_usage += calculate_cpu_usage(prev_stats[i], curr_stats[i]);
    }

    double avg_usage = total_usage / (prev_stats.size() - 1);
    sum += "CPU Usage: " + doubleToStringWithPrecision(avg_usage, 1) + "%" + "\n";
    sum += "CPU Frequency: " + doubleToStringWithPrecision(cpu_frequency, 3) + " GHZ" + "\n";
    sum += "CPU Model: " + cpu_model + "\n";
    sum += "CPU Cores: " + std::to_string(cpu_cores);
    return sum;

}

std::string api_get_system_info()
{
    std::string str;
    str = "===============================================================================";
    str += "\n";
    str += "api_get_system_info";
    str += "\n";
    str += "OS Version: " + get_os_version() + "\n";
    return str;
}


std::string get_process_info()
{
    std::string str;
    str = "===============================================================================";
    str += "\n";
    str += "get_process_info";
    str += "\n";

    FILE *pipe = popen("ps -ef", "r");
    if (!pipe)
    {
        
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

std::string get_net_rate()
{
    std::string interface = "eth0";
    auto prev_stat = get_net_stat(interface);
    std::string str;
    str = "===============================================================================";
    str += "\n";
    str += "get_net_rate";
    str += "\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto curr_stat = get_net_stat(interface);

    double download_speed = (curr_stat.bytes_received - prev_stat.bytes_received) * 8 / 1000.0 / 1000.0;
    double upload_speed = (curr_stat.bytes_sent - prev_stat.bytes_sent) * 8 / 1000.0 / 1000.0;

    std::string download_speed_str = format_speed(download_speed);
    std::string upload_speed_str = format_speed(upload_speed);


    str += "Download speed: " + download_speed_str + "\n";
    str += "Upload speed: " + upload_speed_str + "\n";
    str += "Interface: " + interface + "\n";
    str += "MAC Address: " + get_mac_address(interface);
    str += "Model Info: " + get_network_interface_model(interface);
    prev_stat = curr_stat;
    return str;
}

void get_all_system_info(const Request &req, Response &res)
{
    std::string out_put;
    std::string MemStr;
    int lenNum = getFileLine();
    get_mem_occupy(lenNum, MemStr);
    getDisktype(MemStr);
    out_put = "===============================================================================";
    out_put += "\n";
    out_put += "get_mem_occupy";
    out_put += MemStr;
    out_put += "\n";

    out_put += api_get_cpu_info() + "\n";
    out_put += get_net_rate() + "\n";
    out_put += api_get_system_info() + "\n";
    out_put += get_process_info() + "\n";

    res.set_content(out_put, "text/plain");
}




//complete the net_real_width
void api_get_real_bandwidth(const Request &req, Response &res)
{
    std::string base58;
    if (req.has_param("base58"))
    {
        base58 = req.get_param_value("base58").c_str();
        
    }
    
    if(base58.empty())
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
    string data (10485760,'*');
    testReq.set_data(data);
    testReq.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());
    testReq.set_hash(getsha256hash(data));
    auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
    testReq.set_time(ms.count());
    
    
    bool isRegister = MagicSingleton<netTest>::GetInstance()->getSignal();
    if(!isRegister)
    {
        std::string error = "Please wait. It's still being processed";
        res.set_content(error,"text/plain");
        return;
    }

    bool hasValue = MagicSingleton<netTest>::GetInstance()->getflag();
    if(hasValue)
    {
        std::string speed;
        double time = MagicSingleton<netTest>::GetInstance()->getTime();
        speed +="base58addresss:" + base58 + "\n" + "bandwidth:" + std::to_string(time)+"MB/s";
        res.set_content(speed,"text/plain");
        return;
    }


    auto t = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
	
    bool isSucceed = net_com::send_message(base58, testReq, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_Low_0);
    if (isSucceed == false)
    {
        std::string error = "broadcast TestReq failed!";
        res.set_content(error,"text/plain");
        return;
    }
  

    MagicSingleton<netTest>::GetInstance()->netTestRegister();
    res.set_content("message has sent: ","text/plain");
    return;
}


void api_get_block_info(const Request &req, Response &res)
{
    std::string block_hash;
    if(req.has_param("block_hash"))
    {
        block_hash = req.get_param_value("block_hash").c_str();
    }
    if(block_hash.empty())
    {
        res.set_content("get block_hash error", "text/plain");
        return;
    }
    
    DBReader db_reader;
    std::string strHeader;

    if(DBStatus::DB_SUCCESS != db_reader.GetBlockByBlockHash(block_hash, strHeader))
    {
        res.set_content("block_hash error", "text/plain");
        return;
    }
    CBlock block;

    if (!block.ParseFromString(strHeader))
    {
        res.set_content("block ParseFromString error", "text/plain");
        return;
    }

    std::ostringstream ss;
    printBlock(block, true, ss);

    res.set_content(ss.str(), "text/plain");
}


void api_get_tx_info(const Request &req,Response &res)
{
    std::string tx_hash;
    if(req.has_param("tx_hash"))
    {
        tx_hash = req.get_param_value("tx_hash").c_str();
    }
    if(tx_hash.empty())
    {
        res.set_content("get tx_hash error", "text/plain");
        return;
    }
    
    DBReader db_reader;
    std::string strHeader;
    if(DBStatus::DB_SUCCESS != db_reader.GetTransactionByHash(tx_hash, strHeader))
    {
        res.set_content("tx_hash error", "text/plain");
        return;
    }

    CTransaction tx;
    if (!tx.ParseFromString(strHeader))
    {
        res.set_content("tx ParseFromString error", "text/plain");
        return;
    }

    std::ostringstream ss;

    printTx(tx, true, ss);

    res.set_content(ss.str(), "text/plain");
}


bool tool_DeCode(const std::string & source,std::string & dest,std::string & pubstr){
    std::shared_ptr<envelop> enve= MagicSingleton<envelop>::GetInstance();
    bool bret= RSADeCode(source, enve.get(),pubstr,dest);
    if(bret==false){  
        return false;
    }
    return true;
}



void deploy_contract(const Request & req, Response & res){
    deploy_contract_req req_t;
    bool ret_=req_t.paseFromJson(req.body);
    if(ret_==false){
        return ;
    }

    std::string ret=handle__deploy_contract_rpc((void *)& req_t);
    infoL("ret:" << ret);

}

void jsonrpc_get_StakeUtxos(const Request & req, Response & res)
{
    getStakeUtxo_ack ack_t;
    getStakeUtxo_req req_t;
    ack_t.ErrorCode = "0";

    if(!req_t.paseFromJson(req.body))
    {
        errorL("bad error pase fail");
        ack_t.ErrorCode="-1";
        ack_t.ErrorMessage="bad error pase fail";
        res.set_content(ack_t.paseToString(), "application/json"); 
        return ;
    }

    DBReader db_reader;
    std::vector<string> utxos;
    db_reader.GetStakeAddressUtxo(req_t.fromAddr, utxos);
    std::reverse(utxos.begin(), utxos.end());
    std::map<std::string,uint64_t> output;

    for (auto &utxo : utxos)
    {
        std::string txRaw;
        db_reader.GetTransactionByHash(utxo, txRaw);
        CTransaction tx;
        tx.ParseFromString(txRaw);
        uint64_t value = 0;
        for (auto &vout : tx.utxo().vout())
        {
            if (vout.addr() == global::ca::kVirtualStakeAddr)
            {
                value = vout.value();
            }
            output[utxo] = value;
        }
    }
    ack_t.utxos = output;
    res.set_content(ack_t.paseToString(), "application/json");  
    debugL(ack_t.paseToString());
}


void jsonrpc_get_DisinvestUtxos(const Request & req, Response & res)
{
    getDisInvestUtxo_ack ack_t;
    getDisInvestUtxo_req req_t;
    ack_t.ErrorCode = "0";

    if(!req_t.paseFromJson(req.body))
    {
        errorL("bad error pase fail");
        ack_t.ErrorCode="-1";
        ack_t.ErrorMessage="bad error pase fail";
        res.set_content(ack_t.paseToString(), "application/json"); 
        return ;
    }

    DBReader db_reader;
    std::vector<string> vecUtxos;
    db_reader.GetBonusAddrInvestUtxosByBonusAddr(req_t.toAddr, req_t.fromAddr, vecUtxos);
    std::reverse(vecUtxos.begin(), vecUtxos.end());
    ack_t.utxos = vecUtxos;
    res.set_content(ack_t.paseToString(), "application/json");  
    debugL(ack_t.paseToString());
}


void jsonrpc_get_TxTransaction(const Request & req, Response & res)
{
    rpc_ack ack_t;
    tx_req req_t;
    ack_t.ErrorCode="0";
   bool bret= req_t.paseFromJson(req.body);
   if(bret==false){
        errorL("bad error pase fail");
        ack_t.ErrorCode="-1";
        ack_t.ErrorMessage="bad error pase fail";
        res.set_content(ack_t.paseToString(), "application/json"); 
        return ;

   }
   std::map<std::string ,int64_t> toAddr;

   for(auto iter=req_t.toAddr.begin();iter!=req_t.toAddr.end();iter++){
    toAddr[iter->first]=std::stoul(iter->second)* global::ca::kDecimalNum;
   }
   
    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreateTxTransaction(req_t.fromAddr,toAddr);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-5";
    }

    res.set_content(ack_t.paseToString(), "application/json");  
}


void jsonrpc_get_StakeTransaction(const Request & req, Response & res)
{
    getStake_req req_t;
    rpc_ack ack_t;
    req_t.paseFromJson(req.body);
    debugL("req:" << req.body);

    std::string fromAddr = req_t.fromAddr;
    uint64_t stake_amount = std::stoll(req_t.stake_amount)* global::ca::kDecimalNum;
    int32_t PledgeType = std::stoll(req_t.PledgeType) ;

    ack_t.type = "getStake_ack";
    ack_t.ErrorCode = "0";

    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreateStakeTransaction(fromAddr, stake_amount, PledgeType);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");  
}


void jsonrpc_get_UnstakeTransaction(const Request & req, Response & res)
{
    getUnstake_req req_t;
    rpc_ack ack_t;

    req_t.paseFromJson(req.body);
    debugL("req:" << req.body);

    std::string fromAddr = req_t.fromAddr;
    std::string utxo_hash = req_t.utxo_hash;

    ack_t.type = "getUnstake_ack";
    ack_t.ErrorCode = "0";
    

    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreatUnstakeTransaction(fromAddr, utxo_hash);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");  
}


void jsonrpc_get_InvestTransaction(const Request & req, Response & res)
{
    getInvest_req req_t;
    rpc_ack ack_t;

    req_t.paseFromJson(req.body);
    debugL("req:" << req.body);
    

    std::string fromAddr = req_t.fromAddr;
    std::string toAddr = req_t.toAddr;
    uint64_t stake_amount = std::stoll(req_t.invest_amount) * global::ca::kDecimalNum;
    int32_t investType = std::stoll(req_t.investType);

   
    ack_t.type = "getInvest_ack";
    ack_t.ErrorCode = "0";

    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreateInvestTransaction(fromAddr, toAddr, stake_amount, investType);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }


    res.set_content(ack_t.paseToString(), "application/json");  
}


void jSonrpc_get_DisinvestTransaction(const Request & req, Response & res)
{
    getDisInvest_req req_t;
    rpc_ack ack_t;

    req_t.paseFromJson(req.body);

    std::string fromAddr = req_t.fromAddr;
    std::string toAddr = req_t.toAddr;
    std::string utxo_hash = req_t.utxo_hash;

    ack_t.type = "getDisInvest_ack";
    ack_t.ErrorCode = "0";
   

    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreateDisinvestTransaction(fromAddr, toAddr, utxo_hash);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");  
}


void jSonrpc_get_DeclareTransaction(const Request & req, Response & res)
{
    getDeclare_req req_t;
    rpc_ack ack_t;  

    req_t.paseFromJson(req.body);
    debugL("req:" << req.body);


    std::string fromAddr = req_t.fromAddr;
    std::string toAddr = req_t.toAddr;
    uint64_t amount = std::stoll(req_t.amount) * global::ca::kDecimalNum;

    std::string multiSignPub;
    Base64 base_;
    multiSignPub = base_.Decode((const  char *)req_t.multiSignPub.c_str(),req_t.multiSignPub.size());

    std::vector<std::string> signAddrList = req_t.signAddrList;
    uint64_t signThreshold = std::stoll(req_t.signThreshold);


    ack_t.type = "getDeclare_req";
    ack_t.ErrorCode = "0";

    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreateDeclareTransaction(fromAddr, toAddr, 
                                        amount, multiSignPub, signAddrList, signThreshold);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");  
}


void jSonrpc_get_BonusTransaction(const Request & req, Response & res)
{
    getBonus_req req_t;
    rpc_ack ack_t;

    req_t.paseFromJson(req.body);
    debugL("req:" << req.body);
    

    std::string Addr = req_t.Addr;


    ack_t.type = "getDisInvest_ack";
    ack_t.ErrorCode = "0";

    std::string strError = MagicSingleton<TxHelper>::GetInstance()->ReplaceCreateBonusTransaction(Addr);
    if(strError != "0")
    {
        ack_t.ErrorMessage = strError;
        ack_t.ErrorCode = "-1";
    }

    res.set_content(ack_t.paseToString(), "application/json");  
}


void call_contract(const Request & req,Response & res){
    
}


void jSonrpc_get_RSA_PUBSTR(const Request & req, Response & res){
    RSA_PUBSTR_ack ack_t;
    std::shared_ptr<envelop> enve= MagicSingleton<envelop>::GetInstance();
    std::string pubstr=enve->getPubstr();
    Base64 base_;
    ack_t.rsa_pubstr=base_.Encode((const unsigned char*)pubstr.c_str(),pubstr.size());
    res.set_content(ack_t.paseToString(), "application/json");  
}

