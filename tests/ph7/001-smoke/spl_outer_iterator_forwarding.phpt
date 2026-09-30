--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A dual iterator forwards a method it does not have to the one it wraps
--FILE--
<?php
/* php's spl_dual_it_call_method: everything that extends IteratorIterator hands an
 * unknown method to its inner iterator. Symfony's Finder is built on it -- an
 * ExcludeDirectoryFilterIterator calls $this->getFilename() and means the
 * RecursiveDirectoryIterator's -- so composer's dump-autoload needs it. */
class SoiInner extends ArrayIterator
{
	public function hello($a = 'x') { return "hello:$a"; }
	public function byRef(&$r) { $r = 99; return 'wrote'; }
	protected function hidden() { return 'no'; }
}
function soiTry(string $label, callable $fn)
{
	echo str_pad($label, 26), ': ';
	try { var_export($fn()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(); }
	echo "\n";
}

$soiForwarders = [
	'IteratorIterator'       => fn () => new IteratorIterator(new SoiInner([1])),
	'FilterIterator'         => fn () => new SoiFilter(new SoiInner([1])),
	'CallbackFilterIterator' => fn () => new CallbackFilterIterator(new SoiInner([1]), fn () => true),
	'LimitIterator'          => fn () => new LimitIterator(new SoiInner([1])),
	'CachingIterator'        => fn () => new CachingIterator(new SoiInner([1])),
	'NoRewindIterator'       => fn () => new NoRewindIterator(new SoiInner([1])),
	'InfiniteIterator'       => fn () => new InfiniteIterator(new SoiInner([1])),
	'RegexIterator'          => fn () => new RegexIterator(new SoiInner([1]), '/1/'),
];
class SoiFilter extends FilterIterator { public function accept(): bool { return true; } }
foreach ($soiForwarders as $soiName => $soiMake) {
	$soiIt = $soiMake();
	soiTry($soiName, fn () => $soiIt->hello('q'));
	soiTry("$soiName undefined", fn () => $soiIt->nope());
}

/* Not dual iterators: they forward nothing. */
soiTry('RecursiveIteratorIterator', fn () => (new RecursiveIteratorIterator(new RecursiveArrayIterator([1])))->hello());
soiTry('MultipleIterator', fn () => (new MultipleIterator())->hello());
/* AppendIterator has no inner until something is appended. */
$soiAp = new AppendIterator();
soiTry('AppendIterator empty', fn () => $soiAp->hello());
$soiAp->append(new SoiInner([1]));
soiTry('AppendIterator filled', fn () => $soiAp->hello('z'));

/* The forward is a real call: by-reference parameters and defaults both work. */
$soiIi = new IteratorIterator(new SoiInner([1]));
$soiVar = 1;
soiTry('by-reference', fn () => $soiIi->byRef($soiVar));
echo 'by-reference wrote: ', var_export($soiVar, true), "\n";
soiTry('default argument', fn () => $soiIi->hello());
soiTry('protected inner method', fn () => $soiIi->hidden());
/* An inner that has no such method reports the OUTER class. */
soiTry('plain inner', fn () => (new IteratorIterator(new ArrayIterator([1])))->hello());
/* php's forwarding is an internal handler, not a declared __call. */
echo 'method_exists __call: ', var_export(method_exists('IteratorIterator', '__call'), true), "\n";
echo 'get_class_methods has __call: ',
	var_export(in_array('__call', get_class_methods('IteratorIterator'), true), true), "\n";
/* A real inner method still wins over one the OUTER class declares. */
$soiFilter = new SoiFilter(new SoiInner([1]));
soiTry('outer wins', fn () => get_class($soiFilter->getInnerIterator()));
?>
--EXPECT--
IteratorIterator          : 'hello:q'
IteratorIterator undefined: Error: Call to undefined method IteratorIterator::nope()
FilterIterator            : 'hello:q'
FilterIterator undefined  : Error: Call to undefined method SoiFilter::nope()
CallbackFilterIterator    : 'hello:q'
CallbackFilterIterator undefined: Error: Call to undefined method CallbackFilterIterator::nope()
LimitIterator             : 'hello:q'
LimitIterator undefined   : Error: Call to undefined method LimitIterator::nope()
CachingIterator           : 'hello:q'
CachingIterator undefined : Error: Call to undefined method CachingIterator::nope()
NoRewindIterator          : 'hello:q'
NoRewindIterator undefined: Error: Call to undefined method NoRewindIterator::nope()
InfiniteIterator          : 'hello:q'
InfiniteIterator undefined: Error: Call to undefined method InfiniteIterator::nope()
RegexIterator             : 'hello:q'
RegexIterator undefined   : Error: Call to undefined method RegexIterator::nope()
RecursiveIteratorIterator : Error: Call to undefined method RecursiveIteratorIterator::hello()
MultipleIterator          : Error: Call to undefined method MultipleIterator::hello()
AppendIterator empty      : Error: Call to undefined method AppendIterator::hello()
AppendIterator filled     : 'hello:z'
by-reference              : 'wrote'
by-reference wrote: 1
default argument          : 'hello:x'
protected inner method    : 'no'
plain inner               : Error: Call to undefined method IteratorIterator::hello()
method_exists __call: false
get_class_methods has __call: false
outer wins                : 'SoiInner'
