# Exam System Database Docker Setup

Hệ thống cơ sở dữ liệu cho ứng dụng thi trắc nghiệm sử dụng Docker và MySQL.

## 📋 Yêu cầu

- Docker Desktop đã được cài đặt và đang chạy
- Docker Compose

## 🚀 Cách sử dụng

### 1. Khởi động database

**Trên Windows:**
```cmd
start-database.bat
```

**Trên Linux/macOS:**
```bash
chmod +x start-database.sh
./start-database.sh
```

**Hoặc sử dụng Docker Compose trực tiếp:**
```bash
docker-compose up -d
```

### 2. Thông tin kết nối

**MySQL Database:**
- Host: `localhost`
- Port: `3306`
- Database: `exam_system`
- Username: `exam_user`
- Password: `exam123456`

**phpMyAdmin (Web Interface):**
- URL: http://localhost:8080
- Username: `exam_user`
- Password: `exam123456`

### 3. Dừng services

```bash
docker-compose down
```

### 4. Dừng và xóa dữ liệu (reset database)

```bash
docker-compose down -v
```

## 🔧 Cấu hình

Chỉnh sửa file `.env` để thay đổi cấu hình:

```env
# Database Configuration
MYSQL_ROOT_PASSWORD=root123456
MYSQL_DATABASE=exam_system
MYSQL_USER=exam_user
MYSQL_PASSWORD=exam123456
MYSQL_PORT=3306

# phpMyAdmin Configuration
PHPMYADMIN_PORT=8080
```

## 📊 Cơ sở dữ liệu

Database được tự động khởi tạo với:
- ✅ Tất cả bảng theo schema
- ✅ 50 câu hỏi mẫu
- ✅ 5 tài khoản người dùng mẫu (password: "Password123")
- ✅ Stored procedures và views
- ✅ Indexes để tối ưu performance

## 🔍 Kiểm tra trạng thái

```bash
# Xem containers đang chạy
docker-compose ps

# Xem logs
docker-compose logs mysql
docker-compose logs phpmyadmin

# Kết nối MySQL trực tiếp
docker exec -it exam_system_db mysql -u exam_user -p exam_system
```

## 🛠️ Troubleshooting

**Port bị chiếm:**
- Thay đổi `MYSQL_PORT` và `PHPMYADMIN_PORT` trong file `.env`

**MySQL không khởi động:**
```bash
# Xem logs chi tiết
docker-compose logs mysql

# Reset hoàn toàn
docker-compose down -v
docker-compose up -d
```

**Không thể kết nối:**
- Đảm bảo Docker đang chạy
- Kiểm tra firewall/antivirus
- Chờ MySQL khởi động hoàn tất (~30 giây)