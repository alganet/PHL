--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A debug array's mangled key names the class the slot belongs to
--DESCRIPTION--
php reads every key of an object BODY through zend_unmangle_property_name, so a
key of the form "\0*\0name" prints as ["name":protected] and "\0Class\0name" as
["name":"Class":private]. That is how a get_debug_info handler — and a userland
__debugInfo() — says what a plain array key cannot, and it is the only rendering
php's internal classes have for a private slot they still want on screen. PHL
printed the raw NUL-separated bytes instead. The decode is confined to the object
body: `var_dump((array)$obj)` shows the mangled key exactly as it is, in both
engines, because there the key IS the array's key.
--FILE--
<?php
class DbgMangled {
    private $p = 1;
    protected $q = 2;
    public $r = 3;
    public function __debugInfo(): array {
        return [
            "\0DbgMangled\0secret" => 'own private',
            "\0*\0shielded"        => 'protected',
            "plain"                => 'public',
            "\0DbgOther\0borrowed" => 'private elsewhere',
        ];
    }
}

$dbgObj = new DbgMangled;
ob_start(); var_dump($dbgObj); $dbgDump = trim(ob_get_clean());
echo str_replace("\n", ' ', preg_replace('/#\d+ /', '#N ', $dbgDump)), "\n";
ob_start(); print_r($dbgObj); $dbgPrint = trim(ob_get_clean());
echo str_replace("\n", ' ', $dbgPrint), "\n";

/* The (array) cast is NOT an object body: php shows the mangled key raw, and the
 * cast reads the real properties rather than __debugInfo(). */
echo json_encode(array_keys((array)$dbgObj)), "\n";

/* var_export has its own renderer and never consults __debugInfo(). */
echo str_replace("\n", ' ', var_export($dbgObj, true)), "\n";

/* Only a LEADING NUL starts a mangled name: a '*' anywhere else is part of a
 * plain public key. (php also NOTICEs a leading NUL that is not well-formed and
 * prints those bytes raw, which PHL renders the same way without the notice —
 * the dump is built whole here and streamed there, so the notice could not land
 * where php puts it.) */
class DbgOdd {
    public function __debugInfo(): array {
        return ["*\0notmangled" => 'kept whole', "ordinary" => 'no NUL at all'];
    }
}
ob_start(); var_dump(new DbgOdd); $dbgOdd = trim(ob_get_clean());
echo json_encode(str_replace("\n", ' ', preg_replace('/#\d+ /', '#N ', $dbgOdd))), "\n";
--EXPECT--
object(DbgMangled)#N (4) {   ["secret":"DbgMangled":private]=>   string(11) "own private"   ["shielded":protected]=>   string(9) "protected"   ["plain"]=>   string(6) "public"   ["borrowed":"DbgOther":private]=>   string(17) "private elsewhere" }
DbgMangled Object (     [secret:DbgMangled:private] => own private     [shielded:protected] => protected     [plain] => public     [borrowed:DbgOther:private] => private elsewhere )
["\u0000DbgMangled\u0000p","\u0000*\u0000q","r"]
\DbgMangled::__set_state(array(    'p' => 1,    'q' => 2,    'r' => 3, ))
"object(DbgOdd)#N (2) {   [\"*\u0000notmangled\"]=>   string(10) \"kept whole\"   [\"ordinary\"]=>   string(13) \"no NUL at all\" }"
