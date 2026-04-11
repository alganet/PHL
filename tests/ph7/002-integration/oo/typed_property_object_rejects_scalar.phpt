--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: object type rejects non-object values
--FILE--
<?php
class TpoHolder { public object $o; }
$h = new TpoHolder();
try { $h->o = "str"; } catch (TypeError $e) { echo "s: ", $e->getMessage(), "\n"; }
try { $h->o = 42; } catch (TypeError $e) { echo "i: ", $e->getMessage(), "\n"; }
try { $h->o = [1]; } catch (TypeError $e) { echo "a: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
s: Cannot assign string to property TpoHolder::$o of type object
i: Cannot assign int to property TpoHolder::$o of type object
a: Cannot assign array to property TpoHolder::$o of type object
--CLEAN--
<?php
unset($h);
