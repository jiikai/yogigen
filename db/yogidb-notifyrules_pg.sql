CREATE RULE notifyrule_1 AS ON INSERT TO Generated DO (INSERT INTO Notifier VALUES (1); NOTIFY Notifier, 'ok');

CREATE RULE notifyrule_2 AS ON INSERT TO Generated DO (INSERT INTO Notifier VALUES (2); NOTIFY Notifier, 'err');
