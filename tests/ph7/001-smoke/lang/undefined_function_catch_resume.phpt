--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A caught undefined-function Error resumes execution after the catch
--DESCRIPTION--
The undefined-function arm of OP_CALL threw its Error and then unconditionally
unwound the exec (`goto Exception`) — even when a catch had already RUN in
place inside the throw machinery. `try { nosuchfn(); } catch (Error $e) {}`
ran the catch and then silently dropped the rest of the script with exit 0
(a truncated program reporting success). The arm now routes through the
recorded-resume check like every other in-exec throw site. Undefined METHODS
were never affected; this pins the FUNCTION path across the catch shapes.
--FILE--
<?php
try { undefn_missing(); } catch (Error $e) { echo "1:", $e->getMessage(), "\n"; }
try { $undefn_x = 1 + undefn_missing2(); } catch (Error $e) { echo "2:c\n"; }
function undefn_deep() { undefn_missing_inner(); }
try { undefn_deep(); } catch (Error $e) { echo "3:", $e->getMessage(), "\n"; }
$undefn_f = 'undefn_missing_dyn';
try { $undefn_f(); } catch (Error $e) { echo "4:", $e->getMessage(), "\n"; }
try { undefn_missing_fin(); } catch (Error $e) { echo "5:c\n"; } finally { echo "5:f\n"; }
for ($undefn_i = 0; $undefn_i < 10000; $undefn_i++) {
    try { undefn_nope(); } catch (Error $e) {}
}
echo "6:loop-ok\n";
function undefn_gen() {
    try { undefn_genmiss(); } catch (Error $e) { echo "7:c\n"; }
    yield 1;
}
foreach (undefn_gen() as $undefn_v) { echo "7:", $undefn_v, "\n"; }
try {
    try { undefn_rethr(); } catch (Error $e) { throw $e; }
} catch (Error $e) { echo "8:c\n"; }
try {
    try { undefn_wrongtype(); } catch (TypeError $e) { echo "9:wrong\n"; }
} catch (Error $e) { echo "9:outer\n"; }
try { undefn_spread(...[1, 2]); } catch (Error $e) { echo "10:c\n"; }
try { undefn_named(a: 1); } catch (Error $e) { echo "11:c\n"; }
function undefn_propagate_fin() {
    try { undefn_missing3(); } finally { echo "12:fin\n"; }
}
try { undefn_propagate_fin(); } catch (Error $e) { echo "12:c\n"; }
echo "end\n";
?>
--EXPECT--
1:Call to undefined function undefn_missing()
2:c
3:Call to undefined function undefn_missing_inner()
4:Call to undefined function undefn_missing_dyn()
5:c
5:f
6:loop-ok
7:c
7:1
8:c
9:outer
10:c
11:c
12:fin
12:c
end
--CLEAN--
<?php
unset($undefn_x, $undefn_f, $undefn_i, $undefn_v);
