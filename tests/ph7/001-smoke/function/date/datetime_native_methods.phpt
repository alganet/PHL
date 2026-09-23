--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateTime's methods are C, so they declare php's signatures and bind late
--FILE--
<?php
function dtnmShow($label, $fn) {
    echo $label, ': ';
    try {
        $r = $fn();
        echo is_string($r) ? $r : var_export($r, true);
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage();
    }
    echo "\n";
}

// The declared signature is the one Reflection reports, arity and types alike.
$m = new ReflectionMethod('DateTime', 'setTime');
echo 'setTime ', $m->getNumberOfParameters(), '/', $m->getNumberOfRequiredParameters(), ': ';
foreach ($m->getParameters() as $p) {
    echo (string)$p->getType(), ' $', $p->getName(),
         $p->isDefaultValueAvailable() ? '=' . var_export($p->getDefaultValue(), true) : '', ' ';
}
echo "\n";
$m = new ReflectionMethod('DateTime', 'createFromFormat');
echo 'createFromFormat ', $m->getNumberOfParameters(), '/', $m->getNumberOfRequiredParameters(),
     ' static=', var_export($m->isStatic(), true), "\n";
$f = new ReflectionFunction('strtotime');
echo 'strtotime internal=', var_export($f->isInternal(), true),
     ' ', $f->getNumberOfParameters(), '/', $f->getNumberOfRequiredParameters(), "\n";

// Arity and type enforcement php has and a PHP-source method body never had.
dtnmShow('too many', fn() => (new DateTime('@0'))->format('Y', 'x'));
dtnmShow('too few', fn() => (new DateTime('@0'))->setDate(2020, 1));
dtnmShow('zero-arg', fn() => (new DateTimeZone('UTC'))->getName('x'));
dtnmShow('tz needs one', fn() => new DateTimeZone());
dtnmShow('class type', fn() => (new DateTime('@0'))->diff('x'));
dtnmShow('nullable class', fn() => new DateTime('@0', 'UTC'));
dtnmShow('coerced int', fn() => (new DateTime('@0'))->setDate('2020', '2', '3')->format('Y-m-d'));

// Static factories bind LATE: a subclass gets an instance of itself.
class DtnmSub extends DateTime {}
class DtnmSubImm extends DateTimeImmutable {}
dtnmShow('sub fromFormat', fn() => get_class(DtnmSub::createFromFormat('Y-m-d', '2020-01-02')));
dtnmShow('sub fromIface', fn() => get_class(DtnmSubImm::createFromInterface(new DateTime('@0'))));
dtnmShow('plain fromFormat', fn() => get_class(DateTime::createFromFormat('Y-m-d', '2020-01-02')));

// diff() is asymmetric: the day borrow uses the FIRST operand's month.
$a = new DateTime('2020-01-31 00:00:00', new DateTimeZone('UTC'));
$b = new DateTime('2021-03-02 00:00:00', new DateTimeZone('UTC'));
$fwd = $a->diff($b);
$bwd = $b->diff($a);
echo 'fwd ', $fwd->y, ' ', $fwd->m, ' ', $fwd->d, ' ', $fwd->days, ' ', $fwd->invert, "\n";
echo 'bwd ', $bwd->y, ' ', $bwd->m, ' ', $bwd->d, ' ', $bwd->days, ' ', $bwd->invert, "\n";

// A zone NAMED as an offset is abbreviated GMT+HHMM, whatever the offset's value.
echo 'T ', (new DateTime('@0'))->format('e T'), ' | ',
     (new DateTime('2020-01-01', new DateTimeZone('-00:00')))->format('e T'), ' | ',
     (new DateTime('2020-01-01', new DateTimeZone('UTC')))->format('e T'), "\n";

// getLastErrors() is reset by every parse and set by a failing constructor.
var_dump(DateTime::getLastErrors());
try { new DateTime('2020-01-01 junk'); } catch (Throwable $e) { echo "caught\n"; }
echo json_encode(DateTime::getLastErrors()), "\n";
echo json_encode(DateTimeImmutable::getLastErrors()), "\n";
new DateTime('2020-01-01');
var_dump(DateTime::getLastErrors());
DateTime::createFromFormat('Y-m-d', 'nope');
echo json_encode(DateTime::getLastErrors()), "\n";

// Immutable receivers copy; mutable ones answer the very same object.
$imm = new DateTimeImmutable('2020-01-01', new DateTimeZone('UTC'));
$mut = new DateTime('2020-01-01', new DateTimeZone('UTC'));
var_dump($imm->setTimestamp(1) === $imm, $mut->setTimestamp(1) === $mut);
echo get_class((new DtnmSubImm('@0'))->modify('+1 day')), "\n";
?>
--EXPECT--
setTime 4/2: int $hour int $minute int $second=0 int $microsecond=0 
createFromFormat 3/2 static=true
strtotime internal=true 2/1
too many: ArgumentCountError: DateTime::format() expects exactly 1 argument, 2 given
too few: ArgumentCountError: DateTime::setDate() expects exactly 3 arguments, 2 given
zero-arg: ArgumentCountError: DateTimeZone::getName() expects exactly 0 arguments, 1 given
tz needs one: ArgumentCountError: DateTimeZone::__construct() expects exactly 1 argument, 0 given
class type: TypeError: DateTime::diff(): Argument #1 ($targetObject) must be of type DateTimeInterface, string given
nullable class: TypeError: DateTime::__construct(): Argument #2 ($timezone) must be of type ?DateTimeZone, string given
coerced int: 2020-02-03
sub fromFormat: DtnmSub
sub fromIface: DtnmSubImm
plain fromFormat: DateTime
fwd 1 0 30 396 0
bwd 1 1 2 396 1
T +00:00 GMT+0000 | +00:00 GMT+0000 | UTC UTC
bool(false)
caught
{"warning_count":0,"warnings":[],"error_count":1,"errors":{"11":"The timezone could not be found in the database"}}
{"warning_count":0,"warnings":[],"error_count":1,"errors":{"11":"The timezone could not be found in the database"}}
bool(false)
{"warning_count":0,"warnings":[],"error_count":3,"errors":{"0":"A four digit year could not be found","4":"Not enough data available to satisfy format"}}
bool(false)
bool(true)
DtnmSubImm
--CLEAN--
<?php
unset($m, $f, $a, $b, $fwd, $bwd, $imm, $mut);
