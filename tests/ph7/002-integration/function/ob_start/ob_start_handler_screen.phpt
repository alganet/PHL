--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_start() refuses a handler it cannot call instead of opening a buffer that never filters, and ob_list_handlers() answers an array with nothing buffering
--FILE--
<?php
$obst_err = [];
$obst_log = [];
set_error_handler(function ($n, $m) use (&$obst_err) { $obst_err[] = $m; return true; });

class ObStK
{
    public function m($obst_b, $obst_p) { return strtoupper($obst_b); }
    private function hidden($obst_b, $obst_p) { return $obst_b; }
}
class ObStInv
{
    public function __invoke($obst_b, $obst_p) { return "[$obst_b]"; }
}
function obst_filter($obst_b, $obst_p) { return $obst_b; }

// php SCREENS the handler before it opens the buffer. Every one of these used to
// answer TRUE and open a buffer whose handler never ran, so a misspelled filter
// name passed `if (!ob_start('my_filter'))` and the output came out unfiltered.
$obst_bad = [
    'missing function' => 'obst_no_such_function',
    'empty name'       => '',
    'one member'       => ['ObStK'],
    'three members'    => ['ObStK', 'm', 'extra'],
    'no such method'   => [new ObStK, 'nope'],
    'private method'   => [new ObStK, 'hidden'],
    'missing class'    => ['ObStNoSuchClass', 'm'],
    'an int'           => 5,
    'a float'          => 1.5,
    'a bool'           => true,
    'a plain object'   => new stdClass,
];
foreach ($obst_bad as $obst_what => $obst_cb) {
    $obst_log[] = $obst_what . ': ' . var_export(ob_start($obst_cb), true) . ' level=' . ob_get_level();
}

// ...and the ones it accepts still open one.
$obst_good = [
    'null'          => null,
    'a function'    => 'obst_filter',
    'a method pair' => [new ObStK, 'm'],
    'a class pair'  => ['ObStK', 'm'],
    'a closure'     => null, // replaced below
    '__invoke'      => new ObStInv,
];
$obst_good['a closure'] = function ($obst_b, $obst_p) { return $obst_b; };
foreach ($obst_good as $obst_what => $obst_cb) {
    $obst_open = ob_start($obst_cb);
    $obst_lvl = ob_get_level();
    ob_end_clean();
    $obst_log[] = $obst_what . ': ' . var_export($obst_open, true) . ' level=' . $obst_lvl;
}

// ob_list_handlers() answers an ARRAY when nothing is buffering. It used to
// answer NULL, so `foreach (ob_list_handlers() as $h)` was a TypeError and
// count() a fatal on the ordinary "is anything buffering?" check.
$obst_list = ob_list_handlers();
$obst_log[] = 'empty list: ' . var_export($obst_list, true) . ' count=' . count($obst_list) . ' level=' . ob_get_level();

restore_error_handler();
echo implode("\n", $obst_log), "\n";
print_r($obst_err);
?>
--EXPECT--
missing function: false level=0
empty name: false level=0
one member: false level=0
three members: false level=0
no such method: false level=0
private method: false level=0
missing class: false level=0
an int: false level=0
a float: false level=0
a bool: false level=0
a plain object: false level=0
null: true level=1
a function: true level=1
a method pair: true level=1
a class pair: false level=0
a closure: true level=1
__invoke: true level=1
empty list: array (
) count=0 level=0
Array
(
    [0] => ob_start(): function "obst_no_such_function" not found or invalid function name
    [1] => ob_start(): Failed to create buffer
    [2] => ob_start(): function "" not found or invalid function name
    [3] => ob_start(): Failed to create buffer
    [4] => ob_start(): array callback must have exactly two members
    [5] => ob_start(): Failed to create buffer
    [6] => ob_start(): array callback must have exactly two members
    [7] => ob_start(): Failed to create buffer
    [8] => ob_start(): class ObStK does not have a method "nope"
    [9] => ob_start(): Failed to create buffer
    [10] => ob_start(): cannot access private method ObStK::hidden()
    [11] => ob_start(): Failed to create buffer
    [12] => ob_start(): class "ObStNoSuchClass" not found
    [13] => ob_start(): Failed to create buffer
    [14] => ob_start(): no array or string given
    [15] => ob_start(): Failed to create buffer
    [16] => ob_start(): no array or string given
    [17] => ob_start(): Failed to create buffer
    [18] => ob_start(): no array or string given
    [19] => ob_start(): Failed to create buffer
    [20] => ob_start(): no array or string given
    [21] => ob_start(): Failed to create buffer
    [22] => ob_start(): non-static method ObStK::m() cannot be called statically
    [23] => ob_start(): Failed to create buffer
    [24] => ob_end_clean(): Failed to delete buffer. No buffer to delete
)
