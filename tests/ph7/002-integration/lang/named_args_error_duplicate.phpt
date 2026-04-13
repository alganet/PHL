--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: duplicate named argument throws Error
--FILE--
<?php
function naedf($a, $b) { echo "$a $b\n"; }
naedf(a: 1, a: 2);
?>
--EXPECTF--
%s Fatal error:  Uncaught Error: Named parameter $a overwrites previous argument%A
--CLEAN--
<?php
