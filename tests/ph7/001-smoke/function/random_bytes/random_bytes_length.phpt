--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes returns a string of the requested length
--SKIPIF--
<?php
if (!function_exists('random_bytes')) { echo 'skip: random_bytes not available'; }
?>
--FILE--
<?php
$ok = true;
foreach (array(1, 16, 32, 64) as $n) {
    if (strlen(random_bytes($n)) !== $n) { $ok = false; break; }
}
echo $ok ? "len_ok\n" : "len_fail\n";
?>
--EXPECT--
len_ok
--CLEAN--
<?php
