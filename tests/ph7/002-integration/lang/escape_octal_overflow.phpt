--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Octal escape above \377 warns at compile time and wraps to the low byte
--FILE--
<?php
$s = "\401";
echo bin2hex($s), "\n";
echo "done\n";
?>
--EXPECTF--
%AWarning:%AOctal escape sequence overflow \401 is greater than \377 in %s on line %d
01
done
--CLEAN--
<?php
unset($s);
