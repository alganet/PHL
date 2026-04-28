--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int throws ArgumentCountError when called with wrong arity
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
try { random_int(); echo "n0\n"; } catch (ArgumentCountError $e) { echo "ace0\n"; }
try { random_int(1); echo "n1\n"; } catch (ArgumentCountError $e) { echo "ace1\n"; }
try { random_int(1, 2, 3); echo "n3\n"; } catch (ArgumentCountError $e) { echo "ace3\n"; }
?>
--EXPECT--
ace0
ace1
ace3
--CLEAN--
<?php
