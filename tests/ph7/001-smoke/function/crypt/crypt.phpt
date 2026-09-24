--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
crypt() answers all six schemes: DES, ext-DES, MD5, bcrypt (four minors), SHA-256, SHA-512
--DESCRIPTION--
crypt(3) was a loud undefined function while password_hash() carried its
bcrypt core — so the hash in any pre-bcrypt /etc/shadow, .htpasswd or legacy
user table could be neither made nor checked. The vectors here are fixed
salts, so every engine must answer byte-identically; the bcrypt minors
include "$2x$", whose historical sign-extension bug only shows on a password
with a byte >= 0x80 — exactly the case tested.
--FILE--
<?php
echo crypt('password', 'ab'), "\n";
echo crypt('', 'ab'), "\n";
echo crypt('password', '_1111santiago'), "\n";
echo crypt('hello', '$1$abcdefgh$'), "\n";
echo crypt('password', '$1$verylongsaltvalue$'), "\n";
echo crypt('password', '$5$mysalt$'), "\n";
echo crypt('password', '$5$rounds=1000$mysalt'), "\n";
echo crypt('password', '$6$mysalt$'), "\n";
echo crypt('p', '$6$rounds=1001$xy$'), "\n";
echo crypt('rasmuslerdorf', '$2y$11$6DP.V0nO7YI3iSki4qog6O'), "\n";
$s = '$04$0123456789012345678901';
echo crypt("t\xffst", '$2a' . $s), "\n";
echo crypt("t\xffst", '$2b' . $s), "\n";
echo crypt("t\xffst", '$2x' . $s), "\n";
echo crypt("t\xffst", '$2y' . $s), "\n";
?>
--EXPECT--
abJnggxhB/yWI
abmF1QH4PEr.E
_1111santLeIWOtEeqQ.
$1$abcdefgh$rwnEbRiN0agqVgZBovWNQ/
$1$verylong$x9zZeM.WefbkkW2RgqhN60
$5$mysalt$gOX8uTWm.jCh4cQe.uLtE.hxGjMFVqremjAGFLOJBd5
$5$rounds=1000$mysalt$VMmt3ylGWUOctj33pKkig9KAwkq.tSmLL1Yx1/UK4eC
$6$mysalt$NN1QGsmCO0hcvplH4ahY6ocho6F6TgcY8yNdMFAeO.LAeFodNPGA6KsQM5Or1AKbE4QKSqnEsC/SE0Zz3ts9y1
$6$rounds=1001$xy$xnRli.UqxZJblbGNi3GOlj.qqhGthvPZsdV4bgOqaCx7CRSOGoRlHLjzYD0Rm5Acl1Z7Go54LkbJ37jcZoySQ.
$2y$11$6DP.V0nO7YI3iSki4qog6OQI5eiO6Jnjsqg7vdnb.JgGIsxniOn4C
$2a$04$012345678901234567890u3SDu9heKXupic2B4qhVyuWfABjV9SOS
$2b$04$012345678901234567890u3SDu9heKXupic2B4qhVyuWfABjV9SOS
$2x$04$012345678901234567890uIFHTofnopPZKYkOfK6Q1l5vKIQOSO3S
$2y$04$012345678901234567890u3SDu9heKXupic2B4qhVyuWfABjV9SOS
--CLEAN--
<?php
