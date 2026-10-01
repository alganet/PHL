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
 * ALL_WITH_BC, and each zone's `getLocation()` -- is asked of the PHP running
 * this script, because it is php's own classification rather than anything the
 * TZif files carry. Run it under the oracle, against the same tzdata the oracle
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
$abbrRows = [];
foreach (array_keys(DateTimeZone::listAbbreviations()) as $key) {
    $z = new DateTimeZone($key);
    $d = new DateTime('@0');
    $d->setTimezone($z);
    $abbrRows[] = [
        'name'  => $z->getName(),
        'off'   => (int) $z->getOffset(new DateTime('@0')),
        'isdst' => $d->format('I') === '1' ? 1 : 0,
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

    ];
}

/* --- Emit ------------------------------------------------------------------ */
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
 * aTzZone is sorted by identifier, byte-wise and case-SENSITIVELY, which is the
 * order the binary search and `timezone_identifiers_list()` both want. The
 * lookup itself folds case -- php resolves `europe/paris` and stores the
 * caller's spelling -- so it walks the range rather than comparing folded keys.
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
$w("\tsxu32 iOfst;         /* where its TZif block starts in aTzPayload */\n");
$w("\tsxu32 nByte;\n");
$w("};\n");
$w("static const PH7_TzZoneRow aTzZone[PH7_TZDB_ZONE_COUNT] = {\n");
foreach ($rows as $r) {
    $w(sprintf("\t{ \"%s\", %d, %d, %d, %d },\n",
        $r['id'], strlen($r['id']), $r['bc'] ? 1 : 0, $r['ofst'], $r['byte']));
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
$w("};\n");
$w("/* Sorted by name, upper-case and byte-wise, for the binary search. */\n");
$w("static const PH7_TzAbbrRow aTzAbbr[PH7_TZDB_ABBR_COUNT] = {\n");
foreach ($abbrRows as $r) {
    $w(sprintf("\t{ \"%s\", %d, %d, %d },\n",
        $r['name'], strlen($r['name']), $r['isdst'], $r['off']));
}
$w("};\n");
fclose($fp);

fprintf(STDERR,
    "gen_tzdb: %d zones, %d distinct blocks, %d payload bytes, %d abbreviations, tzdata %s -> %s\n",
    count($rows), count($blobs), strlen($payload), count($abbrRows), $version, $out);
