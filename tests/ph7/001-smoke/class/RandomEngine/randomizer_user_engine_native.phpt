--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Randomizer over a userland engine, including the ways one can be broken
--DESCRIPTION--
The Randomizer cannot tell a built-in engine from a class written in PHP, and
everything about the second kind is observable. Its string's LENGTH is the
engine's width, so a one-byte engine has to be called four times to fill a
32-bit range and nine bytes are truncated back to eight. An EMPTY string is not
a width at all and php refuses it outright. And an engine that keeps answering
the same bits makes every rejection fail: php counts them and gives up after
fifty, which is the only way a caller ever finds out its own engine is broken.
A throw from inside generate() is the engine's, not the Randomizer's, and
reaches the caller unchanged.
--FILE--
<?php
class SmokeWidthEngine implements Random\Engine
{
    public int $calls = 0;
    public function __construct(private int $size) {}
    public function generate(): string
    {
        $this->calls++;
        return substr(pack('P', 0x0102030405060708), 0, $this->size);
    }
}
foreach ([1, 2, 3, 4, 8, 9] as $size) {
    $e = new SmokeWidthEngine($size);
    $r = new Random\Randomizer($e);
    $n = $r->nextInt();
    $c1 = $e->calls;
    $r->getInt(0, 0xFFFFFFFF);
    printf("size %d: nextInt=%d in %d call(s), getInt32 in %d\n",
        $size, $n, $c1, $e->calls - $c1);
}

class SmokeBoomEngine implements Random\Engine
{
    public function generate(): string { throw new LogicException('boom'); }
}
class SmokeEmptyEngine implements Random\Engine
{
    public function generate(): string { return ''; }
}
class SmokeOnesEngine implements Random\Engine
{
    public function generate(): string { return "\xff\xff\xff\xff\xff\xff\xff\xff"; }
}
foreach ([SmokeBoomEngine::class, SmokeEmptyEngine::class, SmokeOnesEngine::class] as $cls) {
    foreach (['nextInt', 'getInt', 'getBytes'] as $call) {
        $r = new Random\Randomizer(new $cls());
        try {
            $v = match ($call) {
                'nextInt' => $r->nextInt(),
                'getInt' => $r->getInt(0, 10),
                'getBytes' => bin2hex($r->getBytes(4)),
            };
            echo $cls, '::', $call, ' ok ', var_export($v, true), "\n";
        } catch (Throwable $t) {
            echo $cls, '::', $call, ' ', get_class($t), ': ', $t->getMessage(), "\n";
        }
    }
}

/* The engine's throw is catchable where it was raised, and nothing after it is
   swallowed. */
$r = new Random\Randomizer(new SmokeBoomEngine());
try { $r->getInt(0, 5); } catch (LogicException $e) { echo 'caught ', $e->getMessage(), "\n"; }
echo "still running\n";
?>
--EXPECT--
size 1: nextInt=4 in 1 call(s), getInt32 in 4
size 2: nextInt=900 in 1 call(s), getInt32 in 2
size 3: nextInt=197508 in 1 call(s), getInt32 in 2
size 4: nextInt=42140548 in 1 call(s), getInt32 in 1
size 8: nextInt=36311929895191428 in 1 call(s), getInt32 in 1
size 9: nextInt=36311929895191428 in 1 call(s), getInt32 in 1
SmokeBoomEngine::nextInt LogicException: boom
SmokeBoomEngine::getInt LogicException: boom
SmokeBoomEngine::getBytes LogicException: boom
SmokeEmptyEngine::nextInt Random\BrokenRandomEngineError: A random engine must return a non-empty string
SmokeEmptyEngine::getInt Random\BrokenRandomEngineError: A random engine must return a non-empty string
SmokeEmptyEngine::getBytes Random\BrokenRandomEngineError: A random engine must return a non-empty string
SmokeOnesEngine::nextInt ok 9223372036854775807
SmokeOnesEngine::getInt Random\BrokenRandomEngineError: Failed to generate an acceptable random number in 50 attempts
SmokeOnesEngine::getBytes ok 'ffffffff'
caught boom
still running
