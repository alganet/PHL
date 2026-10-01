--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a write to an unconstructed DateInterval creates a DEPRECATED dynamic property (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* what the write really does here: the object has no struct, so php's write
 * handler has nothing to convert and the standard one creates a dynamic property
 * holding the RAW value -- 1.5 stays a float, where a constructed interval would
 * have taken the int cast. The deprecation itself is muted: the scope policy keeps
 * E_DEPRECATED off this corpus, and the notice is not what the pair is pinning. */
error_reporting(E_ALL & ~E_DEPRECATED);
$i = (new ReflectionClass('DateInterval'))->newInstanceWithoutConstructor();
foreach (['y', 'f', 'invert', 'days', 'from_string', 'nope'] as $p) {
    try {
        $i->$p = 1.5;
        echo "$p written: ", var_export($i->$p, true), "\n";
    } catch (Error $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
var_dump(get_object_vars($i));

/* An increment reads before it writes, and the read miss is an ordinary
 * undefined-property warning here. Captured through a handler so the line lands
 * on stdout in order, whatever the platform points stderr at. */
set_error_handler(function ($no, $str) {
    if ($no & error_reporting()) { echo 'warn: ', $str, "\n"; }
    return true;
});
$i->d++;
restore_error_handler();
var_dump($i->d);

/* The constructor overwrites what the writes left, and appends the rest of the
 * set behind them -- so the six keep the POSITION their write gave them. */
$i->__construct('P1D');
echo implode(',', array_keys(get_object_vars($i))), "\n";
$i->y = 1.5;
var_dump($i->y);
?>
--EXPECT--
y written: 1.5
f written: 1.5
invert written: 1.5
days written: 1.5
from_string written: 1.5
nope written: 1.5
array(6) {
  ["y"]=>
  float(1.5)
  ["f"]=>
  float(1.5)
  ["invert"]=>
  float(1.5)
  ["days"]=>
  float(1.5)
  ["from_string"]=>
  float(1.5)
  ["nope"]=>
  float(1.5)
}
warn: Undefined property: DateInterval::$d
int(1)
y,f,invert,days,from_string,nope,d,m,h,i,s
int(1)
