--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Undefined/inaccessible calls and bad instantiations raise catchable Errors
--FILE--
<?php
// php raises a CATCHABLE Error for each of these; PH7 printed a notice and carried
// on with NULL (or, for `new Missing()`, killed the script with exit status 0).
class CefC
{
    public $p = 1;
    private function priv() { return 1; }
    public function m() { return 'm'; }
    public static function s() { return 's'; }
}
abstract class CefA { abstract public function am(); }
interface CefI {}

function cefTry($label, $fn)
{
    try {
        echo $label, ' => ', var_export($fn(), true), "\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}

cefTry('undefined method',    fn() => (new CefC)->nope());
cefTry('undefined static',    fn() => CefC::nostatic());
cefTry('private method',      fn() => (new CefC)->priv());
cefTry('abstract call',       fn() => CefA::am());
cefTry('new missing',         fn() => new CefNope());
cefTry('new interface',       fn() => new CefI());
cefTry('new abstract',        fn() => new CefA());
cefTry('method on null',      function () { $x = null; return $x->m(); });
cefTry('method on int',       function () { $x = 5; return $x->m(); });
cefTry('call int',            function () { $x = 5; return $x(); });
cefTry('array cb 1 elem',     function () { $a = [1]; return $a(); });
cefTry('array cb bad method', function () { $a = [new CefC, 'nope']; return $a(); });

// These still work.
cefTry('array cb ok',   function () { $a = [new CefC, 'm']; return $a(); });
cefTry('static cb ok',  function () { $a = ['CefC', 's']; return $a(); });
?>
--EXPECT--
undefined method => undefined method => Error: Call to undefined method CefC::nope()
undefined static => undefined static => Error: Call to undefined method CefC::nostatic()
private method => private method => Error: Call to private method CefC::priv() from global scope
abstract call => abstract call => Error: Cannot call abstract method CefA::am()
new missing => new missing => Error: Class "CefNope" not found
new interface => new interface => Error: Cannot instantiate interface CefI
new abstract => new abstract => Error: Cannot instantiate abstract class CefA
method on null => method on null => Error: Call to a member function m() on null
method on int => method on int => Error: Call to a member function m() on int
call int => call int => Error: Value of type int is not callable
array cb 1 elem => array cb 1 elem => Error: Array callback must have exactly two elements
array cb bad method => array cb bad method => Error: Call to undefined method CefC::nope()
array cb ok => 'm'
static cb ok => 's'
--CLEAN--
<?php
