--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: unknown parameter name throws Error
--FILE--
<?php
function naeupf($a) { echo "a=$a\n"; }
naeupf(b: 1);
?>
--EXPECTF--
%APHP Fatal error:  Uncaught Error: Unknown named parameter $b in %s:3%A
--CLEAN--
<?php
