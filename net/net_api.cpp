#include <sstream>
#include "net_api.h"
#include "./global.h"
#include <string>
#include <arpa/inet.h>
#include <signal.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>
#include "./dispatcher.h"
#include "net.pb.h"
#include "common.pb.h"
#include "./socket_buf.h"
#include "./work_thread.h"
#include "./epoll_mode.h"
#include "http_server.h"
#include <utility>
#include "./ip_port.h"
#include "../common/global.h"
#include "logging.h"
#include "../ca/ca_global.h"
#include "../ca/ca_transaction.h"
#include "./peer_node.h"
#include "../utils/time_util.h"
#include "../utils/console.h"
#include "utils/AccountManager.h"
#include "utils/Cycliclist.hpp"
#include "utils/tmplog.h"
#include "global.h"
#include "ca/ca_algorithm.h"

int net_tcp::Socket(int family, int type, int protocol)
{
	int n;

	if ((n = socket(family, type, protocol)) < 0)
		ERRORLOG("can't create socket file");
	return n;
}

int net_tcp::Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
	int n;

	if ((n = accept(fd, sa, salenptr)) < 0)
	{
		if ((errno == ECONNABORTED) || (errno == EINTR) || (errno == EWOULDBLOCK))
		{
			goto ret;
		}
		else
		{
			ERRORLOG("accept error");
		}
	}
ret:
	return n;
}

int net_tcp::Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
	int n;

	if ((n = bind(fd, sa, salen)) < 0)
		ERRORLOG("bind error");
	return n;
}

int net_tcp::Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
	int n;
	
	int nBufLen;
	int nOptLen = sizeof(nBufLen);
	getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&nBufLen, (socklen_t *)&nOptLen);

	int nRecvBuf = 1 * 1024 * 1024;
	Setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const void *)&nRecvBuf, sizeof(int));
	int nSndBuf = 1 * 1024 * 1024;
	Setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const void *)&nSndBuf, sizeof(int));
	getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&nBufLen, (socklen_t *)&nOptLen);

	if ((n = connect(fd, sa, salen)) < 0)
	{
	}

	return n;
}

int net_tcp::Listen(int fd, int backlog)
{
	int n;

	if ((n = listen(fd, backlog)) < 0)
		ERRORLOG("listen error");
	return n;
}

int net_tcp::Send(int sockfd, const void *buf, size_t len, int flags)
{
	if (sockfd < 0)
	{
		ERRORLOG("Send func: file description err"); // Error sending file descriptor
		return -1;
	}
	int bytes_left;
	int written_bytes;
	char *ptr;
	ptr = (char *)buf;
	bytes_left = len;
	while (bytes_left > 0)
	{
		written_bytes = write(sockfd, ptr, bytes_left);
		if (written_bytes <= 0) /* Something went wrong */
		{
			if (written_bytes == 0)
			{
				continue;
			}
			if (errno == EINTR)
			{
				continue;
			}
			else if (errno == EAGAIN) /* EAGAIN : Resource temporarily unavailable*/
			{

				return len - bytes_left;
			}
			else 
			{
				MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(sockfd);
				return -1;
			}
		}

		bytes_left -= written_bytes;
		ptr += written_bytes; /* Continue writing from the rest of the place */
	}
	return len;
}

int net_tcp::Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int ret;

	if ((ret = setsockopt(fd, level, optname, optval, optlen)) == -1)
		ERRORLOG("setsockopt error");
	return ret;
}

int net_tcp::listen_server_init(int port, int listen_num)
{
	struct sockaddr_in servaddr;
	int listener;
	int opt = 1;
	listener = Socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any addr
	servaddr.sin_port = htons(port);

	Setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt,
			   sizeof(opt));
	Setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, (const void *)&opt,
			   sizeof(opt));

	Bind(listener, (struct sockaddr *)&servaddr, sizeof(servaddr));

	int nRecvBuf = 1 * 1024 * 1024;
	Setsockopt(listener, SOL_SOCKET, SO_RCVBUF, (const void *)&nRecvBuf, sizeof(int));
	int nSndBuf = 1 * 1024 * 1024;
	Setsockopt(listener, SOL_SOCKET, SO_SNDBUF, (const void *)&nSndBuf, sizeof(int));
	Listen(listener, listen_num);

	return listener;
}

