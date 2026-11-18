-- mod-temucraft-auto-learn: each character's choice (no row = the server default).
CREATE TABLE IF NOT EXISTS `mod_temucraft_auto_learn` (
    `guid`    INT UNSIGNED NOT NULL,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
