--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RecursiveArrayIterator descends into an object entry's properties
--DESCRIPTION--
The object half of RecursiveArrayIterator's recursion, split out of
spl_recursive_native.phpt because php 8.5 DEPRECATES the construction it rests
on: getChildren() over an object entry builds the child with that object as its
backing array, and php answers `Using an object as a backing array for
ArrayIterator is deprecated, as it allows violating class constraints and
invariants`. The notice is invisible from php's CLI and loud under test-compat
(§4 rule 23), so the probe cannot share a file with oracle-run expectations.
PHL emits nothing there yet; what it SHOULD do is a policy question still open
— the 19 Jul non-deprecated-compatibility policy points at refusing
the construction outright, which is a change to ArrayIterator, not to the
recursion this file probes.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip php 8.5 deprecates an object backing array; the notice is a §7.3 policy item";
}
--FILE--
<?php
class SrocPlain { public $a = 1; public $b = 2; }

$r = new RecursiveArrayIterator([new SrocPlain]);
$r->rewind();
echo 'children of an object => ', str_replace("\n", '', var_export(iterator_to_array($r->getChildren()), true)), "\n";
echo 'child class => ', get_class($r->getChildren()), "\n";
echo 'hasChildren => ', var_export($r->hasChildren(), true), "\n";
--EXPECT--
children of an object => array (  'a' => 1,  'b' => 2,)
child class => RecursiveArrayIterator
hasChildren => true
