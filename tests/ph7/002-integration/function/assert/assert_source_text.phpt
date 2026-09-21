--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert() failure message renders the assertion's source (php's AST-export shape)
--INI--
zend.assertions=1
--FILE--
<?php
$x = 0;
try { assert(1==2); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(1   ==   2); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert($x ?? 1 == 2); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert("abc" == "x"); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert('it\'s' == "x"); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(!(1 < 2)); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert((1==2)); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(count([]) > 0); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(in_array(9, [1, 2])); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(array(1,2) == []); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(0x10 == 17); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(1_000 == 7); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(1e3 == 0.5); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(true and false); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(false ?: false); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(-1.5 + 1.5); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(PHP_INT_MAX < 0); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { assert(assertion: 2 == 3); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
try { \assert(3 == 4); } catch (AssertionError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
assert(1 == 2)
assert(1 == 2)
assert($x ?? 1 == 2)
assert('abc' == 'x')
assert('it\'s' == 'x')
assert(!(1 < 2))
assert(1 == 2)
assert(count([]) > 0)
assert(in_array(9, [1, 2]))
assert([1, 2] == [])
assert(16 == 17)
assert(1000 == 7)
assert(1000.0 == 0.5)
assert(true && false)
assert(false ?: false)
assert(-1.5 + 1.5)
assert(PHP_INT_MAX < 0)
assert(assertion: 2 == 3)
assert(3 == 4)
--CLEAN--
<?php
