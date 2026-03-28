--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace with extends, implements, and instanceof
--FILE--
<?php
namespace App\Contracts;
interface Greetable {
    function greet();
}

namespace App\Base;
class BaseModel {
    function type() { return "base"; }
}

namespace App\Models;
use App\Contracts\Greetable;
use App\Base\BaseModel;

class User extends BaseModel implements Greetable {
    function greet() { return "Hello, I am a " . $this->type(); }
}

$u = new User();
echo $u->greet(), "\n";
echo ($u instanceof User) ? "is User" : "not User", "\n";
echo ($u instanceof \App\Base\BaseModel) ? "is BaseModel" : "not BaseModel", "\n";
echo ($u instanceof Greetable) ? "is Greetable" : "not Greetable", "\n";
?>
--EXPECT--
Hello, I am a base
is User
is BaseModel
is Greetable
--CLEAN--
<?php