int net_tcp::set_fd_noblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
	{
		ERRORLOG("setnonblock error");
		return -1;
	}
	return 0;
}

int net_com::connect_init(u32 u32_ip, u16 u16_port, u16 &connect_port)
{
	int confd = 0;
	struct sockaddr_in servaddr = {0};
	struct sockaddr_in my_addr = {0};
	int ret = 0;

	confd = Socket(AF_INET, SOCK_STREAM, 0);
	int flags = 1;
	Setsockopt(confd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
	flags = 1;
	Setsockopt(confd, SOL_SOCKET, SO_REUSEPORT, &flags, sizeof(int));

	// Connect to each other
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(u16_port);
	struct in_addr addr = {0};
	memcpy(&addr, &u32_ip, sizeof(u32_ip));
	inet_pton(AF_INET, inet_ntoa(addr), &servaddr.sin_addr);

	/*The default timeout timeout for Linux systems is 75s during blocking conditions*/
	if (set_fd_noblocking(confd) < 0)
	{
		DEBUGLOG("setnonblock error");
		return -1;
	}

	ret = Connect(confd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
	getsockname(confd, (struct sockaddr*)&clientAddr, &clientAddrLen); // Gets the local address on the connection represented by sockfd
	connect_port = ntohs(clientAddr.sin_port);

	if (ret != 0)
	{
		if (errno == EINPROGRESS)
		{
			struct epoll_event newPeerConnectionEvent;
			int epollFD = -1;
			struct epoll_event processableEvents;
			unsigned int numEvents = -1;

			if ((epollFD = epoll_create(1)) == -1)
			{
				ERRORLOG("Could not create the epoll FD list!");
				close(confd);
				return -2;
			}     

			newPeerConnectionEvent.data.fd = confd;
			newPeerConnectionEvent.events = EPOLLOUT | EPOLLIN | EPOLLERR;

			if (epoll_ctl(epollFD, EPOLL_CTL_ADD, confd, &newPeerConnectionEvent) == -1)
			{
				ERRORLOG("Could not add the socket FD to the epoll FD list!");
				close(confd);
				close(epollFD);
				return -3;
			}

			numEvents = epoll_wait(epollFD, &processableEvents, 1, 3*1000);

			if (numEvents < 0)
			{
				ERRORLOG("Serious error in epoll setup: epoll_wait () returned < 0 status!");
				close(epollFD);
				close(confd);
				return -4;
			}
			int retVal = -1;
			socklen_t retValLen = sizeof (retVal);
			if (getsockopt(confd, SOL_SOCKET, SO_ERROR, &retVal, &retValLen) < 0)
			{
				ERRORLOG("getsockopt SO_ERROR error!");
				close(confd);
				close(epollFD);
				return -5;
			}

			if (retVal == 0)  // succeed
			{
				close(epollFD);
				return confd;
			} 
			else
			{
				close(epollFD);
				close(confd);
				return -6;
			}	
		}
		else
		{
			close(confd);
			return -7;			
		}
	}

	return confd;
}


bool net_com::send_one_message(const Node &to, const net_pack &pack)
{
	auto msg = Pack::packag_to_str(pack);
	uint8_t priority = pack.flag & 0xF;

	return send_one_message(to, msg, priority);
}

bool net_com::send_one_message(const Node &to, const std::string &msg, const int8_t priority)
{
	MsgData send_data;
	send_data.type = E_WRITE;
	send_data.fd = to.fd;
	send_data.ip = to.public_ip;
	send_data.port = to.public_port;
	
	MagicSingleton<BufferCrol>::GetInstance()->add_write_pack(send_data.ip, send_data.port, msg);
	bool bRet = global::queue_write.push(send_data);
	return true;

}

bool net_com::send_one_message(const MsgData& to, const net_pack &pack)
{
	MsgData send_data;
	send_data.type = E_WRITE;
	send_data.fd = to.fd;
	send_data.ip = to.ip;
	send_data.port = to.port;

	auto msg = Pack::packag_to_str(pack);	
	MagicSingleton<BufferCrol>::GetInstance()->add_write_pack(send_data.ip, send_data.port, msg);
	bool bRet = global::queue_write.push(send_data);
	return bRet;
}


uint64_t net_data::pack_port_and_ip(uint16_t port, uint32_t ip)
{
	uint64_t ret = port;
	ret = ret << 32 | ip;
	return ret;
}

uint64_t net_data::pack_port_and_ip(int port, std::string ip)
{
	uint64_t ret = port;
	uint32_t tmp;
	inet_pton(AF_INET, ip.c_str(), &tmp);
	ret = ret << 32 | tmp;
	return ret;
}
std::pair<uint16_t, uint32_t> net_data::apack_port_and_ip_to_int(uint64_t port_and_ip)
{
	uint64_t tmp = port_and_ip;
	uint32_t ip = tmp << 32 >> 32;
	uint16_t port = port_and_ip >> 32;
	return std::pair<uint16_t, uint32_t>(port, ip);
}
std::pair<int, std::string> net_data::apack_port_and_ip_to_str(uint64_t port_and_ip)
{
	uint64_t tmp = port_and_ip;
	uint32_t ip = tmp << 32 >> 32;
	uint16_t port = port_and_ip >> 32;
	char buf[100];
	inet_ntop(AF_INET, (void *)&ip, buf, 16);
	return std::pair<uint16_t, std::string>(port, buf);
}

int net_com::parse_conn_kind(Node &to)
{
	to.conn_kind = DRTO2O; //Outer and external direct connection
	return to.conn_kind;
}

bool net_com::is_need_send_trans_message(const Node & to)
{
	Node node;
	bool isFind = MagicSingleton<PeerNode>::GetInstance()->find_node(to.base58address, node);
	auto self_node  = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	
	if ( isFind )
	{
		if ((to.conn_kind != NOTYET))
		{
			// Found, and not unconnected or forwarded, can be sent directly
			return false;
		}
		else
		{
			// Found, need to forward
			return true;
		}
	}
	else
	{
		if (to.public_base58addr.empty())
		{
			// There is no connection to the public network forwarding server, 
			// and it is generally necessary to connect for the first time
			if (to.listen_ip != 0)
			{
				return false;
			}
			else
			{
				return true;
			}
		}
		else
		{
			// Nodes that cannot be seen by relying on Internet forwarding
			return true;
		}
	}
}


void handle_pipe(int sig)
{
	// Do nothing
}

bool net_com::net_init()
{
	// Capture SIGPIPE signal to prevent accidental exit of the program
	struct sigaction sa;
	sa.sa_handler = handle_pipe;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, NULL);

	// Block the SIGPIPE signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);

	// Ignore the SIGPIPE signal
	signal(SIGPIPE, SIG_IGN);


	// Register the callback function
	MagicSingleton<ProtobufDispatcher>::GetInstance()->registerAll();

	if (MagicSingleton<Config>::GetInstance()->GetIP().empty())
	{
		std::string localhost_ip;
		if (!IpPort::get_localhost_ip(localhost_ip))
		{
			DEBUGLOG("Failed to obtain the local Intranet IP address.");
			return false;
		}
		MagicSingleton<Config>::GetInstance()->SetIP(localhost_ip);
	}

	global::local_ip = MagicSingleton<Config>::GetInstance()->GetIP();	
	
	// Get the native intranet IP address
	if(global::local_ip.empty())
	{
		DEBUGLOG("IP address is empty.");
		return false;
	}
	
	if (global::kBuildType == global::BuildType::kBuildType_Primary && 
		IpPort::is_public_ip(global::local_ip) == false)
	{
		ERRORLOG("Primary net local IP must be public IP");
		return false;
	}


	INFOLOG("The Intranet ip is not empty");
	
	Account acc;
	EVP_PKEY_free(acc.GetKey());
	if (MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
	{
		return false;
	}

	MagicSingleton<PeerNode>::GetInstance()->set_self_id(acc.GetBase58());
	MagicSingleton<PeerNode>::GetInstance()->set_self_identity(acc.GetPubStr());
	MagicSingleton<PeerNode>::GetInstance()->set_self_height();
	
	MagicSingleton<PeerNode>::GetInstance()->set_self_ip_l(IpPort::ipnum(global::local_ip.c_str()));
	MagicSingleton<PeerNode>::GetInstance()->set_self_port_l(SERVERMAINPORT);
	MagicSingleton<PeerNode>::GetInstance()->set_self_ip_p(IpPort::ipnum(global::local_ip.c_str()));
	MagicSingleton<PeerNode>::GetInstance()->set_self_port_p(SERVERMAINPORT);

	Config::Info info = {};
	MagicSingleton<Config>::GetInstance()->GetInfo(info);

	MagicSingleton<PeerNode>::GetInstance()->set_self_name(info.name);
	MagicSingleton<PeerNode>::GetInstance()->set_self_logo(  info.logo );

	MagicSingleton<PeerNode>::GetInstance()->set_self_ver(global::kVersion);
		
	

	// Work thread pool start
	MagicSingleton<WorkThreads>::GetInstance()->start();

	// Create a listening thread
	MagicSingleton<EpollMode>::GetInstance()->start();
	
	// Start "refresh nodelist" thread 
    MagicSingleton<PeerNode>::GetInstance()->nodelist_refresh_thread_init();
	
	//Start Network node switching
	MagicSingleton<PeerNode>::GetInstance()->nodelist_switch_thread();

	// Start the heartbeat
	global::heart_timer.AsyncLoop(HEART_INTVL * 1000, net_com::DealHeart);


	return true;
}

//Test single message
int net_com::input_send_one_message()
{
	DEBUGLOG(RED "input_send_one_message start" RESET);
	string id;
	cout << "please input id:";
	cin >> id;

	while (true)
	{
		//Verify that the ID is legitimate
		bool result = CheckBase58Addr(id, Base58Ver::kBase58Ver_Normal);
		if (false == result)
		{
			cout << "invalid id , please input id:";
			cin >> id;
			continue;
		}
		else
		{
			break;
		}
	};

	string msg;
	cout << "please input msg:";
	cin >> msg;

	int num;
	cout << "please input num:";
	cin >> num;

	bool bl;
	for (int i = 0; i < num; ++i)
	{
		bl = net_com::SendPrintMsgReq(id, msg);

		if (bl)
		{
			printf("The %d send success\n", i + 1);
		}
		else
		{
			printf("The0 %d send fail\n", i + 1);
		}

	}
	return bl ? 0 : -1;
}



//Test the broadcast information
int net_com::test_broadcast_message()
{
	string str_buf = "Hello World!";

	PrintMsgReq printMsgReq;
	printMsgReq.set_data(str_buf);

	bool isSucceed = net_com::broadcast_message(printMsgReq);
    if(isSucceed == false)
    {
        ERRORLOG(":broadcast PrintMsgReq failed!");
        return -1;
    }
	return 0;
}


bool net_com::test_send_big_data()
{
	string id;
	cout << "please input id:";
	cin >> id;
	auto is_vaild = [](string id_str) {
		int count = 0;
		for (auto i : id_str)
		{
			if (i != '1' || i != '0')
				return false;
			count++;
		}
		return count == 16;
	};
	while (is_vaild(id))
	{
		cout << "invalid id , please input id:";
		cin >> id;
	};
	Node tmp_node;
	if (!MagicSingleton<PeerNode>::GetInstance()->find_node(std::string(id), tmp_node))
	{
		DEBUGLOG("invaild id, not in my peer node");
		return false;
	}
	string tmp_data;
	int txtnum;
	cout << "please input test byte num:";
	cin >> txtnum;
	for (int i = 0; i < txtnum; i++)
	{
		char x, s;									  
		s = (char)rand() % 2;						  
		if (s == 1)									  
			x = (char)rand() % ('Z' - 'A' + 1) + 'A'; 
		else
			x = (char)rand() % ('z' - 'a' + 1) + 'a'; 
		tmp_data.push_back(x);						 
	}
	tmp_data.push_back('z');
	tmp_data.push_back('z');
	tmp_data.push_back('z');
	tmp_data.push_back('z');
	tmp_data.push_back('z');


	net_com::SendPrintMsgReq(tmp_node, tmp_data, 1);
	return true;
}

bool net_com::SendPrintMsgReq(Node &to, const std::string data, int type)
{
	PrintMsgReq printMsgReq;
	printMsgReq.set_data(data);
	printMsgReq.set_type(type);
	net_com::send_message(to, printMsgReq);
	return true;
}

bool net_com::SendPrintMsgReq(const std::string & id, const std::string data, int type)
{
	PrintMsgReq printMsgReq;
	printMsgReq.set_data(data);
	printMsgReq.set_type(type);
	net_com::send_message(id, printMsgReq);
	return true;
}


int net_com::SendRegisterNodeReq(Node& dest, std::string &msg_id, bool get_nodelist)
{
	INFOLOG("SendRegisterNodeReq");

	RegisterNodeReq getNodes;
	getNodes.set_is_get_nodelist(get_nodelist);
	getNodes.set_msg_id(msg_id);
	NodeInfo* mynode = getNodes.mutable_mynode();
	const Node & selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();

	if (dest.fd > 0)
	{
		return -1;
	}

	if(!CheckBase58Addr(selfNode.base58address, Base58Ver::kBase58Ver_Normal))
	{
		ERRORLOG(" SendRegisterNodeReq selfNode.base58address {} error",selfNode.base58address);
		return -3;
	}

    mynode->set_base58addr(selfNode.base58address);
	mynode->set_name(selfNode.name);
	mynode->set_listen_ip( selfNode.listen_ip);
	mynode->set_logo(selfNode.logo);
	mynode->set_listen_port( selfNode.listen_port);

	mynode->set_time_stamp(MagicSingleton<TimeUtil>::GetInstance()->getUTCTimestamp());
	mynode->set_height(MagicSingleton<PeerNode>::GetInstance()->get_self_chain_height_newest());
	mynode->set_version(global::kVersion);
	// sign
	std::string signature;
	Account acc;
	EVP_PKEY_free(acc.GetKey());
	if(MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(acc) != 0)
	{
		return -4;
	}
	if (selfNode.base58address != acc.GetBase58())
	{
		return -5;
	}
	if(!acc.Sign(getsha256hash(acc.GetBase58()), signature))
	{
		return -6;
	}

	mynode->set_identity(acc.GetPubStr());
	mynode->set_sign(signature);

	// connect
	if (MagicSingleton<PeerNode>::GetInstance()->connect_node(dest) != 0)
	{
		ERRORLOG(" connect_node error ip:{}", IpPort::ipsz(dest.public_ip));
		return -7;
	}
 
	net_com::send_message(dest, getNodes, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
	
	return 0;
}

void net_com::SendPingReq(const Node& dest)
{
	PingReq pingReq;
	pingReq.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());
	net_com::send_message(dest, pingReq, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
}

void net_com::SendPongReq(const Node& dest)
{
	PongReq pongReq;
	pongReq.set_id(MagicSingleton<PeerNode>::GetInstance()->get_self_id());

	net_com::send_message(dest, pongReq, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
}

void net_com::DealHeart()
{
	Node mynode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();	
	vector<Node> pubNodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();

	//Exclude yourself
	vector<Node>::iterator end = pubNodeList.end();
	for(vector<Node>::iterator it = pubNodeList.begin(); it != end; ++it)
	{
		if(mynode.base58address == it->base58address)
		{
			it = pubNodeList.erase(it);
		}
	}
	vector<Node> nodelist;
	nodelist.insert(nodelist.end(),pubNodeList.begin(),pubNodeList.end());
	for(auto &node:nodelist)
	{
		node.heart_probes -= 1;
		if(node.heart_probes <= 0)
		{
			DEBUGLOG("DealHeart delete node: {}", node.base58address);

			MagicSingleton<PeerNode>::GetInstance()->delete_node(node.base58address);
		}
		else
		{
			net_com::SendPingReq(node);
		}
		MagicSingleton<PeerNode>::GetInstance()->update(node);
	}	
}

bool net_com::SendSyncNodeReq(const Node& dest, std::string &msg_id)
{
	DEBUGLOG("SendSyncNodeReq from.ip:{}", IpPort::ipsz(dest.public_ip));
	SyncNodeReq syncNodeReq;
	//Get its own node information
	auto self_node = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	
	if(nodelist.size() == 0)
	{
		return false;
	}
	//Stores its own node ID
	syncNodeReq.set_ids(std::move(self_node.base58address));
	syncNodeReq.set_msg_id(msg_id);
	return net_com::send_message(dest, syncNodeReq, net_com::Compress::kCompress_True, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
}

void net_com::SendNodeHeightChanged()
{
	NodeHeightChangedReq heightChangeReq;
	std::string selfId = MagicSingleton<PeerNode>::GetInstance()->get_self_id();

	heightChangeReq.set_id(selfId);
	uint32 chainHeight = 0;
	int ret = net_callback::chain_height_callback(chainHeight);
	heightChangeReq.set_height(chainHeight);

	Account defaultEd;
	MagicSingleton<AccountManager>::GetInstance()->GetDefaultAccount(defaultEd);

	std::stringstream base58Height;

	base58Height << selfId << "_" << std::to_string(chainHeight);
	std::string serVinHash = getsha256hash(base58Height.str());
	std::string signature;
	std::string pub;

	if (defaultEd.Sign(serVinHash, signature) == false)
	{
		std::cout << "tx sign fail !" << std::endl;
	}
	CSign * sign = heightChangeReq.mutable_sign();
	sign->set_sign(signature);
	sign->set_pub(defaultEd.GetPubStr());


	auto selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
	std::vector<Node> publicNodes = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	for (auto& node : publicNodes)
	{
		// DEBUGLOG("send node height {} to {}", chainHeight, node.base58address);
		net_com::send_message(node, heightChangeReq, net_com::Compress::kCompress_False, net_com::Encrypt::kEncrypt_False, net_com::Priority::kPriority_High_2);
	}
}


namespace net_callback
{
	std::function<int(uint32_t &)> chain_height_callback =  nullptr;
}

void net_callback::register_chain_height_callback(std::function<int(uint32_t &)> callback)
{
	net_callback::chain_height_callback = callback;
}



bool net_com::BlockBroadcast_message( BuildBlockBroadcastMsg& BuildBlockMsg, const net_com::Compress isCompress, const net_com::Encrypt isEncrypt, const net_com::Priority priority)
{	

	const std::vector<Node>&& publicNodeList = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	if(global::kBuildType == global::BuildType::kBuildType_Dev)
	{
		std::cout << "Total number of public nodelists:" << publicNodeList.size() << std::endl;
	}
	if(publicNodeList.empty())
	{
		ERRORLOG("publicNodeList is empty!");
		return false;
	}

	const double threshold = 0.25;
	const std::size_t cntUnConnected = std::count_if(publicNodeList.cbegin(), publicNodeList.cend(), [](const Node &node){ return node.fd == -1;});
	double percent = static_cast<double>(cntUnConnected) / publicNodeList.size();
	if(percent > threshold)
	{
		ERRORLOG("Unconnected nodes are {},accounting for {}%", cntUnConnected, percent * 100);
		return false;
	}
	std::cout << "Verification passed, start broadcasting!" << std::endl;

	CBlock block;
    block.ParseFromString(BuildBlockMsg.blockraw());

	int top_=block.height();
	
	auto getNextNumber=[&](int limit) ->int {
	  	std::random_device seed;
	 	std::ranlux48 engine(seed());
	 	std::uniform_int_distribution<int> u(0, limit-1);
	 	return u(engine);
	};

	auto getTargetIndexs=[&](int num,int limit,const std::vector<Node> & source)->std::set<std::string>
	{
		std::set<std::string> all_addrs;
		if(limit < num){
			ERRORLOG(" The source is less the num !! [limit:{}],[num:{}]",limit,num);
			return all_addrs;
		}
		else if(limit == num)
		{
			for(const auto & node : source)
			{
				all_addrs.insert(node.base58address);
			}
			return all_addrs;
		}

		while(all_addrs.size()< num){
			int index=getNextNumber(limit);
			all_addrs.insert(source[index].base58address);
		}
		return all_addrs;
	};

	auto getRootOfEquation=[](int list_size)->int
	{
		int x1=(-1+std::sqrt(1-4*(-list_size)))/2;
		int x2=(-1-std::sqrt(1-4*(-list_size)))/2;
		return (x1 >0) ? x1:x2;
	};

	std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
	std::set<std::string> addrs;
	if(block.height() < 500)
	{
		if(nodelist.size() <= global::broadcast_threshold)
		{
			BuildBlockMsg.set_type(2);//Set the number of broadcasts to two
			for(auto & node:nodelist){
				net_com::send_message(node.base58address, BuildBlockMsg);
				addrs.insert(node.base58address);
			}

		}else{
			std::set<std::string> addrs = getTargetIndexs(global::broadcast_threshold,nodelist.size(),nodelist);
			BuildBlockMsg.set_type(1);//Set the number of broadcasts to 1
			
			for(auto &addr:addrs)
			{
				BuildBlockMsg.add_castaddrs(addr);	
			}
			for(auto & addr:addrs)
			{
				net_com::send_message(addr, BuildBlockMsg);	
			}
		}
	}
	else
	{
		std::vector<Node> eligibleAddress;
		for(auto & node : nodelist)
		{
			//Verification of  pledge
			int64_t stake_time = ca_algorithm::GetPledgeTimeByAddr(node.base58address, global::ca::StakeType::kStakeType_Node);
			if (stake_time > 0)
			{
				eligibleAddress.push_back(node);
			}
		}

		if(nodelist.size() >= global::broadcast_threshold * global::broadcast_threshold)
		{//2500 m+m^2=nodelist.size();
			int m = getRootOfEquation(nodelist.size());
			int threshold = eligibleAddress.size() > m ? m : eligibleAddress.size();
			addrs = getTargetIndexs(threshold, eligibleAddress.size(), eligibleAddress);
			
			BuildBlockMsg.set_type(1);//Set the number of broadcasts to 1
			for(auto &addr:addrs)
			{
				BuildBlockMsg.add_castaddrs(addr);	
			}
			
			for(auto & addr:addrs){
				DEBUGLOG("Level 1 broadcasting address: {} , block hash : {}", addr,block.hash().substr(0,6));			
				net_com::send_message(addr, BuildBlockMsg);
			}
		}
		else
		{
			int threshold = eligibleAddress.size() > global::broadcast_threshold ? global::broadcast_threshold : eligibleAddress.size();
			std::set<std::string> addrs = getTargetIndexs(threshold, eligibleAddress.size(), eligibleAddress);

			BuildBlockMsg.set_type(1);//Set the number of broadcasts to 1
			
			for(auto & addr : addrs)
			{
				BuildBlockMsg.add_castaddrs(addr);	
			}

			for(auto & addr : addrs)
			{
				//DEBUGLOG("Level 1 broadcasting address {} , block hash : {}", addr,block.hash().substr(0,6));
				net_com::send_message(addr, BuildBlockMsg);
			}
		}
	}
	for(auto & addr : addrs)
	{
		DEBUGLOG("Level 1 broadcasting address {} , block hash : {}", addr,block.hash().substr(0,6));
	}
	MagicSingleton<BlockStroage>::GetInstance()->AddBlockStatus(block.hash(), block, addrs);
	return true;
}