--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zip: the two ciphers a member can be under, and the three ways a key is refused
--DESCRIPTION--
The traditional PKWARE cipher and WinZip AES at all three key lengths, written
and read back through the archive. Nothing here prints a ciphertext -- a salt
and a header are random by construction -- so what is measured is the SHAPE the
format leaves behind (the sizes each cipher adds, the compression method the
extra field carries, and the CRC WinZip omits for a member under twenty bytes)
and the three errors php words differently.

One thing this deliberately does NOT do is set an mtime on a traditionally
encrypted entry. libzip computes that cipher's check byte from the DOS time and
then writes a DIFFERENT one when `setMtime*()` moved it, so php cannot read its
own archive back; this engine uses the stamp the header will carry and can. It
is a php defect rather than a contract, and reproducing it would mean writing an
archive nothing on earth can open.
--SKIPIF--
<?php
/* a capability guard: CI's Windows php is built without ext/zip */
if (!class_exists('ZipArchive')) {
    die("skip this php has no ext/zip\n");
}
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-zipe-' . getmypid();
@mkdir($dir);
function mask($s) {
    global $dir;
    $s = str_replace('\\', '/', (string) $s);
    return str_replace(str_replace('\\', '/', $dir), '<dir>', $s);
}
set_error_handler(function ($n, $s) {
    if (!(error_reporting() & $n)) { return true; }
    echo '  [', $n, '] ', mask($s), "\n";
    return true;
});
function show($label, $cb) {
    echo $label, ': ';
    try { echo var_export($cb(), true), "\n"; }
    catch (Throwable $e) { echo get_class($e), ': ', mask($e->getMessage()), "\n"; }
}

echo "-- each cipher, written and read back\n";
foreach ([
    'trad'   => ZipArchive::EM_TRAD_PKWARE,
    'aes128' => ZipArchive::EM_AES_128,
    'aes192' => ZipArchive::EM_AES_192,
    'aes256' => ZipArchive::EM_AES_256,
] as $name => $method) {
    $p = $dir . '/' . $name . '.zip';
    @unlink($p);
    $z = new ZipArchive();
    $z->open($p, ZipArchive::CREATE);
    $z->setPassword('s3cret');
    /* short enough that WinZip writes no CRC, and long enough that it does */
    $z->addFromString('short.txt', 'hello');
    $z->setEncryptionName('short.txt', $method);
    $z->addFromString('long.txt', str_repeat('AB', 600));
    $z->setEncryptionName('long.txt', $method);
    $z->addFromString('plain.txt', 'not encrypted');
    show($name . ' close', fn() => $z->close());
    $y = new ZipArchive();
    $y->open($p);
    for ($i = 0; $i < $y->numFiles; $i++) {
        $s = $y->statIndex($i);
        printf("  %-10s size=%-5d comp=%-5d method=%d enc=%-3d crc=%u\n", $s['name'],
            $s['size'], $s['comp_size'], $s['comp_method'], $s['encryption_method'], $s['crc']);
    }
    show('  with no key', fn() => $y->getFromName('short.txt'));
    show('  status', fn() => [$y->status, $y->getStatusString()]);
    $y->setPassword('wrong');
    show('  with the wrong key', fn() => $y->getFromName('short.txt'));
    show('  status', fn() => [$y->status, $y->getStatusString()]);
    $y->setPassword('s3cret');
    show('  short', fn() => $y->getFromName('short.txt'));
    show('  long', fn() => $y->getFromName('long.txt') === str_repeat('AB', 600));
    show('  the plain one never needed a key', fn() => $y->getFromName('plain.txt'));
    $y->close();
}

