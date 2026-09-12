--TEST--
fclose() leaves the handle a "closed resource": is_resource()==false, gettype()=='resource (closed)', shared across copies, double-close TypeError
--FILE--
<?php
// Open handle: live resource
$crA = fopen('php://memory', 'r+');
echo is_resource($crA) ? 'open-yes' : 'open-no', "\n";
echo gettype($crA), "\n";
// A second value sharing the same handle must observe the close too
$crB = $crA;
fclose($crA);
echo is_resource($crA) ? 'a-yes' : 'a-no', "\n";
echo is_resource($crB) ? 'b-yes' : 'b-no', "\n";
echo gettype($crA), "\n";
echo gettype($crB), "\n";
echo get_resource_type($crB), "\n";
echo get_debug_type($crB), "\n";
// The check php's PHPUnit IsType constraint performs
echo (gettype($crB) === 'resource (closed)') ? 'closed-ok' : 'closed-bad', "\n";
// Double close raises a catchable TypeError
try { fclose($crA); echo "no-throw\n"; }
catch (\TypeError $e) { echo 'TypeError: ', $e->getMessage(), "\n"; }
--EXPECT--
open-yes
resource
a-no
b-no
resource (closed)
resource (closed)
Unknown
resource (closed)
closed-ok
TypeError: fclose(): Argument #1 ($stream) must be an open stream resource
