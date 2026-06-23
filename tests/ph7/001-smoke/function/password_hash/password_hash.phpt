--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_hash produces a verifiable 60-char $2y bcrypt hash (shape, default cost, truncation)
--FILE--
<?php
$h = password_hash("hunter2", PASSWORD_BCRYPT, ["cost" => 4]);
echo strlen($h) === 60 ? "len60\n" : "BADLEN\n";
echo substr($h, 0, 7) === '$2y$04$' ? "prefix\n" : "BADPREFIX\n";
echo password_verify("hunter2", $h) ? "ok\n" : "no\n";
echo password_verify("nope", $h) ? "ok\n" : "no\n";
// PASSWORD_DEFAULT is bcrypt at the engine's default cost (10 before PHP 8.4,
// 12 since), which must equal PASSWORD_BCRYPT_DEFAULT_COST — derive it rather
// than hardcode so the test is version-agnostic.
$di = password_get_info(password_hash("x", PASSWORD_DEFAULT));
echo ($di["algoName"] === "bcrypt" && $di["options"]["cost"] === PASSWORD_BCRYPT_DEFAULT_COST)
    ? "defaultcost\n" : "BADDEFAULT\n";
// Empty password round-trips.
$e = password_hash("", PASSWORD_BCRYPT, ["cost" => 4]);
echo password_verify("", $e) ? "ok\n" : "no\n";
// bcrypt truncates at 72 bytes: a 72-byte password and that same password plus
// a 73rd byte produce the same hash.
$a = str_repeat("a", 72);
echo password_verify($a . "X", password_hash($a, PASSWORD_BCRYPT, ["cost" => 4])) ? "ok\n" : "no\n";
?>
--EXPECT--
len60
prefix
ok
no
defaultcost
ok
ok
--CLEAN--
<?php
