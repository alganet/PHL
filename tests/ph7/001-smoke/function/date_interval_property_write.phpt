--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DateInterval property write is php's conversion, not a store
--FILE--
<?php
/* php keeps a DateInterval in its own C struct, so a write to one of its
 * properties is a CONVERSION rather than a store: the six relative fields take
 * php's int cast, `invert` is narrowed to the 32-bit int timelib declares it
 * as, and `f` is an int64 count of MICROSECONDS the float only renders
 * (`$i->f = 0.1234567` reads back 0.123456). PHL kept whatever the script
 * wrote, so `$i->y = 1.5` read back 1.5 and the microseconds never reached
 * format(). */
set_error_handler(function ($no, $msg) { echo "W: $msg\n"; return true; });
$i = new DateInterval('PT0S');
foreach ([1.5, -1.5, '7', '12abc', 'abc', true, false, null, [1], '1e3', 0.9] as $v) {
    $i->y = $v;
    printf("y = %-22s -> %s\n", str_replace("\n", '', var_export($v, true)),
        var_export($i->y, true));
}
/* the float cast's whole contract, php's warning included: a value no int64
 * holds WRAPS its low bits, and a NaN or an infinity is 0 */
foreach ([1.0E+30, -1.0E+30, NAN, INF] as $v) {
    $i->m = $v;
    printf("m = %-8s -> %s\n", var_export($v, true), var_export($i->m, true));
}
/* `invert` is the one field php declares as a C int: it wraps at 32 bits where
 * `y` beside it keeps all 64 */
foreach ([1, 1000, 2147483648, -2147483649, 4294967296, 3000000000] as $v) {
    $i->invert = $v;
    $i->y = $v;
    printf("invert = %-11s -> %-12s (y = %s)\n", $v, var_export($i->invert, true),
        var_export($i->y, true));
}
/* `f` is microseconds: the write truncates toward zero, and the property shows
 * the count that was kept */
$j = new DateInterval('PT0S');
foreach ([0.5, -0.75, 0.1234567, 1.9999999, 5.0E-7, 2.5, 1234.567891] as $v) {
    $j->f = $v;
    printf("f = %-12s -> %-12s %%f=%-12s %%F=%s\n", var_export($v, true),
        var_export($j->f, true), $j->format('%f'), $j->format('%F'));
}
/* format() prints five of the six fields through an `(int)` -- a 32-bit
 * NARROWING of a property php hands back whole. The SECONDS are its exception,
 * in both the padded and the plain spelling. */
$k = new DateInterval('PT0S');
foreach (['y', 'm', 'd', 'h', 'i', 's'] as $p) { $k->$p = 7960523868075137518; }
echo $k->format('%y|%Y|%m|%M|%d|%D|%h|%H|%i|%I|%s|%S'), "\n";
var_dump($k->y === 7960523868075137518);
/* every write shape converts, because php writes each through the same handler:
 * a compound assign, a ++/--, a destructuring target */
$l = new DateInterval('PT0S');
$l->f = 0.5;   $l->f += 0.25;                var_dump($l->f);
$l->y = 1;     $l->y += 0.7;                 var_dump($l->y);
$l->d = 1;     $l->d .= 'x';                 var_dump($l->d);
$l->h = 2;     $l->h++;                      var_dump($l->h);
$l->i = PHP_INT_MAX; $l->i++;                var_dump($l->i);
[$l->s] = ['9.9'];                           var_dump($l->s);
foreach ([['4.4']] as [$l->m]) {}            var_dump($l->m);
/* ...and the ++ writes back through it too, so the property is the truncation
 * of the sum while the EXPRESSION is the sum itself */
$l->f = 1.456008; var_dump(++$l->f, $l->f, $l->format('%f'));
/* none of the shapes that would need a slot to ALIAS reach the struct: php has
 * no property-pointer handler for one, so a reference takes a silent copy, a
 * by-reference argument writes nothing, and unset() is ignored */
$m = new DateInterval('PT0S');
$m->s = 3;
$r = &$m->s;  $r = 7;                        var_dump($m->s, $r);
preg_match('/x/', 'x', $m->s);               var_dump($m->s);
unset($m->s);                                var_dump($m->s);
/* ...and php REFUSES the one shape that would hand a script the slot itself */
$q = new DateInterval('PT0S');
$qx = 2.5;
try { $q->f =& $qx; } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$qx = 9.5;
var_dump($q->f);
/* a user subclass inherits the handler (php's handler inheritance) while its
 * OWN property stays an ordinary slot */
class PhlIvSub extends DateInterval { public $own; }
$n = new PhlIvSub('PT0S');
$n->d = 2.9;   $n->own = 2.9;                var_dump($n->d, $n->own);
restore_error_handler();
?>
--EXPECT--
y = 1.5                    -> 1
y = -1.5                   -> -1
y = '7'                    -> 7
y = '12abc'                -> 12
y = 'abc'                  -> 0
y = true                   -> 1
y = false                  -> 0
y = NULL                   -> 0
y = array (  0 => 1,)      -> 1
y = '1e3'                  -> 1000
y = 0.9                    -> 0
W: The float 1.0E+30 is not representable as an int, cast occurred
m = 1.0E+30  -> 5076964154930102272
W: The float -1.0E+30 is not representable as an int, cast occurred
m = -1.0E+30 -> -5076964154930102272
W: The float NAN is not representable as an int, cast occurred
m = NAN      -> 0
W: The float INF is not representable as an int, cast occurred
m = INF      -> 0
invert = 1           -> 1            (y = 1)
invert = 1000        -> 1000         (y = 1000)
invert = 2147483648  -> -2147483648  (y = 2147483648)
invert = -2147483649 -> 2147483647   (y = -2147483649)
invert = 4294967296  -> 0            (y = 4294967296)
invert = 3000000000  -> -1294967296  (y = 3000000000)
f = 0.5          -> 0.5          %f=500000       %F=500000
f = -0.75        -> -0.75        %f=-750000      %F=-750000
f = 0.1234567    -> 0.123456     %f=123456       %F=123456
f = 1.9999999    -> 1.999999     %f=1999999      %F=1999999
f = 5.0E-7       -> 0.0          %f=0            %F=000000
f = 2.5          -> 2.5          %f=2500000      %F=2500000
f = 1234.567891  -> 1234.567891  %f=1234567891   %F=1234567891
111352302|111352302|111352302|111352302|111352302|111352302|111352302|111352302|111352302|111352302|7960523868075137518|7960523868075137518
bool(true)
float(0.75)
int(1)
int(1)
int(3)
W: The float 9.223372036854776E+18 is not representable as an int, cast occurred
int(-9223372036854775808)
int(9)
int(4)
float(2.4560079999999997)
float(2.456007)
string(7) "2456007"
int(3)
int(7)
int(3)
int(3)
Error: Cannot assign by reference to overloaded object
float(0)
int(2)
float(2.9)
