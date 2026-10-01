--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An ini value's bitwise operators share one precedence and associate left
--DESCRIPTION--
C reads `1 | 2 & 4` as `1 | (2 & 4)`, which is 1. The php.ini grammar gives its
three binary operators ONE precedence and associates them to the left, so the
same text is `(1 | 2) & 4`, which is 0. The stored value settles it: a 1 would
leave E_ERROR reportable, and 0 reports nothing at all.
--INI--
error_reporting=1 | 2 & 4
display_errors=1
log_errors=0
--FILE--
<?php
var_dump(error_reporting());
var_dump(ini_get('error_reporting'));
$a = [];
$x = $a['gone'];                        // E_WARNING: not in an empty mask
trigger_error('quiet', E_USER_WARNING);
echo "END\n";
?>
--EXPECT--
int(0)
string(1) "0"
END
