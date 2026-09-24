--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_file/hash_hmac_file/hash_update_file/hash_update_stream read the bytes out of a file or an open stream
--FILE--
<?php
$hashFile = sys_get_temp_dir() . '/phl_hash_' . getmypid() . '.bin';
// More than one 8 KB read, and NUL bytes, so a text-mode or C-string reader
// would answer something else.
$hashData = "first line\nsecond line\n" . str_repeat("payload\x00\xff", 3000);
file_put_contents($hashFile, $hashData);

echo "hash_file: ", var_export(hash_file('sha256', $hashFile) === hash('sha256', $hashData), true), "\n";
echo "hash_file raw: ", strlen(hash_file('md5', $hashFile, true)), "\n";
echo "hash_file seeded: ", var_export(hash_file('murmur3a', $hashFile, false, ['seed' => 7])
    === hash('murmur3a', $hashData, false, ['seed' => 7]), true), "\n";
echo "hash_hmac_file: ", var_export(hash_hmac_file('sha3-256', $hashFile, 'the key')
    === hash_hmac('sha3-256', $hashData, 'the key'), true), "\n";
$hashCtx = hash_init('sha512');
echo "hash_update_file: ", var_export(hash_update_file($hashCtx, $hashFile), true),
     " ", var_export(hash_final($hashCtx) === hash('sha512', $hashData), true), "\n";

// A stream the caller already holds: the digest starts where the SCRIPT is,
// not where the device is, so a line already read is not hashed again.
$hashHandle = fopen($hashFile, 'rb');
$hashFirst = fgets($hashHandle);
$hashCtx = hash_init('md5');
$hashCount = hash_update_stream($hashCtx, $hashHandle);
fclose($hashHandle);
echo "after fgets: ", $hashCount, " ",
     var_export(hash_final($hashCtx) === hash('md5', substr($hashData, strlen($hashFirst))), true), "\n";

// $length is a window: negative means the rest, zero reads nothing, and two
// windows in a row continue where the first stopped.
foreach ([-1, 0, 10, 5000] as $hashLen) {
    $hashHandle = fopen($hashFile, 'rb');
    $hashCtx = hash_init('sha256');
    $hashCount = hash_update_stream($hashCtx, $hashHandle, $hashLen);
    fclose($hashHandle);
    $hashWant = $hashLen < 0 ? $hashData : substr($hashData, 0, $hashLen);
    echo "len=$hashLen n=$hashCount ", var_export(hash_final($hashCtx) === hash('sha256', $hashWant), true), "\n";
}
$hashHandle = fopen($hashFile, 'rb');
$hashCtx = hash_init('sha1');
hash_update_stream($hashCtx, $hashHandle, 100);
hash_update_stream($hashCtx, $hashHandle, 100);
fclose($hashHandle);
echo "two windows: ", var_export(hash_final($hashCtx) === hash('sha1', substr($hashData, 0, 200)), true), "\n";

// A memory stream is a stream too.
$hashMem = fopen('php://memory', 'r+');
fwrite($hashMem, "in memory");
rewind($hashMem);
$hashCtx = hash_init('md5');
hash_update_stream($hashCtx, $hashMem);
fclose($hashMem);
echo "php://memory: ", var_export(hash_final($hashCtx) === hash('md5', "in memory"), true), "\n";

// What each one answers when it cannot read: a warning and false, never a
// digest of nothing.
var_dump(@hash_file('md5', '/nonexistent'));
var_dump(@hash_hmac_file('md5', '/nonexistent', 'k'));
$hashCtx = hash_init('md5');
var_dump(@hash_update_file($hashCtx, '/nonexistent'));
echo "untouched: ", var_export(hash_final($hashCtx) === hash('md5', ""), true), "\n";
// A read that FAILS is not the end of the file — a directory is the case that
// tells them apart, and md5_file() (php's own included) answers the empty
// digest for it where hash_file() refuses.
var_dump(@hash_file('md5', sys_get_temp_dir()));
foreach ([['hash_file', fn() => hash_file('nope', $GLOBALS['hashFile'])],
          ['hash_hmac_file', fn() => hash_hmac_file('crc32b', $GLOBALS['hashFile'], 'k')]] as [$hashName, $hashFn]) {
    try { $hashFn(); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
}
$hashCtx = hash_init('md5');
try { hash_update_stream($hashCtx, "x"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$hashHandle = fopen($hashFile, 'rb');
fclose($hashHandle);
try { hash_update_stream($hashCtx, $hashHandle); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// A path with a NUL in it is refused before the filesystem is touched.
try { hash_update_file($hashCtx, "/tmp/x\0y"); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { hash_hmac_file('md5', "/tmp/x\0y", 'k'); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
@unlink($hashFile);
?>
--EXPECT--
hash_file: true
hash_file raw: 16
hash_file seeded: true
hash_hmac_file: true
hash_update_file: true true
after fgets: 27012 true
len=-1 n=27023 true
len=0 n=0 true
len=10 n=10 true
len=5000 n=5000 true
two windows: true
php://memory: true
bool(false)
bool(false)
bool(false)
untouched: true
bool(false)
hash_file(): Argument #1 ($algo) must be a valid hashing algorithm
hash_hmac_file(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
hash_update_stream(): Argument #2 ($stream) must be of type resource, string given
hash_update_stream(): Argument #2 ($stream) must be an open stream resource
hash_update_file(): Argument #2 ($filename) must not contain any null bytes
hash_hmac_file(): Argument #2 ($filename) must not contain any null bytes
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . '/phl_hash_' . getmypid() . '.bin');
