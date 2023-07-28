#include "../net/http_server.h"
#include "ca_test.h"
#include "../net/unregister_node.h"
#include "net/httplib.h"

void ca_register_http_callbacks();

#ifndef NDEBUG
void api_get_log_line(const Request & req,Response & res);
void get_all_system_info(const Request &req, Response &res);
std::string api_get_cpu_info();
std::string api_get_system_info();
std::string get_net_rate();
std::string get_process_info();
std::string api_time();
void api_get_real_bandwidth(const Request &req, Response &res);
void api_get_block_info(const Request &req, Response &res);
void api_get_tx_info(const Request &req,Response &res);

void api_print_block(const Request & req, Response & res);
void api_info(const Request & req, Response & res);
void api_get_block(const Request & req, Response & res);

void api_start_autotx(const Request & req, Response & res);
void api_end_autotx(const Request & req, Response & res);
void api_status_autotx(const Request & req, Response & res);

void api_pub(const Request & req, Response & res);
void api_filter_height(const Request &req, Response &res);

void api_jsonrpc(const Request & req, Response & res);
void api_test_echo(const Request & req, Response & res);
void api_get_rates_info(const Request &req,Response &res);

#endif


bool api_status_autotx_test(Response & res);
void api_end_autotx_test(Response & res);

void jsonrpc_get_height(const Request &req,Response &res);
void jsonrpc_get_balance(const Request &req, Response &res) ;
void get_stakeutxo(const Request & req, Response & res);
void get_disinvestutxo(const Request & req, Response & res);
void get_transaction(const Request & req, Response & res);
void get_stake(const Request & req, Response & res);
void get_unstake(const Request & req, Response & res);
void get_invest(const Request & req, Response & res);
void get_disinvest(const Request & req, Response & res);
void get_declare(const Request & req, Response & res);
void get_bonus(const Request & req, Response & res);
void get_rsa_pub(const Request & req, Response & res);
void deploy_contract(const Request & req, Response & res);
void call_contract(const Request & req, Response & res);

void send_message(const Request & req, Response & res);

void get_isonchain(const Request & req, Response & res);

void get_deployer(const Request & req, Response & res);

void get_deployerutxo(const Request & req, Response & res);


void get_restinvest(const Request & req, Response & res);
void api_ip(const Request &req, Response & res);
void api_normal(const Request & req, Response & res);

void get_all_stake_node_list_ack(const Request & req,Response & res);
void api_get_rates_info(const Request &req,Response &res);
