--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
After `new`, a property is a class NAME and the parentheses are the constructor's
--DESCRIPTION--
php's class expression after `new` is a `new_variable`, which has no call in it:
`new $config->defType()` reads the property and the parentheses that follow are
always the CONSTRUCTOR's argument list. PHL compiled the property fetch into a
METHOD call, so the line died on `Call to undefined method stdClass::defType()`
— which is how nette/di builds every service it defines, and it is what stopped
phpstan.phar once the parse gaps in front of it were closed.
Two halves: the OP_MEMBER stays a property READ (the exception the explicitly
parenthesised `($o->p)()` already had), and the trailing OP_CALL is folded into
OP_NEW like any other constructor list — the fold read the opcode alone and kept
the call, so the property's VALUE was then invoked as a function.
--FILE--
<?php
class NcepMade {
    public $n;
    public function __construct($n = 'def') { $this->n = $n; }
    public function who() { return 'NcepMade'; }
}
class NcepFact {
    public $cls = 'NcepMade';
    public static $scls = 'NcepMade';
    public array $map = ['k' => 'NcepMade'];
    public $inner;
    public function make() { return new NcepMade('via-method'); }
}
$ncep_f = new NcepFact;
$ncep_f->inner = new NcepFact;

echo get_class($ncep_f->make()), ' ', $ncep_f->make()->n, "\n";   // a real method call
echo get_class(new $ncep_f->cls), "\n";                            // no parentheses at all
echo get_class(new $ncep_f->cls()), "\n";                          // the shape that failed
echo (new $ncep_f->cls('arg'))->n, "\n";                           // ...with constructor args
echo get_class(new $ncep_f->inner->cls()), "\n";                   // two links
echo get_class(new NcepFact::$scls()), "\n";                       // a static property
echo get_class(new $ncep_f->map['k']()), "\n";                     // a subscript on a property
$ncep_v = 'NcepMade';
echo get_class(new $ncep_v()), "\n";                               // a plain variable
echo get_class(new ('NcepMade')()), "\n";                          // a parenthesised expression
echo (new NcepMade())->who(), "\n";                                // php 8.4: chain off new

$ncep_objs = [new NcepFact];
echo get_class(new $ncep_objs[0]->cls()), "\n";                    // subscript, then property
$ncep_args = ['spread'];
echo (new $ncep_f->cls(...$ncep_args))->n, "\n";                   // a spread constructor list
echo (new $ncep_f->cls(n: 'named'))->n, "\n";                      // a named argument
?>
--EXPECT--
NcepMade via-method
NcepMade
NcepMade
arg
NcepMade
NcepMade
NcepMade
NcepMade
NcepMade
NcepMade
NcepMade
spread
named
--CLEAN--
<?php
unset($ncep_f, $ncep_v, $ncep_objs, $ncep_args);
