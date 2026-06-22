--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_algos() returns a 0-indexed list whose every entry is a usable algorithm
--FILE--
<?php
$algos = hash_algos();
// 0-indexed list (PHL supports a curated subset; assert membership, not the full set)
echo array_key_exists(0, $algos) ? "list\n" : "notlist\n";
foreach (['md5','sha1','sha224','sha256','sha384','sha512'] as $a) {
    echo $a, in_array($a, $algos, true) ? " present\n" : " MISSING\n";
}
// every advertised algorithm must actually work
$ok = true;
foreach ($algos as $a) {
    if (strlen(hash($a, "x")) < 1) { $ok = false; }
}
echo $ok ? "all-usable\n" : "BROKEN\n";
?>
--EXPECT--
list
md5 present
sha1 present
sha224 present
sha256 present
sha384 present
sha512 present
all-usable
--CLEAN--
<?php
