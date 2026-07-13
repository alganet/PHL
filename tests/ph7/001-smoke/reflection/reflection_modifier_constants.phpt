--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection modifier constants match PHP (oracle self-check)
--FILE--
<?php
echo 'IS_IMPLICIT_ABSTRACT=', ReflectionClass::IS_IMPLICIT_ABSTRACT, "\n";
echo 'IS_EXPLICIT_ABSTRACT=', ReflectionClass::IS_EXPLICIT_ABSTRACT, "\n";
echo 'IS_FINAL=', ReflectionClass::IS_FINAL, "\n";
echo 'IS_READONLY=', ReflectionClass::IS_READONLY, "\n";
echo 'SKIP_INITIALIZATION_ON_SERIALIZE=', ReflectionClass::SKIP_INITIALIZATION_ON_SERIALIZE, "\n";
echo 'SKIP_DESTRUCTOR=', ReflectionClass::SKIP_DESTRUCTOR, "\n";
echo implode(',', Reflection::getModifierNames(1 | 16)), "\n";
echo implode(',', Reflection::getModifierNames(4 | 32)), "\n";
echo implode(',', Reflection::getModifierNames(2 | 64)), "\n";
echo implode(',', Reflection::getModifierNames(0)) === '' ? 'empty' : 'nonempty', "\n";
?>
--EXPECT--
IS_IMPLICIT_ABSTRACT=16
IS_EXPLICIT_ABSTRACT=64
IS_FINAL=32
IS_READONLY=65536
SKIP_INITIALIZATION_ON_SERIALIZE=8
SKIP_DESTRUCTOR=16
public,static
final,private
abstract,protected
empty
