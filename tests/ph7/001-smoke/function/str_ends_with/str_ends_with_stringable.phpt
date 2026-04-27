--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with accepts an object with __toString()
--FILE--
<?php
class StringableEndsWith {
    public function __toString(): string { return "World"; }
}
class EmptyStringableEndsWith {
    public function __toString(): string { return ""; }
}
$obj = new StringableEndsWith();
$empty = new EmptyStringableEndsWith();
echo "h_obj="       . (str_ends_with($obj, "rld") ? 'true' : 'false') . "\n";
echo "n_obj="       . (str_ends_with("Hello World", $obj) ? 'true' : 'false') . "\n";
echo "n_empty_obj=" . (str_ends_with("abc", $empty) ? 'true' : 'false') . "\n";
echo "h_empty_obj=" . (str_ends_with($empty, "x") ? 'true' : 'false') . "\n";
echo "both_empty="  . (str_ends_with($empty, $empty) ? 'true' : 'false') . "\n";
?>
--EXPECT--
h_obj=true
n_obj=true
n_empty_obj=true
h_empty_obj=false
both_empty=true
--CLEAN--
<?php

