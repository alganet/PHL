--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
setMicrosecond() refuses what is not a fraction of a second
--FILE--
<?php
/* php bounds the microseconds a date can carry, which is what lets every other
 * door assume they are a fraction of ONE second. PHL stored whatever int it was
 * handed, so `setMicrosecond(1000000)` formatted as `00:00:00.1000000` and a
 * negative one as `00:00:00.-00001` -- neither of them a time. The refusal is
 * php's DateRangeError, one of the three ERROR classes of php's date tree that
 * were undefined here, so `catch (DateRangeError $e)` could not be spelled. */
date_default_timezone_set('UTC');
class MyDtSub extends DateTime {}
$cases = [
    [new DateTime('2020-01-01 00:00:00'), 'DateTime'],
    [new DateTimeImmutable('2020-01-01 00:00:00'), 'DateTimeImmutable'],
    /* the message names the DECLARING class, not the receiver's */
    [new MyDtSub('2020-01-01 00:00:00'), 'MyDtSub'],
];
foreach ($cases as [$o, $n]) {
    foreach ([0, 999999, 1000000, -1, PHP_INT_MAX, PHP_INT_MIN] as $u) {
        try {
            $r = $o->setMicrosecond($u);
            echo "$n($u) -> ", (is_object($r) ? $r : $o)->format('H:i:s.u'), "\n";
        } catch (Throwable $e) {
            echo "$n($u) -> ", get_class($e), ': ', $e->getMessage(), "\n";
        }
    }
}
/* the tree the refusal hangs from */
try { throw new DateRangeError('x'); } catch (DateError $e) { echo 'caught as DateError: ', get_class($e), "\n"; }
try { (new DateTime)->setMicrosecond(-1); } catch (Error $e) { echo "caught as Error\n"; }
var_dump(get_parent_class('DateError'), get_parent_class('DateObjectError'),
    get_parent_class('DateInvalidOperationException'),
    is_a('DateInvalidOperationException', 'Exception', true));
?>
--EXPECT--
DateTime(0) -> 00:00:00.000000
DateTime(999999) -> 00:00:00.999999
DateTime(1000000) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, 1000000 given
DateTime(-1) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, -1 given
DateTime(9223372036854775807) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, 9223372036854775807 given
DateTime(-9223372036854775808) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, -9223372036854775808 given
DateTimeImmutable(0) -> 00:00:00.000000
DateTimeImmutable(999999) -> 00:00:00.999999
DateTimeImmutable(1000000) -> DateRangeError: DateTimeImmutable::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, 1000000 given
DateTimeImmutable(-1) -> DateRangeError: DateTimeImmutable::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, -1 given
DateTimeImmutable(9223372036854775807) -> DateRangeError: DateTimeImmutable::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, 9223372036854775807 given
DateTimeImmutable(-9223372036854775808) -> DateRangeError: DateTimeImmutable::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, -9223372036854775808 given
MyDtSub(0) -> 00:00:00.000000
MyDtSub(999999) -> 00:00:00.999999
MyDtSub(1000000) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, 1000000 given
MyDtSub(-1) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, -1 given
MyDtSub(9223372036854775807) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, 9223372036854775807 given
MyDtSub(-9223372036854775808) -> DateRangeError: DateTime::setMicrosecond(): Argument #1 ($microsecond) must be between 0 and 999999, -9223372036854775808 given
caught as DateError: DateRangeError
caught as Error
string(5) "Error"
string(9) "DateError"
string(13) "DateException"
bool(true)
--CLEAN--
<?php
/* the smoke corpus runs in ONE interpreter: leave no globals behind */
unset($cases, $o, $n, $u, $r);
