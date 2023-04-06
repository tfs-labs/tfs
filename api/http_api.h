#include "../net/http_server.h"
#include "ca_test.h"



void ca_register_http_callbacks();

void api_start_autotx(const Request & req, Response & res);

void api_end_autotx(const Request & req, Response & res);
void api_status_autotx(const Request & req, Response & res);
bool api_status_autotx_test(Response & res);
void api_end_autotx_test(Response & res);


void api_jsonrpc(const Request & req, Response & res);

void api_print_block(const Request & req, Response & res);
void api_info(const Request & req, Response & res);
void api_get_block(const Request & req, Response & res);

void api_pub(const Request & req, Response & res);
void api_filter_height(const Request &req, Response &res);


void api_get_log_line(const Request & req,Response & res);
void get_all_system_info(const Request &req, Response &res);
std::string api_get_cpu_info();
std::string api_get_system_info();
std::string get_net_rate();
std::string get_process_info();
void api_get_real_bandwidth(const Request &req, Response &res);
void api_get_block_info(const Request &req, Response &res);
void api_get_tx_info(const Request &req,Response &res);


nlohmann::json jsonrpc_get_height(const nlohmann::json & param);
nlohmann::json jsonrpc_get_balance(const nlohmann::json & param);


bool jsonrpc_get_sigvalue(const std::string& addr, const std::string& message, std::string & signature, std::string& pub);
void jsonrpc_get_StakeUtxos(const Request & req, Response & res);
void jsonrpc_get_DisinvestUtxos(const Request & req, Response & res);
void jsonrpc_get_TxTransaction(const Request & req, Response & res);
void jsonrpc_get_StakeTransaction(const Request & req, Response & res);
void jsonrpc_get_UnstakeTransaction(const Request & req, Response & res);
void jsonrpc_get_InvestTransaction(const Request & req, Response & res);
void jSonrpc_get_DisinvestTransaction(const Request & req, Response & res);
void jSonrpc_get_DeclareTransaction(const Request & req, Response & res);
void jSonrpc_get_BonusTransaction(const Request & req, Response & res);
void jSonrpc_get_RSA_PUBSTR(const Request & req, Response & res);
void deploy_contract(const Request & req, Response & res);
