--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class_alias() autoloads the class it is aliasing, and names what it could not find
--FILE--
<?php
// Print diagnostics without the file path so the expectation is portable; the
// redeclare sentence names a file and a line, and only its head is asserted.
set_error_handler(function ($n, $s) {
    $head = strstr($s, ' (previously declared in', true);
    echo "[E$n] ", $head === false ? $s : $head . ' (previously declared in …)', "\n";
    return true;
});
spl_autoload_register(function ($c) {
    echo "autoload($c)\n";
    if ($c === 'CaaLazy') {
        eval('class CaaLazy { public static function who() { return "CaaLazy"; } }');
    } elseif ($c === 'CaaLazyIface') {
        eval('interface CaaLazyIface {}');
    }
});
// The ordinary shape of a compatibility shim: alias a class the AUTOLOADER owns.
// php's $autoload defaults to true and applies to the SOURCE name.
var_dump(class_alias('CaaLazy', 'CaaAlias'));
var_dump(class_exists('CaaAlias', false));
var_dump(CaaAlias::who());
var_dump((new CaaAlias) instanceof CaaLazy);
// A leading '\' on either name is the global anchor and is not part of it.
var_dump(class_alias('\\CaaLazy', '\\CaaAnchored'), class_exists('CaaAnchored', false));
// An interface aliases the same way.
var_dump(class_alias('CaaLazyIface', 'CaaIfaceAlias'), interface_exists('CaaIfaceAlias', false));
// $autoload = false refuses to look, and php names what it could not find.
var_dump(class_alias('CaaNeverLoaded', 'CaaNope', false));
// ...and with autoload on, the loader runs and the name is still not there.
var_dump(class_alias('CaaStillMissing', 'CaaNope2'));
// An alias name that is already taken is refused, and php's sentence names the
// file and line of the class being ALIASED, not of the name already in the way.
class CaaTaken {}
var_dump(class_alias('CaaTaken', 'CaaAlias'));
?>
--EXPECT--
autoload(CaaLazy)
bool(true)
bool(true)
string(7) "CaaLazy"
bool(true)
bool(true)
bool(true)
autoload(CaaLazyIface)
bool(true)
bool(true)
[E2] Class "CaaNeverLoaded" not found
bool(false)
autoload(CaaStillMissing)
[E2] Class "CaaStillMissing" not found
bool(false)
[E2] Cannot redeclare class CaaAlias (previously declared in …)
bool(false)
