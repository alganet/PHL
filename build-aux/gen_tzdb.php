<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Generate src/ph7/builtin_date_tzdb.h -- the embedded IANA timezone database.
 *
 * Usage:  php build-aux/gen_tzdb.php [<zoneinfo-dir>] [<out-header>]
 *
 * The PAYLOAD is the TZif files themselves, verbatim: for each zone only the
 * version-2+ block (the 64-bit one, from the file's SECOND "TZif" magic) is
 * kept, because the v1 block ahead of it is the same data truncated to 32 bits
 * and nothing reads it. Blocks are deduplicated by content -- 599 identifiers
 * share 447 distinct blocks -- so a link like `Europe/Oslo` costs a row, not a
 * copy.
 *
 * The METADATA -- which identifiers `listIdentifiers()` shows without
 * ALL_WITH_BC, each zone's COUNTRY (out of `getLocation()`), and the 144-row
 * ABBREVIATION table php resolves ahead of the database -- is asked of the PHP
 * running this script, because it is php's own classification rather than
 * anything the TZif files carry. Run it under the oracle, against the same tzdata the oracle
 * reads. Which files are zones at all is decided HERE, by the `TZif` magic:
 * a php built --with-system-tzdata answers `listIdentifiers(ALL_WITH_BC)` off a
 * directory walk and hands back `leapseconds` and `tzdata.zi` beside the real
 * ones -- two names its OWN constructor then refuses -- so its list cannot be
 * the input.
 */

$dir = $argv[1] ?? '/usr/share/zoneinfo';
$out = $argv[2] ?? (__DIR__ . '/../src/ph7/builtin_date_tzdb.h');

/* --- Which files are zones ------------------------------------------------
 * Every regular file whose first four bytes are "TZif", minus the `right/`
 * subtree (the leap-second-counting variants, which php does not list either)
 * and minus the tables that live beside the zones.
 */
$zones = [];
$it = new RecursiveIteratorIterator(
    new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS),
    RecursiveIteratorIterator::SELF_FIRST
);
foreach ($it as $path => $info) {
    if (!$info->isFile()) {
        continue;
    }
    $id = substr($path, strlen($dir) + 1);
    if (str_starts_with($id, 'right/') || str_starts_with($id, 'posix/')) {
        continue;
    }
    if ($id === 'posixrules') {
        continue;
    }
    $data = file_get_contents($path);
    if (substr($data, 0, 4) !== 'TZif') {
        continue;
    }
    $zones[$id] = $data;
}
ksort($zones, SORT_STRING);
if (!$zones) {
    fwrite(STDERR, "gen_tzdb: no TZif files under $dir\n");
    exit(1);
}

/* --- php's own classification --------------------------------------------- */
$all = array_flip(DateTimeZone::listIdentifiers(DateTimeZone::ALL));

/* Each zone's COUNTRY, which is what listIdentifiers(PER_COUNTRY, 'BR') sorts
 * on. It comes from tzdata's zone.tab and php answers it through
 * getLocation(); a zone with no country -- every backward link, `UTC`, the
 * `Etc/` set -- reads `??` there and matches no country. Two bytes per zone. */
$cc = [];
foreach ($zones as $id => $_) {
    $l = (new DateTimeZone($id))->getLocation();
    $cc[$id] = ($l === false || !isset($l['country_code'])) ? '??' : $l['country_code'];
}

/* --- The ABBREVIATION table -----------------------------------------------
 * php resolves a zone name against this BEFORE the database, which is the only
 * reason `new DateTimeZone('CET')` is a fixed +01:00 that never observes
 * daylight time while the zone FILE of the same name switches twice a year.
 * Ten names are in both (cet, eet, est, gmt, hst, met, mst, uct, utc, wet) and
 * the abbreviation wins every one of them.
 *
 * Each row is a name, a FIXED offset and a daylight flag that `I` prints and
 * nothing else reads. The 144 rows are the 25 military letters, 77 three-letter
 * abbreviations and 42 four-letter ones; `getName()` answers the canonical
 * upper-case spelling whatever case the caller wrote, which is where an
 * abbreviation differs from an identifier -- an identifier is stored verbatim.
 */
