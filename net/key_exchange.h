#ifndef _KEYEXCHANGE_H_
#define _KEYEXCHANGE_H_

#include <map>
#include <memory>
#include <cstdint>
#include <mutex>
#include "utils/crypto.h"
#include "node.hpp"
#include "./msg_queue.h"
#include "key_exchange.pb.h"
// Struct definitions
struct ownkey_s {
    uint8_t ec_pub_key[CRYPTO_EC_PUB_KEY_LEN];
    uint8_t ec_priv_key[CRYPTO_EC_PRIV_KEY_LEN];
    uint8_t salt[CRYPTO_SALT_LEN]; // Just used in key exchange
};

struct peerkey_s {
    uint8_t ec_pub_key[CRYPTO_EC_PUB_KEY_LEN];
    uint8_t aes_key[CRYPTO_AES_KEY_LEN];
    uint8_t salt[CRYPTO_SALT_LEN]; // Just used in key exchange
};

struct EcdhKey {
    ownkey_s own_key;
    peerkey_s peer_key;
    uint64_t timeout;
};

class KeyExchangeManager {
public:
    // Typedef for KeyExchangeMap entry
    using KeyPtr = std::shared_ptr<EcdhKey>;

    // Interface functions
    void addKey(int fd, const EcdhKey& key);
    void removeKey(int fd);
    KeyPtr getKey(int fd);
    bool hasKey(int fd);

    bool key_calculate(const ownkey_s &ownkey, peerkey_s &peerkey);

    int SendKeyExchangeReq(Node &dest);
    int HandleKeyExchangeReq(const std::shared_ptr<KeyExchangeRequest> &keyExchangeReq, const MsgData &from);
    int handleKeyExchangeAck(const std::shared_ptr<KeyExchangeResponse> &keyExchangeAck, const MsgData &from);

private:
    std::map<int, KeyPtr> KeyExchangeMap;
    mutable std::shared_mutex mtx; // Mutex for protecting concurrent access to KeyExchangeMap
};

int HandleKeyExchangeReq(const std::shared_ptr<KeyExchangeRequest> &keyExchangeReq, const MsgData &from);
int HandleKeyExchangeAck(const std::shared_ptr<KeyExchangeResponse> &keyExchangeAck, const MsgData &from);

bool encrypt_plaintext(const peerkey_s &peerkey, const std::string &str_plaintext, Ciphertext &ciphertext);
bool decrypt_ciphertext(const peerkey_s &peerkey, const Ciphertext &ciphertext, std::string &plaintext);
bool verify_token(const uint8_t ecdh_pub_key[CRYPTO_EC_PUB_KEY_LEN], const Token &token);
bool generate_token(const uint8_t ecdh_pub_key[CRYPTO_EC_PUB_KEY_LEN], Token &token);

#endif // _KEYEXCHANGE_H_