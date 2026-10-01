--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A language construct is not a function, at every door that answers about a name
--FILE--
<?php
// php has no `empty`, `isset`, `unset`, `eval`, `echo`, `print`, `include`,
// `include_once`, `require` or `require_once` in its function table: they are
// grammar. Every door that answers about a NAME has to say so, while the
// constructs themselves keep working.
$lcNames = ['empty', 'isset', 'unset', 'eval', 'echo', 'print',
            'include', 'include_once', 'require', 'require_once'];
$lcInternal = array_flip(get_defined_functions()['internal']);
foreach ($lcNames as $lcName) {
    printf("%-13s exists=%d callable=%d listed=%d\n", $lcName,
        (int) function_exists($lcName), (int) is_callable($lcName),
        (int) isset($lcInternal[$lcName]));
}

// ...and neither a variable call nor a callback resolves one.
foreach ($lcNames as $lcName) {
    $lcVar = $lcName;
    try { $lcVar('x'); echo "$lcName: DISPATCHED\n"; }
    catch (Error $lcErr) { echo $lcErr->getMessage(), "\n"; }
}
foreach (['empty', 'print'] as $lcName) {
    try { call_user_func($lcName, 'x'); }
    catch (TypeError $lcErr) { echo $lcErr->getMessage(), "\n"; }
    try { Closure::fromCallable($lcName); }
    catch (TypeError $lcErr) { echo $lcErr->getMessage(), "\n"; }
    try { new ReflectionFunction($lcName); }
    catch (ReflectionException $lcErr) { echo $lcErr->getMessage(), "\n"; }
}

// php 8.5 DOES have these three as real internal functions.
foreach (['exit', 'die', 'clone'] as $lcName) {
    printf("%-6s exists=%d\n", $lcName, (int) function_exists($lcName));
}

// The constructs themselves, including the case-insensitive spellings.
$lcArr = ['k' => 1, 'n' => null];
$lcObj = new stdClass();
$lcObj->p = 5;
var_dump(isset($lcArr['k']), ISSET($lcArr['n']), isset($lcArr['k'], $lcArr['n']),
    empty($lcArr['n']), EMPTY($lcArr['k']), isset($lcObj->p));
unset($lcArr['k'], $lcObj->p);
var_dump($lcArr, isset($lcObj->p));
EVAL('$lcEval = 40 + 2;');
var_dump($lcEval, print('printed' . PHP_EOL));
echo "echoed\n";

// An include still names ITSELF in its diagnostic, and still returns a value.
$lcFile = sys_get_temp_dir() . '/phl-lang-construct-inc.php';
file_put_contents($lcFile, '<?php return 7;');
var_dump(include $lcFile, include_once $lcFile, require $lcFile, require_once $lcFile);
unlink($lcFile);

// A METHOD may be named after a construct; only the FUNCTION table may not.
class LcConstructHolder { public function empty(): string { return 'method'; } }
$lcHolder = new LcConstructHolder();
var_dump($lcHolder->empty(), method_exists($lcHolder, 'empty'),
    is_callable([$lcHolder, 'empty']));
?>
--EXPECT--
empty         exists=0 callable=0 listed=0
isset         exists=0 callable=0 listed=0
unset         exists=0 callable=0 listed=0
eval          exists=0 callable=0 listed=0
echo          exists=0 callable=0 listed=0
print         exists=0 callable=0 listed=0
include       exists=0 callable=0 listed=0
include_once  exists=0 callable=0 listed=0
require       exists=0 callable=0 listed=0
require_once  exists=0 callable=0 listed=0
Call to undefined function empty()
Call to undefined function isset()
Call to undefined function unset()
Call to undefined function eval()
Call to undefined function echo()
Call to undefined function print()
Call to undefined function include()
Call to undefined function include_once()
Call to undefined function require()
Call to undefined function require_once()
call_user_func(): Argument #1 ($callback) must be a valid callback, function "empty" not found or invalid function name
Failed to create closure from callable: function "empty" not found or invalid function name
Function empty() does not exist
call_user_func(): Argument #1 ($callback) must be a valid callback, function "print" not found or invalid function name
Failed to create closure from callable: function "print" not found or invalid function name
Function print() does not exist
exit   exists=1
die    exists=1
clone  exists=1
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
array(1) {
  ["n"]=>
  NULL
}
bool(false)
printed
int(42)
int(1)
echoed
int(7)
bool(true)
int(7)
bool(true)
string(6) "method"
bool(true)
bool(true)
--CLEAN--
<?php
