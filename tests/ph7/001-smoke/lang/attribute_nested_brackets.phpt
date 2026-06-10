--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested arrays and grouped attributes are skipped as one balanced group
--FILE--
<?php
#[ListOf([1, [2, 3], ["k" => [4]]])] function attr_nested_f(){ return "nested"; }
echo attr_nested_f(), "\n";
#[First, Second(1), Third(name: "x")] function attr_grouped_f(){ return "grouped"; }
echo attr_grouped_f(), "\n";
?>
--EXPECT--
nested
grouped
--CLEAN--
<?php
?>
