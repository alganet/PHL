--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
0xG names the identifier "xG": php reads a base prefix only after a lone 0 followed by a valid digit (was a bare skip; PHL consumed the prefix regardless and named just "G")

--FILE--
<?php
$a = 0xG;
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "xG"%A
--CLEAN--
<?php
unset($a);
