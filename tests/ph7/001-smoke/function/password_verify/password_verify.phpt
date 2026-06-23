--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_verify against published bcrypt vectors, cross-engine hashes, and malformed input
--FILE--
<?php
// Published OpenBSD bcrypt vectors (deterministic; $2y == $2a for ASCII).
echo password_verify("U*U",  '$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW') ? "ok\n" : "no\n";
echo password_verify("U*U*", '$2a$05$CCCCCCCCCCCCCCCCCCCCC.VGOzA784oUp/Z0DY336zx7pLYAy0lwK') ? "ok\n" : "no\n";
echo password_verify("wrong",'$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW') ? "ok\n" : "no\n";
// A PHP-generated $2y hash (cross-engine).
echo password_verify("correct horse", '$2y$05$mAEmCH5o5q9R8V1lr1rYtenLMhW7k2ewUmc8DHexlRPX/lE.AwbNK') ? "ok\n" : "no\n";
// High-bit (>0x7F) password — exercises the corrected unsigned "$2y" key handling.
echo password_verify("p\xa3ss\xffword", '$2y$05$kIgcIMyL7mW1phrQ3of4P.Tr7hV/hf9/wxx9sbTSc86rUkyL0AzO.') ? "ok\n" : "no\n";
// Malformed hashes must return false, never throw.
foreach (["", "x", str_repeat("z", 59), '$1$abcdefgh', '$2y$99$' . str_repeat("x", 53)] as $bad) {
    echo password_verify("p", $bad) ? "ok\n" : "no\n";
}
?>
--EXPECT--
ok
ok
no
ok
ok
no
no
no
no
no
--CLEAN--
<?php
