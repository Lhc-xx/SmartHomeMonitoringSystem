CREATE DATABASE IF NOT EXISTS
smarthome;

USE smarthome;


CREATE TABLE users(
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64)
    NOT NULL UNIQUE,
    password_hash VARCHAR(256)
    NOT NULL,
    salt VARCHAR(64)
    NOT NULL,
    created_at TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP
);

/*
================================================
设备表
================================================
*/

CREATE TABLE devices
(

    id BIGINT PRIMARY KEY AUTO_INCREMENT,


    /*
     * 属于哪个用户
     *
     * 对应users.id
     */
    user_id BIGINT
    NOT NULL,


    /*
     * 设备名称
     */
    device_name VARCHAR(128)
    NOT NULL,


    /*
     * 唯一设备编号
     */
    device_code VARCHAR(128)
    UNIQUE,


    /*
     * RTSP地址
     */
    stream_url VARCHAR(512),


    /*
     * 在线状态

     0 离线
     1 在线

     */
    status INT
    DEFAULT 0,


    created_at TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP,


    /*
     * 外键
     */
    FOREIGN KEY(user_id)
    REFERENCES users(id)

);



/*
================================================
录像表
================================================
*/

CREATE TABLE recordings
(

    id BIGINT PRIMARY KEY AUTO_INCREMENT,


    /*
     * 哪个设备产生
     */
    device_id BIGINT
    NOT NULL,


    /*
     * 文件路径
     */
    file_path VARCHAR(512)
    NOT NULL,


    /*
     * 开始时间
     */
    start_time DATETIME,


    /*
     * 结束时间
     */
    end_time DATETIME,


    /*
     * 文件大小
     */
    file_size BIGINT,


    created_at TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP,


    FOREIGN KEY(device_id)
    REFERENCES devices(id)

);