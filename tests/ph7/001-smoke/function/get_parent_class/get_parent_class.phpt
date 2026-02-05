--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_parent_class builtin basic checks
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
if (!function_exists('get_parent_class')) { echo "skip: function not available\n"; }
?>
--FILE--
<?php
class P {}
class C extends P {}
echo get_parent_class('C') === 'P' ? "ok\n" : "fail\n";
echo get_parent_class(new C) === 'P' ? "ok\n" : "fail\n";
echo get_parent_class('P') === false ? "ok\n" : "fail\n";
function check_in_context(){
    class InnerP{}
    class InnerC extends InnerP{
        public static function who(){ echo get_parent_class() === 'InnerP' ? "ok\n":"fail\n"; }
    }
    InnerC::who();
}
check_in_context();
?>
--EXPECT--
ok
ok
ok
ok
--CLEAN--
<?php

