--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SPL's recursive return types are each stub's own, not one rule
--DESCRIPTION--
The last five rows of the return-type sweep are SPL methods this engine had
made CONSISTENT where php's stubs are not. Six classes ship a getChildren()
that builds its child exactly the same way, and php types three of them
non-nullable and the rest `?T` — so a declaration is the stub's own text, not a
rule to be derived from the behaviour. SplFileObject is the other end of it:
a file has no children, and php types the two methods that say so with the
CONSTANT (`false` and `null`) rather than with `bool` and `?RecursiveIterator`.
Its `current()` union is php's order too, `array|string|false`, which this
engine had written the other way round.
--FILE--
<?php
function srtRow($class, $method) {
    $m = new ReflectionMethod($class, $method);
    $tent = $m->hasTentativeReturnType() ? (string)$m->getTentativeReturnType() : '-';
    $real = $m->hasReturnType() ? (string)$m->getReturnType() : '-';
    printf("%-46s real=%-6s tentative=%s\n", "$class::$method", $real, $tent);
}
/* Every getChildren php ships, in one table: three are NOT nullable and the
 * rest are, and all of them build their child the same way. */
foreach (['RecursiveIterator', 'RecursiveArrayIterator', 'RecursiveFilterIterator',
          'ParentIterator', 'RecursiveCallbackFilterIterator', 'RecursiveRegexIterator',
          'RecursiveCachingIterator', 'RecursiveDirectoryIterator', 'SplFileObject'] as $c) {
    srtRow($c, 'getChildren');
}
/* php types the two SplFileObject answers CONSTANTLY with the constant. */
srtRow('SplFileObject', 'hasChildren');
srtRow('RecursiveDirectoryIterator', 'hasChildren');
/* And a union prints in the stub's order. */
srtRow('SplFileObject', 'current');

$srtFile = new SplFileObject(__FILE__);
echo var_export($srtFile->hasChildren(), true), " ",
     var_export($srtFile->getChildren(), true), "\n";
unset($srtFile);   /* release the handle: the runner deletes this file next */
--EXPECT--
RecursiveIterator::getChildren                 real=-      tentative=?RecursiveIterator
RecursiveArrayIterator::getChildren            real=-      tentative=?RecursiveArrayIterator
RecursiveFilterIterator::getChildren           real=-      tentative=?RecursiveFilterIterator
ParentIterator::getChildren                    real=-      tentative=?RecursiveFilterIterator
RecursiveCallbackFilterIterator::getChildren   real=-      tentative=RecursiveCallbackFilterIterator
RecursiveRegexIterator::getChildren            real=-      tentative=RecursiveRegexIterator
RecursiveCachingIterator::getChildren          real=-      tentative=?RecursiveCachingIterator
RecursiveDirectoryIterator::getChildren        real=-      tentative=RecursiveDirectoryIterator
SplFileObject::getChildren                     real=-      tentative=null
SplFileObject::hasChildren                     real=-      tentative=false
RecursiveDirectoryIterator::hasChildren        real=-      tentative=bool
SplFileObject::current                         real=-      tentative=array|string|false
false NULL
