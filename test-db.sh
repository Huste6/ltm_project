#!/bin/bash

echo "========================================="
echo "        TEST KẾT NỐI DATABASE           "
echo "========================================="

# Kiểm tra xem Docker có đang chạy không
if ! docker info > /dev/null 2>&1; then
    echo "❌ Docker không chạy! Vui lòng khởi động Docker Desktop."
    exit 1
fi

# Di chuyển đến thư mục gốc của project
cd "$(dirname "$0")"

echo "1. Khởi động MySQL container..."
docker-compose up -d mysql

echo "2. Đợi MySQL khởi động hoàn tất (30 giây)..."
sleep 30

echo "3. Kiểm tra trạng thái container..."
docker-compose ps mysql

echo "4. Build chương trình test..."
cd server
make clean
make install-deps 2>/dev/null || echo "Dependencies đã được cài đặt hoặc cần sudo"
make

if [ $? -eq 0 ]; then
    echo "5. Chạy test kết nối database..."
    echo "========================================="
    ./test_db
    echo "========================================="
else
    echo "❌ Build thất bại! Kiểm tra lỗi ở trên."
    exit 1
fi

echo "6. Thông tin database container:"
echo "   - Host: localhost"
echo "   - Port: 3306"
echo "   - User: exam_user"
echo "   - Password: exam123456"
echo "   - Database: exam_system"

echo ""
echo "Để dừng database: docker-compose down"
echo "Để xem logs: docker-compose logs mysql"