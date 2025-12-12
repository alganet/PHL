--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
final class compile
--FILE--
<?php
final class F { public function v(){ return 1; } }
echo class_exists('F') ? "true" : "false";
?>
--EXPECT--
true
