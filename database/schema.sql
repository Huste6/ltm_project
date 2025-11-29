-- ==========================================
-- EXAM SYSTEM DATABASE SCHEMA
-- ==========================================

DROP DATABASE IF EXISTS exam_system;
CREATE DATABASE exam_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE exam_system;

-- ==========================================
-- 1. BẢNG USERS - Quản lý tài khoản người dùng
-- ==========================================
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(20) NOT NULL UNIQUE,
    password_hash VARCHAR(65) NOT NULL,  -- SHA256 hash
    is_locked TINYINT(1) DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    INDEX idx_username (username)
) ENGINE=InnoDB;

-- ==========================================
-- 2. BẢNG SESSIONS - Quản lý phiên đăng nhập
-- ==========================================
CREATE TABLE sessions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    session_id VARCHAR(64) NOT NULL UNIQUE,
    username VARCHAR(20) NOT NULL,
    login_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    is_active TINYINT(1) DEFAULT 1,
    INDEX idx_session_id (session_id),
    INDEX idx_username (username),
    FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ==========================================
-- 3. BẢNG QUESTIONS - Kho câu hỏi trắc nghiệm
-- ==========================================
CREATE TABLE questions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    question_text TEXT NOT NULL,
    option_a VARCHAR(500) NOT NULL,
    option_b VARCHAR(500) NOT NULL,
    option_c VARCHAR(500) NOT NULL,
    option_d VARCHAR(500) NOT NULL,
    correct_answer CHAR(1) NOT NULL,  -- A, B, C, D
    difficulty ENUM('easy', 'medium', 'hard') DEFAULT 'medium',
    category VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CHECK (correct_answer IN ('A', 'B', 'C', 'D'))
) ENGINE=InnoDB;

-- ==========================================
-- 4. BẢNG ROOMS - Phòng thi
-- ==========================================
CREATE TABLE rooms (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id VARCHAR(32) NOT NULL UNIQUE,
    room_name VARCHAR(100) NOT NULL,
    creator VARCHAR(20) NOT NULL,
    num_questions INT NOT NULL,
    time_limit_minutes INT NOT NULL,
    max_participants INT DEFAULT 50,
    status ENUM('NOT_STARTED', 'IN_PROGRESS', 'FINISHED') DEFAULT 'NOT_STARTED',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    start_time TIMESTAMP NULL,
    finish_time TIMESTAMP NULL,
    INDEX idx_room_id (room_id),
    INDEX idx_status (status),
    INDEX idx_creator (creator),
    FOREIGN KEY (creator) REFERENCES users(username) ON DELETE CASCADE,
    CHECK (num_questions >= 5 AND num_questions <= 50),
    CHECK (time_limit_minutes >= 5 AND time_limit_minutes <= 120)
) ENGINE=InnoDB;

-- ==========================================
-- 5. BẢNG ROOM_QUESTIONS - Câu hỏi của mỗi phòng
-- ==========================================
CREATE TABLE room_questions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id VARCHAR(32) NOT NULL,
    question_id INT NOT NULL,
    question_order INT NOT NULL,  -- Thứ tự câu hỏi trong đề thi
    INDEX idx_room_id (room_id),
    FOREIGN KEY (room_id) REFERENCES rooms(room_id) ON DELETE CASCADE,
    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE,
    UNIQUE KEY unique_room_question (room_id, question_id)
) ENGINE=InnoDB;

-- ==========================================
-- 6. BẢNG PARTICIPANTS - Người tham gia phòng thi
-- ==========================================
CREATE TABLE participants (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id VARCHAR(32) NOT NULL,
    username VARCHAR(20) NOT NULL,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_room_id (room_id),
    INDEX idx_username (username),
    FOREIGN KEY (room_id) REFERENCES rooms(room_id) ON DELETE CASCADE,
    FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE,
    UNIQUE KEY unique_participant (room_id, username)
) ENGINE=InnoDB;