echo "-- the three refusals, each from a different place\n";
$p = $dir . '/r.zip';
@unlink($p);
$z = new ZipArchive();
$z->open($p, ZipArchive::CREATE);
$z->addFromString('a', 'AAA');
show('EM_NONE needs no key at all', fn() => $z->setEncryptionName('a', ZipArchive::EM_NONE));
show('status', fn() => [$z->status, $z->getStatusString()]);
show('a method no build writes', fn() => $z->setEncryptionName('a', 12345));
show('status', fn() => [$z->status, $z->getStatusString()]);
$z->clearError();
show('EM_UNKNOWN', fn() => $z->setEncryptionName('a', ZipArchive::EM_UNKNOWN));
show('status', fn() => [$z->status, $z->getStatusString()]);
$z->clearError();
show('a name nothing answers to', fn() => $z->setEncryptionName('zz', ZipArchive::EM_AES_256));
show('status', fn() => [$z->status, $z->getStatusString()]);
$z->clearError();
show('an index out of range', fn() => $z->setEncryptionIndex(99, ZipArchive::EM_AES_256));
show('status', fn() => [$z->status, $z->getStatusString()]);
$z->clearError();
show('...even for EM_NONE', fn() => $z->setEncryptionIndex(99, ZipArchive::EM_NONE));
show('status', fn() => [$z->status, $z->getStatusString()]);
$z->clearError();
show('a cipher with no key anywhere is taken', fn() => $z->setEncryptionName('a', ZipArchive::EM_AES_256));
show('...and the CLOSE is where it fails', fn() => $z->close());
show('status', fn() => [$z->status, $z->getStatusString()]);

echo "-- a key named on the entry outranks the archive's, for the write\n";
$p = $dir . '/p.zip';
@unlink($p);
$w = new ZipArchive();
$w->open($p, ZipArchive::CREATE);
$w->addFromString('a', 'AAA');
$w->setEncryptionName('a', ZipArchive::EM_AES_256, 'perentry');
$w->setPassword('archive');
show('close', fn() => $w->close());
$r = new ZipArchive();
$r->open($p);
$r->setPassword('archive');
show("the archive's", fn() => $r->getFromName('a'));
$r->setPassword('perentry');
show("the entry's", fn() => $r->getFromName('a'));
$r->close();

echo "-- an entry can be re-ciphered without being rewritten\n";
$p = $dir . '/c.zip';
@unlink($p);
$v = new ZipArchive();
$v->open($p, ZipArchive::CREATE);
$v->setPassword('pw');
$v->addFromString('a', str_repeat('A', 24));
$v->setEncryptionName('a', ZipArchive::EM_AES_256);
$v->close();
$u = new ZipArchive();
$u->open($p);
$u->setPassword('pw');
show('as written', fn() => $u->statIndex(0)['encryption_method']);
show('ask for the other one', fn() => $u->setEncryptionName('a', ZipArchive::EM_TRAD_PKWARE));
show('reported before the write', fn() => $u->statIndex(0)['encryption_method']);
show('close', fn() => $u->close());
$q = new ZipArchive();
$q->open($p);
$q->setPassword('pw');
show('after', fn() => [$q->statIndex(0)['encryption_method'], $q->statIndex(0)['comp_method'],
    $q->getFromName('a')]);
show('and back to none', fn() => $q->setEncryptionName('a', ZipArchive::EM_NONE));
show('close', fn() => $q->close());
$o = new ZipArchive();
$o->open($p);
show('plain now', fn() => [$o->statIndex(0)['encryption_method'], $o->getFromName('a')]);
$o->close();

echo "-- an encrypted entry survives a rewrite it was not part of\n";
$p = $dir . '/s.zip';
@unlink($p);
$a = new ZipArchive();
$a->open($p, ZipArchive::CREATE);
$a->setPassword('pw');
$a->addFromString('e', str_repeat('E', 40));
$a->setEncryptionName('e', ZipArchive::EM_AES_192);
$a->addFromString('p', 'plain');
$a->close();
$b = new ZipArchive();
$b->open($p);
$b->addFromString('added', 'X');
show('close', fn() => $b->close());
$c = new ZipArchive();
$c->open($p);
$c->setPassword('pw');
for ($i = 0; $i < $c->numFiles; $i++) {
    $s = $c->statIndex($i);
    printf("  %-6s enc=%-3d method=%d comp=%d\n", $s['name'], $s['encryption_method'],
        $s['comp_method'], $s['comp_size']);
}
show('still readable', fn() => $c->getFromName('e') === str_repeat('E', 40));
$c->close();

