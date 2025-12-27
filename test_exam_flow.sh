#!/bin/bash

# Test script for exam flow
# This script tests the complete exam flow:
# 1. Register
# 2. Login
# 3. Create Room
# 4. Start Exam
# 5. Get Exam
# 6. Save Answers
# 7. Submit Exam

HOST="127.0.0.1"
PORT="8888"

# Function to send command and receive response
send_command() {
    local cmd="$1"
    echo ">>> $cmd"
    (sleep 0.1; echo "$cmd") | nc -q 1 $HOST $PORT | head -20
    echo ""
}

echo "========== EXAM FLOW TEST =========="
echo ""

# Register
send_command "REGISTER test_user|Test1234"

# Login
LOGIN_RESP=$(echo "LOGIN test_user|Test1234" | nc -q 1 $HOST $PORT | head -1)
SESSION_ID=$(echo "$LOGIN_RESP" | grep -oP 'sess_\d+_\w+')
echo "Session ID: $SESSION_ID"
echo ""

# Create Room
ROOM_RESP=$(echo "CREATE_ROOM myroom|5|1" | nc -q 1 $HOST $PORT | head -1)
ROOM_ID=$(echo "$ROOM_RESP" | grep -oP '\d{6}')
echo "Room ID: $ROOM_ID"
echo ""

# Start Exam
send_command "START_EXAM $ROOM_ID"

# Get Exam
echo ">>> GET_EXAM $ROOM_ID"
GET_RESP=$(echo "GET_EXAM $ROOM_ID" | nc -q 1 $HOST $PORT)
echo "$GET_RESP" | head -30
echo ""

# Extract question IDs
Q_IDS=$(echo "$GET_RESP" | grep -oP '"question_id":\K\d+' | tr '\n' '|' | sed 's/|$//')
echo "Question IDs: $Q_IDS"
echo ""

# Save Answers
for i in 1 2 3 4 5; do
    send_command "SAVE_ANSWER $ROOM_ID|$i|A"
done

# Submit Exam
send_command "SUBMIT_EXAM $ROOM_ID"

echo "========== TEST COMPLETE =========="
