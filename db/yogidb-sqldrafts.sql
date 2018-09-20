SELECT
    (SELECT sum(CASE WHEN type=0 THEN 1 ELSE 0 END) verb_count,
    sum(CASE WHEN type=1 THEN 1 ELSE 0 END) adj_count,
    sum(CASE WHEN type=2 THEN 1 ELSE 0 END) cept_count,
    sum(CASE WHEN type=3 THEN 1 ELSE 0 END) obj_count FROM Expression) AS expr_count,
    SELECT count(*) AS frmt_count FROM Format_String;

SELECT count(*) FILTER (WHERE type=0) AS verb_count,
    count(*) FILTER (WHERE type=1) AS adj_count,
    count(*) FILTER (WHERE type=2) AS cept_count,
    count(*) FILTER (WHERE type=3) AS obj_count
FROM Expression;
