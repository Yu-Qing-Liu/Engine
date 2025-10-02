# 0) Put file into the container
docker cp ./cables.csv pg-db:/tmp/cables.csv

# 1) Normalize: always produce a 6-column CSV with quoted column 5 (handles:
#    - files with only 4 columns => col5/col6 empty
#    - files where col5 has commas (unquoted) => joins 5..NF-1 and quotes
#    - trims spaces; escapes quotes inside col5)
docker exec -it pg-db bash -lc '
awk -F, '\''BEGIN{OFS=","}
NR==1 {
  # Write canonical header for the normalized file
  print "id_gm","cable","origine","destination","cond_et_calibre","longueur_retenue"; next
}
{
  n  = NF;
  id = (n>=1 ? $1 : "");
  cab= (n>=2 ? $2 : "");
  org= (n>=3 ? $3 : "");
  dst= (n>=4 ? $4 : "");

  # default empty for missing cols
  len=""; cond="";

  if (n >= 6) {
    # join fields 5..(n-1) as cond, last as len
    len = $n;
    for (i=5; i<=n-1; i++) cond = (cond=="" ? $i : cond ", " $i);
  } else if (n == 5) {
    # have cond, but no length
    cond = $5;
  } else if (n == 4) {
    # only first 4 columns present; leave cond/len empty
  }

  # trim leading/trailing spaces from cond/len
  gsub(/^ +| +$/,"",cond);
  gsub(/^ +| +$/,"",len);

  # escape quotes in cond and wrap in double quotes if non-empty
  gsub(/"/,"""",cond);
  if (cond != "") cond="\"" cond "\"";

  print id, cab, org, dst, cond, len;
}
'\'' /tmp/cables.csv > /tmp/cables_norm.csv
'

# 2) Create tables (final + staging)
docker exec -it pg-db psql -U postgres -d appdb -v ON_ERROR_STOP=1 -c "
CREATE TABLE IF NOT EXISTS public.cables (
  id_gm             integer,
  cable             text,
  origine           text,
  destination       text,
  cond_et_calibre   text,
  longueur_retenue  numeric
);
CREATE TABLE IF NOT EXISTS public.cables_raw (
  id_gm             text,
  cable             text,
  origine           text,
  destination       text,
  cond_et_calibre   text,
  longueur_retenue  text
);"

# 3) Staging load from the normalized file (always 6 columns now)
docker exec -it pg-db psql -U postgres -d appdb -v ON_ERROR_STOP=1 -c "
BEGIN;
TRUNCATE TABLE public.cables;
TRUNCATE TABLE public.cables_raw;

COPY public.cables_raw (id_gm, cable, origine, destination, cond_et_calibre, longueur_retenue)
FROM '/tmp/cables_norm.csv'
WITH (FORMAT csv, HEADER true, DELIMITER ',', QUOTE '\"');

-- 4) Normalize/cast into final; blanks -> NULL; numeric cleaned
INSERT INTO public.cables (id_gm, cable, origine, destination, cond_et_calibre, longueur_retenue)
SELECT
  NULLIF(id_gm, '')::integer,
  NULLIF(cable, ''),
  NULLIF(origine, ''),
  NULLIF(destination, ''),
  NULLIF(cond_et_calibre, ''),
  CASE
    WHEN longueur_retenue IS NULL OR longueur_retenue = '' THEN NULL
    ELSE NULLIF(regexp_replace(longueur_retenue, '[^0-9.+-]', '', 'g'), '')::numeric
  END
FROM public.cables_raw;

COMMIT;
"

