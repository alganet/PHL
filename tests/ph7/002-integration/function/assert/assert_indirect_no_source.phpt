--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
indirect assert() calls have no compile-time source: empty AssertionError message
--INI--
zend.assertions=1
--FILE--
<?php
$f = 'assert';
try { $f(1 == 2); } catch (AssertionError $e) { var_dump($e->getMessage()); }
try { call_user_func('assert', false); } catch (AssertionError $e) { var_dump($e->getMessage()); }
try { assert(1 == 2, 'my description'); } catch (AssertionError $e) { var_dump($e->getMessage()); }
?>
--EXPECT--
string(0) ""
string(0) ""
string(14) "my description"
--CLEAN--
<?php
