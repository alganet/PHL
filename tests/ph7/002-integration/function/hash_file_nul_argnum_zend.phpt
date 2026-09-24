--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: hash_file() names $algo in its null-byte refusal, where its two neighbours name $filename (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists("zend_version")) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php refuses a NUL inside a path before touching the filesystem, and names
 * the offending argument — except in hash_file(), where it names Argument #1
 * ($algo) for a NUL in argument #2. Its own two neighbours, hash_hmac_file()
 * and hash_update_file(), name $filename for the same byte in the same
 * position, so this is php disagreeing with itself rather than a rule. PHL
 * decides all 40 rows from the DECLARED position, so it names $filename in
 * all three. See the other half of the pair. */
try { hash_file('md5', "/tmp/x\0y"); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { hash_hmac_file('md5', "/tmp/x\0y", 'k'); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
$ctx = hash_init('md5');
try { hash_update_file($ctx, "/tmp/x\0y"); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
hash_file(): Argument #1 ($algo) must not contain any null bytes
hash_hmac_file(): Argument #2 ($filename) must not contain any null bytes
hash_update_file(): Argument #2 ($filename) must not contain any null bytes
--CLEAN--
<?php
