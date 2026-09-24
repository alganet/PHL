--TEST--
get_debug_type() names a resource's TYPE, the way php does: "resource (stream)" for an open handle
--FILE--
<?php
/* get_debug_type() exists to NAME a value's type for a diagnostic, and for
 * every open handle it answered the bare "resource" gettype() answers — a
 * different string from php's in exactly the messages it is called for. The
 * type name has been available all along; get_resource_type() answers it. */
$gdtA = fopen('php://memory', 'r+');
echo get_debug_type($gdtA), "\n";
echo get_resource_type($gdtA), "\n";
/* Which is not gettype()'s answer: php's older function keeps the bare word. */
echo gettype($gdtA), "\n";
$gdtB = fopen('php://temp', 'r+');
echo get_debug_type($gdtB), "\n";
$gdtC = opendir('.');
echo get_debug_type($gdtC), "\n";
closedir($gdtC);
/* A CLOSED one is php's other spelling, and names no type — there is nothing
 * left to ask. */
fclose($gdtA);
echo get_debug_type($gdtA), "\n";
echo get_resource_type($gdtB), "|", get_debug_type($gdtB), "\n";
fclose($gdtB);
/* (A handle that is not a stream is not called one either — proc_open()'s
 * process is asserted beside the rest of that family, in 002-integration.) */
/* The scalars stay the short names, which is the whole difference from
 * gettype(). */
echo get_debug_type(null), ' ', get_debug_type(true), ' ', get_debug_type(1), ' ',
     get_debug_type(1.5), ' ', get_debug_type('s'), ' ', get_debug_type([]), ' ',
     get_debug_type(new stdClass), "\n";
--EXPECT--
resource (stream)
stream
resource
resource (stream)
resource (stream)
resource (closed)
stream|resource (stream)
null bool int float string array stdClass
