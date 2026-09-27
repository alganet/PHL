--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A from-string DateInterval presents the two names php presents
--FILE--
<?php
/* php 8.2 made `DateInterval::createFromDateString()` answer a different OBJECT:
 * it keeps the STRING and presents `from_string` and `date_string` alone, while
 * still answering the ten fields from what it parsed. PHL filled the ten and
 * showed them, so every SHAPE surface disagreed -- var_dump showed ten entries
 * against php's two, `serialize()` wrote bytes another engine reads back, and
 * `json_encode()` sent ten keys. `date_string` did not exist here at all.
 *
 * The values were already right and still are: the ten stay real slots that
 * format(), add()/sub() and DatePeriod read, and what changed is that this
 * OBJECT hides them from every surface that shows it. An ordinary interval is
 * untouched and carries no `date_string` -- php declares neither name, so
 * `isset()` and `property_exists()` say so. */
date_default_timezone_set('UTC');
$mk = fn() => DateInterval::createFromDateString('1 day + 2 hours');
$i = $mk();
/* php REBUILDS the table at every presentation and the two names reach a plain
 * read only after one, so present first -- the order a program uses. */
print_r($i);
/* no var_dump of an OBJECT anywhere here: its `#id` is the interpreter's own
 * allocation count, which the shared smoke VM makes unpredictable */
var_export($i); echo "\n";
echo serialize($i), "\n";
echo json_encode($i), "\n";
var_dump(get_object_vars($i));
var_dump((array)$i);
foreach ($i as $k => $v) { echo "key $k\n"; }
var_dump(array_map(fn($p) => $p->getName(), (new ReflectionObject($i))->getProperties()));
/* ...and the ten answer from what the string parsed to */
var_dump($i->y, $i->m, $i->d, $i->h, $i->i, $i->s, $i->f, $i->invert, $i->days);
var_dump($i->from_string, $i->date_string);
var_dump(isset($i->d), isset($i->date_string), property_exists($i, 'd'), property_exists($i, 'date_string'));
var_dump($i->format('%R %y %m %d %h %i %s %f %a'));
var_dump((new DateTime('2020-06-15 08:09:10'))->add($i)->format('Y-m-d H:i:s'));
$c = clone $i;
print_r($c);
var_dump($c->d, $c->h);
/* a payload carrying date_string comes back as the same object */
$u = unserialize(serialize($i));
print_r($u);
var_dump($u->d, $u->h, $u->days);
/* ...and one without it is an ordinary interval whatever its from_string says */
print_r(unserialize('O:12:"DateInterval":1:{s:11:"from_string";b:1;}'));
/* an ORDINARY interval carries no date_string at all */
$j = new DateInterval('P1DT2H');
print_r($j);
var_dump(isset($j->date_string), property_exists($j, 'date_string'));
var_dump(array_map(fn($p) => $p->getName(), (new ReflectionObject($j))->getProperties()));
?>
--EXPECT--
DateInterval Object
(
    [from_string] => 1
    [date_string] => 1 day + 2 hours
)
\DateInterval::__set_state(array(
   'from_string' => true,
   'date_string' => '1 day + 2 hours',
))
O:12:"DateInterval":2:{s:11:"from_string";b:1;s:11:"date_string";s:15:"1 day + 2 hours";}
{"from_string":true,"date_string":"1 day + 2 hours"}
array(2) {
  ["from_string"]=>
  bool(true)
  ["date_string"]=>
  string(15) "1 day + 2 hours"
}
array(2) {
  ["from_string"]=>
  bool(true)
  ["date_string"]=>
  string(15) "1 day + 2 hours"
}
key from_string
key date_string
array(2) {
  [0]=>
  string(11) "from_string"
  [1]=>
  string(11) "date_string"
}
int(0)
int(0)
int(1)
int(2)
int(0)
int(0)
float(0)
int(0)
bool(false)
bool(true)
string(15) "1 day + 2 hours"
bool(true)
bool(true)
bool(true)
bool(true)
string(25) "+ 0 0 1 2 0 0 0 (unknown)"
string(19) "2020-06-16 10:09:10"
DateInterval Object
(
    [from_string] => 1
    [date_string] => 1 day + 2 hours
)
int(1)
int(2)
DateInterval Object
(
    [from_string] => 1
    [date_string] => 1 day + 2 hours
)
int(1)
int(2)
bool(false)
DateInterval Object
(
    [y] => -1
    [m] => -1
    [d] => -1
    [h] => -1
    [i] => -1
    [s] => -1
    [f] => 0
    [invert] => 0
    [days] => -1
    [from_string] => 
)
DateInterval Object
(
    [y] => 0
    [m] => 0
    [d] => 1
    [h] => 2
    [i] => 0
    [s] => 0
    [f] => 0
    [invert] => 0
    [days] => 
    [from_string] => 
)
bool(false)
bool(false)
array(10) {
  [0]=>
  string(1) "y"
  [1]=>
  string(1) "m"
  [2]=>
  string(1) "d"
  [3]=>
  string(1) "h"
  [4]=>
  string(1) "i"
  [5]=>
  string(1) "s"
  [6]=>
  string(1) "f"
  [7]=>
  string(6) "invert"
  [8]=>
  string(4) "days"
  [9]=>
  string(11) "from_string"
}
