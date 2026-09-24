--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_needs_rehash resolves $algo BEFORE looking at the hash: unknown algo is false, not true
--DESCRIPTION--
php's password_needs_rehash() looks $algo up in the registry first and answers
FALSE when the lookup fails — password_needs_rehash('anything', 'nope') is not
a rehash request, whatever the hash looks like. Only with a resolved algo does
the hash matter: a hash that algo did not make is TRUE, and a bcrypt hash
against bcrypt compares costs. PHL answered TRUE for the unknown algo (it
tested the hash first) and refused the legacy int ids the lookup accepts.
--FILE--
<?php
$b4 = password_hash('pw', PASSWORD_BCRYPT, ['cost' => 4]);
var_dump(password_needs_rehash($b4, 'nope'));
var_dump(password_needs_rehash('garbage', 'nope'));
var_dump(password_needs_rehash('garbage', 99));
var_dump(password_needs_rehash('garbage', PASSWORD_BCRYPT));
var_dump(password_needs_rehash('garbage', null));
var_dump(password_needs_rehash($b4, 1, ['cost' => 4]));
var_dump(password_needs_rehash($b4, 1));
var_dump(password_needs_rehash($b4, 0, ['cost' => 4]));
var_dump(password_needs_rehash($b4, PASSWORD_BCRYPT, ['cost' => 5]));
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
--CLEAN--
<?php
