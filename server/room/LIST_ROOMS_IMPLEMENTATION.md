# LIST_ROOMS Implementation

## Overview
Implemented the `LIST_ROOMS` command handler following the specification.

## Protocol

### Request Format
```
LIST_ROOMS [status_filter]\n
```

- `status_filter` (optional): `NOT_STARTED`, `IN_PROGRESS`, or `FINISHED`
- If omitted or empty, returns all rooms

### Response Format
```
121 DATA <length>\n<json_data>
```

JSON structure:
```json
{
  "rooms": [
    {
      "room_id": "abc123",
      "name": "Test Room",
      "creator": "admin",
      "status": "NOT_STARTED",
      "participants": 5,
      "num_questions": 20,
      "time_limit": 30,
      "created_at": "2025-12-07 10:30:00"
    }
  ]
}
```

### Error Codes
- `302 INVALID_PARAMS`: Invalid status filter (not one of the 3 valid values)
- `500 INTERNAL_ERROR`: Database query failed

## Implementation Details

### Files Modified/Created
1. **server/room/room.h** - Added function prototypes
2. **server/room/room.c** - Implemented `handle_list_rooms()` with:
   - Parameter validation (trim whitespace)
   - Status filter validation
   - JSON response building
   
3. **server/database/database.h** - Added `db_get_rooms_json()` prototype
4. **server/database/database.c** - Implemented database query with:
   - Dynamic SQL based on filter
   - LEFT JOIN with participants table to count members
   - JSON string building with dynamic memory allocation
   
5. **server/server.c** - Added dispatcher for `MSG_LIST_ROOMS`
6. **server/server.h** - Changed `Database *db` to `Database db` (object instead of pointer)

### Key Functions

#### `handle_list_rooms(Server *server, ClientSession *client, Message *msg)`
- Validates status filter parameter
- Calls `db_get_rooms_json()` to fetch data
- Sends response using `send_full()` helper

#### `db_get_rooms_json(Database *db, const char *status_filter, char **json_out)`
- Executes SQL query with optional WHERE clause
- Builds JSON string dynamically
- Returns allocated string (caller must free)

### SQL Query
```sql
SELECT r.room_id, r.room_name, r.creator, r.status,
       COALESCE(COUNT(p.username), 0) AS participants,
       r.num_questions, r.time_limit_minutes, r.created_at
FROM rooms r
LEFT JOIN participants p ON r.room_id = p.room_id
[WHERE r.status = ?]
GROUP BY r.room_id
ORDER BY r.created_at DESC
```

## Testing

### Test Cases
1. **No filter**: `LIST_ROOMS\n` → returns all rooms
2. **Valid filter**: `LIST_ROOMS NOT_STARTED\n` → returns only NOT_STARTED rooms
3. **Invalid filter**: `LIST_ROOMS INVALID\n` → returns 302 error
4. **Empty database**: Should return `{"rooms":[]}`

### Manual Test
```bash
# Connect to server
telnet localhost 8888

# Send command
LIST_ROOMS
# or
LIST_ROOMS NOT_STARTED
```

## Dependencies
- MySQL client library (`libmysqlclient`)
- OpenSSL (`libssl`) for auth module
- POSIX threads (`pthread`)

## Build
```bash
cd server
make clean
make
./server
```

## Notes
- Thread-safe: Uses mutex for database operations
- Memory management: Dynamically allocates JSON string, caller must free
- Logging: Activity logged to `activity_logs` table
- Performance: Uses LEFT JOIN for efficient participant counting
