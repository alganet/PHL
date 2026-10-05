--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
register_shutdown_function() refuses an uncallable callback when it is registered
--FILE--
<?php
class P {
    private function priv() {}
    public function pub() { echo "pub\n"; }
    static function st() { echo "st\n"; }
}
function t($c) {
    try {
        var_dump(register_shutdown_function($c, 1));
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
t('nope_fn');
t(['P', 'nope']);
t(5);
t(null);
t([1, 2]);
t('P::pub');
t([new P, 'priv']);
t(new stdClass);
t('P::st');
t([new P, 'pub']);
t(fn($x) => print("arrow $x\n"));
echo "end\n";
--EXPECT--
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, function "nope_fn" not found or invalid function name
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, class P does not have a method "nope"
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, no array or string given
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, no array or string given
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, first array member is not a valid class name or object
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, non-static method P::pub() cannot be called statically
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, cannot access private method P::priv()
register_shutdown_function(): Argument #1 ($callback) must be a valid callback, no array or string given
NULL
NULL
NULL
end
st
pub
arrow 1
