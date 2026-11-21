-- mod-temucraft-auto-learn: its player commands (all under .temucraft) as RBAC permissions 101950-101999. Linked to the stock player
-- role 195 and the Temucraft player role 1010, so every account has them by default. To take one away, remove its
-- link here (or deny it per account with .rbac account deny <account> <id>); to give it to staff only, link it to
-- 1011-1014 instead.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 101950 AND 101999;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 101950 AND 101999;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(101950, 'Command: temucraft autolearn on'),
(101951, 'Command: temucraft autolearn off'),
(101952, 'Command: temucraft autolearn now'),
(101953, 'Command: temucraft autolearn list'),
(101954, 'Command: temucraft autolearn');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 101950),
(1010, 101950),
(195, 101951),
(1010, 101951),
(195, 101952),
(1010, 101952),
(195, 101953),
(1010, 101953),
(195, 101954),
(1010, 101954);
