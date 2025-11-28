--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Simple exception throw/catch test
--FILE--
<?php
try {
  throw new Exception("hello");
} catch (Exception $e) {
  echo "caught: " . $e->getMessage() . "\n";
}

try {
  function foo() {
    throw new Exception("in foo");
  }
  foo();
} catch (Exception $e) {
  echo "caught: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
caught: hello
caught: in foo

--CLEAN--
<?php
unset($e);
?>
