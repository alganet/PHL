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
  function exception_throw_and_catch_func() {
    throw new Exception("in exception_throw_and_catch_func");
  }
  exception_throw_and_catch_func();
} catch (Exception $e) {
  echo "caught: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
caught: hello
caught: in exception_throw_and_catch_func
--CLEAN--
<?php

