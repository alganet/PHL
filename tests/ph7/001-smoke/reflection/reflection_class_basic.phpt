--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass basics: name, flags, modifiers
--FILE--
<?php
final class ReflBasicFinal {}
abstract class ReflBasicAbstract {}
interface ReflBasicIface {}
trait ReflBasicTrait {}
class ReflBasicPlain {}

$rc = new ReflectionClass('ReflBasicFinal');
echo $rc->getName(), "\n";
echo $rc->getShortName(), "\n";
echo $rc->getNamespaceName() === '' ? 'global' : 'namespaced', "\n";
echo $rc->inNamespace() ? 'in-ns' : 'not-in-ns', "\n";
echo $rc->isFinal() ? 'final' : 'not-final', "\n";
echo $rc->isAbstract() ? 'abstract' : 'not-abstract', "\n";
echo $rc->isInterface() ? 'iface' : 'not-iface', "\n";
echo $rc->isTrait() ? 'trait' : 'not-trait', "\n";
echo $rc->isAnonymous() ? 'anon' : 'not-anon', "\n";
echo $rc->isEnum() ? 'enum' : 'not-enum', "\n";
echo $rc->getModifiers(), "\n";
echo implode(',', Reflection::getModifierNames($rc->getModifiers())), "\n";

$ra = new ReflectionClass('ReflBasicAbstract');
echo $ra->isAbstract() ? 'abstract' : 'not-abstract', "\n";
echo $ra->getModifiers(), "\n";
echo $ra->isInstantiable() ? 'inst' : 'not-inst', "\n";
echo $ra->isCloneable() ? 'clone' : 'not-clone', "\n";

$ri = new ReflectionClass('ReflBasicIface');
echo $ri->isInterface() ? 'iface' : 'not-iface', "\n";
echo $ri->isInstantiable() ? 'inst' : 'not-inst', "\n";

$rt = new ReflectionClass('ReflBasicTrait');
echo $rt->isTrait() ? 'trait' : 'not-trait', "\n";

$rp = new ReflectionClass('ReflBasicPlain');
echo $rp->getModifiers(), "\n";
echo $rp->isInstantiable() ? 'inst' : 'not-inst', "\n";
echo $rp->isCloneable() ? 'clone' : 'not-clone', "\n";
?>
--EXPECT--
ReflBasicFinal
ReflBasicFinal
global
not-in-ns
final
not-abstract
not-iface
not-trait
not-anon
not-enum
32
final
abstract
64
not-inst
not-clone
iface
not-inst
trait
0
inst
clone
