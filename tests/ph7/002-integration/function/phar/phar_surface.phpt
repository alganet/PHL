--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/phar: the inventory, the three classes and what phar.readonly refuses
--DESCRIPTION--
The API half: what the extension IS, and the five different sentences php's
write doors answer with while `phar.readonly` is on -- which is php's own
default, so they are what a program normally sees.
--SKIPIF--
<?php
/* php on macOS answers the read-only tar doors through its resolved
 * /private/var path, and so takes different sentences than the ones pinned. */
if (PHP_OS_FAMILY !== 'Linux') {
    die("skip the read-only door sentences pinned here are Linux's\n");
}
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-pharro-' . getmypid();
@mkdir($dir);
set_error_handler(function ($n, $s) use (&$dir) {
    if (!(error_reporting() & $n)) { return true; }
    echo '  [', $n, '] ', str_replace([$dir, strtr($dir, '/', '\\')], '<dir>', $s), "\n";
    return true;
});
function show($label, $cb) {
    global $dir;
    echo $label, ': ';
    try { echo var_export($cb(), true), "\n"; }
    catch (Throwable $e) {
        echo get_class($e), ': ', str_replace($dir, '<dir>', $e->getMessage()), "\n";
    }
}

echo "-- the extension\n";
var_dump(extension_loaded('Phar'), in_array('phar', stream_get_wrappers(), true));
$ext = new ReflectionExtension('Phar');
echo implode(', ', $ext->getClassNames()), "\n";
foreach ($ext->getINIEntries() as $k => $v) { printf("%-20s %s\n", $k, var_export($v, true)); }
echo "-- and its class constants, which are also its wire values\n";
$c = new ReflectionClass('Phar');
foreach ($c->getConstants() as $k => $v) {
    /* The directory-iterator constants come from the parent and are its test's. */
    if (in_array($k, ['BZ2','GZ','NONE','PHAR','TAR','ZIP','COMPRESSED','PHP','PHPS',
                      'MD5','SHA1','SHA256','SHA512','OPENSSL','OPENSSL_SHA256','OPENSSL_SHA512'], true)) {
        printf("%-16s %d\n", $k, $v);
    }
}
echo "-- what each class IS\n";
foreach (['Phar', 'PharData', 'PharFileInfo', 'PharException'] as $name) {
    $r = new ReflectionClass($name);
    printf("%-14s parent=%-26s countable=%d arrayaccess=%d iterator=%d\n", $name,
        $r->getParentClass() ? $r->getParentClass()->getName() : '-',
        (int) $r->implementsInterface('Countable'),
        (int) $r->implementsInterface('ArrayAccess'),
        (int) $r->implementsInterface('Iterator'));
}
echo "-- the statics that answer for the BUILD\n";
/* canCompress(Phar::BZ2) is left out: it answers whether THIS php was built
 * with ext/bz2, which setup-php's is and a distro's often is not. */
var_dump(Phar::apiVersion(), Phar::canWrite(),
    Phar::interceptFileFuncs(), Phar::running(), Phar::running(false));
$sig = Phar::getSupportedSignatures();
foreach (['MD5', 'SHA-1', 'SHA-256', 'SHA-512',
          'OpenSSL', 'OpenSSL_SHA256', 'OpenSSL_SHA512'] as $want) {
    printf("  %-16s %d\n", $want, (int) in_array($want, $sig, true));
}
var_dump($sig === array_values(array_unique($sig)), count($sig));
echo "-- which names are archive names\n";
foreach ([['a.phar', true], ['a.txt', true], ['a.phar.tar', true], ['dir/b.phar', true],
          ['a.tar', false], ['a.zip', false], ['a.txt', false]] as [$name, $exec]) {
    printf("  %-12s executable=%d -> %d\n", $name, (int) $exec,
        (int) Phar::isValidPharFilename($name, $exec));
}
echo "-- the default stub is a program that stops at the halt\n";
$stub = Phar::createDefaultStub();
var_dump(str_contains($stub, '__HALT_COMPILER();'), str_starts_with($stub, '<?php'));

echo "-- with phar.readonly ON, every write door has its own sentence\n";
$p = $dir . '/ro.phar';
show('creating one at all', fn() => new Phar($p));
/* A DATA archive is not gated by the directive: php lets a script build a tar
 * on a stock install, which is what an installer does. */
$tar = $dir . '/ro.tar';
$pd = new PharData($tar);
$pd->addFromString('a.txt', 'A');
var_dump(count($pd), file_get_contents("phar://$tar/a.txt"));
show('...but a stub is still refused on one', fn() => $pd->setStub('<?php __HALT_COMPILER();'));

echo "-- and the wrapper refuses a write the same way\n";
show('fopen w', fn() => fopen("phar://$p/new.txt", 'w'));
show('unlink', fn() => unlink("phar://$tar/a.txt"));
var_dump(file_get_contents("phar://$tar/a.txt"));

echo "-- with the directive ON, each path door has a refusal of its own\n";
/* An archive has to EXIST before a door can refuse to write it, and building one
 * is what the directive forbids -- so a child process with it off builds it,
 * exactly as an installer would. */
