--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SQLite3::backup() copies a database and openBlob() hands one value back as a stream
--DESCRIPTION--
`backup()` is sqlite's own online backup run to completion in one call. What
php does NOT do is check the two database NAMES: `sqlite3_backup_init` answers
nothing for a name neither connection carries, and php reads that as having no
work to do -- so a misspelt source is a quiet true over an empty destination.
The one refusal it words itself is the pair being the same connection.

`openBlob()` is a HANDLE on the bytes of one value, behind a php stream. It
addresses bytes that already exist: there is nowhere past the end to seek to,
and a failed seek leaves the position UNKNOWN, so ftell() answers false until a
seek succeeds again. End-of-file is a READ reaching the last byte and not a
position -- seeking to the end leaves feof() false.

Its two write refusals are not the pair they look like. The read-only one tests
the READONLY BIT being set rather than READWRITE being absent, so a handle
opened with neither (0, or CREATE, which means nothing here) says nothing at all
and simply fails; and the size one, which only a handle past that first test can
reach, is what a blob being fixed-length means.

An open blob is also the one thing that makes close() FAIL: php closes with
sqlite3_close rather than the forgiving _v2, so the refusal is reported, the
connection stays open and usable, and the statements it finalized on the way in
are gone regardless.
--FILE--
<?php
$sq8dv = function ($v) use (&$sq8dv) {
    if (is_array($v)) {
        $o = [];
        foreach ($v as $k => $x) { $o[] = var_export($k, true) . '=>' . $sq8dv($x); }
        return '[' . implode(', ', $o) . ']';
    }
    if (is_string($v)) { return ctype_print($v) ? '"' . $v . '"' : 'hex:' . bin2hex($v); }
    if (is_resource($v)) { return 'res:' . get_resource_type($v); }
    if (is_object($v)) { return 'obj:' . get_class($v); }
    return var_export($v, true);
};
$sq8show = function ($label, $fn) use ($sq8dv) {
    $notes = [];
    set_error_handler(function ($no, $str) use (&$notes) { $notes[] = "[$no] $str"; return true; });
    try { $out = $sq8dv($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    foreach ($notes as $n) { echo '  ', $n, "\n"; }
    echo str_pad($label, 36), ' => ', $out, "\n";
};
$sq8mk = function () {
    $d = new SQLite3(':memory:');
    $d->exec('CREATE TABLE v (a INTEGER PRIMARY KEY, b BLOB, c TEXT)');
    $d->exec("INSERT INTO v VALUES (1, x'0102030405', 'txt'), (2, NULL, ''), (3, x'', 'y')");
    return $d;
};

/* backup: sqlite's own, run to completion in one call */
$sq8show('backup', function () use ($sq8mk) {
    $s = $sq8mk(); $t = new SQLite3(':memory:');
    return [$s->backup($t), $t->querySingle('SELECT c FROM v WHERE a = 1')]; });
$sq8show('a name neither carries', function () use ($sq8mk) {
    $s = $sq8mk(); $t = new SQLite3(':memory:');
    return [$s->backup($t, 'nope'), $t->querySingle('SELECT count(*) FROM sqlite_master')]; });
$sq8show('to itself', function () use ($sq8mk) { $s = $sq8mk(); return $s->backup($s); });
$sq8show('to a closed database', function () use ($sq8mk) {
    $s = $sq8mk(); $t = new SQLite3(':memory:'); $t->close(); return $s->backup($t); });
$sq8show('from a closed database', function () use ($sq8mk) {
    $s = $sq8mk(); $s->close(); return $s->backup(new SQLite3(':memory:')); });
$sq8show('not a SQLite3 at all', function () use ($sq8mk) { $s = $sq8mk(); return $s->backup(null); });

/* openBlob: a handle on the bytes of ONE value */
$sq8show('openBlob', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    return [$sq8dv($h), stream_get_contents($h)]; });
$sq8show('reading it', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    return [fread($h, 3), ftell($h), feof($h), fread($h, 10), feof($h)]; });
$sq8show('the end is a READ, not a seek', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    fseek($h, 0, SEEK_END); $atEnd = feof($h);
    fseek($h, 0); fread($h, 5);
    return [$atEnd, feof($h)]; });
$sq8show('seeking', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    fseek($h, 2);
    return [ftell($h), fread($h, 2), fseek($h, -1, SEEK_END), ftell($h), fread($h, 5)]; });
$sq8show('nowhere past the end', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    return [fseek($h, 99), ftell($h), fseek($h, 1), ftell($h)]; });
$sq8show('its stat', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1); $s = fstat($h);
    return [count($s), $s['size'], $s['mode'], $s[7]]; });
$sq8show('its meta', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    return stream_get_meta_data($h); });
$sq8show('read-only by default', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1); return fwrite($h, 'zz'); });
$sq8show('and the size comes first', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1); fseek($h, 4); return fwrite($h, 'zz'); });
$sq8show('writing in place', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1, 'main', SQLITE3_OPEN_READWRITE);
    $n = fwrite($h, "\xff\xfe"); fclose($h);
    return [$n, bin2hex($d->querySingle('SELECT b FROM v WHERE a = 1'))]; });
$sq8show('a blob cannot grow', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1, 'main', SQLITE3_OPEN_READWRITE);
    fseek($h, 4);
    return [fwrite($h, 'ab'), bin2hex($d->querySingle('SELECT b FROM v WHERE a = 1'))]; });