$abbrList = DateTimeZone::listAbbreviations();
$abbrRows = [];
foreach (array_keys($abbrList) as $key) {
    $z = new DateTimeZone($key);
    $d = new DateTime('@0');
    $d->setTimezone($z);
    $abbrRows[] = [
        'name'  => $z->getName(),
        'off'   => (int) $z->getOffset(new DateTime('@0')),
        'isdst' => $d->format('I') === '1' ? 1 : 0,
        'key'   => $key,
        'zones' => $abbrList[$key],
    ];
}
usort($abbrRows, static fn(array $a, array $b): int => strcmp($a['name'], $b['name']));

/* --- The version string ---------------------------------------------------
 * tzdata.zi's first line is `# version 2026c`. A tree without one is stamped
 * with the directory's own mtime instead, so the header always says what it
 * was cut from.
 */
$version = 'unknown';
if (is_file("$dir/tzdata.zi")) {
    $first = fgets(fopen("$dir/tzdata.zi", 'r'));
    if (preg_match('/#\s*version\s+(\S+)/', (string) $first, $m)) {
        $version = $m[1];
    }
}

/* --- Deduplicate the v2 blocks -------------------------------------------- */
$blobs = [];      /* content => offset */
$payload = '';
$rows = [];
foreach ($zones as $id => $data) {
    $second = strpos($data, 'TZif', 4);
    if ($second === false) {
        fwrite(STDERR, "gen_tzdb: $id has no version-2 block (v1-only TZif)\n");
        exit(1);
    }
    $blk = substr($data, $second);
    if (!isset($blobs[$blk])) {
        $blobs[$blk] = strlen($payload);
        $payload .= $blk;
    }
    $rows[] = [
        'id'    => $id,
        'ofst'  => $blobs[$blk],
        'byte'  => strlen($blk),
        'bc'    => !isset($all[$id]),
        'cc'    => $cc[$id],
    ];
}

/* --- Emit ------------------------------------------------------------------ */
/* --- What ONE abbreviation is a shorthand FOR -----------------------------
 * The row above is what `new DateTimeZone('CET')` becomes: a single fixed
 * offset. listAbbreviations() answers the other half -- every (daylight,
 * offset, zone) triple the database ever wrote that abbreviation for, 1127 of
 * them across the 144 names, and `cet` alone accounts for 52. The FIRST triple
 * of a name is the row above, because the reverse lookup that resolves a bare
 * `CET` returns the first match; the rest are only reachable by asking for a
 * specific offset.
 *
 * The listing order is the flat table's own, which is neither the byte order
 * aTzAbbr is sorted in nor anything derivable from the names -- so it is
 * recorded as its own index (aTzAbbrOrder) rather than recomputed.
 *
 * A triple may name NO zone: 25 of the 1127 are military letters and other
 * offsets the database never gave a canonical zone to, and those print null.
 */
$zoneIx = [];
foreach ($rows as $i => $r) {
    $zoneIx[$r['id']] = $i;
}
$order = [];
foreach (array_keys($abbrList) as $key) {
    foreach ($abbrRows as $i => $r) {
        if ($r['key'] === $key) { $order[] = $i; break; }
    }
}
if (count($order) !== count($abbrRows)) {
    fwrite(STDERR, "gen_tzdb: abbreviation listing order is incomplete\n");
    exit(1);
}

/* --- The offset-only fallback ---------------------------------------------
 * When an abbreviation names nothing at all, php falls back to a second, much
 * smaller table keyed on the (offset, daylight) pair alone and answers whatever
 * zone it happens to list there -- which is how an unknown abbreviation at
 * -18000 still resolves to `America/New_York`. The table is not reachable by
 * name, so it is swept out of the oracle by asking for a name that cannot
 * match and walking every minute of the legal offset range.
 */
