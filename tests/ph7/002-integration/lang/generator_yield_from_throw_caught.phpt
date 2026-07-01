--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throwing sub-generator delegated via yield from is caught by the outer generator's inline try (ROOT C Face 1)
--FILE--
<?php
function sub(){ yield 1; throw new Exception("s"); }
function g(){
    try { yield from sub(); echo "unreachable\n"; }
    catch (Exception $e) { echo "caught: ", $e->getMessage(), "\n"; }
    yield 9;
}
foreach (g() as $v) { echo "y:$v\n"; }
?>
--EXPECT--
y:1
caught: s
y:9
--CLEAN--
<?php
