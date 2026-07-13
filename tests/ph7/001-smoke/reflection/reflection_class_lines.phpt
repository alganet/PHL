--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass getStartLine/getEndLine
--FILE--
<?php
class ReflLineOne {}
class ReflLineMulti
{
    public $a;

    public function b() {}
}

$r1 = new ReflectionClass('ReflLineOne');
echo $r1->getStartLine(), '-', $r1->getEndLine(), "\n";
$r2 = new ReflectionClass('ReflLineMulti');
echo $r2->getStartLine(), '-', $r2->getEndLine(), "\n";
echo basename($r2->getFileName()) === basename(__FILE__) ? 'same-file' : 'diff-file', "\n";
?>
--EXPECT--
2-2
3-8
same-file
