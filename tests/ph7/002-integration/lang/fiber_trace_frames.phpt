--TEST--
A trace taken inside a fiber shows the body as a callback of the Fiber method that entered it last
--FILE--
<?php
function tr(Throwable $e) { echo str_replace(__FILE__, 'F', $e->getTraceAsString()), "\n"; }
function f($a) {
    tr(new Exception);
    Fiber::suspend(1);
    tr(new Exception);
    try { Fiber::suspend(2); } catch (LogicException $e) { tr(new Exception); }
}
echo "-- start, resume, throw\n";
$fb = new Fiber('f');
$fb->start(5);
$fb->resume(2);
$fb->throw(new LogicException('t'));

echo "-- a throw escaping the body\n";
$fb = new Fiber(function () { throw new Exception('x'); });
try { $fb->start(); } catch (Exception $e) { tr($e); }

echo "-- a method body, and a nested fiber\n";
class K { function m() { tr(new Exception); } }
(new Fiber([new K, 'm']))->start();
function k() { (new Fiber(function () { tr(new Exception); }))->start(); }
(new Fiber('k'))->start();

echo "-- suspended deep, resumed from a function\n";
function inner() { tr(new Exception); Fiber::suspend(); tr(new Exception); }
function outer() { inner(); }
$fb = new Fiber('outer');
$fb->start();
function res($fb) { $fb->resume(); }
res($fb);

echo "-- a generator inside the body\n";
function g() { yield 1; throw new Exception('g'); }
$fb = new Fiber(function () { foreach (g() as $v) {} });
try { $fb->start(); } catch (Exception $e) { tr($e); }

echo "-- bodies the fiber reaches through a trampoline\n";
class M { function __call($n, $a) { tr(new Exception); Fiber::suspend(); tr(new Exception); } }
$fb = new Fiber([new M, 'zz']);
$fb->start();
$fb->resume();
$fb = new Fiber('str_repeat');
try { $fb->start('x', -1); } catch (ValueError $e) { tr($e); }

echo "-- debug_backtrace()\n";
function h() {
    foreach (debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT | DEBUG_BACKTRACE_IGNORE_ARGS) as $i => $fr) {
        echo $i, ': ', implode(',', array_keys($fr)), ' ', $fr['function'],
            isset($fr['object']) ? ' ' . get_class($fr['object']) : '', "\n";
    }
    echo count(debug_backtrace(0, 1)), "\n";
}
(new Fiber('h'))->start();
--EXPECT--
-- start, resume, throw
#0 [internal function]: f()
#1 F(11): Fiber->start()
#2 {main}
#0 [internal function]: f()
#1 F(12): Fiber->resume()
#2 {main}
#0 [internal function]: f()
#1 F(13): Fiber->throw()
#2 {main}
-- a throw escaping the body
#0 [internal function]: {closure:F:16}()
#1 F(17): Fiber->start()
#2 {main}
-- a method body, and a nested fiber
#0 [internal function]: K->m()
#1 F(21): Fiber->start()
#2 {main}
#0 [internal function]: {closure:k():22}()
#1 F(22): Fiber->start()
#2 [internal function]: k()
#3 F(23): Fiber->start()
#4 {main}
-- suspended deep, resumed from a function
#0 F(27): inner()
#1 [internal function]: outer()
#2 F(29): Fiber->start()
#3 {main}
#0 F(27): inner()
#1 [internal function]: outer()
#2 F(30): Fiber->resume()
#3 F(31): res()
#4 {main}
-- a generator inside the body
#0 F(35): g()
#1 [internal function]: {closure:F:35}()
#2 F(36): Fiber->start()
#3 {main}
-- bodies the fiber reaches through a trampoline
#0 [internal function]: M->__call()
#1 F(41): Fiber->start()
#2 {main}
#0 [internal function]: M->__call()
#1 F(42): Fiber->resume()
#2 {main}
#0 [internal function]: str_repeat()
#1 F(44): Fiber->start()
#2 {main}
-- debug_backtrace()
0: function h
1: file,line,function,class,object,type start Fiber
1
