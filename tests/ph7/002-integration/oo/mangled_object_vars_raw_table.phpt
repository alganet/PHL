--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_mangled_object_vars reports the RAW property table: no visibility screen, no get hook, no cast handler
--FILE--
<?php
function show(string $label, object $o): void {
    $keys = array_map(fn($k) => str_replace("\0", '@', (string)$k),
                      array_keys(get_mangled_object_vars($o)));
    echo $label, ': ', json_encode($keys), "\n";
}

enum MovSuit: string { case Hearts = 'H'; }

class MovHooks {
    public int $backed = 7 { get => $this->backed + 100; }
    public int $virtual { get => 42; }
    private string $secret = 's';
}
foreach (get_mangled_object_vars(new MovHooks) as $k => $v) {
    echo str_replace("\0", '@', $k), ' => ', var_export($v, true), "\n";
}

show('ArrayObject', new ArrayObject([1, 2]));
var_dump(array_keys((array)new ArrayObject([1, 2])));
show('Exception', new Exception('m'));
show('stdClass', (object)['a' => 1]);
show('enum case', MovSuit::Hearts);
?>
--EXPECT--
backed => 7
@MovHooks@secret => 's'
ArrayObject: []
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(1)
}
Exception: ["@*@message","@Exception@string","@*@code","@*@file","@*@line","@Exception@trace","@Exception@previous"]
stdClass: ["a"]
enum case: ["name","value"]
--CLEAN--
<?php
