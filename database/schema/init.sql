/* 智能家居监控系统基础表；统一使用 InnoDB 和 utf8mb4，支持外键事务。 */
CREATE DATABASE IF NOT EXISTS smarthome
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE smarthome;

/* 先按外键依赖逆序删除旧表，保证重复执行能得到干净一致的 schema。 */
DROP TABLE IF EXISTS user_sessions;
DROP TABLE IF EXISTS recordings;
DROP TABLE IF EXISTS records;
DROP TABLE IF EXISTS devices;
DROP TABLE IF EXISTS users;

/* 用户凭据只保存 PBKDF2 摘要与随机 salt，绝不保存明文密码。 */
CREATE TABLE IF NOT EXISTS users (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'user primary key',
    username VARCHAR(64) NOT NULL COMMENT 'login username',
    password_hash VARCHAR(256) NOT NULL COMMENT 'PBKDF2 password digest',
    salt VARCHAR(64) NOT NULL COMMENT 'per-user random salt',
    created_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'creation time',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'legacy creation field',
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'update time',
    PRIMARY KEY (id),
    UNIQUE KEY uk_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='user accounts';

/* 设备表保留旧版 device_code/stream_url，并补齐 B 成员所需设备类型和状态。 */
CREATE TABLE IF NOT EXISTS devices (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'device primary key',
    user_id BIGINT UNSIGNED NOT NULL COMMENT 'owner user id',
    device_name VARCHAR(128) NOT NULL COMMENT 'display name',
    device_code VARCHAR(128) NULL COMMENT 'legacy device code',
    stream_url VARCHAR(512) NULL COMMENT 'legacy stream url',
    device_type VARCHAR(64) NOT NULL DEFAULT 'unknown' COMMENT 'device type',
    status INT NOT NULL DEFAULT 0 COMMENT '0 means offline',
    created_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'creation time',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'legacy creation field',
    PRIMARY KEY (id),
    UNIQUE KEY uk_devices_device_code (device_code),
    KEY idx_devices_user_id (user_id),
    CONSTRAINT fk_devices_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='user devices';

/* records 是 B 成员录像查询使用的元数据表，不涉及媒体包或 FFmpeg。 */
CREATE TABLE IF NOT EXISTS records (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'record primary key',
    device_id BIGINT UNSIGNED NOT NULL COMMENT 'source device id',
    file_path VARCHAR(512) NOT NULL COMMENT 'record file path',
    start_time DATETIME NOT NULL COMMENT 'record start time',
    end_time DATETIME NOT NULL COMMENT 'record end time',
    file_size BIGINT UNSIGNED NULL COMMENT 'legacy file size',
    created_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'creation time',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'legacy creation field',
    PRIMARY KEY (id),
    KEY idx_records_device_start (device_id, start_time),
    KEY idx_records_device_end (device_id, end_time),
    CONSTRAINT fk_records_device FOREIGN KEY (device_id) REFERENCES devices (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='record metadata';

/* 兼容旧版 C 成员代码仍使用的 recordings 表；运维迁移时可将其数据导入 records。 */
CREATE TABLE IF NOT EXISTS recordings (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    device_id BIGINT UNSIGNED NOT NULL,
    file_path VARCHAR(512) NOT NULL,
    start_time DATETIME NULL,
    end_time DATETIME NULL,
    file_size BIGINT UNSIGNED NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_recordings_device_id (device_id),
    CONSTRAINT fk_recordings_device FOREIGN KEY (device_id) REFERENCES devices (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

/* 登录会话只落库 SHA-512(token) 摘要，明文 token 只存在登录响应的内存路径。 */
CREATE TABLE IF NOT EXISTS user_sessions (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'session primary key',
    user_id BIGINT UNSIGNED NOT NULL COMMENT 'owner user id',
    token_hash VARCHAR(128) NOT NULL COMMENT 'SHA-512 token digest',
    expires_at DATETIME NOT NULL COMMENT 'expiration time',
    revoked_at DATETIME NULL DEFAULT NULL COMMENT 'revocation time',
    created_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'creation time',
    PRIMARY KEY (id),
    UNIQUE KEY uk_user_sessions_token_hash (token_hash),
    KEY idx_user_sessions_user_expiry (user_id, expires_at),
    CONSTRAINT fk_user_sessions_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='login sessions';
