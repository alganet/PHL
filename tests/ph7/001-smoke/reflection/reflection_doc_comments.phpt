--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getDocComment on classes, members, functions, closures
--FILE--
<?php
/** Class doc.
 * @author x
 */
class ReflDocCls {
    /** prop doc */
    public $p = 1;
    /** const doc */
    const K = 2;
    /**
     * method doc
     */
    public function m() {}
    public function nodoc() {}
}
/** fn doc */
function reflDocFn() {}
/* not a doc */
function reflDocPlain() {}
/** orphan */
$reflDocX = 1;
function reflDocAfter() {}
/** closure doc */
$reflDocCl = function () {};

echo (new ReflectionClass('ReflDocCls'))->getDocComment(), "\n";
echo (new ReflectionProperty('ReflDocCls', 'p'))->getDocComment(), "\n";
echo (new ReflectionClassConstant('ReflDocCls', 'K'))->getDocComment(), "\n";
echo (new ReflectionMethod('ReflDocCls', 'm'))->getDocComment(), "\n";
echo (new ReflectionMethod('ReflDocCls', 'nodoc'))->getDocComment() === false ? 'false' : 'doc', "\n";
echo (new ReflectionFunction('reflDocFn'))->getDocComment(), "\n";
echo (new ReflectionFunction('reflDocPlain'))->getDocComment() === false ? 'false' : 'doc', "\n";
echo (new ReflectionFunction('reflDocAfter'))->getDocComment() === false ? 'false' : 'doc', "\n";
echo (new ReflectionFunction($reflDocCl))->getDocComment(), "\n";
echo (new ReflectionClass('Exception'))->getDocComment() === false ? 'false' : 'doc', "\n";
--EXPECT--
/** Class doc.
 * @author x
 */
/** prop doc */
/** const doc */
/**
     * method doc
     */
false
/** fn doc */
false
doc
/** closure doc */
false
