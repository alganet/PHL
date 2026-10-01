--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`[&$this, 'm']` is a legal array entry, and it COPIES
--DESCRIPTION--
php's write-target rules refuse `$this = …`, but an array literal's `&$x` entry is
not that: it takes a REFERENCE to a slot, and `$this` has none to take, so php
copies the receiver into the entry and a write through the entry leaves `$this`
alone. PHL raised its compile fatal `Cannot re-assign $this` for the whole shape,
which is the pre-5.4 callable idiom phpseclib's SFTP still writes
(`uasort($contents, [&$this, 'comparator'])`).

`[&f()]` and `[&(new A)->p]` stay refused: the entry is still compiled in write
context, and only the `$this` rule is lifted.
--FILE--
<?php
class ArteC {
    public $v = 'kept';
    public function m() {
        $a = [&$this, 'cmp'];
        $b = ['n' => 1];
        $c = [&$this];
        $c[0] = 5;                 // php: the entry, not the receiver
        $d = array(&$this, 'cmp'); // the long spelling
        $e = 1; $f = [&$this, &$e]; $f[1] = 9;
        return get_class($a[0]) . '|' . $a[1] . '|' . count($a) . '|'
             . gettype($this) . '|' . $this->v . '|' . count($d) . '|' . $e
             . '|' . gettype($c[0]) . '|' . $b['n'];
    }
    public function cmp($x, $y) { return $x <=> $y; }
    public function sorted() {
        $a = [3, 1, 2];
        usort($a, [&$this, 'cmp']);
        return implode(',', $a);
    }
}
$arteO = new ArteC;
var_dump($arteO->m());
var_dump($arteO->sorted());

// An ordinary by-reference entry still aliases.
function arteRef() { $x = 1; $a = [&$x]; $a[0] = 7; return $x; }
var_dump(arteRef());

// The refusals the array entry still owes. Through a FILE rather than `-r`:
// cmd.exe does not quote an inline program the way a POSIX shell does.
foreach (['function arteF() { return 1; } $a = [&arteF()];',
          'class ArteD {} $a = [&(new ArteD)->p];'] as $arteSrc) {
    $arteF = sys_get_temp_dir() . '/phl_arte_' . getmypid() . '.php';
    file_put_contents($arteF, "<?php\n" . $arteSrc . "\n");
    $arteOut = trim((string) shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($arteF) . ' 2>&1'));
    @unlink($arteF);
    foreach (["Can't use function return value in write context",
              'Cannot use temporary expression in write context'] as $arteWant) {
        if (strpos($arteOut, $arteWant) !== false) { echo $arteWant, "\n"; continue 2; }
    }
    echo 'OTHER: ', trim(explode("\n", $arteOut)[0]), "\n";
}
?>
--EXPECTF--
string(%d) "ArteC|cmp|2|object|kept|2|9|integer|1"
string(5) "1,2,3"
int(7)
Can't use function return value in write context
Cannot use temporary expression in write context
