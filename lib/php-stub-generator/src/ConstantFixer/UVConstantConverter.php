<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\StubUtils\ConstantFixer;

class UVConstantConverter{
    static ?UVConstantConverter $inst=null;
    /** @var array<string, int> */
    private array $consts = [];
    const UV_HEADER = __DIR__ . '/../../../../ext/deps/libcat/deps/libuv/include/uv/errno.h';
    const UV_RE = '/#\s*define\s*UV__(?<name>E[A-Z0-9]+)\s*\((?<value>-\d+)\)/';
    private function __construct()
    {
        if(is_file(static::UV_HEADER)){
            $uvHeader = file_get_contents(static::UV_HEADER);
            preg_match_all(static::UV_RE, $uvHeader, $matches);
            foreach($matches['name'] as $i=>$name){
                // there will be an overwrite on EHOSTDOWN
                $this->consts[$name] =(int)$matches['value'][$i];
            }
        }
    }
    private static function getInst(): static{
        if (!static::$inst){
            static::$inst = new static();
        }
        return static::$inst;
    }
    public static function convert(ConstantDefinitionMap $map):void{
        $that = static::getInst();
        $that->_convert($map);
    }
    private function _convert(ConstantDefinitionMap $map):void{
        if ($map->getOS()!=='Windows'){
            // unix-like: negative real value, if not exist, redefine instead
            foreach($this->consts as $name=>$value){
                $comment = '';
                if($cd = $map[$name]??null){
                    $value = -$cd->value;
                    $comment = $cd->comment;
                }else {
                    $value = $value;
                }
                $map["UV_$name"] = new ConstantDefinition(
                    value: $value,
                    comment: $comment,
                );
            }
        }else{
            // windows: redefined
            foreach($this->consts as $name=>$value){
                $map["UV_$name"] = new ConstantDefinition(
                    value: $value,
                );
            }
        }
    }
}
