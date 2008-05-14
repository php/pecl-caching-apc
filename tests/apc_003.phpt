--TEST--
APC: apc_store/fetch with objects
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

class foo { }
$foo = new foo;
print_r($foo);
apc_store('foo',$foo);
unset($foo);
$bar = apc_fetch('foo');
print_r($bar);
$bar->a = true;
print_r($bar);

class bar extends foo
{
	public    $pub = 'bar';
	protected $pro = 'bar';
	private   $pri = 'bar'; // we don't see this, we'd need php 5.1 new serialization
	
	function __construct()
	{
		$this->bar = true;
	}
	
	function change()
	{
		$this->pri = 'mod';
	}
}

class baz extends bar
{
	private $pri = 'baz';

	function __construct()
	{
		parent::__construct();
		$this->baz = true;
	}
}

$baz = new baz;
print_r($baz);
$baz->change();
print_r($baz);
apc_store('baz', $baz);
unset($baz);
print_r(apc_fetch('baz'));

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
foo Object
(
)
foo Object
(
)
foo Object
(
    [a] => 1
)
baz Object
(
    [pri%sprivate] => baz
    [pub] => bar
    [pro:protected] => bar
    [pri%sprivate] => bar
    [bar] => 1
    [baz] => 1
)
baz Object
(
    [pri%sprivate] => baz
    [pub] => bar
    [pro:protected] => bar
    [pri%sprivate] => mod
    [bar] => 1
    [baz] => 1
)
baz Object
(
    [pri%sprivate] => baz
    [pub] => bar
    [pro:protected] => bar
    [pri%sprivate] => mod
    [bar] => 1
    [baz] => 1
)
===DONE===
