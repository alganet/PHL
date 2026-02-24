--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex inheritance test
--FILE--
<?php
class Animal {
    protected $name;
    
    public function __construct($name) {
        $this->name = $name;
    }
    
    public function speak() {
        return "Some sound";
    }
    
    public function getName() {
        return $this->name;
    }
}

class Dog extends Animal {
    public function speak() {
        return "Woof!";
    }
    
    public function fetch() {
        return $this->name . " fetches the ball";
    }
}

class Cat extends Animal {
    public function speak() {
        return "Meow!";
    }
    
    public function climb() {
        return $this->name . " climbs the tree";
    }
}

$dog = new Dog("Buddy");
$cat = new Cat("Whiskers");

echo $dog->getName() . " says: " . $dog->speak() . "\n";
echo $dog->fetch() . "\n";
echo $cat->getName() . " says: " . $cat->speak() . "\n";
echo $cat->climb() . "\n";

// Test instanceof
echo "Dog instanceof Animal: " . ($dog instanceof Animal ? "yes" : "no") . "\n";
echo "Cat instanceof Animal: " . ($cat instanceof Animal ? "yes" : "no") . "\n";
?>
--EXPECT--
Buddy says: Woof!
Buddy fetches the ball
Whiskers says: Meow!
Whiskers climbs the tree
Dog instanceof Animal: yes
Cat instanceof Animal: yes
--CLEAN--
<?php
unset($dog, $cat);
