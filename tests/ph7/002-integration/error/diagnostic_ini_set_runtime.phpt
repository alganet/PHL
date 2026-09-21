--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ini_set('display_errors', ...) at runtime re-gates the diagnostic streams
--DESCRIPTION--
Toggling display_errors mid-script takes effect on the very next diagnostic: the
first warning (display off) reaches only stderr, then after ini_set('display_
errors','1') the second warning also prints its display copy to stdout. The log
copy stays on stderr for both.
--INI--
display_errors=0
log_errors=1
--FILE--
<?php
echo "OUT\n";
$a = [];
$x1 = $a["p"];
ini_set("display_errors", "1");
$x2 = $a["q"];
echo "END\n";
?>
--EXPECTF--
OUT

Warning: Undefined array key "q" in %s on line %d
END
--EXPECT_STDERR--
PHP Warning:  Undefined array key "p" in %s on line %d
PHP Warning:  Undefined array key "q" in %s on line %d
