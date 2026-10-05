--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class(), get_parent_class() and get_called_class() without arguments: executed scope, and the refusal outside a class
--FILE--
<?php
// Without an argument get_class() and get_parent_class() read the EXECUTED
// scope -- the class the running code was written in, the using class for a
// trait -- while get_called_class() reads the late-static-bound one. Outside
// any class get_class() and get_called_class() throw Error; get_parent_class()
// answers false. A literal get_called_class() is compiled away, so its Error
// carries no frame of its own; a dynamic call keeps one. php 8.3 deprecated
// the no-argument get_class() -- raised only once a scope is found -- and
// get_parent_class(), raised before it looks.
$gcsReporting = error_reporting(E_ALL);
set_error_handler(function ($n, $m) { echo "  [$n] $m\n"; return true; });
function gcsProbe($l, $f) {
    try { $r = json_encode($f()); echo $l, ': ', $r, "\n"; }
    catch (Throwable $e) { echo $l, ': ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
class GcsP {
    function m() { return [get_class(), get_parent_class(), get_called_class()]; }
    static function s() { return [get_class(), get_parent_class(), get_called_class()]; }
    function c() { return (fn() => [get_class(), get_called_class()])(); }
    function viaCuf() { return [call_user_func('get_class'), call_user_func('get_called_class')]; }
}
class GcsK extends GcsP {}
trait GcsT { function t() { return [get_class(), get_parent_class(), get_called_class()]; } }
class GcsU extends GcsP { use GcsT; }
class GcsV extends GcsU {}

gcsProbe('method', fn() => (new GcsK)->m());
gcsProbe('static', fn() => GcsK::s());
gcsProbe('closure', fn() => (new GcsK)->c());
gcsProbe('call_user_func', fn() => (new GcsK)->viaCuf());
gcsProbe('trait', fn() => (new GcsV)->t());
gcsProbe('top get_class', fn() => get_class());
gcsProbe('top get_parent_class', fn() => get_parent_class());
gcsProbe('top get_called_class', fn() => get_called_class());
gcsProbe('static closure', static function () { return get_class(); });
gcsProbe('unbound', (function () { return get_class(); })->bindTo(null, null));
gcsProbe('scope only', Closure::bind(function () { return [get_class(), get_called_class()]; }, null, GcsK::class));
gcsProbe('dynamic', function () { $f = 'get_class'; return $f(); });

function gcsLiteral() { get_called_class(); }
function gcsDynamic() { $f = 'get_called_class'; $f(); }
foreach (['gcsLiteral', 'gcsDynamic'] as $fn) {
    try { $fn(); } catch (Error $e) {
        echo $fn, ': innermost frame ', $e->getTrace()[0]['function'], "\n";
    }
}
restore_error_handler();
error_reporting($gcsReporting);
--EXPECT--
  [8192] Calling get_class() without arguments is deprecated
  [8192] Calling get_parent_class() without arguments is deprecated
method: ["GcsP",false,"GcsK"]
  [8192] Calling get_class() without arguments is deprecated
  [8192] Calling get_parent_class() without arguments is deprecated
static: ["GcsP",false,"GcsK"]
  [8192] Calling get_class() without arguments is deprecated
closure: ["GcsP","GcsK"]
  [8192] Calling get_class() without arguments is deprecated
call_user_func: ["GcsP","GcsK"]
  [8192] Calling get_class() without arguments is deprecated
  [8192] Calling get_parent_class() without arguments is deprecated
trait: ["GcsU","GcsP","GcsV"]
top get_class: Error: get_class() without arguments must be called from within a class
  [8192] Calling get_parent_class() without arguments is deprecated
top get_parent_class: false
top get_called_class: Error: get_called_class() must be called from within a class
static closure: Error: get_class() without arguments must be called from within a class
unbound: Error: get_class() without arguments must be called from within a class
  [8192] Calling get_class() without arguments is deprecated
scope only: ["GcsK","GcsK"]
dynamic: Error: get_class() without arguments must be called from within a class
gcsLiteral: innermost frame gcsLiteral
gcsDynamic: innermost frame get_called_class