-- ==========================================
-- 7. BẢNG EXAM_RESULTS - Kết quả thi
-- ==========================================
CREATE TABLE exam_results (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id VARCHAR(32) NOT NULL,
    username VARCHAR(20) NOT NULL,
    score INT NOT NULL,
    total_questions INT NOT NULL,
    submit_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    time_taken_seconds INT,  -- Thời gian làm bài (giây)
    answers TEXT,  -- JSON array: ["A","B","C",...]
    INDEX idx_room_id (room_id),
    INDEX idx_username (username),
    INDEX idx_score (score),
    FOREIGN KEY (room_id) REFERENCES rooms(room_id) ON DELETE CASCADE,
    FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE,
    UNIQUE KEY unique_result (room_id, username)
) ENGINE=InnoDB;

-- ==========================================
-- 8. BẢNG PRACTICE_SESSIONS - Phiên luyện tập
-- ==========================================
CREATE TABLE practice_sessions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    practice_id VARCHAR(32) NOT NULL UNIQUE,
    username VARCHAR(20) NOT NULL,
    num_questions INT NOT NULL,
    time_limit_minutes INT NOT NULL,
    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    score INT,
    is_completed TINYINT(1) DEFAULT 0,
    INDEX idx_practice_id (practice_id),
    INDEX idx_username (username),
    FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ==========================================
-- 9. BẢNG PRACTICE_QUESTIONS - Câu hỏi luyện tập
-- ==========================================
CREATE TABLE practice_questions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    practice_id VARCHAR(32) NOT NULL,
    question_id INT NOT NULL,
    question_order INT NOT NULL,
    INDEX idx_practice_id (practice_id),
    FOREIGN KEY (practice_id) REFERENCES practice_sessions(practice_id) ON DELETE CASCADE,
    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ==========================================
-- 10. BẢNG ACTIVITY_LOGS - Log hoạt động
-- ==========================================
CREATE TABLE activity_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    level ENUM('INFO', 'WARNING', 'ERROR') DEFAULT 'INFO',
    username VARCHAR(20),
    action VARCHAR(50) NOT NULL,
    details TEXT,
    ip_address VARCHAR(45),
    INDEX idx_timestamp (timestamp),
    INDEX idx_username (username),
    INDEX idx_level (level)
) ENGINE=InnoDB;

-- ==========================================
-- DỮ LIỆU MẪU
-- ==========================================

