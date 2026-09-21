--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class/interface/pseudo-typed variadic elements are checked, positional and named
--FILE--
<?php
// A class, interface, self, or pseudo-type (true) hint on a variadic must
// reject each mismatching element with php's TypeError: numbered by the
// element's own 1-based call position (positional) or
// max(positional, declared non-variadic formals) + 1 (named), no ($name).
function shortMsg(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    return $msg;
}

class CvBase {}
class CvSub extends CvBase {}
class CvOther {}
interface CvIface {}
class CvImpl implements CvIface {}
class CvSelfy {
    function m(self ...$a) { return count($a); }
}
function cvClass(CvBase ...$a) { return count($a); }
function cvLead(CvBase $first, CvBase ...$rest) {}
function cvIface(CvIface ...$a) {}
function cvNullable(?CvBase ...$a) { return count($a); }
function cvTrue(true ...$a) {}
function cvIter(iterable ...$a) { return count($a); }

// Positional elements: numbered by their own call position.
try { cvClass(new CvOther);            } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvClass(new CvSub, new CvOther); } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvClass(new CvBase, 5);          } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvIface(new CvImpl, "s");        } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { (new CvSelfy)->m(new CvSelfy, 1);} catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvTrue(true, false);             } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Named elements: max(positional, declared formals) + 1.
try { cvClass(x: 1);                   } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvLead(new CvSub, x: 5);         } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvLead(first: new CvSub, x: 5);  } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { cvTrue(true, x: false);          } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Valid calls still bind: subclasses, nulls through a nullable hint,
// iterables, named keys preserved.
var_dump(cvClass(new CvSub, new CvBase, x: new CvSub));
var_dump(cvNullable(null, new CvBase, x: null));
var_dump((new CvSelfy)->m(new CvSelfy));
var_dump(cvIter([1], [2]));
echo "ok\n";
?>
--EXPECT--
cvClass(): Argument #1 must be of type CvBase, CvOther given
cvClass(): Argument #2 must be of type CvBase, CvOther given
cvClass(): Argument #2 must be of type CvBase, int given
cvIface(): Argument #2 must be of type CvIface, string given
CvSelfy::m(): Argument #2 must be of type CvSelfy, int given
cvTrue(): Argument #2 must be of type true, false given
cvClass(): Argument #1 must be of type CvBase, int given
cvLead(): Argument #2 must be of type CvBase, int given
cvLead(): Argument #2 must be of type CvBase, int given
cvTrue(): Argument #2 must be of type true, false given
int(3)
int(3)
int(1)
int(2)
ok
--CLEAN--
<?php
