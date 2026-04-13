--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: positional argument after named argument is a compile error
--FILE--
<?php
function naepaf($a, $b) { echo "$a $b\n"; }
naepaf(a: 1, 2);
?>
--EXPECTF--
%s Fatal error:  Cannot use positional argument after named argument%A
--CLEAN--
<?php
