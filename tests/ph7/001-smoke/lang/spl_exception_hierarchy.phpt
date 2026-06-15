--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SPL exception parent chain matches PHP (get_parent_class)
--FILE--
<?php
$classes = ['LogicException','RuntimeException','BadFunctionCallException',
  'BadMethodCallException','DomainException','InvalidArgumentException',
  'LengthException','OutOfRangeException','OutOfBoundsException',
  'OverflowException','RangeException','UnderflowException','UnexpectedValueException'];
foreach ($classes as $c) {
  echo $c, '->', get_parent_class($c), "\n";
}
?>
--EXPECT--
LogicException->Exception
RuntimeException->Exception
BadFunctionCallException->LogicException
BadMethodCallException->BadFunctionCallException
DomainException->LogicException
InvalidArgumentException->LogicException
LengthException->LogicException
OutOfRangeException->LogicException
OutOfBoundsException->RuntimeException
OverflowException->RuntimeException
RangeException->RuntimeException
UnderflowException->RuntimeException
UnexpectedValueException->RuntimeException
--CLEAN--
<?php
unset($classes);
