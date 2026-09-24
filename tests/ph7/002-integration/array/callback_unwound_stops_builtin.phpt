--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a callback that does NOT return (exit(), an uncaught throw) stops the builtin driving it: one call, not one per remaining element
--FILE--
<?php
// Each case runs in a CHILD interpreter because the event under test terminates
// the script: the callback exits from inside the builtin's own loop. What is
// asserted is the number of "call" lines before "bye" -- php runs the callback
// once and stops, and a builtin that kept looping would print one per element.
function cbu_run($cbu_code)
{
    $cbu_f = tempnam(sys_get_temp_dir(), 'cbu');
    file_put_contents($cbu_f, "<?php\n" . $cbu_code . "\necho \"NEVER\\n\";\n");
    $cbu_out = shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($cbu_f));
    unlink($cbu_f);
    echo str_replace(["\r\n", "\n"], ['|', '|'], (string)$cbu_out), "\n";
}

$cbu_cases = [
    'array_filter'      => '$a=[1,2,3]; array_filter($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_map'         => '$a=[1,2,3]; array_map(function($v){ echo "call($v)\n"; exit("bye\n"); }, $a);',
    'array_map_zip'     => '$a=[1,2,3]; array_map(function($x,$y){ echo "call($x)\n"; exit("bye\n"); }, $a, $a);',
    'array_walk'        => '$a=[1,2,3]; array_walk($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_walk_rec'    => '$a=[[1],[2]]; array_walk_recursive($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_reduce'      => '$a=[1,2,3]; array_reduce($a, function($c,$v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_any'         => '$a=[1,2,3]; array_any($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_all'         => '$a=[1,2,3]; array_all($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_find'        => '$a=[1,2,3]; array_find($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'array_find_key'    => '$a=[1,2,3]; array_find_key($a, function($v){ echo "call($v)\n"; exit("bye\n"); });',
    'usort'             => '$a=[4,3,2,1]; usort($a, function($x,$y){ echo "call\n"; exit("bye\n"); });',
    'uasort'            => '$a=[4,3,2,1]; uasort($a, function($x,$y){ echo "call\n"; exit("bye\n"); });',
    'uksort'            => '$a=[4=>1,3=>1,2=>1,1=>1]; uksort($a, function($x,$y){ echo "call\n"; exit("bye\n"); });',
    'array_udiff'       => '$a=[1,2]; array_udiff($a, [3,4], function($x,$y){ echo "call\n"; exit("bye\n"); });',
    'array_uintersect'  => '$a=[1,2]; array_uintersect($a, [3,4], function($x,$y){ echo "call\n"; exit("bye\n"); });',
    'array_diff_ukey'   => '$a=[1,2]; array_diff_ukey($a, [3,4], function($x,$y){ echo "call\n"; exit("bye\n"); });',
    'preg_repl_cb'      => 'preg_replace_callback("/./", function($m){ echo "call({$m[0]})\n"; exit("bye\n"); }, "abc");',
    'preg_repl_cb_pats' => 'preg_replace_callback(["/a/","/b/"], function($m){ echo "call({$m[0]})\n"; exit("bye\n"); }, "ab");',
    'preg_repl_cb_subj' => 'preg_replace_callback("/./", function($m){ echo "call({$m[0]})\n"; exit("bye\n"); }, ["ab","cd"]);',
    'iterator_apply'    => 'iterator_apply(new ArrayIterator([1,2,3]), function(){ echo "call\n"; exit("bye\n"); });',
];
foreach ($cbu_cases as $cbu_name => $cbu_code) {
    echo str_pad($cbu_name, 18), ': ';
    cbu_run($cbu_code);
}
?>
--EXPECT--
array_filter      : call(1)|bye|
array_map         : call(1)|bye|
array_map_zip     : call(1)|bye|
array_walk        : call(1)|bye|
array_walk_rec    : call(1)|bye|
array_reduce      : call(1)|bye|
array_any         : call(1)|bye|
array_all         : call(1)|bye|
array_find        : call(1)|bye|
array_find_key    : call(1)|bye|
usort             : call|bye|
uasort            : call|bye|
uksort            : call|bye|
array_udiff       : call|bye|
array_uintersect  : call|bye|
array_diff_ukey   : call|bye|
preg_repl_cb      : call(a)|bye|
preg_repl_cb_pats : call(a)|bye|
preg_repl_cb_subj : call(a)|bye|
iterator_apply    : call|bye|
--CLEAN--
<?php
unset($cbu_cases, $cbu_name, $cbu_code);
