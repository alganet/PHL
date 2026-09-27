--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The Random\Engine contract, its two errors and the Secure engine
--DESCRIPTION--
ext/random's whole point is that an engine is an INTERFACE: a class written in
PHP is as much an engine as a built-in one. Random\CryptoSafeEngine is the
marker a library checks when reproducible is not good enough, and only Secure
carries it. Secure has no state at all, which is why php refuses to clone it
and refuses to serialize it -- there would be nothing in the copy. Its two
errors are Errors, not Exceptions: BrokenRandomEngineError is what a caller
gets when its OWN engine misbehaves, and it extends RandomError so one catch
covers both.
--FILE--
<?php
var_dump(interface_exists('Random\Engine'), interface_exists('Random\CryptoSafeEngine'));
var_dump(class_exists('Random\Engine\Mt19937'), class_exists('Random\Engine\Secure'));

$m = new ReflectionMethod('Random\Engine', 'generate');
var_dump($m->isAbstract(), (string)$m->getReturnType(), $m->getNumberOfParameters());

/* A PHP class is an engine too. */
class SmokeConstantEngine implements Random\Engine
{
    public function generate(): string { return "\x01\x02\x03\x04"; }
}
$u = new SmokeConstantEngine();
var_dump($u instanceof Random\Engine, $u instanceof Random\CryptoSafeEngine);

$s = new Random\Engine\Secure();
var_dump($s instanceof Random\CryptoSafeEngine, $s instanceof Random\Engine);
var_dump(strlen($s->generate()));
$draws = [];
foreach (range(1, 8) as $ignored) {
    $draws[] = $s->generate();
}
var_dump(count(array_unique($draws)));

try { $bad = clone $s; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { serialize($s); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }

/* The error hierarchy. */
var_dump(is_subclass_of('Random\RandomError', 'Error'),
         is_subclass_of('Random\BrokenRandomEngineError', 'Random\RandomError'),
         is_subclass_of('Random\RandomException', 'Exception'));
try {
    throw new Random\BrokenRandomEngineError('made up');
} catch (Random\RandomError $t) {
    echo get_class($t), ' caught as Random\RandomError', "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(6) "string"
int(0)
bool(true)
bool(false)
bool(true)
bool(true)
int(8)
int(8)
Error: Trying to clone an uncloneable object of class Random\Engine\Secure
Exception: Serialization of 'Random\Engine\Secure' is not allowed
bool(true)
bool(true)
bool(true)
Random\BrokenRandomEngineError caught as Random\RandomError