-- Thêm câu hỏi mẫu (50 câu)
INSERT INTO questions (question_text, option_a, option_b, option_c, option_d, correct_answer, difficulty, category) VALUES
('What is the capital of France?', 'London', 'Berlin', 'Paris', 'Madrid', 'C', 'easy', 'Geography'),
('What is 2 + 2?', '3', '4', '5', '6', 'B', 'easy', 'Math'),
('Who wrote Romeo and Juliet?', 'Charles Dickens', 'William Shakespeare', 'Jane Austen', 'Mark Twain', 'B', 'medium', 'Literature'),
('What is the largest planet in our solar system?', 'Earth', 'Mars', 'Jupiter', 'Saturn', 'C', 'medium', 'Science'),
('In which year did World War II end?', '1943', '1944', '1945', '1946', 'C', 'medium', 'History'),
('What is the chemical symbol for gold?', 'Go', 'Gd', 'Au', 'Ag', 'C', 'medium', 'Chemistry'),
('Who painted the Mona Lisa?', 'Vincent van Gogh', 'Pablo Picasso', 'Leonardo da Vinci', 'Michelangelo', 'C', 'easy', 'Art'),
('What is the speed of light?', '299,792 km/s', '150,000 km/s', '500,000 km/s', '1,000,000 km/s', 'A', 'hard', 'Physics'),
('Which programming language is known as the mother of all languages?', 'C', 'Python', 'Java', 'Assembly', 'D', 'hard', 'Computer Science'),
('What is the largest ocean on Earth?', 'Atlantic Ocean', 'Indian Ocean', 'Arctic Ocean', 'Pacific Ocean', 'D', 'easy', 'Geography'),
('How many continents are there?', '5', '6', '7', '8', 'C', 'easy', 'Geography'),
('What is the smallest prime number?', '0', '1', '2', '3', 'C', 'easy', 'Math'),
('Who discovered penicillin?', 'Marie Curie', 'Alexander Fleming', 'Louis Pasteur', 'Isaac Newton', 'B', 'medium', 'Science'),
('What is the currency of Japan?', 'Yuan', 'Won', 'Yen', 'Baht', 'C', 'easy', 'Economics'),
('Which planet is known as the Red Planet?', 'Venus', 'Mars', 'Mercury', 'Jupiter', 'B', 'easy', 'Science'),
('What is the square root of 144?', '10', '11', '12', '13', 'C', 'easy', 'Math'),
('Who is the author of Harry Potter series?', 'J.R.R. Tolkien', 'J.K. Rowling', 'George R.R. Martin', 'C.S. Lewis', 'B', 'easy', 'Literature'),
('What is the boiling point of water at sea level?', '90°C', '95°C', '100°C', '105°C', 'C', 'easy', 'Chemistry'),
('Which country is known as the Land of the Rising Sun?', 'China', 'Japan', 'Korea', 'Thailand', 'B', 'easy', 'Geography'),
('What is the largest mammal on Earth?', 'Elephant', 'Blue Whale', 'Giraffe', 'Polar Bear', 'B', 'easy', 'Biology'),
('Who invented the telephone?', 'Thomas Edison', 'Nikola Tesla', 'Alexander Graham Bell', 'Benjamin Franklin', 'C', 'medium', 'History'),
('What is the chemical formula for water?', 'H2O', 'CO2', 'O2', 'H2O2', 'A', 'easy', 'Chemistry'),
('Which language has the most native speakers?', 'English', 'Spanish', 'Mandarin Chinese', 'Hindi', 'C', 'medium', 'Linguistics'),
('What is the longest river in the world?', 'Amazon', 'Nile', 'Yangtze', 'Mississippi', 'B', 'medium', 'Geography'),
('Who developed the theory of relativity?', 'Isaac Newton', 'Albert Einstein', 'Stephen Hawking', 'Niels Bohr', 'B', 'medium', 'Physics'),
('What is the hardest natural substance on Earth?', 'Gold', 'Iron', 'Diamond', 'Platinum', 'C', 'easy', 'Chemistry'),
('In which year did the first man land on the moon?', '1967', '1968', '1969', '1970', 'C', 'medium', 'History'),
('What is the main component of the sun?', 'Oxygen', 'Hydrogen', 'Helium', 'Carbon', 'B', 'medium', 'Science'),
('Which organ in the human body filters blood?', 'Heart', 'Liver', 'Kidney', 'Lungs', 'C', 'easy', 'Biology'),
('What is the capital of Australia?', 'Sydney', 'Melbourne', 'Canberra', 'Brisbane', 'C', 'medium', 'Geography'),
('Who painted The Starry Night?', 'Claude Monet', 'Vincent van Gogh', 'Pablo Picasso', 'Salvador Dali', 'B', 'medium', 'Art'),
('What is the smallest country in the world?', 'Monaco', 'Vatican City', 'San Marino', 'Liechtenstein', 'B', 'medium', 'Geography'),
('Which element has the atomic number 1?', 'Helium', 'Hydrogen', 'Oxygen', 'Carbon', 'B', 'easy', 'Chemistry'),
('What is the largest desert in the world?', 'Sahara', 'Arabian', 'Gobi', 'Antarctic', 'D', 'hard', 'Geography'),
('Who wrote 1984?', 'Aldous Huxley', 'George Orwell', 'Ray Bradbury', 'H.G. Wells', 'B', 'medium', 'Literature'),
('What is the process by which plants make food?', 'Respiration', 'Photosynthesis', 'Digestion', 'Fermentation', 'B', 'easy', 'Biology'),
('Which programming language is primarily used for web development?', 'C++', 'JavaScript', 'Python', 'Assembly', 'B', 'medium', 'Computer Science'),
('What is the freezing point of water in Fahrenheit?', '0°F', '32°F', '100°F', '212°F', 'B', 'easy', 'Physics'),
('Who is known as the father of computers?', 'Bill Gates', 'Steve Jobs', 'Charles Babbage', 'Alan Turing', 'C', 'medium', 'Computer Science'),
('What is the largest bone in the human body?', 'Tibia', 'Femur', 'Humerus', 'Fibula', 'B', 'easy', 'Biology'),
('Which gas do plants absorb from the atmosphere?', 'Oxygen', 'Nitrogen', 'Carbon Dioxide', 'Hydrogen', 'C', 'easy', 'Biology'),
('What is the capital of Canada?', 'Toronto', 'Vancouver', 'Ottawa', 'Montreal', 'C', 'easy', 'Geography'),
('Who composed the Four Seasons?', 'Mozart', 'Beethoven', 'Vivaldi', 'Bach', 'C', 'medium', 'Music'),
('What is the powerhouse of the cell?', 'Nucleus', 'Mitochondria', 'Ribosome', 'Chloroplast', 'B', 'easy', 'Biology'),
('Which country gifted the Statue of Liberty to the USA?', 'England', 'France', 'Spain', 'Italy', 'B', 'easy', 'History'),
('What is the value of Pi to two decimal places?', '3.12', '3.14', '3.16', '3.18', 'B', 'easy', 'Math'),
('Who is the CEO of Tesla?', 'Jeff Bezos', 'Elon Musk', 'Bill Gates', 'Mark Zuckerberg', 'B', 'easy', 'Business'),
('What is the most abundant gas in Earth atmosphere?', 'Oxygen', 'Carbon Dioxide', 'Nitrogen', 'Hydrogen', 'C', 'easy', 'Science'),
('Which ocean is the smallest?', 'Atlantic', 'Indian', 'Arctic', 'Pacific', 'C', 'easy', 'Geography'),
('What does HTTP stand for?', 'HyperText Transfer Protocol', 'High Transfer Text Protocol', 'HyperText Transport Protocol', 'High Text Transfer Protocol', 'A', 'medium', 'Computer Science');

