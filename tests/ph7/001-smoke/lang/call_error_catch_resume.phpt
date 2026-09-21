--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A caught call-shape Error resumes AFTER the catch, not inside the try
--DESCRIPTION--
The non-callable-value, malformed-array-callable, and method-visibility throw
sites in OP_CALL routed through PH7_DISPATCH_ENFORCE_RC, which never consults
the recorded in-place-catch resume target — so after the catch ran, execution
FELL THROUGH and continued inside the try right after the failed call
(`try { $f = 42; $f(); echo "inside"; } catch (Error $e) { echo "c"; }`
printed "c" then "inside"; php prints only "c"). The three sites now route
through PH7_THROW_ROUTE_MIDEXPR like OP_THROW. Sibling of the
undefined-function fix (undefined_function_catch_resume.phpt).
--FILE--
<?php
$cerr_nc = 42;
try { $cerr_nc(); echo "1:inside\n"; } catch (Error $e) { echo "1:", $e->getMessage(), "\n"; }
try { $cerr_arr = [1, 2, 3]; $cerr_arr(); echo "2:inside\n"; } catch (Error $e) { echo "2:", $e->getMessage(), "\n"; }
class CerrVis { private function p() { return 1; } }
$cerr_o = new CerrVis;
try { $cerr_o->p(); echo "3:inside\n"; } catch (Error $e) { echo "3:", $e->getMessage(), "\n"; }
try { $cerr_pair = [new CerrVis, "nom"]; $cerr_pair(); echo "4:inside\n"; } catch (Error $e) { echo "4:", $e->getMessage(), "\n"; }
try { $cerr_po = new stdClass; $cerr_po(); echo "5:inside\n"; } catch (Error $e) { echo "5:", $e->getMessage(), "\n"; }
try { $cerr_null = null; $cerr_null(); echo "6:inside\n"; } catch (Error $e) { echo "6:", $e->getMessage(), "\n"; }
echo "end\n";
?>
--EXPECT--
1:Value of type int is not callable
2:Array callback must have exactly two elements
3:Call to private method CerrVis::p() from global scope
4:Call to undefined method CerrVis::nom()
5:Object of type stdClass is not callable
6:Value of type null is not callable
end
--CLEAN--
<?php
unset($cerr_nc, $cerr_arr, $cerr_o, $cerr_pair, $cerr_po, $cerr_null);
