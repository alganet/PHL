--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_hash $algo is a registry lookup: legacy int ids, name strings, and the two bcrypt refusals
--DESCRIPTION--
php resolves $algo through its algorithm registry: NULL and the pre-7.4 INT ids
0 (PASSWORD_DEFAULT) and 1 (PASSWORD_BCRYPT) select bcrypt, a bool or an
integral float arrives as that int under ZPP, and a STRING is a NAME — so '1'
and '0' resolve to nothing and are the ValueError, even though int 1 hashes.
Beside the lookup sit two refusals inside the bcrypt arm itself: a password
carrying a NUL byte is a ValueError (the C crypt under it is NUL-terminated, so
"a\0b" would silently hash as "a"), and the php 5/7 "salt" option is ignored
with a warning, never read. PHL answered a ValueError for every int, hashed the
NUL password in silence, and said nothing about the salt.
--FILE--
<?php
set_error_handler(function ($n, $s) { echo '  [warn] ', $s, "\n"; return true; });
foreach ([null, 0, 1, true, 1.0, '2y'] as $algo) {
    $h = password_hash('pw', $algo, ['cost' => 4]);
    echo str_replace("\n", '', var_export($algo, true)), ' => ', substr($h, 0, 4), "\n";
}
foreach ([4, '1', '0', ''] as $algo) {
    try {
        password_hash('pw', $algo, ['cost' => 4]);
        echo str_replace("\n", '', var_export($algo, true)), " => HASHED\n";
    } catch (ValueError $e) {
        echo str_replace("\n", '', var_export($algo, true)), ' => ', $e->getMessage(), "\n";
    }
}
try {
    password_hash("a\0b", PASSWORD_BCRYPT, ['cost' => 4]);
    echo "nul => HASHED\n";
} catch (ValueError $e) {
    echo 'nul => ', $e->getMessage(), "\n";
}
$h = password_hash('pw', PASSWORD_BCRYPT, ['salt' => str_repeat('s', 22), 'cost' => 4]);
echo 'salt ignored, hash still made: ', substr($h, 0, 7), "\n";
?>
--EXPECT--
NULL => $2y$
0 => $2y$
1 => $2y$
true => $2y$
1.0 => $2y$
'2y' => $2y$
4 => password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm
'1' => password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm
'0' => password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm
'' => password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm
nul => Bcrypt password must not contain null character
  [warn] password_hash(): The "salt" option has been ignored, since providing a custom salt is no longer supported
salt ignored, hash still made: $2y$04$
--CLEAN--
<?php
