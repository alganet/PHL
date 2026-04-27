--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with accepts an object with __toString()
--FILE--
<?php
class StringableStartsWith {
    public function __toString(): string { return "Hello"; }
}
class EmptyStringableStartsWith {
    public function __toString(): string { return ""; }
}
$obj = new StringableStartsWith();
$empty = new EmptyStringableStartsWith();
echo "h_obj="       . (str_starts_with($obj, "Hel") ? 'true' : 'false') . "\n";
echo "n_obj="       . (str_starts_with("Hello World", $obj) ? 'true' : 'false') . "\n";
echo "n_empty_obj=" . (str_starts_with("abc", $empty) ? 'true' : 'false') . "\n";
echo "h_empty_obj=" . (str_starts_with($empty, "x") ? 'true' : 'false') . "\n";
echo "both_empty="  . (str_starts_with($empty, $empty) ? 'true' : 'false') . "\n";
?>
--EXPECT--
h_obj=true
n_obj=true
n_empty_obj=true
h_empty_obj=false
both_empty=true
--CLEAN--
<?php

