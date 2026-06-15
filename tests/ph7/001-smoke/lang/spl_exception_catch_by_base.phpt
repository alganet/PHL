--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SPL exceptions: caught by a base type, inherited ctor/getMessage/getCode
--FILE--
<?php
try {
  throw new InvalidArgumentException("bad", 3);
} catch (LogicException $e) {
  echo "caught LogicException ", get_class($e), ":", $e->getMessage(), ":", $e->getCode(), "\n";
}
try {
  throw new RangeException("r");
} catch (Exception $e) {
  echo "caught Exception ", get_class($e), "\n";
}
try {
  throw new OverflowException("o");
} catch (Throwable $e) {
  echo "caught Throwable ", get_class($e), "\n";
}
?>
--EXPECT--
caught LogicException InvalidArgumentException:bad:3
caught Exception RangeException
caught Throwable OverflowException
--CLEAN--
<?php
