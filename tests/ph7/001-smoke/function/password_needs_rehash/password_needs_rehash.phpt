--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_needs_rehash compares the stored cost/algo against the requested ones
--FILE--
<?php
$h = password_hash("x", PASSWORD_BCRYPT, ["cost" => 10]);
echo password_needs_rehash($h, PASSWORD_BCRYPT, ["cost" => 10]) ? "yes\n" : "no\n"; // same cost
echo password_needs_rehash($h, PASSWORD_BCRYPT, ["cost" => 11]) ? "yes\n" : "no\n"; // different cost
// (the default-options case is intentionally not asserted: the default bcrypt
// cost is version-dependent — 10 before PHP 8.4, 12 since.)
echo password_needs_rehash("garbage", PASSWORD_BCRYPT) ? "yes\n" : "no\n";
?>
--EXPECT--
no
yes
yes
--CLEAN--
<?php