$fallback = [];
$absent = 'zzzzzzzz';
if (isset($abbrList[$absent])) {
    fwrite(STDERR, "gen_tzdb: the fallback sweep's sentinel is a real abbreviation\n");
    exit(1);
}
for ($o = -14 * 3600; $o <= 14 * 3600; $o += 60) {
    foreach ([0, 1] as $dst) {
        $id = timezone_name_from_abbr($absent, $o, $dst);
        if ($id === false) {
            continue;
        }
        if (!isset($zoneIx[$id])) {
            fwrite(STDERR, "gen_tzdb: fallback names an unknown zone $id\n");
            exit(1);
        }
        $fallback[] = ['off' => $o, 'dst' => $dst, 'zone' => $zoneIx[$id]];
    }
}

$fp = fopen($out, 'w');
$w = static function (string $s) use ($fp): void { fwrite($fp, $s); };

$w(<<<HDR
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The IANA timezone database, embedded. GENERATED -- do not hand-edit; re-cut
 * it with `php build-aux/gen_tzdb.php` when tzdata moves.
 *
 * aTzPayload holds every zone's TZif version-2 block VERBATIM (RFC 8536), with
 * the 32-bit version-1 block ahead of it dropped and identical blocks shared:
 * the reader in builtin_date_tzdb.c is a TZif reader, so what this table says
 * is exactly what the platform's own zoneinfo said.
 *
 * aTzZone is in byte order, and NOTHING reads it in that order: both the lookup
 * and the listing go through aTzFold, the same rows sorted case-INSENSITIVELY.
 * That is one order for two reasons -- php resolves `europe/paris` and stores
 * the caller's spelling, so the lookup folds case; and php PRINTS
 * `listIdentifiers()` folded too, which only ALL_WITH_BC's odd-cased backward
 * links reveal (`CET` between `Canada/Yukon` and `Chile/Continental`).
 *
 * aTzAbbr is php's abbreviation table, which it resolves BEFORE this one.
 *
 * cut from tzdata $version

HDR);
$w(" */\n");
$w("#define PH7_TZDB_VERSION \"$version\"\n");
$w("#define PH7_TZDB_ZONE_COUNT " . count($rows) . "\n\n");

/* The payload. 16 bytes to a line keeps the generated source diffable. */
$w("static const unsigned char aTzPayload[" . strlen($payload) . "] = {\n");
$n = strlen($payload);
for ($i = 0; $i < $n; $i += 16) {
    $chunk = substr($payload, $i, 16);
    $line = '';
    for ($j = 0, $m = strlen($chunk); $j < $m; $j++) {
        $line .= sprintf('%d,', ord($chunk[$j]));
    }
    $w($line . "\n");
}
$w("};\n\n");

$w("typedef struct PH7_TzZoneRow PH7_TzZoneRow;\n");
$w("struct PH7_TzZoneRow\n{\n");
$w("\tconst char *zName;   /* canonical identifier */\n");
$w("\tsxu8 nName;\n");
$w("\tsxu8 bBackward;      /* 1 = a compatibility LINK: out of ALL, in ALL_WITH_BC */\n");
$w("\tchar zCc[2];         /* its ISO 3166-1 country, or `??` for a zone with none */\n");
$w("\tsxu32 iOfst;         /* where its TZif block starts in aTzPayload */\n");
$w("\tsxu32 nByte;\n");
$w("};\n");
$w("static const PH7_TzZoneRow aTzZone[PH7_TZDB_ZONE_COUNT] = {\n");
foreach ($rows as $r) {
    $w(sprintf("\t{ \"%s\", %d, %d, { '%s', '%s' }, %d, %d },\n",
        $r['id'], strlen($r['id']), $r['bc'] ? 1 : 0,
        $r['cc'][0], $r['cc'][1], $r['ofst'], $r['byte']));
}
$w("};\n\n");

/* The same rows in FOLDED order, so PH7_TzFind() can binary-search a lookup
 * that ignores case while aTzZone itself stays in the byte order
 * timezone_identifiers_list() prints. */
$fold = range(0, count($rows) - 1);
usort($fold, static function (int $a, int $b) use ($rows): int {
    $c = strcmp(strtolower($rows[$a]['id']), strtolower($rows[$b]['id']));
    return $c !== 0 ? $c : $a <=> $b;
});
$w("static const sxu16 aTzFold[PH7_TZDB_ZONE_COUNT] = {\n");
for ($i = 0, $n = count($fold); $i < $n; $i += 16) {
    $w("\t" . implode(',', array_slice($fold, $i, 16)) . ",\n");
}
$w("};\n\n");