/* the read-only refusal tests the READONLY BIT, not the absence of READWRITE:
   a handle opened with neither says nothing and simply fails */
$sq8show('a flag that means neither', function () use ($sq8mk) {
    $out = [];
    foreach ([0, SQLITE3_OPEN_READONLY, SQLITE3_OPEN_READWRITE, SQLITE3_OPEN_CREATE,
              SQLITE3_OPEN_READONLY | SQLITE3_OPEN_READWRITE] as $fl) {
        $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1, 'main', $fl);
        $out[] = [$fl, fwrite($h, 'z'), bin2hex($d->querySingle('SELECT b FROM v WHERE a = 1'))];
    }
    return $out; });
$sq8show('a text column is bytes too', function () use ($sq8mk) {
    $d = $sq8mk(); return stream_get_contents($d->openBlob('v', 'c', 1)); });
$sq8show('an empty value', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 3);
    return [fstat($h)['size'], stream_get_contents($h)]; });
foreach ([['a NULL value', 'v', 'b', 2, 'main'], ['no such row', 'v', 'b', 99, 'main'],
          ['no such column', 'v', 'zz', 1, 'main'], ['no such table', 'zz', 'b', 1, 'main'],
          ['no such database', 'v', 'b', 1, 'nope']] as $c) {
    $sq8show($c[0], function () use ($sq8mk, $c) {
        $d = $sq8mk(); return $d->openBlob($c[1], $c[2], $c[3], $c[4]); });
}

/* an open blob is what makes close() FAIL */
$sq8show('close with a blob open', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1);
    $first = $d->close();
    $still = [$d->lastErrorCode(), $d->querySingle('SELECT 1')];
    $bytes = bin2hex(stream_get_contents($h));
    fclose($h);
    return [$first, $still, $bytes, $d->close()]; });
$sq8show('and the statements went anyway', function () use ($sq8mk) {
    $d = $sq8mk(); $h = $d->openBlob('v', 'b', 1); $st = $d->prepare('SELECT a FROM v');
    $d->close();
    return $st->execute(); });
--EXPECT--
backup                               => [0=>true, 1=>"txt"]
a name neither carries               => [0=>true, 1=>0]
  [2] SQLite3::backup(): Backup failed: source and destination must be distinct
to itself                            => false
to a closed database                 => Error: The SQLite3 object has not been correctly initialised or is already closed
from a closed database               => Error: The SQLite3 object has not been correctly initialised or is already closed
not a SQLite3 at all                 => TypeError: SQLite3::backup(): Argument #1 ($destination) must be of type SQLite3, null given
  [2] Undefined variable $sq8dv
openBlob                             => Error: Value of type null is not callable
reading it                           => [0=>hex:010203, 1=>3, 2=>false, 3=>hex:0405, 4=>true]
the end is a READ, not a seek        => [0=>false, 1=>true]
seeking                              => [0=>2, 1=>hex:0304, 2=>0, 3=>4, 4=>hex:05]
nowhere past the end                 => [0=>-1, 1=>false, 2=>0, 3=>1]
its stat                             => [0=>26, 1=>5, 2=>0, 3=>5]
its meta                             => ['timed_out'=>false, 'blocked'=>true, 'eof'=>false, 'stream_type'=>"SQLite3", 'mode'=>"rb", 'unread_bytes'=>0, 'seekable'=>true]
  [2] fwrite(): Can't write to blob stream: is open as read only
read-only by default                 => false
  [2] fwrite(): Can't write to blob stream: is open as read only
and the size comes first             => false
writing in place                     => [0=>2, 1=>"fffe030405"]
  [2] fwrite(): It is not possible to increase the size of a BLOB
a blob cannot grow                   => [0=>false, 1=>"0102030405"]
  [2] fwrite(): Can't write to blob stream: is open as read only
  [2] fwrite(): Can't write to blob stream: is open as read only
a flag that means neither            => [0=>[0=>0, 1=>false, 2=>"0102030405"], 1=>[0=>1, 1=>false, 2=>"0102030405"], 2=>[0=>2, 1=>1, 2=>"7a02030405"], 3=>[0=>4, 1=>false, 2=>"0102030405"], 4=>[0=>3, 1=>false, 2=>"0102030405"]]
a text column is bytes too           => "txt"
an empty value                       => [0=>0, 1=>hex:]
  [2] SQLite3::openBlob(): Unable to open blob: cannot open value of type null
a NULL value                         => false
  [2] SQLite3::openBlob(): Unable to open blob: no such rowid: 99
no such row                          => false
  [2] SQLite3::openBlob(): Unable to open blob: no such column: "zz"
no such column                       => false
  [2] SQLite3::openBlob(): Unable to open blob: no such table: main.zz
no such table                        => false
  [2] SQLite3::openBlob(): Unable to open blob: no such table: nope.v
no such database                     => false
  [2] SQLite3::close(): Unable to close database: unable to close due to unfinalized statements or unfinished backups
close with a blob open               => [0=>false, 1=>[0=>5, 1=>1], 2=>"0102030405", 3=>true]
  [2] SQLite3::close(): Unable to close database: unable to close due to unfinalized statements or unfinished backups
and the statements went anyway       => Error: The SQLite3 object has not been correctly initialised or is already closed
