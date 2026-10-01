--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The two Core classes php 8.5 declares and this engine did not
--DESCRIPTION--
`DelayedTargetValidation` is php 8.5's marker for an attribute whose TARGET is
checked late. It is a final class with no members at all, and the one attribute
class php declares with NO constructor -- every other marker carries the empty
one -- so a script writes it bare and newInstance() builds it with nothing.

`ClosedGeneratorException` is declared beside Generator and thrown from nowhere
a script can reach: php's own paths answer null for a resume of a closed
generator rather than raising it. A program may still name it, catch it and
throw it, which is the whole of what declaring it buys -- and what a
`Class "ClosedGeneratorException" not found` fatal cost before.

With these two, the class inventory owes nothing: the last names of the
`get_declared_classes()` diff against php 8.5.9 are declared.
--FILE--
<?php
$sqBshow = function ($label, $fn) {
    try { $out = json_encode($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo str_pad($label, 40), ' => ', $out, "\n";
};

/* php 8.5's marker for an attribute whose target is validated late: a final
   class with no members at all, and the one attribute class php declares with
   no constructor. */
$sqBshow('it exists', fn () => class_exists('DelayedTargetValidation'));
$sqBshow('its shape', function () {
    $r = new ReflectionClass('DelayedTargetValidation');
    return [$r->isFinal(), $r->isInternal(), $r->getExtensionName(),
            count($r->getMethods()), count($r->getConstants()), count($r->getProperties())]; });
$sqBshow('its own attribute', function () {
    $a = (new ReflectionClass('DelayedTargetValidation'))->getAttributes();
    return [count($a), $a[0]->getName(), $a[0]->getArguments()]; });
$sqBshow('a script may write it', function () {
    eval('#[DelayedTargetValidation] function sqB_marked() {}');
    $a = (new ReflectionFunction('sqB_marked'))->getAttributes();
    return [$a[0]->getName(), get_class($a[0]->newInstance())]; });

/* and the exception php declares beside Generator */
$sqBshow('the exception exists', fn () => class_exists('ClosedGeneratorException'));
$sqBshow('its shape', function () {
    $r = new ReflectionClass('ClosedGeneratorException');
    return [$r->getParentClass()->getName(), $r->getInterfaceNames(), $r->isFinal(),
            $r->isInternal(), $r->getExtensionName()]; });
$sqBshow('a script may throw it', function () {
    try { throw new ClosedGeneratorException('done', 3); }
    catch (Exception $e) { return [get_class($e), $e->getMessage(), $e->getCode()]; } });
$sqBshow('a closed generator answers null', function () {
    $g = (function () { yield 1; })();
    foreach ($g as $v) { }
    return [$g->send(5), $g->valid()]; });
--EXPECT--
it exists                                => true
its shape                                => [true,true,"Core",0,0,0]
its own attribute                        => [1,"Attribute",[127]]
a script may write it                    => ["DelayedTargetValidation","DelayedTargetValidation"]
the exception exists                     => true
its shape                                => ["Exception",["Throwable","Stringable"],false,true,"Core"]
a script may throw it                    => ["ClosedGeneratorException","done",3]
a closed generator answers null          => [null,false]
