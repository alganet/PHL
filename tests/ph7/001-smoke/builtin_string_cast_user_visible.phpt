--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin's own string cast is php's USER-VISIBLE one: it warns, and it throws
--FILE--
<?php
/* php's C functions turn `mixed` arguments and array ELEMENTS into strings with
 * the same coercion `echo` uses: an array warns `Array to string conversion` and
 * renders "Array", and an object with no `__toString()` is the catchable
 * `Object of class X could not be converted to string`. PHL used the SILENT
 * embedder cast at these doors, so the warning never fired and the object came
 * back as the literal "Object" -- a string php never produces, written into
 * files, matched against subjects and stored as a class name.
 *
 * php also does not STOP for that throw: zval_get_string leaves the empty
 * string behind and the C function runs to the end, so the bytes a writer had
 * already produced reach the stream and only then does the Error surface. */
class BscStr   { public function __toString(): string { return 'BscInfo'; } }
class BscPlain { }
class BscInfo extends SplFileInfo { }
class BscIter extends ArrayIterator { }

function bsc(string $label, callable $fn): void {
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = "E$no: $msg"; return true; });
    try { $out = $fn(); } catch (\Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    printf("%-34s -> %s\n", $label, str_replace("\n", '', var_export($out, true)));
    foreach ($seen as $line) { echo '    ', $line, "\n"; }
}

$bscArr = [1]; $bscBad = new BscPlain; $bscStr = new BscStr;

echo "## str_replace(): every one of its three doors\n";
bsc('an array subject element',  function () use ($bscArr) { return str_replace('a', 'b', [$bscArr, 'aa']); });
bsc('an array search term',      function () use ($bscArr) { return str_replace([$bscArr, 'a'], 'b', 'aa'); });
bsc('an array replacement',      function () use ($bscArr) { return str_replace(['a'], [$bscArr], 'aa'); });
bsc('a scalar search term',      function () use ($bscArr) { return str_replace($bscArr, 'b', 'Array'); });
bsc('str_ireplace too',          function () use ($bscArr) { return str_ireplace('A', 'b', [$bscArr]); });
bsc('an unstringable subject',   function () use ($bscBad) { return str_replace('a', 'b', [$bscBad]); });
bsc('...and &$count is written', function () use ($bscBad) {
    $c = 'untouched';
    try { str_replace([$bscBad], 'b', 'aa', $c); } catch (\Throwable $e) { }
    return $c;
});

echo "## preg_replace() and preg_replace_callback()\n";
bsc('a subject element',    function () use ($bscArr) { return preg_replace('/a/', 'b', [$bscArr, 'aa']); });
bsc('a replacement element',function () use ($bscArr) { return preg_replace(['/a/'], [$bscArr], 'aa'); });
bsc('a callback subject',   function () use ($bscArr) { return preg_replace_callback('/A/', fn($m) => 'b', [$bscArr]); });
bsc('an unstringable one',  function () use ($bscBad) { return preg_replace('/a/', 'b', [$bscBad]); });
bsc('...and &$count is NOT',function () use ($bscBad) {
    $c = 'untouched';
    try { preg_replace('/a/', $bscBad, 'aa', -1, $c); } catch (\Throwable $e) { }
    return $c;
});
bsc('a string pattern, array repl',  function () { return preg_replace('/a/', ['b'], 'aa'); });
bsc('preg_filter says its own name', function () { return preg_filter('/a/', ['b'], 'aa'); });

echo "## strtr()'s pairs: the KEY is already a string, the VALUE is not\n";
bsc('an array value',        function () use ($bscArr) { return strtr('ab', ['a' => $bscArr, 'b' => 'Z']); });
bsc('an unstringable value', function () use ($bscBad) { return strtr('ab', ['a' => $bscBad, 'b' => 'Z']); });

