--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An error_reporting ini value is a number, not an on/off gate
--DESCRIPTION--
A numeric directive names the LEVEL. `error_reporting=2` is E_WARNING alone: the
warning prints and the notice-level diagnostics beside it do not, and
error_reporting() answers 2 rather than the E_ALL the engine started with.
--INI--
error_reporting=2
display_errors=1
log_errors=0
--FILE--
<?php
var_dump(error_reporting());
$a = [];
$x = $a['gone'];            // E_WARNING: in the mask, prints
$y = 1 + 1;
echo "END\n";
?>
--EXPECTF--
int(2)

Warning: Undefined array key "gone" in %s on line %d
END
