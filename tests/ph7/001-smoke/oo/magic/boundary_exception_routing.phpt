--TEST--
Exceptions thrown by PHP callees invoked from engine C sites propagate (no silent resume)
--DESCRIPTION--
A throw inside __toString / offsetGet / offsetSet / count() / __clone /
__destruct / a builtin's callback / an error handler must unwind to the
enclosing catch and must NOT let the surrounding expression resume with a
fallback value (the pre-fix engine ran the catch AND kept executing).
--FILE--
<?php
class BdryThrowStr { public function __toString(): string { throw new Exception("ts"); } }

// 1. concat: catch runs, the try body does NOT resume, $s keeps its prior value
$s = "prior";
try { $s = "a" . new BdryThrowStr(); echo "resumed-concat\n"; }
catch (Exception $e) { echo "caught-concat:", $e->getMessage(), "\n"; }
echo "s=", $s, "\n";

// 2. argument coercion: the outer call is never entered
function bdry_take_str($x) { echo "entered-f\n"; }
try { bdry_take_str("a" . new BdryThrowStr()); echo "resumed-arg\n"; }
catch (Exception $e) { echo "caught-arg\n"; }

// 3. builtin-internal coercion (sprintf)
try { $r = sprintf("%s", new BdryThrowStr()); echo "resumed-sprintf\n"; }
catch (Exception $e) { echo "caught-sprintf\n"; }

// 4. ArrayAccess offsetGet / offsetSet
class BdryAA implements ArrayAccess {
    public function offsetExists($o): bool { return true; }
    public function offsetGet($o): mixed { throw new Exception("og"); }
    public function offsetSet($o, $v): void { throw new Exception("os"); }
    public function offsetUnset($o): void {}
}
$aa = new BdryAA();
try { $x = $aa[1]; echo "resumed-og\n"; } catch (Exception $e) { echo "caught-og\n"; }
try { $aa[1] = 2; echo "resumed-os\n"; } catch (Exception $e) { echo "caught-os\n"; }

// 5. Countable::count()
class BdryCnt implements Countable { public function count(): int { throw new Exception("c"); } }
try { $n = count(new BdryCnt()); echo "resumed-count\n"; } catch (Exception $e) { echo "caught-count\n"; }

// 6. __clone
class BdryClone { public function __clone() { throw new Exception("cl"); } }
try { $c = clone new BdryClone(); echo "resumed-clone\n"; } catch (Exception $e) { echo "caught-clone\n"; }

// 7. __destruct on unset
class BdryDtor { public function __destruct() { throw new Exception("d"); } }
try { $d = new BdryDtor(); unset($d); echo "resumed-dtor\n"; } catch (Exception $e) { echo "caught-dtor\n"; }

// 8. callback inside a builtin (usort / array_map)
try { $arr = [3, 1, 2]; usort($arr, function ($x, $y) { throw new Exception("u"); }); echo "resumed-usort\n"; }
catch (Exception $e) { echo "caught-usort\n"; }
try { array_map(function ($v) { throw new Exception("m"); }, [1, 2]); echo "resumed-map\n"; }
catch (Exception $e) { echo "caught-map\n"; }

// 9. a throwing error handler supersedes the diagnostic (no Warning printed).
// error_reporting must be live here: PHL only consults the handler when
// reporting is on (php calls it regardless — recorded divergence, NEWPLAN §6),
// and an earlier suite test may leak error_reporting(0) into this shared
// interpreter.
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { throw new Exception("eh"); });
try { trigger_error("boom", E_USER_WARNING); echo "resumed-eh\n"; }
catch (Exception $e) { echo "caught-eh\n"; }
restore_error_handler();

// 10. inline try inside a generator catches a __toString throw at the yield
function bdry_gen() {
    $b = new BdryThrowStr();
    try { yield "a" . $b; } catch (Exception $e) { echo "caught-in-gen\n"; yield "z"; }
}
foreach (bdry_gen() as $v) { echo "got:", $v, "\n"; }
echo "done\n";
?>
--EXPECT--
caught-concat:ts
s=prior
caught-arg
caught-sprintf
caught-og
caught-os
caught-count
caught-clone
caught-dtor
caught-usort
caught-map
caught-eh
caught-in-gen
got:z
done
