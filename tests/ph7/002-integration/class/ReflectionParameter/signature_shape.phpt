--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A comma inside a quoted default is not a parameter separator
--DESCRIPTION--
A signature is text, and one of these has a comma in it: `string $separator =
","`. Reflection's own scan tracked the single quote only, so the comma inside
that DOUBLE-quoted default split the parameter in two -- and SplFileObject's csv
trio reported FOUR parameters, the second of them named `$"`, with every default
after it shifted one place along. The arity scan next to it had learned the same
lesson long ago; this one had not.

Two smaller shapes ride with it. php prints its own `<default>` placeholder for
an OPTIONAL parameter with no default a caller can read -- mt_rand()'s two
bounds, get_class()'s $object -- and prints nothing of the sort for a variadic
tail, whose "default" is having no further arguments. And two functions had no
signature row at all, so stream_isatty() took any number of arguments and
answered false for none, where php refuses.
--FILE--
<?php
$sqAshow = function ($label, $fn) {
    try { $out = json_encode($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo str_pad($label, 44), ' => ', $out, "\n";
};
$sqAsig = function ($what) {
    $r = is_array($what) ? new ReflectionMethod($what[0], $what[1]) : new ReflectionFunction($what);
    $o = [$r->getNumberOfParameters(), $r->getNumberOfRequiredParameters()];
    foreach ($r->getParameters() as $p) { $o[] = (string)$p; }
    return $o;
};

/* A comma inside a QUOTED default is not a parameter separator. Reading it as
   one gave these three a fourth parameter -- named `$"` -- and moved every
   default one place along. */
$sqAshow('setCsvControl', fn () => $sqAsig(['SplFileObject', 'setCsvControl']));
$sqAshow('fgetcsv', fn () => $sqAsig(['SplFileObject', 'fgetcsv']));
$sqAshow('fputcsv', fn () => $sqAsig(['SplFileObject', 'fputcsv']));

/* An optional parameter with no default a caller can READ still prints php's
   own placeholder, and a variadic tail does not. */
$sqAshow('mt_rand', fn () => $sqAsig('mt_rand'));
$sqAshow('get_class', fn () => $sqAsig('get_class'));
$sqAshow('array_walk', fn () => $sqAsig('array_walk'));
$sqAshow('stream_filter_append', fn () => $sqAsig('stream_filter_append'));
$sqAshow('and none of them HAS one', fn () => [
    (new ReflectionFunction('mt_rand'))->getParameters()[0]->isDefaultValueAvailable(),
    (new ReflectionFunction('stream_filter_append'))->getParameters()[3]->isDefaultValueAvailable()]);

/* two signatures this engine simply did not have */
$sqAshow('stream_isatty', fn () => $sqAsig('stream_isatty'));
$sqAshow('stream_isatty with none', fn () => stream_isatty());
$sqAshow('stream_context_get_params', fn () => $sqAsig('stream_context_get_params'));
$sqAshow('and the name its refusal uses', fn () => stream_context_get_params('x'));
--EXPECT--
setCsvControl                                => [3,0,"Parameter #0 [ <optional> string $separator = \",\" ]","Parameter #1 [ <optional> string $enclosure = \"\\\"\" ]","Parameter #2 [ <optional> string $escape = \"\\\\\" ]"]
fgetcsv                                      => [3,0,"Parameter #0 [ <optional> string $separator = \",\" ]","Parameter #1 [ <optional> string $enclosure = \"\\\"\" ]","Parameter #2 [ <optional> string $escape = \"\\\\\" ]"]
fputcsv                                      => [5,1,"Parameter #0 [ <required> array $fields ]","Parameter #1 [ <optional> string $separator = \",\" ]","Parameter #2 [ <optional> string $enclosure = \"\\\"\" ]","Parameter #3 [ <optional> string $escape = \"\\\\\" ]","Parameter #4 [ <optional> string $eol = \"\\n\" ]"]
mt_rand                                      => [2,0,"Parameter #0 [ <optional> int $min = <default> ]","Parameter #1 [ <optional> int $max = <default> ]"]
get_class                                    => [1,0,"Parameter #0 [ <optional> object $object = <default> ]"]
array_walk                                   => [3,2,"Parameter #0 [ <required> object|array &$array ]","Parameter #1 [ <required> callable $callback ]","Parameter #2 [ <optional> mixed $arg = <default> ]"]
stream_filter_append                         => [4,2,"Parameter #0 [ <required> $stream ]","Parameter #1 [ <required> string $filter_name ]","Parameter #2 [ <optional> int $mode = 0 ]","Parameter #3 [ <optional> mixed $params = <default> ]"]
and none of them HAS one                     => [false,false]
stream_isatty                                => [1,1,"Parameter #0 [ <required> $stream ]"]
stream_isatty with none                      => ArgumentCountError: stream_isatty() expects exactly 1 argument, 0 given
stream_context_get_params                    => [1,1,"Parameter #0 [ <required> $context ]"]
and the name its refusal uses                => TypeError: stream_context_get_params(): Argument #1 ($context) must be of type resource, string given