echo "## the two WRITERS finish their output before the Error surfaces\n";
bsc('file_put_contents joins', function () use ($bscArr, $bscStr) {
    $f = tempnam(sys_get_temp_dir(), 'bsc');
    $n = file_put_contents($f, [$bscArr, '|', 1, 2.5, true, $bscStr]);
    $o = file_get_contents($f); unlink($f);
    return [$n, $o];
});
bsc('...and copies a STREAM', function () {
    $f = tempnam(sys_get_temp_dir(), 'bsc');
    file_put_contents($f, 'seed');
    $h = fopen($f, 'r');
    $g = tempnam(sys_get_temp_dir(), 'bs2');
    $n = file_put_contents($g, $h);
    fclose($h);
    $o = file_get_contents($g); unlink($f); unlink($g);
    return [$n, $o];
});
bsc('the elements after it still land', function () use ($bscBad) {
    $f = tempnam(sys_get_temp_dir(), 'bsc');
    $caught = '';
    try { file_put_contents($f, ['A', $bscBad, 'B']); } catch (\Throwable $e) { $caught = get_class($e); }
    $o = file_get_contents($f); unlink($f);
    return [$o, $caught];
});
bsc('a SCALAR one is FALSE, not a throw', function () use ($bscBad) {
    $f = tempnam(sys_get_temp_dir(), 'bsc');
    file_put_contents($f, 'PRESEEDED');
    $r = file_put_contents($f, $bscBad);
    $o = file_get_contents($f); unlink($f);
    return [$r, $o];
});
bsc('fputcsv writes its whole row', function () use ($bscBad) {
    $f = tempnam(sys_get_temp_dir(), 'bsc');
    $h = fopen($f, 'w');
    $caught = '';
    try { fputcsv($h, ['A', $bscBad, 'B'], ',', '"', '\\'); } catch (\Throwable $e) { $caught = get_class($e); }
    fclose($h);
    $o = file_get_contents($f); unlink($f);
    return [$o, $caught];
});
bsc('an array FIELD is "Array"', function () use ($bscArr) {
    $f = tempnam(sys_get_temp_dir(), 'bsc');
    $h = fopen($f, 'w');
    $n = fputcsv($h, [$bscArr, 'x'], ',', '"', '\\');
    fclose($h);
    $o = file_get_contents($f); unlink($f);
    return [$n, $o];
});

echo "## the CLASS-NAME arguments: six doors, each worded from where it is\n";
foreach (['array' => [1], 'unstringable' => new BscPlain, '__toString' => new BscStr,
          'int' => 5, 'not a class' => 'BscNope', 'not derived' => 'stdClass'] as $kind => $cls) {
    bsc("setInfoClass($kind)",     function () use ($cls) { (new SplFileInfo(__FILE__))->setInfoClass($cls); return 'ok'; });
    bsc("setFileClass($kind)",     function () use ($cls) { (new SplFileInfo(__FILE__))->setFileClass($cls); return 'ok'; });
    bsc("getFileInfo($kind)",      function () use ($cls) { return get_class((new SplFileInfo(__FILE__))->getFileInfo($cls)); });
    bsc("getPathInfo($kind)",      function () use ($cls) { return get_class((new SplFileInfo(__FILE__))->getPathInfo($cls)); });
    bsc("ArrayObject ctor($kind)", function () use ($cls) { return get_class((new ArrayObject([], 0, $cls))->getIterator()); });
    bsc("setIteratorClass($kind)", function () use ($cls) { $o = new ArrayObject([]); $o->setIteratorClass($cls); return $o->getIteratorClass(); });
}
bsc('a derived iterator is taken', function () { $o = new ArrayObject([]); $o->setIteratorClass('BscIter'); return get_class($o->getIterator()); });
bsc('a derived info class too',    function () { return get_class((new SplFileInfo(__FILE__))->getFileInfo('BscInfo')); });
--EXPECT--
## str_replace(): every one of its three doors
an array subject element           -> array (  0 => 'Arrby',  1 => 'bb',)
    E2: Array to string conversion
an array search term               -> 'bb'
    E2: Array to string conversion
an array replacement               -> 'ArrayArray'
    E2: Array to string conversion
a scalar search term               -> 'Array'
str_ireplace too                   -> array (  0 => 'brrby',)
    E2: Array to string conversion
an unstringable subject            -> 'Error: Object of class BscPlain could not be converted to string'
...and &$count is written          -> 0
## preg_replace() and preg_replace_callback()
a subject element                  -> array (  0 => 'Arrby',  1 => 'bb',)
    E2: Array to string conversion
a replacement element              -> 'ArrayArray'
    E2: Array to string conversion
a callback subject                 -> array (  0 => 'brray',)
    E2: Array to string conversion
an unstringable one                -> 'Error: Object of class BscPlain could not be converted to string'
...and &$count is NOT              -> 'untouched'
a string pattern, array repl       -> 'TypeError: preg_replace(): Argument #1 ($pattern) must be of type array when argument #2 ($replacement) is an array, string given'
preg_filter says its own name      -> 'TypeError: preg_filter(): Argument #1 ($pattern) must be of type array when argument #2 ($replacement) is an array, string given'
## strtr()'s pairs: the KEY is already a string, the VALUE is not
an array value                     -> 'ArrayZ'
    E2: Array to string conversion
an unstringable value              -> 'Error: Object of class BscPlain could not be converted to string'
## the two WRITERS finish their output before the Error surfaces
file_put_contents joins            -> array (  0 => 18,  1 => 'Array|12.51BscInfo',)
    E2: Array to string conversion
...and copies a STREAM             -> array (  0 => 4,  1 => 'seed',)
the elements after it still land   -> array (  0 => 'AB',  1 => 'Error',)
a SCALAR one is FALSE, not a throw -> array (  0 => false,  1 => '',)
fputcsv writes its whole row       -> array (  0 => 'A,,B',  1 => 'Error',)
an array FIELD is "Array"          -> array (  0 => 8,  1 => 'Array,x',)
    E2: Array to string conversion
