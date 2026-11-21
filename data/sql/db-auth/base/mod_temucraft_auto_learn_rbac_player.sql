-- mod-temucraft-auto-learn: its commands (all under .temucraft) as RBAC permissions 101950-101999. Every command is admin only by
-- default: linked to the administrator roles 192 and 1014. To give one to players, link it to 195 and 1010 here (or
-- grant it per account with .rbac account grant <account> <id>). "Temucraft: use ..." permissions are not
-- commands: they stay linked to the player roles so the items keep working.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 101950 AND 101999;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 101950 AND 101999;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(101950, 'Command: temucraft autolearn on'),
(101951, 'Command: temucraft autolearn off'),
(101952, 'Command: temucraft autolearn now'),
(101953, 'Command: temucraft autolearn list'),
(101954, 'Command: temucraft autolearn');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192, 101950),
(1014, 101950),
(192, 101951),
(1014, 101951),
(192, 101952),
(1014, 101952),
(192, 101953),
(1014, 101953),
(192, 101954),
(1014, 101954);
