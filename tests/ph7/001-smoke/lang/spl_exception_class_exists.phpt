--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SPL exception classes are declared (class_exists)
--FILE--
<?php
$classes = ['LogicException','RuntimeException','BadFunctionCallException',
  'BadMethodCallException','DomainException','InvalidArgumentException',
  'LengthException','OutOfRangeException','OutOfBoundsException',
  'OverflowException','RangeException','UnderflowException','UnexpectedValueException'];
foreach ($classes as $c) {
  echo $c, '=', class_exists($c) ? '1' : '0', "\n";
}
?>
--EXPECT--
LogicException=1
RuntimeException=1
BadFunctionCallException=1
BadMethodCallException=1
DomainException=1
InvalidArgumentException=1
LengthException=1
OutOfRangeException=1
OutOfBoundsException=1
OverflowException=1
RangeException=1
UnderflowException=1
UnexpectedValueException=1
--CLEAN--
<?php
unset($classes);
