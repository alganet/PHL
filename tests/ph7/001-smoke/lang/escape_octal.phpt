--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Octal escape sequence with \o prefix
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// \o followed by octal digits produces the character
$bell = "\o007";
if (ord($bell) === 7) {
    echo "Bell char ok\n";
}

$tab = "\o011";
if (ord($tab) === 9) {
    echo "Tab char ok\n";
}

// \o101 = octal 101 = decimal 65 = 'A'
$a = "\o101";
if ($a === "A") {
    echo "A char ok\n";
}

// \o not followed by valid octal just outputs 'o'
$literal = "\o";
if ($literal === "o") {
    echo "Literal o ok\n";
}

// \o followed by non-octal digit outputs 'o' and the rest
$literal2 = "\o9";
if ($literal2 === "o9") {
    echo "Literal o9 ok\n";
}
?>
--EXPECT--
Bell char ok
Tab char ok
A char ok
Literal o ok
Literal o9 ok
--CLEAN--
<?php
unset($bell, $tab, $a, $literal, $literal2);
