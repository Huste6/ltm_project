#ifndef AUTH_H
#define AUTH_H

#include "../database/database.h"
#include "../protocol/protocol.h"

typedef struct ClientSession ClientSession;
typedef struct Server Server;

/**
 * @brief Xử lý đăng ký tài khoản mới
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (REGISTER username|password)
 * 
 * Flow:
 * 1. Validate username format (3-20 chars, alphanumeric + underscore)
 * 2. Validate password strength (8+ chars, upper+lower+digit)
 * 3. Check username không tồn tại trong DB
 * 4. Hash password bằng SHA256
 * 5. Lưu vào database
 * 6. Response: 100 CREATED hoặc 4xx error
 */
void handle_register(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý đăng nhập
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (LOGIN username|password)
 * 
 * Flow:
 * 1. Hash password và verify với DB
 * 2. Check user chưa login ở nơi khác
 * 3. Check tài khoản không bị khóa
 * 4. Generate session_id unique
 * 5. Lưu session vào DB
 * 6. Update client state
 * 7. Response: 110 LOGIN_OK <session_id>
 */
void handle_login(Server *server, ClientSession *client, Message *msg);

/**
 * @brief Xử lý đăng xuất
 * @param server Pointer tới Server instance
 * @param client Pointer tới ClientSession
 * @param msg Message đã parse (LOGOUT)
 * 
 * Flow:
 * 1. Destroy session trong DB
 * 2. Clear client session data
 * 3. Update state về CONNECTED
 * 4. Response: 132 LOGOUT_OK
 */
void handle_logout(Server *server, ClientSession *client);

/**
 * @brief Kiểm tra client đã authenticate chưa
 * @param client ClientSession cần check
 * @return 1 nếu đã auth, 0 nếu chưa
 */
int check_authentication(ClientSession *client);

/**
 * @brief Hash password bằng SHA256
 * @param input Password plaintext
 * @param output Buffer để lưu hash (65 bytes: 64 hex + null)
 */
void sha256_hash(const char *input, char output[65]);

#endif // AUTH_H