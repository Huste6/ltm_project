#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "database/database.h"

int main() {
    Database db;
    
    const char *host = "127.0.0.1"; 
    const char *user = "exam_user";
    const char *password = "exam123456";
    const char *dbname = "exam_system";
    unsigned int port = 3306; 
    
    printf("=== TEST KẾT NỐI CƠ SỞ DỮ LIỆU ===\n");
    printf("Đang kết nối đến database...\n");
    printf("Host: %s\n", host);
    printf("User: %s\n", user);
    printf("Database: %s\n", dbname);
    printf("========================================\n\n");
    
    // Thử kết nối
    if (db_connect_with_port(&db, host, user, password, dbname, port) == 0) {
        printf("✅ KẾT NỐI THÀNH CÔNG!\n");
        printf("Database đã sẵn sàng để sử dụng.\n\n");
        
        // Test một query đơn giản
        printf("Đang test query cơ bản...\n");
        MYSQL_RES *result;
        MYSQL_ROW row;
        
        if (mysql_query(db.conn, "SELECT COUNT(*) FROM users") == 0) {
            result = mysql_store_result(db.conn);
            if (result != NULL) {
                row = mysql_fetch_row(result);
                if (row != NULL) {
                    printf("✅ Query test thành công!\n");
                    printf("Số lượng users trong database: %s\n", row[0]);
                } else {
                    printf("⚠️  Không thể lấy dữ liệu từ query\n");
                }
                mysql_free_result(result);
            } else {
                printf("⚠️  Không thể lưu kết quả query: %s\n", mysql_error(db.conn));
            }
        } else {
            printf("❌ Query test thất bại: %s\n", mysql_error(db.conn));
        }
        
        // Test thêm: kiểm tra các bảng
        printf("\nKiểm tra các bảng trong database...\n");
        if (mysql_query(db.conn, "SHOW TABLES") == 0) {
            result = mysql_store_result(db.conn);
            if (result != NULL) {
                printf("✅ Các bảng có trong database:\n");
                while ((row = mysql_fetch_row(result)) != NULL) {
                    printf("  - %s\n", row[0]);
                }
                mysql_free_result(result);
            }
        } else {
            printf("❌ Không thể lấy danh sách bảng: %s\n", mysql_error(db.conn));
        }
        
        // Đóng kết nối
        db_disconnect(&db);
        printf("\n✅ Đã đóng kết nối database.\n");
        
    } else {
        printf("❌ KẾT NỐI THẤT BẠI!\n");
        printf("Vui lòng kiểm tra:\n");
        printf("1. MySQL server đã chạy chưa?\n");
        printf("2. Thông tin kết nối có đúng không?\n");
        printf("3. Database 'exam_system' đã được tạo chưa?\n");
        printf("4. User 'exam_user' có quyền truy cập không?\n");
        return 1;
    }
    
    return 0;
}
