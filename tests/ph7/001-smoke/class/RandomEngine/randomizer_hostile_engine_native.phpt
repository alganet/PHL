--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A userland engine that rewrites the Randomizer's own arguments mid-draw
--DESCRIPTION--
Every draw a Randomizer makes may run arbitrary PHP, because the engine is an
interface -- so an engine can reach back and rewrite the very array or string
the Randomizer was handed, between one draw and the next. php never notices:
its arguments are copies. An implementation that holds a pointer into the
caller's data across a draw does notice, and what it notices is freed memory.
shuffleArray() is the one that has to work for it: it draws the whole
permutation over an INDEX vector first and touches the array only afterwards,
so there is nothing of the caller's to dangle. This test is the regression
guard for that, and it is only meaningful under a sanitizer -- what it asserts
is that all eight methods still answer, and answer the same thing php does.
--FILE--
<?php
class SmokeHostileEngine implements Random\Engine
{
    private int $i = 0;
    public function generate(): string
    {
        $GLOBALS['smokeHostileArr'][] = 999;
        unset($GLOBALS['smokeHostileArr'][2]);
        $GLOBALS['smokeHostileArr'] = array_merge($GLOBALS['smokeHostileArr'], range(100, 200));
        $GLOBALS['smokeHostileAlpha'] = str_repeat('z', 300 + $this->i);
        $GLOBALS['smokeHostileBytes'] = str_repeat('w', 40 + $this->i);
        $this->i++;
        return pack('P', 0x0102030405060708 ^ ($this->i * 7919));
    }
}
function smokeHostileFresh(): void
{
    $GLOBALS['smokeHostileArr'] = range(1, 40);
    $GLOBALS['smokeHostileAlpha'] = 'abcdefghijklmnop';
    $GLOBALS['smokeHostileBytes'] = str_repeat('q', 30);
}
foreach (['shuffleArray', 'getBytesFromString', 'pickArrayKeys', 'shuffleBytes',
          'getBytes', 'getInt', 'nextFloat', 'getFloat'] as $call) {
    smokeHostileFresh();
    $r = new Random\Randomizer(new SmokeHostileEngine());
    try {
        $v = match ($call) {
            'shuffleArray' => count($r->shuffleArray($GLOBALS['smokeHostileArr'])),
            'getBytesFromString' => strlen($r->getBytesFromString($GLOBALS['smokeHostileAlpha'], 24)),
            'pickArrayKeys' => count($r->pickArrayKeys($GLOBALS['smokeHostileArr'], 9)),
            'shuffleBytes' => strlen($r->shuffleBytes($GLOBALS['smokeHostileBytes'])),
            'getBytes' => strlen($r->getBytes(30)),
            'getInt' => $r->getInt(0, 1000) >= 0,
            'nextFloat' => $r->nextFloat() >= 0,
            'getFloat' => $r->getFloat(-5.0, 5.0, Random\IntervalBoundary::ClosedClosed) >= -5.0,
        };
        echo $call, ' ok ', var_export($v, true), "\n";
    } catch (Throwable $t) {
        echo $call, ' ', get_class($t), "\n";
    }
}
echo "survived\n";
?>
--EXPECT--
shuffleArray ok 40
getBytesFromString ok 24
pickArrayKeys ok 9
shuffleBytes ok 30
getBytes ok 30
getInt ok true
nextFloat ok true
getFloat ok true
survived
