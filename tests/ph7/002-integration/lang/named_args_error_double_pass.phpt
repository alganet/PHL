--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: same parameter passed positionally and by name throws Error
--FILE--
<?php
function naedpf($a, $b) { echo "$a $b\n"; }
naedpf(1, a: 2);
?>
--EXPECTF--
%s Fatal error:  Uncaught Error: Named parameter $a overwrites previous argument%A
--CLEAN--
<?php
