--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type error message order: classes (declaration order), then object, array, string, int, float, bool, null
--FILE--
<?php
class UcoFoo {}
class UcoBar {}

// Strip the ", called in ... on line N" tail that PHP 8+ appends to user-land
// TypeErrors, so the expected-output pinning works on both engines.
function uco_trim($m) {
    $i = strpos($m, ", called in ");
    return $i === false ? $m : substr($m, 0, $i);
}

function uco_f(int|object|array|string $x) {}
try { uco_f(null); } catch (TypeError $e) { echo uco_trim($e->getMessage()), "\n"; }

function uco_g(bool|float|int|string|array|object $x) {}
try { uco_g(null); } catch (TypeError $e) { echo uco_trim($e->getMessage()), "\n"; }

function uco_h(UcoFoo|int|UcoBar|array $x) {}
try { uco_h(null); } catch (TypeError $e) { echo uco_trim($e->getMessage()), "\n"; }

function uco_i(int|null|float $x) {}
try { uco_i(new UcoFoo()); } catch (TypeError $e) { echo uco_trim($e->getMessage()), "\n"; }
?>
--EXPECT--
uco_f(): Argument #1 ($x) must be of type object|array|string|int, null given
uco_g(): Argument #1 ($x) must be of type object|array|string|int|float|bool, null given
uco_h(): Argument #1 ($x) must be of type UcoFoo|UcoBar|array|int, null given
uco_i(): Argument #1 ($x) must be of type int|float|null, UcoFoo given
--CLEAN--
<?php