$build = $dir . '/build.php';
file_put_contents($build, '<?php $d = ' . var_export($dir, true) . ';'
    . ' $o = new Phar("$d/ro2.phar"); $o->addFromString("x.txt", "X");'
    . ' $o->addFromString("sub/y.txt", "Y"); unset($o);'
    . ' $t = new PharData("$d/ro2.tar"); $t->addFromString("x.txt", "X");'
    . ' $t->addFromString("sub/y.txt", "Y"); unset($t);');
exec(escapeshellarg(PHP_BINARY) . ' -d phar.readonly=0 ' . escapeshellarg($build) . ' 2>&1', $out);
var_dump($out, file_exists($dir . '/ro2.phar'), file_exists($dir . '/ro2.tar'));
foreach (['ro2.phar', 'ro2.tar'] as $name) {
    $a = $dir . '/' . $name;
    echo "-- $name\n";
    echo "  unlink : "; var_dump(unlink("phar://$a/x.txt"));
    echo "  rmdir  : "; var_dump(rmdir("phar://$a/sub"));
    echo "  mkdir  : "; var_dump(mkdir("phar://$a/nd"));
    echo "  rename : "; var_dump(rename("phar://$a/sub/y.txt", "phar://$a/sub/z.txt"));
    echo "  touch  : "; var_dump(touch("phar://$a/sub/y.txt"));
    echo "  still there: "; var_dump(@file_get_contents("phar://$a/sub/y.txt"));
}

foreach (glob($dir . '/*') as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
-- the extension
bool(true)
bool(true)
PharException, Phar, PharData, PharFileInfo
phar.readonly        '1'
phar.require_hash    '1'
phar.cache_list      ''
-- and its class constants, which are also its wire values
BZ2              8192
GZ               4096
NONE             0
PHAR             1
TAR              2
ZIP              3
COMPRESSED       61440
PHP              0
PHPS             1
MD5              1
OPENSSL          16
OPENSSL_SHA256   17
OPENSSL_SHA512   18
SHA1             2
SHA256           3
SHA512           4
-- what each class IS
Phar           parent=RecursiveDirectoryIterator countable=1 arrayaccess=1 iterator=1
PharData       parent=RecursiveDirectoryIterator countable=1 arrayaccess=1 iterator=1
PharFileInfo   parent=SplFileInfo                countable=0 arrayaccess=0 iterator=0
PharException  parent=Exception                  countable=0 arrayaccess=0 iterator=0
-- the statics that answer for the BUILD
string(5) "1.1.1"
bool(false)
NULL
string(0) ""
string(0) ""
  MD5              1
  SHA-1            1
  SHA-256          1
  SHA-512          1
  OpenSSL          1
  OpenSSL_SHA256   1
  OpenSSL_SHA512   1
bool(true)
int(7)
-- which names are archive names
  a.phar       executable=1 -> 1
  a.txt        executable=1 -> 0
  a.phar.tar   executable=1 -> 1
  dir/b.phar   executable=1 -> 0
  a.tar        executable=0 -> 1
  a.zip        executable=0 -> 1
  a.txt        executable=0 -> 1
-- the default stub is a program that stops at the halt
bool(true)
bool(true)
-- with phar.readonly ON, every write door has its own sentence
creating one at all: UnexpectedValueException: creating archive "<dir>/ro.phar" disabled by the php.ini setting phar.readonly
int(1)
string(1) "A"
...but a stub is still refused on one: UnexpectedValueException: A Phar stub cannot be set in a plain tar archive
-- and the wrapper refuses a write the same way
fopen w:   [2] fopen(phar://<dir>/ro.phar/new.txt): Failed to open stream: phar error: write operations disabled by the php.ini setting phar.readonly
false
unlink: true
  [2] file_get_contents(phar://<dir>/ro.tar/a.txt): Failed to open stream: phar error: "a.txt" is not a file in phar "<dir>/ro.tar"
bool(false)
-- with the directive ON, each path door has a refusal of its own
array(0) {
}
bool(true)
bool(true)
-- ro2.phar
  unlink :   [2] unlink(): phar error: write operations disabled by the php.ini setting phar.readonly
bool(false)
  rmdir  :   [2] rmdir(): phar error: cannot rmdir directory "phar://<dir>/ro2.phar/sub", write operations disabled
bool(false)
  mkdir  :   [2] mkdir(): phar error: cannot create directory "phar://<dir>/ro2.phar/nd", write operations disabled
bool(false)
  rename :   [2] rename(): phar error: cannot rename "phar://<dir>/ro2.phar/sub/y.txt" to "phar://<dir>/ro2.phar/sub/z.txt": invalid or non-writable url "phar://<dir>/ro2.phar/sub/y.txt"
bool(false)
  touch  : bool(true)
  still there: string(1) "Y"
-- ro2.tar
  unlink : bool(true)
  rmdir  :   [2] rmdir(): Cannot create phar '<dir>/ro2.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  mkdir  :   [2] mkdir(): Cannot create phar '<dir>/ro2.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  rename :   [2] rename(): phar error: cannot rename "phar://<dir>/ro2.tar/sub/y.txt" to "phar://<dir>/ro2.tar/sub/z.txt": invalid or non-writable url "phar://<dir>/ro2.tar/sub/y.txt"
bool(false)
  touch  : bool(true)
  still there: string(1) "Y"
