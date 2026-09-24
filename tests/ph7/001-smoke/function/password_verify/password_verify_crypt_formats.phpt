--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_verify checks ANY crypt(3) hash, not only bcrypt
--DESCRIPTION--
php's password_verify() falls back to crypt(password, hash) when the hash is
not a shape it knows, so a stored DES, ext-DES, MD5-crypt or SHA-crypt entry
— the migration case password_needs_rehash() exists for — verifies. PHL
answered false for every one of them. The "*0" token and the empty hash stay
false: no crypt output is shorter than 13 bytes.
--FILE--
<?php
$hashes = [
    crypt('secret', 'ab'),
    crypt('secret', '_1111sant'),
    crypt('secret', '$1$abcdefgh$'),
    crypt('secret', '$5$mysalt$'),
    crypt('secret', '$6$rounds=1001$xy$'),
];
foreach ($hashes as $h) {
    var_dump(password_verify('secret', $h));
    var_dump(password_verify('WRONG', $h));
}
var_dump(password_verify('p', '*0'));
var_dump(password_verify('p', '*1'));
var_dump(password_verify('', ''));
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
--CLEAN--
<?php
