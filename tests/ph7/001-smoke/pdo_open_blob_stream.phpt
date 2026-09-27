--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Pdo\Sqlite::openBlob() answers a stream, and a PARAM_LOB column reads as one
--DESCRIPTION--
openBlob() hands back a php STREAM over one column of one row —
`stream_get_meta_data()` names its type `PDOSQLite` — and every stream verb
drives it: fread, stream_get_contents, ftell, fseek, feof, fstat's size and
fclose. Its LENGTH is fixed, because a blob handle addresses bytes that already
exist: a seek past the end FAILS and leaves the position unknown (ftell answers
false and a read answers nothing) until a seek succeeds again, and a write that
would need more bytes than there are is refused rather than truncated. A handle
opened READONLY refuses every write; only OPEN_READWRITE among the flags means
anything, since sqlite's blob handle is readable either way.

Every failure is a WARNING carrying sqlite's own message and a false answer —
the PDO error mode has no say, because this is a stream opener and not a
statement — and that covers a table, a column, a rowid and a database name that
are not there, plus a column whose value is not a string at all.

The other stream this driver hands out is what a PARAM_LOB bound column reads
as: php converts a STRING column into a read-only memory stream there and
leaves every other type as the driver typed it.
--FILE--
<?php
/* the warnings are captured rather than printed: this corpus runs in ONE
 * interpreter, so the handler goes back at the end */
set_error_handler(function ($no, $str) { echo "warning: $str\n"; return true; });
$obDb = new Pdo\Sqlite('sqlite::memory:');
$obDb->exec('CREATE TABLE ob (id INTEGER PRIMARY KEY, data BLOB, txt TEXT, num INTEGER, nil TEXT)');
$obDb->exec("INSERT INTO ob VALUES (1, x'0102030405', 'plain text', 42, NULL)");
$obMeta = function ($h) {
    $m = stream_get_meta_data($h);
    /* unread_bytes reports php's own read-ahead buffer, which PHL's streams do
     * not keep for ANY device (a plain file answers 0 here too) */
    unset($m['unread_bytes']);
    return $m;
};

$obM = new ReflectionMethod('Pdo\Sqlite', 'openBlob');
foreach ($obM->getParameters() as $obP) {
    printf("%s $%s%s\n", (string)$obP->getType(), $obP->getName(),
        $obP->isDefaultValueAvailable() ? ' = ' . var_export($obP->getDefaultValue(), true) : '');
}
var_dump(method_exists('PDO', 'openBlob'));

$obH = $obDb->openBlob('ob', 'data', 1);
var_dump(get_resource_type($obH), $obMeta($obH), fstat($obH)['size'], ftell($obH), feof($obH));
var_dump(bin2hex(fread($obH, 2)), ftell($obH));
var_dump(bin2hex(stream_get_contents($obH)), ftell($obH), feof($obH));
var_dump(fseek($obH, 1), ftell($obH), bin2hex(fread($obH, 2)));
var_dump(fseek($obH, 0, SEEK_END), ftell($obH));
/* out of the blob: the position goes unknown until a seek succeeds */
var_dump(fseek($obH, 99), ftell($obH), bin2hex(fread($obH, 2)), feof($obH));
var_dump(fseek($obH, -1), ftell($obH));
var_dump(fseek($obH, 0), ftell($obH), bin2hex(fread($obH, 1)));
/* a read-only handle refuses every write */
var_dump(fwrite($obH, 'XX'));
var_dump(fclose($obH));

/* OPEN_READWRITE writes IN PLACE, and never past the end */
$obRw = $obDb->openBlob('ob', 'data', 1, 'main', Pdo\Sqlite::OPEN_READWRITE);
var_dump(fwrite($obRw, "\xAA\xBB"), $obMeta($obRw)['mode']);
fseek($obRw, 4);
var_dump(fwrite($obRw, 'TOOLONG'));
fclose($obRw);
var_dump(bin2hex($obDb->query('SELECT data FROM ob')->fetchColumn()));

/* a TEXT column is a blob to sqlite; every other failure is a warning + false */
var_dump(stream_get_contents($obDb->openBlob('ob', 'txt', 1)));
var_dump($obDb->openBlob('ob', 'num', 1));
var_dump($obDb->openBlob('nosuch', 'data', 1));
var_dump($obDb->openBlob('ob', 'nosuch', 1));
var_dump($obDb->openBlob('ob', 'data', 99));
var_dump($obDb->openBlob('ob', 'data', 1, 'nosuchdb'));

/* a PARAM_LOB bound column reads as a memory stream, and only for a string */
$obSt = $obDb->query('SELECT txt, num, nil FROM ob');
$obSt->bindColumn(1, $obText, PDO::PARAM_LOB);
$obSt->bindColumn(2, $obNum, PDO::PARAM_LOB);
$obSt->bindColumn(3, $obNil, PDO::PARAM_LOB);
$obSt->fetch();
var_dump(get_resource_type($obText), $obMeta($obText), fstat($obText)['size']);
var_dump(stream_get_contents($obText), ftell($obText));
var_dump(fwrite($obText, 'X'));
rewind($obText);
var_dump(fread($obText, 5), $obNum, $obNil);
var_dump(fclose($obText));
restore_error_handler();
?>
--EXPECT--
string $table
string $column
int $rowid
?string $dbname = 'main'
int $flags = 1
bool(false)
string(6) "stream"
array(6) {
  ["timed_out"]=>
  bool(false)
  ["blocked"]=>
  bool(true)
  ["eof"]=>
  bool(false)
  ["stream_type"]=>
  string(9) "PDOSQLite"
  ["mode"]=>
  string(2) "rb"
  ["seekable"]=>
  bool(true)
}
int(5)
int(0)
bool(false)
string(4) "0102"
int(2)
string(6) "030405"
int(5)
bool(true)
int(0)
int(1)
string(4) "0203"
int(0)
int(5)
int(-1)
bool(false)
string(0) ""
bool(true)
int(-1)
bool(false)
int(0)
int(0)
string(2) "01"
warning: fwrite(): Can't write to blob stream: is open as read only
bool(false)
bool(true)
int(2)
string(3) "r+b"
warning: fwrite(): It is not possible to increase the size of a BLOB
bool(false)
string(10) "aabb030405"
string(10) "plain text"
warning: Unable to open blob: cannot open value of type integer
bool(false)
warning: Unable to open blob: no such table: main.nosuch
bool(false)
warning: Unable to open blob: no such column: "nosuch"
bool(false)
warning: Unable to open blob: no such rowid: 99
bool(false)
warning: Unable to open blob: no such table: nosuchdb.ob
bool(false)
string(6) "stream"
array(6) {
  ["timed_out"]=>
  bool(false)
  ["blocked"]=>
  bool(true)
  ["eof"]=>
  bool(false)
  ["stream_type"]=>
  string(6) "MEMORY"
  ["mode"]=>
  string(2) "rb"
  ["seekable"]=>
  bool(true)
}
int(10)
string(10) "plain text"
int(10)
bool(false)
string(5) "plain"
int(42)
NULL
bool(true)
