-- ============================================================
--  llfc 数据库结构
--
--  说明：本文件只有建表语句与存储过程，**不含任何业务数据**。
--        由本机 MySQL 8.4 的实际库反推而来，与代码使用的字段一致。
--
--  用法：mysql -h 127.0.0.1 -P 3308 -u root -p < server/sql/schema.sql
-- ============================================================

CREATE DATABASE IF NOT EXISTS `llfc`
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;
USE `llfc`;

-- ------------------------------------------------------------
-- 用户主表
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `user` (
  `id`    int NOT NULL AUTO_INCREMENT,
  `uid`   int NOT NULL,
  `name`  varchar(255) NOT NULL DEFAULT '',
  `email` varchar(255) NOT NULL DEFAULT '',
  `pwd`   varchar(255) NOT NULL DEFAULT '',
  `nick`  varchar(255) NOT NULL DEFAULT '',
  `desc`  varchar(255) NOT NULL DEFAULT '',
  `sex`   int NOT NULL DEFAULT '0',
  `icon`  varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`id` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ------------------------------------------------------------
-- uid 发号器：单行计数表，reg_user 存储过程在此自增取号
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `user_id` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ------------------------------------------------------------
-- 好友关系：双向各存一行（A→B 与 B→A），唯一键防止重复添加
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `friend` (
  `id`        int NOT NULL AUTO_INCREMENT,
  `self_id`   int NOT NULL,
  `friend_id` int NOT NULL,
  `back`      varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `self_friend` (`self_id`,`friend_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ------------------------------------------------------------
-- 好友申请：status 0=待处理 1=已同意 2=已拒绝
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `friend_apply` (
  `id`       bigint NOT NULL AUTO_INCREMENT,
  `from_uid` int NOT NULL,
  `to_uid`   int NOT NULL,
  `status`   smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `from_to_uid` (`from_uid`,`to_uid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- ------------------------------------------------------------
-- 注册存储过程：事务内校验重名并原子发号
--   返回 result = 新 uid 成功 / 0 用户名或邮箱已存在 / -1 异常
-- ------------------------------------------------------------
DROP PROCEDURE IF EXISTS `reg_user`;
DELIMITER ;;
CREATE PROCEDURE `reg_user`(
    IN  `new_name`  VARCHAR(255),
    IN  `new_email` VARCHAR(255),
    IN  `new_pwd`   VARCHAR(255),
    OUT `result`    INT
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SET result = -1;
    END;

    START TRANSACTION;

    IF EXISTS (SELECT 1 FROM `user` WHERE `name` = new_name) THEN
        SET result = 0;
        COMMIT;
    ELSE
        IF EXISTS (SELECT 1 FROM `user` WHERE `email` = new_email) THEN
            SET result = 0;
            COMMIT;
        ELSE
            UPDATE `user_id` SET `id` = `id` + 1;
            SELECT `id` INTO @new_id FROM `user_id`;
            INSERT INTO `user` (`uid`, `name`, `email`, `pwd`)
                 VALUES (@new_id, new_name, new_email, new_pwd);
            SET result = @new_id;
            COMMIT;
        END IF;
    END IF;
END ;;
DELIMITER ;

-- ------------------------------------------------------------
-- 发号器初始值：属于结构的一部分，不是业务数据
-- 若库里已有用户，改成本机当前的 uid 最大值即可
-- ------------------------------------------------------------
INSERT IGNORE INTO `user_id` (`id`) VALUES (0);