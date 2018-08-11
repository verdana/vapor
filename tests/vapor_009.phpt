--TEST--
Check for vapor Engine::make()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$v = new Vapor\Engine('/tmp', 'php');
$tpl = $v->make('home');

var_dump($tpl);
?>
--EXPECT--
object(Vapor\Template)#2 (0) {
}