$w("#define PH7_TZDB_ABBR_COUNT " . count($abbrRows) . "\n");
$w("typedef struct PH7_TzAbbrRow PH7_TzAbbrRow;\n");
$w("struct PH7_TzAbbrRow\n{\n");
$w("\tconst char *zName;   /* the canonical UPPER-CASE spelling getName() answers */\n");
$w("\tsxu8 nName;\n");
$w("\tsxu8 bDst;           /* what `I` prints; the offset is fixed either way */\n");
$w("\tsxi32 iOff;\n");
$w("\tsxu16 iZone;         /* where its triples start in aTzAbbrZone */\n");
$w("\tsxu8 nZone;          /* how many -- 62 at most, for `cst` */\n");
$w("};\n");
$w("/* Sorted by name, upper-case and byte-wise, for the binary search. */\n");
$w("static const PH7_TzAbbrRow aTzAbbr[PH7_TZDB_ABBR_COUNT] = {\n");
$iRow = 0;
foreach ($abbrRows as $r) {
    $w(sprintf("\t{ \"%s\", %d, %d, %d, %d, %d },\n",
        $r['name'], strlen($r['name']), $r['isdst'], $r['off'],
        $iRow, count($r['zones'])));
    $iRow += count($r['zones']);
}
$w("};\n\n");

$w("#define PH7_TZDB_ABBR_ZONE_COUNT $iRow\n");
$w("#define PH7_TZ_NOZONE 0xFFFF   /* a triple php prints a null timezone_id for */\n");
$w("typedef struct PH7_TzAbbrZoneRow PH7_TzAbbrZoneRow;\n");
$w("struct PH7_TzAbbrZoneRow\n{\n");
$w("\tsxi32 iOff;\n");
$w("\tsxu16 iZone;         /* into aTzZone, or PH7_TZ_NOZONE */\n");
$w("\tsxu8 bDst;\n");
$w("};\n");
$w("/* Grouped by aTzAbbr row, in php's own order WITHIN a group -- the first of\n");
$w(" * a group is that abbreviation's own fixed offset. */\n");
$w("static const PH7_TzAbbrZoneRow aTzAbbrZone[PH7_TZDB_ABBR_ZONE_COUNT] = {\n");
foreach ($abbrRows as $r) {
    foreach ($r['zones'] as $t) {
        $w(sprintf("\t{ %d, %s, %d },\n", $t['offset'],
            $t['timezone_id'] === null ? 'PH7_TZ_NOZONE' : (string) $zoneIx[$t['timezone_id']],
            $t['dst'] ? 1 : 0));
    }
}
$w("};\n\n");

$w("/* aTzAbbr walked in the order listAbbreviations() PRINTS, which is the flat\n");
$w(" * table's own and not the byte order aTzAbbr is stored in. */\n");
$w("static const sxu8 aTzAbbrOrder[PH7_TZDB_ABBR_COUNT] = {\n");
for ($i = 0, $n = count($order); $i < $n; $i += 16) {
    $w("\t" . implode(',', array_slice($order, $i, 16)) . ",\n");
}
$w("};\n\n");

$w("#define PH7_TZDB_ABBR_FALLBACK_COUNT " . count($fallback) . "\n");
$w("/* Keyed on (offset, daylight) alone, for an abbreviation that names nothing. */\n");
$w("static const PH7_TzAbbrZoneRow aTzAbbrFallback[PH7_TZDB_ABBR_FALLBACK_COUNT] = {\n");
foreach ($fallback as $r) {
    $w(sprintf("\t{ %d, %d, %d },\n", $r['off'], $r['zone'], $r['dst']));
}
$w("};\n");
fclose($fp);

fprintf(STDERR,
    "gen_tzdb: %d zones, %d distinct blocks, %d payload bytes, %d abbreviations "
    . "(%d triples, %d fallback rows), tzdata %s -> %s\n",
    count($rows), count($blobs), strlen($payload), count($abbrRows),
    $iRow, count($fallback), $version, $out);
