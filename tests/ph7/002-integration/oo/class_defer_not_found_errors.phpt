--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declaring against a truly missing parent/interface/trait throws php's catchable not-found Error
--DESCRIPTION--
A deferred declaration whose dependency never materializes throws the catchable
`Class/Interface/Trait "X" not found` Error at the declaration point (php's
runtime declaration model), and execution continues after a catch. Referencing
a deferred class BEFORE its declaration statement also matches php.
--FILE--
<?php
try { class DcBad1 extends \Dep\DcNope {} } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { class DcBad2 implements \Dep\DcNopeIf {} } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { class DcBad3 { use \Dep\DcNopeTrait; } } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $x = new class extends \Dep\DcNope {}; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $pre = new DcLate; } catch (Error $e) { echo "pre: ", $e->getMessage(), "\n"; }
spl_autoload_register(function ($cls) {
    if ($cls === 'Dep\DcBase') {
        eval('namespace Dep; class DcBase { public function b() { return "b"; } }');
    }
});
class DcLate extends \Dep\DcBase { }
echo (new DcLate)->b(), "\n";
echo "done\n";
?>
--EXPECT--
Error: Class "Dep\DcNope" not found
Error: Interface "Dep\DcNopeIf" not found
Error: Trait "Dep\DcNopeTrait" not found
Error: Class "Dep\DcNope" not found
pre: Class "DcLate" not found
b
done
--CLEAN--
<?php
