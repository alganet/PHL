--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A refused call names the unit it is written in, at the line it begins on
--FILE--
<?php
/* Two halves of one question: WHERE is the code a diagnostic is about.
 * The FILE is the unit the code is WRITTEN in — a file some function
 * included is its own unit, not the includer's — and the LINE of a call
 * that php refuses is where the call BEGINS, not where its argument list
 * closes. */
function stShow(string $tag, callable $fn)
{
    try { $fn(); echo $tag, " no-throw\n"; }
    catch (Throwable $e) {
        $stWhere = basename($e->getFile());
        if (str_starts_with($stWhere, 'st_unit_')) { $stWhere = 'st_unit'; }
        else { $stWhere = 'self'; }
        echo $tag, ' ', get_class($e), ' @', $stWhere, ':', $e->getLine(),
            ' ', $e->getMessage(), "\n";
    }
}

/* (1) A call php refuses, written across several lines. */
stShow('function ', function () {
    st_no_such_function(
        1,
        2
    );
});
stShow('argless  ', function () {
    st_no_such_function(
    );
});
stShow('method   ', function () {
    $o = new stdClass();
    $o->stNoSuchMethod(
        1,
        2
    );
});
stShow('static   ', function () {
    StNoSuchClass::stat(
        1
    );
});
stShow('private  ', function () {
    (new StPriv())->hidden(
        1
    );
});
class StPriv { private function hidden($a) {} }

/* (2) The screen runs BEFORE the arguments, so an argument that would
 * throw never gets the chance — php's ordering, and the line stays the
 * call's. */
function stBoom() { throw new LogicException('argument ran'); }
stShow('ordering ', function () {
    $o = new stdClass();
    $o->stNoSuchMethod(
        stBoom()
    );
});

/* (3) A backtrace frame names the call's first line too. */
function stTrace() { echo 'trace @', (new Exception())->getTrace()[0]['line'], "\n"; }
stTrace(
    );

/* (4) The FILE is the unit the code is WRITTEN in. A file loaded by a
 * METHOD is its own unit: the diagnostic names IT, not the loader. This
 * is what PHPUnit's TestSuiteLoader does, and it named itself. */
$stTmp = sys_get_temp_dir() . '/st_unit_' . getmypid() . '.php';
file_put_contents($stTmp, "<?php\n// line 2\n// line 3\nst_not_here_either();\n");
class StLoader { public function load(string $f) { require $f; } }
stShow('included ', function () use ($stTmp) { (new StLoader())->load($stTmp); });
@unlink($stTmp);
?>
--EXPECT--
function  Error @self:21 Call to undefined function st_no_such_function()
argless   Error @self:27 Call to undefined function st_no_such_function()
method    Error @self:32 Call to undefined method stdClass::stNoSuchMethod()
static    Error @self:38 Class "StNoSuchClass" not found
private   Error @self:43 Call to private method StPriv::hidden() from global scope
ordering  Error @self:55 Call to undefined method stdClass::stNoSuchMethod()
trace @62
included  Error @st_unit:4 Call to undefined function st_not_here_either()