$rm = function ($d) use (&$rm) {
    if (!is_dir($d)) { @unlink($d); return; }
    foreach (scandir($d) as $f) { if ($f !== '.' && $f !== '..') { $rm("$d/$f"); } }
    @rmdir($d);
};
$rm($dir);
--EXPECT--
-- each cipher, written and read back
trad close: true
  short.txt  size=5     comp=17    method=0 enc=1   crc=907060870
  long.txt   size=1200  comp=25    method=8 enc=1   crc=784825680
  plain.txt  size=13    comp=13    method=0 enc=0   crc=292558185
  with no key: false
  status: array (
  0 => 26,
  1 => 'No password provided',
)
  with the wrong key: false
  status: array (
  0 => 27,
  1 => 'Wrong password provided',
)
  short: 'hello'
  long: true
  the plain one never needed a key: 'not encrypted'
aes128 close: true
  short.txt  size=5     comp=25    method=0 enc=257 crc=0
  long.txt   size=1200  comp=33    method=8 enc=257 crc=784825680
  plain.txt  size=13    comp=13    method=0 enc=0   crc=292558185
  with no key: false
  status: array (
  0 => 26,
  1 => 'No password provided',
)
  with the wrong key: false
  status: array (
  0 => 27,
  1 => 'Wrong password provided',
)
  short: 'hello'
  long: true
  the plain one never needed a key: 'not encrypted'
aes192 close: true
  short.txt  size=5     comp=29    method=0 enc=258 crc=0
  long.txt   size=1200  comp=37    method=8 enc=258 crc=784825680
  plain.txt  size=13    comp=13    method=0 enc=0   crc=292558185
  with no key: false
  status: array (
  0 => 26,
  1 => 'No password provided',
)
  with the wrong key: false
  status: array (
  0 => 27,
  1 => 'Wrong password provided',
)
  short: 'hello'
  long: true
  the plain one never needed a key: 'not encrypted'
aes256 close: true
  short.txt  size=5     comp=33    method=0 enc=259 crc=0
  long.txt   size=1200  comp=41    method=8 enc=259 crc=784825680
  plain.txt  size=13    comp=13    method=0 enc=0   crc=292558185
  with no key: false
  status: array (
  0 => 26,
  1 => 'No password provided',
)
  with the wrong key: false
  status: array (
  0 => 27,
  1 => 'Wrong password provided',
)
  short: 'hello'
  long: true
  the plain one never needed a key: 'not encrypted'
-- the three refusals, each from a different place
EM_NONE needs no key at all: true
status: array (
  0 => 0,
  1 => 'No error',
)
a method no build writes: false
status: array (
  0 => 24,
  1 => 'Encryption method not supported',
)
EM_UNKNOWN: false
status: array (
  0 => 24,
  1 => 'Encryption method not supported',
)
a name nothing answers to: false
status: array (
  0 => 9,
  1 => 'No such file',
)
an index out of range:   [2] ZipArchive::setEncryptionIndex(): password reset failed
false
status: array (
  0 => 18,
  1 => 'Invalid argument',
)
...even for EM_NONE:   [2] ZipArchive::setEncryptionIndex(): password reset failed
false
status: array (
  0 => 18,
  1 => 'Invalid argument',
)
a cipher with no key anywhere is taken: true
...and the CLOSE is where it fails:   [2] ZipArchive::close(): Invalid argument
false
status: array (
  0 => 18,
  1 => 'Invalid argument',
)
-- a key named on the entry outranks the archive's, for the write
close: true
the archive's: false
the entry's: 'AAA'
-- an entry can be re-ciphered without being rewritten
as written: 259
ask for the other one: true
reported before the write: 1
close: true
after: array (
  0 => 1,
  1 => 8,
  2 => 'AAAAAAAAAAAAAAAAAAAAAAAA',
)
and back to none: true
close: true
plain now: array (
  0 => 0,
  1 => 'AAAAAAAAAAAAAAAAAAAAAAAA',
)
-- an encrypted entry survives a rewrite it was not part of
close: true
  e      enc=258 method=8 comp=30
  p      enc=0   method=0 comp=5
  added  enc=0   method=0 comp=1
still readable: true