-- Tạo user mẫu (password: "Password123")
-- Hash SHA256 của "Password123" = ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f
INSERT INTO users (username, password_hash) VALUES
('admin', 'ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f'),
('john123', 'ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f'),
('alice', 'ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f'),
('bob', 'ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f'),
('charlie', 'ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f');

-- View để dễ query
CREATE VIEW active_sessions_view AS
SELECT s.session_id, s.username, s.login_time, s.last_activity
FROM sessions s
WHERE s.is_active = 1;

CREATE VIEW room_leaderboard_view AS
SELECT 
    r.room_id,
    r.room_name,
    er.username,
    er.score,
    er.total_questions,
    er.submit_time,
    er.time_taken_seconds,
    RANK() OVER (PARTITION BY r.room_id ORDER BY er.score DESC, er.submit_time ASC) as rank_position
FROM rooms r
JOIN exam_results er ON r.room_id = er.room_id
WHERE r.status = 'FINISHED'
ORDER BY r.room_id, rank_position;

-- ==========================================
-- STORED PROCEDURES
-- ==========================================

DELIMITER $$

-- Procedure: Tạo phòng thi mới
CREATE PROCEDURE create_room(
    IN p_room_id VARCHAR(32),
    IN p_room_name VARCHAR(100),
    IN p_creator VARCHAR(20),
    IN p_num_questions INT,
    IN p_time_limit INT
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Error creating room';
    END;
    
    START TRANSACTION;
    
    -- Insert room
    INSERT INTO rooms (room_id, room_name, creator, num_questions, time_limit_minutes)
    VALUES (p_room_id, p_room_name, p_creator, p_num_questions, p_time_limit);
    
    -- Random chọn câu hỏi
    INSERT INTO room_questions (room_id, question_id, question_order)
    SELECT p_room_id, id, @rownum := @rownum + 1
    FROM questions, (SELECT @rownum := 0) r
    ORDER BY RAND()
    LIMIT p_num_questions;
    
    -- Creator tự động join
    INSERT INTO participants (room_id, username)
    VALUES (p_room_id, p_creator);
    
    COMMIT;
END$$

-- Procedure: Lấy bảng xếp hạng
CREATE PROCEDURE get_leaderboard(IN p_room_id VARCHAR(32))
BEGIN
    SELECT 
        username,
        score,
        total_questions,
        submit_time,
        time_taken_seconds
    FROM exam_results
    WHERE room_id = p_room_id
    ORDER BY score DESC, submit_time ASC;
END$$

DELIMITER ;

-- Tạo indexes cho performance
CREATE INDEX idx_room_status_created ON rooms(status, created_at);
CREATE INDEX idx_results_room_score ON exam_results(room_id, score DESC, submit_time ASC);
CREATE INDEX idx_sessions_active ON sessions(is_active, last_activity);

SHOW TABLES;