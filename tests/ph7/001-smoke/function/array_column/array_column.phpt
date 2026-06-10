--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_column: column extraction, index keys, null column, objects
--SKIPIF--
<?php if (!function_exists('array_column')) echo 'skip array_column unavailable'; ?>
--FILE--
<?php
// Engine-neutral recursive formatter: avoids json_encode/var_export output
// quirks. Recursion needs a name, so it is prefixed to stay unique across the
// in-process smoke runner, which includes every test into one process.
function acol_fmt($v){
    if ($v === null) return 'null';
    if ($v === true) return 'true';
    if ($v === false) return 'false';
    if (is_array($v)) {
        $p = [];
        foreach ($v as $k => $x) $p[] = $k . ':' . acol_fmt($x);
        return '{' . implode(',', $p) . '}';
    }
    return (string) $v;
}
$rows = [['id' => 1, 'n' => 'x'], ['id' => 2, 'n' => 'y']];
echo acol_fmt(array_column($rows, 'n')), "\n";              // {0:x,1:y}
echo acol_fmt(array_column($rows, 'n', 'id')), "\n";        // {1:x,2:y}
echo acol_fmt(array_column([['id' => 1, 'n' => 'x']], null, 'id')), "\n"; // whole row keyed by id
echo acol_fmt(array_column([['id' => 1]], 'missing')), "\n"; // {} missing column skipped
// A row missing the index key is appended with a numeric key.
echo acol_fmt(array_column([['id' => 1, 'n' => 'x'], ['n' => 'y']], 'n', 'id')), "\n";
// Object rows with declared properties.
class Row { public $id; public $n; function __construct($i, $s){ $this->id = $i; $this->n = $s; } }
echo acol_fmt(array_column([new Row(10, 'A'), new Row(20, 'B')], 'n', 'id')), "\n";
?>
--EXPECT--
{0:x,1:y}
{1:x,2:y}
{1:{id:1,n:x}}
{}
{1:x,2:y}
{10:A,20:B}
--CLEAN--
<?php
?>
