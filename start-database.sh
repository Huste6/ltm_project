#!/bin/bash
# ==============================================
# EXAM SYSTEM DATABASE SETUP SCRIPT
# ==============================================

echo "🚀 Starting Exam System Database Setup..."
echo "=========================================="

# Check if Docker is running
if ! docker info >/dev/null 2>&1; then
    echo "❌ Docker is not running. Please start Docker and try again."
    exit 1
fi

# Check if .env file exists
if [ ! -f ".env" ]; then
    echo "❌ .env file not found. Please make sure .env exists in the project root."
    exit 1
fi

# Stop and remove existing containers if they exist
echo "📦 Stopping existing containers..."
docker-compose down

# Remove old volumes (uncomment if you want fresh database each time)
# echo "🗑️ Removing old database volumes..."
# docker volume rm project_quizz_mysql_data 2>/dev/null || true

# Build and start containers
echo "🏗️ Building and starting containers..."
docker-compose up -d

# Wait for MySQL to be ready
echo "⏳ Waiting for MySQL to be ready..."
until docker exec exam_system_db mysqladmin ping --silent; do
    echo "   Waiting for MySQL connection..."
    sleep 2
done

echo ""
echo "✅ Database setup completed successfully!"
echo "=========================================="
echo "📋 Connection Information:"
echo "   MySQL Host: localhost"
echo "   MySQL Port: 3306"
echo "   Database: exam_system"
echo "   Username: exam_user"
echo "   Password: exam123456"
echo ""
echo "🌐 phpMyAdmin Web Interface:"
echo "   URL: http://localhost:8080"
echo "   Username: exam_user"
echo "   Password: exam123456"
echo ""
echo "📊 You can now connect to the database using any MySQL client!"
echo "=========================================="

# Show running containers
echo "🐳 Running containers:"
docker-compose ps