--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
uniqid() is the clock, not a random number
--FILE--
<?php
/* php's uniqid() is the CLOCK, not a random number: `%08x%05x` of the epoch
 * seconds and the microseconds inside them. Nothing here pins a VALUE -- the
 * clock moves -- so every row is a shape, a length or a relation. */
echo "-- the plain form is thirteen hex digits of the clock\n";
$id = uniqid();
printf("len=%d hex=%d\n", strlen($id), (int) ctype_xdigit($id));
printf("seconds field is now=%d microseconds field fits five digits=%d\n",
    (int) (abs(hexdec(substr($id, 0, 8)) - time()) <= 2),
    (int) (hexdec(substr($id, 8)) < 0x100000));

echo "-- a prefix is prepended verbatim, and nothing else changes\n";
foreach (['', 'pfx_', 'x', str_repeat('z', 40)] as $prefix) {
    $one = uniqid($prefix);
    printf("prefix=%-2d len=%d starts=%d tail_hex=%d\n", strlen($prefix), strlen($one),
        (int) str_starts_with($one, $prefix), (int) ctype_xdigit(substr($one, strlen($prefix))));
}

echo "-- \$more_entropy appends php's combined LCG times ten, `%.8F`\n";
foreach ([['', 23], ['pfx_', 27]] as [$prefix, $expected]) {
    $one = uniqid($prefix, true);
    printf("len=%d expected=%d shape=%s\n", strlen($one), $expected,
        preg_match('/^' . preg_quote($prefix, '/') . '[0-9a-f]{13}\d\.\d{8}$/', $one) ? 'ok' : 'BAD');
}

echo "-- the plain form only ever INCREASES: it is a clock reading\n";
/* php sleeps a microsecond before reading it when $more_entropy is false, so on
 * POSIX no two ids are equal either -- but its own Windows build does not, and
 * neither does this, so the row that holds on every platform is the ORDER. */
$ids = [];
for ($i = 0; $i < 300; $i++) { $ids[] = uniqid(); }
$sorted = $ids;
sort($sorted);
printf("ordered=%d\n", (int) ($ids === $sorted));
$ents = [];
for ($i = 0; $i < 300; $i++) { $ents[] = uniqid('', true); }
printf("entropy distinct=%d of %d\n", count(array_unique($ents)), count($ents));

echo "-- an id is not ALL DIGITS: it carries the epoch, which is not\n";
$digits = 0;
foreach ($ids as $one) { $digits += (int) ctype_digit($one); }
printf("all-digit ids=%d\n", $digits);

echo "-- and it is a plain string, whatever the argument shapes\n";
var_dump(is_string(uniqid()), is_string(uniqid('a', false)), uniqid('z') !== uniqid('y'));
--EXPECT--
-- the plain form is thirteen hex digits of the clock
len=13 hex=1
seconds field is now=1 microseconds field fits five digits=1
-- a prefix is prepended verbatim, and nothing else changes
prefix=0  len=13 starts=1 tail_hex=1
prefix=4  len=17 starts=1 tail_hex=1
prefix=1  len=14 starts=1 tail_hex=1
prefix=40 len=53 starts=1 tail_hex=1
-- $more_entropy appends php's combined LCG times ten, `%.8F`
len=23 expected=23 shape=ok
len=27 expected=27 shape=ok
-- the plain form only ever INCREASES: it is a clock reading
ordered=1
entropy distinct=300 of 300
-- an id is not ALL DIGITS: it carries the epoch, which is not
all-digit ids=0
-- and it is a plain string, whatever the argument shapes
bool(true)
bool(true)
bool(true)
