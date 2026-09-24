--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a sort COMPARISON that runs user code which does not return (a throwing/exiting __toString) stops the sort: the body runs once
--FILE--
<?php
// A comparison is the other half of the callback boundary: SORT_REGULAR's value
// comparison and the string flags' coercion both reach an object operand's
// __toString(), and that body can throw or exit(). The comparator answers an
// ORDERING and has no way to report what happened, so the sort used to compare
// the next pair -- running the body again. Where a try/catch had already caught
// the first throw in place, the SECOND one was uncaught and killed a script php
// merely prints "caught" in.
$stu_f = tempnam(sys_get_temp_dir(), 'stu');

function stu_run($stu_body)
{
    global $stu_f;
    file_put_contents($stu_f, "<?php\n" . $stu_body . "\necho \"AFTER\\n\";\n");
    $stu_out = shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($stu_f) . ' 2>&1');
    // Keep only the lines the test is about; an uncaught report's trace names the
    // internal frame differently in the two engines (a separate divergence).
    $stu_keep = [];
    foreach (explode("\n", str_replace("\r\n", "\n", (string)$stu_out)) as $stu_line) {
        if ($stu_line === 'ts' || $stu_line === 'bye' || $stu_line === 'caught'
            || $stu_line === 'AFTER' || $stu_line === 'NEVER') {
            $stu_keep[] = $stu_line;
        } elseif (strpos($stu_line, 'Uncaught') !== false) {
            $stu_keep[] = 'UNCAUGHT';
        }
    }
    echo implode(',', $stu_keep), "\n";
}

$stu_thrower = 'class T { public function __toString(){ echo "ts\n"; throw new Exception("x"); } }';
$stu_exiter  = 'class T { public function __toString(){ echo "ts\n"; exit("bye\n"); } }';

foreach (['throw' => $stu_thrower, 'exit' => $stu_exiter] as $stu_kind => $stu_class) {
    foreach ([
        'sort'            => '$a=[new T,"b","c"]; sort($a);',
        'sort_string'     => '$a=[new T,"b","c"]; sort($a, SORT_STRING);',
        'asort'           => '$a=[new T,"b","c"]; asort($a);',
        'rsort'           => '$a=[new T,"b","c"]; rsort($a);',
        'multisort'       => '$a=[new T,"b","c"]; array_multisort($a);',
        'caught_sort'     => '$a=[new T,"b","c"]; try { sort($a); } catch (Throwable $e) { echo "caught\n"; }',
        'caught_asort'    => '$a=[new T,"b","c"]; try { asort($a); } catch (Throwable $e) { echo "caught\n"; }',
        'caught_multi'    => '$a=[new T,"b","c"]; try { array_multisort($a); } catch (Throwable $e) { echo "caught\n"; }',
    ] as $stu_name => $stu_code) {
        echo str_pad($stu_kind . '/' . $stu_name, 22), ': ';
        stu_run($stu_class . "\n" . $stu_code);
    }
}
unlink($stu_f);

// Standing down is about not RE-ENTERING user code, not about giving up on the
// order: php keeps comparing, with the operand whose coercion failed ordered the
// way a refused cast orders it. These pin the resulting ORDER, which is what a
// "every later pair compares equal" shortcut would silently change.
function stu_shape(array $stu_arr): string
{
    $stu_parts = [];
    foreach ($stu_arr as $stu_k => $stu_v) {
        $stu_parts[] = $stu_k . '=>' . (is_object($stu_v) ? 'OBJ' : var_export($stu_v, true));
    }
    return implode(',', $stu_parts);
}
class StuThrower
{
    public function __toString(): string { throw new Exception('x'); }
}
foreach ([
    'sort'          => static function (array $a) { sort($a); return $a; },
    'rsort'         => static function (array $a) { rsort($a); return $a; },
    'asort'         => static function (array $a) { asort($a); return $a; },
    'sort_string'   => static function (array $a) { sort($a, SORT_STRING); return $a; },
    'rsort_string'  => static function (array $a) { rsort($a, SORT_STRING); return $a; },
] as $stu_name => $stu_fn) {
    foreach (['throwing' => new StuThrower, 'plain' => new stdClass] as $stu_kind => $stu_obj) {
        try {
            $stu_r = $stu_fn(['b', $stu_obj, 'a', 'c']);
            echo str_pad($stu_name . '/' . $stu_kind, 22), ': ', stu_shape($stu_r), "\n";
        } catch (Throwable $stu_e) {
            echo str_pad($stu_name . '/' . $stu_kind, 22), ': caught, ', stu_shape($stu_e ? [] : []), "\n";
        }
    }
}
// The catch is outside so the array is still observable: php sorts after the
// throw, so this prints the ORDER it reached.
$stu_a = ['b', new StuThrower, 'a', 'c'];
try {
    sort($stu_a);
} catch (Throwable $stu_e) {
    echo 'caught: ', $stu_e->getMessage(), "\n";
}
echo 'after-throw sort: ', stu_shape($stu_a), "\n";

