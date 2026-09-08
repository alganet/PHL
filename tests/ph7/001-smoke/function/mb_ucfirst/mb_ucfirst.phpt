--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_ucfirst and mb_lcfirst case-map only the first multibyte character
--FILE--
<?php
$u = [];
foreach (["hello", "\xc3\xa9\xc3\xa0\xc3\xae", "\xc3\x9cber", "a", "", "HELLO", "stra\xc3\x9fe", "\xce\xb3\xce\xb5\xce\xb9\xce\xac"] as $s) {
    $u[] = mb_ucfirst($s);
}
echo implode("|", $u), "\n";
$l = [];
foreach (["HELLO", "\xc3\x89\xc3\xa0", "hello", "A", "", "\xce\x93\xce\x95\xce\x99\xce\x86"] as $s) {
    $l[] = mb_lcfirst($s);
}
echo implode("|", $l), "\n";
--EXPECT--
Hello|Éàî|Über|A||HELLO|Straße|Γειά
hELLO|éà|hello|a||γΕΙΆ
--CLEAN--
<?php
