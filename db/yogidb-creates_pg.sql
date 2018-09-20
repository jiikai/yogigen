CREATE TABLE Expression (
    id SERIAL PRIMARY KEY,
    field_1 text,
    field_2 text,
    type int,
    flags int
);

CREATE TABLE Format_String (
    id SERIAL PRIMARY KEY,
    str text,
    data bigint,
    count int
);

CREATE TABLE Generated (
    id bigint PRIMARY KEY,
    insertion_time timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
    phrase text
);

CREATE TABLE Notifier (i int4);
CREATE TABLE Select_Notifier (i int4);
