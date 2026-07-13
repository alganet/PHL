--TEST--
Uncaught exception from __toString inside a concat halts execution (exit 255)
--DESCRIPTION--
Pre-fix, the engine printed the uncaught report and KEPT EXECUTING with the
"Object" fallback string (silent wrong answer, exit 0). The fatal report
format differs per engine (%A), but both must stop before the echo and both
must print the pre-throw marker.
--FILE--
<?php
class UncStrHalt { public function __toString(): string { throw new Exception("halt"); } }
echo "before\n";
$s = "a" . new UncStrHalt();
echo "resumed: ", $s, "\n";
?>
--EXPECTF--
before
%AUncaught Exception: halt%A
