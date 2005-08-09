--TEST--
APC: apc_store/fecth with objects
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
var_dump($foo);
apc_store('foo',$foo);
$bar = apc_fetch('foo');
var_dump($bar);
$bar->a = true;
var_dump($bar);

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
===DONE===
