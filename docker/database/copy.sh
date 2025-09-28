docker cp ./cables.csv pg-db:/tmp/cables.csv

docker exec -it pg-db psql -U postgres -d appdb -c "\
CREATE TABLE IF NOT EXISTS cables (
  id_gm integer,
  cable text,
  origine text,
  destination text,
  cond_et_calibre text,
  longueur_retenue numeric
);" &&

docker exec -it pg-db psql -U postgres -d appdb -c "
BEGIN;
TRUNCATE TABLE public.cables;
COPY public.cables
FROM '/tmp/cables.csv'
WITH (FORMAT csv, HEADER true, DELIMITER ',', QUOTE '\"');
COMMIT;"