// array_multisort is the one that does NOT write back: php leaves every column
// exactly as it found it when a comparison raised.
$stu_c1 = ['b', new StuThrower, 'a'];
$stu_c2 = [1, 2, 3];
try {
    array_multisort($stu_c1, $stu_c2);
} catch (Throwable $stu_e) {
    echo 'caught multisort', "\n";
}
echo 'multisort col1: ', stu_shape($stu_c1), "\n";
echo 'multisort col2: ', stu_shape($stu_c2), "\n";

// The other rail, unchanged: an object with NO __toString() raises php's
// coercion Error once and php goes on comparing, so the array still comes out
// sorted with the object first.
$stu_a = [new stdClass, "b", "a"];
try {
    sort($stu_a, SORT_STRING);
} catch (Throwable $stu_e) {
    echo 'caught: ', $stu_e->getMessage(), "\n";
}
echo 'not-stringable sorted: ', stu_shape($stu_a), "\n";

// A string-flag sort whose SCALARS must keep their order around the raising
// object: blanking them all would sort "" against "".
$stu_a = [3, 1, new stdClass, 2];
try {
    sort($stu_a, SORT_STRING);
} catch (Throwable $stu_e) {
    echo 'caught scalars', "\n";
}
echo 'scalars kept: ', stu_shape($stu_a), "\n";
?>
--EXPECT--
throw/sort            : ts,UNCAUGHT
throw/sort_string     : ts,UNCAUGHT
throw/asort           : ts,UNCAUGHT
throw/rsort           : ts,UNCAUGHT
throw/multisort       : ts,UNCAUGHT
throw/caught_sort     : ts,caught,AFTER
throw/caught_asort    : ts,caught,AFTER
throw/caught_multi    : ts,caught,AFTER
exit/sort             : ts,bye
exit/sort_string      : ts,bye
exit/asort            : ts,bye
exit/rsort            : ts,bye
exit/multisort        : ts,bye
exit/caught_sort      : ts,bye
exit/caught_asort     : ts,bye
exit/caught_multi     : ts,bye
sort/throwing         : caught, 
sort/plain            : 0=>'a',1=>'b',2=>'c',3=>OBJ
rsort/throwing        : caught, 
rsort/plain           : 0=>OBJ,1=>'c',2=>'b',3=>'a'
asort/throwing        : caught, 
asort/plain           : 2=>'a',0=>'b',3=>'c',1=>OBJ
sort_string/throwing  : caught, 
sort_string/plain     : caught, 
rsort_string/throwing : caught, 
rsort_string/plain    : caught, 
caught: x
after-throw sort: 0=>'a',1=>'b',2=>'c',3=>OBJ
caught multisort
multisort col1: 0=>'b',1=>OBJ,2=>'a'
multisort col2: 0=>1,1=>2,2=>3
caught: Object of class stdClass could not be converted to string
not-stringable sorted: 0=>OBJ,1=>'a',2=>'b'
caught scalars
scalars kept: 0=>OBJ,1=>1,2=>2,3=>3
--CLEAN--
<?php
unset($stu_a, $stu_e, $stu_f, $stu_c1, $stu_c2, $stu_r, $stu_obj);
