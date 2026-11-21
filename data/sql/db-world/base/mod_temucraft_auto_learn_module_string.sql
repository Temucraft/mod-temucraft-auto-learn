-- mod-temucraft-auto-learn: player-facing texts. Translations go in module_string_locale (same module and id, plus the locale).
DELETE FROM `module_string` WHERE `module` = 'mod-temucraft-auto-learn';
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-temucraft-auto-learn', 1, '|cff33ff99New spells:|r you learned {} spell(s).'),
('mod-temucraft-auto-learn', 2, '|cff33ff99New spells:|r there is nothing new to learn right now.'),
('mod-temucraft-auto-learn', 3, '|cff33ff99New spells:|r {} more can be learned for {} gold {} silver.'),
('mod-temucraft-auto-learn', 4, '|cff33ff99New spells:|r new class spells will be learned as soon as you can learn them.'),
('mod-temucraft-auto-learn', 5, '|cff33ff99New spells:|r new class spells are learned at your trainer again.'),
('mod-temucraft-auto-learn', 6, 'This server decides that for everyone.'),
('mod-temucraft-auto-learn', 7, '|cff33ff99New spells:|r automatic learning is {}; {} spell(s) can be learned now.'),
('mod-temucraft-auto-learn', 8, 'Teach me everything I can learn ({} spells).'),
('mod-temucraft-auto-learn', 9, 'Teach me new spells as soon as I''m ready, wherever I am.'),
('mod-temucraft-auto-learn', 10, 'I will come to you for new spells again.'),
('mod-temucraft-auto-learn', 11, 'Learning without a trainer is turned off on this server.'),
('mod-temucraft-auto-learn', 12, '|cff33ff99New spells:|r {} can be learned now.'),
('mod-temucraft-auto-learn', 13, '  {} {} (level {}, {} silver)'),
('mod-temucraft-auto-learn', 14, '.temucraft autolearn on | off | now | list');
