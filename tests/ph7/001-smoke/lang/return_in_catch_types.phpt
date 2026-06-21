--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return value types are preserved when returning from a catch (int, array, object)
--FILE--
<?php
class RcfTypesObj {
    public $v = 5;
    public function __destruct() { echo "~"; }
}
function rcfInt()   { try { throw new Exception(); } catch (Exception $e) { return 42; } }
function rcfArr()   { try { throw new Exception(); } catch (Exception $e) { return [1, 2, 3]; } }
function rcfObj()   { try { throw new Exception(); } catch (Exception $e) { return new RcfTypesObj(); } }
$i = rcfInt();
echo (is_int($i) ? "int" : "FAIL") . ":" . $i . "\n";
$a = rcfArr();
echo (is_array($a) ? implode(",", $a) : "FAIL") . "\n";
$o = rcfObj();
echo $o->v . "\n";
unset($o);
echo "end\n";
?>
--EXPECT--
int:42
1,2,3
5
~end
--CLEAN--
<?php
