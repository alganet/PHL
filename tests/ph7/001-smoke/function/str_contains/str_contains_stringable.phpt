--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains accepts an object with __toString()
--FILE--
<?php
class StringableContains {
    public function __toString(): string { return "Hello World"; }
}
class EmptyStringableContains {
    public function __toString(): string { return ""; }
}
$obj = new StringableContains();
$empty = new EmptyStringableContains();
echo "h_obj="        . (str_contains($obj, "World") ? 'true' : 'false') . "\n";
echo "n_obj="        . (str_contains("Greetings, Hello World", $obj) ? 'true' : 'false') . "\n";
echo "n_empty_obj="  . (str_contains("abc", $empty) ? 'true' : 'false') . "\n";
echo "h_empty_obj="  . (str_contains($empty, "x") ? 'true' : 'false') . "\n";
echo "both_empty="   . (str_contains($empty, $empty) ? 'true' : 'false') . "\n";
?>
--EXPECT--
h_obj=true
n_obj=true
n_empty_obj=true
h_empty_obj=false
both_empty=true
--CLEAN--
<?php