## the CLASS-NAME arguments: six doors, each worded from where it is
setInfoClass(array)                -> 'TypeError: SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name derived from SplFileInfo, Array given'
    E2: Array to string conversion
setFileClass(array)                -> 'TypeError: SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name derived from SplFileObject, Array given'
    E2: Array to string conversion
getFileInfo(array)                 -> 'TypeError: SplFileInfo::getFileInfo(): Argument #1 ($class) must be a class name derived from SplFileInfo or null, Array given'
    E2: Array to string conversion
getPathInfo(array)                 -> 'TypeError: SplFileInfo::getPathInfo(): Argument #1 ($class) must be a valid class name or null, Array given'
    E2: Array to string conversion
ArrayObject ctor(array)            -> 'TypeError: ArrayObject::__construct(): Argument #3 ($iteratorClass) must be a class name derived from ArrayIterator, Array given'
    E2: Array to string conversion
setIteratorClass(array)            -> 'TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, Array given'
    E2: Array to string conversion
setInfoClass(unstringable)         -> 'Error: Object of class BscPlain could not be converted to string'
setFileClass(unstringable)         -> 'Error: Object of class BscPlain could not be converted to string'
getFileInfo(unstringable)          -> 'Error: Object of class BscPlain could not be converted to string'
getPathInfo(unstringable)          -> 'Error: Object of class BscPlain could not be converted to string'
ArrayObject ctor(unstringable)     -> 'Error: Object of class BscPlain could not be converted to string'
setIteratorClass(unstringable)     -> 'Error: Object of class BscPlain could not be converted to string'
setInfoClass(__toString)           -> 'ok'
setFileClass(__toString)           -> 'TypeError: SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name derived from SplFileObject, BscInfo given'
getFileInfo(__toString)            -> 'BscInfo'
getPathInfo(__toString)            -> 'BscInfo'
ArrayObject ctor(__toString)       -> 'TypeError: ArrayObject::__construct(): Argument #3 ($iteratorClass) must be a class name derived from ArrayIterator, BscInfo given'
setIteratorClass(__toString)       -> 'TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, BscInfo given'
setInfoClass(int)                  -> 'TypeError: SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name derived from SplFileInfo, 5 given'
setFileClass(int)                  -> 'TypeError: SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name derived from SplFileObject, 5 given'
getFileInfo(int)                   -> 'TypeError: SplFileInfo::getFileInfo(): Argument #1 ($class) must be a class name derived from SplFileInfo or null, 5 given'
getPathInfo(int)                   -> 'TypeError: SplFileInfo::getPathInfo(): Argument #1 ($class) must be a valid class name or null, 5 given'
ArrayObject ctor(int)              -> 'TypeError: ArrayObject::__construct(): Argument #3 ($iteratorClass) must be a class name derived from ArrayIterator, 5 given'
setIteratorClass(int)              -> 'TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, 5 given'
setInfoClass(not a class)          -> 'TypeError: SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name derived from SplFileInfo, BscNope given'
setFileClass(not a class)          -> 'TypeError: SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name derived from SplFileObject, BscNope given'
getFileInfo(not a class)           -> 'TypeError: SplFileInfo::getFileInfo(): Argument #1 ($class) must be a class name derived from SplFileInfo or null, BscNope given'
getPathInfo(not a class)           -> 'TypeError: SplFileInfo::getPathInfo(): Argument #1 ($class) must be a valid class name or null, BscNope given'
ArrayObject ctor(not a class)      -> 'TypeError: ArrayObject::__construct(): Argument #3 ($iteratorClass) must be a class name derived from ArrayIterator, BscNope given'
setIteratorClass(not a class)      -> 'TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, BscNope given'
setInfoClass(not derived)          -> 'TypeError: SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name derived from SplFileInfo, stdClass given'
setFileClass(not derived)          -> 'TypeError: SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name derived from SplFileObject, stdClass given'
getFileInfo(not derived)           -> 'TypeError: SplFileInfo::getFileInfo(): Argument #1 ($class) must be a class name derived from SplFileInfo or null, stdClass given'
getPathInfo(not derived)           -> 'TypeError: SplFileInfo::getPathInfo(): Argument #1 ($class) must be a class name derived from SplFileInfo or null, stdClass given'
ArrayObject ctor(not derived)      -> 'TypeError: ArrayObject::__construct(): Argument #3 ($iteratorClass) must be a class name derived from ArrayIterator, stdClass given'
setIteratorClass(not derived)      -> 'TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, stdClass given'
a derived iterator is taken        -> 'BscIter'
a derived info class too           -> 'BscInfo'
