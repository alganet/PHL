--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract holds no extra reference on the values it copies: destructors still run
--FILE--
<?php
class ExtractDtor
{
    public $n;
    public function __construct($n) { $this->n = $n; }
    public function __destruct() { echo "dtor {$this->n}\n"; }
}
function extractDtorRun()
{
    $src = ['x' => new ExtractDtor('X'), 'y' => new ExtractDtor('Y'), 'z' => new ExtractDtor('Z')];
    extract($src);
    unset($src, $x, $y, $z);
    echo "-- after unset\n";
}
extractDtorRun();
echo "-- after call\n";
// A mode that installs NOTHING must not retain the values either.
function extractDtorSkipped()
{
    $src = ['a' => new ExtractDtor('A'), 'b' => new ExtractDtor('B')];
    var_dump(extract($src, EXTR_IF_EXISTS));
    unset($src);
    echo "-- after unset\n";
}
extractDtorSkipped();
echo "-- done\n";
?>
--EXPECT--
dtor X
dtor Y
dtor Z
-- after unset
-- after call
int(0)
dtor A
dtor B
-- after unset
-- done
--CLEAN--
<?php
