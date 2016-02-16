<?php

require_once('Controller.inc');
require_once('Helper.inc');

/**
 * Core class for expanded config.
 *
 * <pre>
 * plugin name="config" table="sys_config" expander="sys_expand"
 * </pre>
 *
 * @property-read string $context
 * @table `config` (
 * `id` int(10) NOT NULL AUTO_INCREMENT,
 * `parent` varchar(32) NOT NULL DEFAULT '',
 * `name` varchar(32) NOT NULL DEFAULT '',
 * `type` varchar(32) NOT NULL DEFAULT '',
 * `value` varchar(255) NOT NULL DEFAULT '',
 * `text` text,
 * PRIMARY KEY (`id`),
 * UNIQUE KEY `uniq` (`parent`,`name`),
 * KEY `IX_parent` (`parent`),
 * KEY `IX_name` (`name`) ) DEFAULT CHARSET=utf8;
 *
 * @table `expander` (
 * `id` int(10) NOT NULL AUTO_INCREMENT,
 * `owner` int(10) NOT NULL,
 * `parent` varchar(32) NOT NULL,
 * `name` varchar(255) NOT NULL,
 * `type` varchar(32) NOT NULL,
 * `value` varchar(255) NOT NULL,
 * `text` blob,
 * `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
 * PRIMARY KEY (`id`),
 * UNIQUE KEY `uniq` (`owner`,`parent`,`name`),
 * KEY `IX_owner` (`owner`),
 * KEY `IX_parent` (`parent`),
 * KEY `IX_name` (`name`) ) DEFAULT CHARSET=utf8;
 */
class config
        extends \Controller
        implements \ArrayAccess, \Countable, \IteratorAggregate {
    private $attrs = null;
    private $_config = null;
    private $_expand = null;

    public function getIterator() {
        return new \ArrayIterator((array)$this->_config);
    }
    public function count() {
        return count((array)$this->_config);
    }
    public function offsetSet( $offset, $value ) {
        if( $offset !== null ) {
            $this->set($offset, $value);
        }
    }
    public function offsetExists( $offset ) {
        if( ($offset !== null) && $this->_context ) {
            return isset($this->_config[$offset]);
        }
        return(false);
    }
    public function offsetUnset( $offset ) {
        if( $offset !== null ) {
            $this->set($offset, null);
        }
    }
    public function offsetGet( $offset ) {
        if( ($offset !== null) && $this->_context ) {
            return(isset($this->_config[$offset]) ? $this->_config[$offset] : null);
        }
        return(null);
    }
  /**
   * Read-only properties.
   * 
   * @param string $name
   */
  public function __get( $name ) {
    switch( $name ) {
      case 'context':
        return($this->_context);
      default:
        return parent::__get($name);
    }
  }
    /**
     * Convert database typed value to PHP.
     * 
     * @param array $rec Typed value as array
     * @return mixed Resulted value
     */
    public function getTyped( $rec ) {
        $value = $rec['value'] ? $rec['value'] : $rec['text'];
        switch( $rec['type'] ) {
            case 'onoff':
                return filter_var($value, FILTER_VALIDATE_BOOLEAN);
            case 'array':
                return parse_ini_string($value, true);
            case 'xml':
                return simplexml_load_string($value);
        }
        return($value);
    }
    /**
     * Restore config from session or read current config from database.
     * 
     * @param array $args Controller attributes
     */
    function __construct( $args = null ) {
        $this->attrs = (array) $args;
        $this->database = self::uses('database');
        if( !($this->_config = \Helper::getVar($_SESSION, 'config', array())) ) {
            // base config
            if( $this->attrs['table'] && ($config = $this->database->getAll('SELECT * FROM ?n', $this->attrs['table'])) ) {
                foreach( $config as $rec ) {
                    $this->_config[$rec['parent']][$rec['name']] = $this->getTyped($rec);
                }
            }
        }
        if( !($this->_expand = \Helper::getVar($_SESSION, 'expand', array())) ) {
            // expanded config
            if( $this->attrs['expander'] && ($config = $this->database->getAll('SELECT * FROM ?n WHERE `owner`=?i', $this->attrs['expander'], $this->security->uid)) ) {
                foreach( $config as $rec ) {
                    $this->_config[$rec['parent']][$rec['name']] = $this->_expand[$rec['parent']][$rec['name']] = $this->getTyped($rec);
                }
            }
        }
        register_shutdown_function(array( &$this, 'done' ));
    }
    /**
     *  Store current config & expand into session.
     */
    public function done() {
        $_SESSION['config'] = $this->_config;
        $_SESSION['expand'] = $this->_expand;
    }
    /**
     * Conventional function to forget stored settings.
     *
     * @return bool true (for chaining)
     */
    public function forget() {
        $this->_config = null;
        $this->_expand = null;
        if( isset($_SESSION['config']) ) {
            unset($_SESSION['config']);
        }
        if( isset($_SESSION['expand']) ) {
            unset($_SESSION['expand']);
        }
        return(true);
    }
    /**
     * Invoking.
     * 
     * Create context dependent branch of basic config.
     *
     * @return $this
     */
    public function __invoke( $args = null ) {
        if( !$this->_context && isset($args['context']) ) {
            $this->_context = $args['context'];
            if( isset($this->_config[$this->_context]) ) {
                $this->_config = $this->_config[$this->_context];
            } else {
                $this->_config = array();
            }
            if( isset($this->_expand[$this->_context]) ) {
                $this->_expand = $this->_expand[$this->_context];
            } else {
                $this->_expand = array();
            }
        }
        return($this);
    }
    /**
     *  Get named value.
     *
     *  @param string $key Element name
     *  @param mixed $default Default value if not exists
     *  @return mixed Current value or $default or null if not in context
     */
    public function get( $key, $default = null ) {
        if( $this->_context ) {
            return( isset($this->_config[$key]) ? $this->_config[$key] : $default );
        }
        return(null);
    }
    /**
     * 	Update database expander or config in current context.
     *
     * 	@param string $name Key name
     * 	@param string $valueString representation of value
     * 	@param string$ type Known type for value
     *  @return bool true on updated | false on error
     */
    public function update( $name, $value, $type ) {
        if( strlen($value) > 255 ) {
            $text = $value;
            $val = '';
        } else {
            $text = '';
            $val = $value;
        }
        $ret = 0;
        if( array_key_exists($name, $this->_expand) ) {
            if( $this->database->query('UPDATE ?n SET (`type`,`value`,`text`) VALUES (?s,?s,?s) WHERE `owner`=?i, `parent`=?s, `name`=?s', $this->attrs['expand'], $type, $val, $text, $this->security->uid, $this->_context, $name) ) {
                $this->_expand[$name] = $value;
                $ret = $this->database->numRows();
            }
        } elseif( $this->security->uid < 0 ) {
            if( $this->database->query('UPDATE ?n (`type`,`value`,`text`) VALUES (?s,?s,?s) WHERE `parent`=?s AND `name`=?s', $this->attrs['table'], $type, $val, $text, $this->_context, $name) ) {
                $ret = $this->database->numRows();
            }
        }
        return($ret > 0);
    }
    /**
     *  Set named value.
     *
     *  @param string $name Element name
     *  @param mixed $value Value
     *  @return mixed new value | null not in context or database error
     */
    public function set( $name, $value ) {
        if( $this->_context ) {
            if( is_object($value) ) {
                if( !$this->security->uid || $this->update($name, $value->asXML(), 'xml') ) {
                    return( $this->_config[$name] = $value );
                }
            } elseif( is_array($value) ) {
                if( !$$this->security->uid || $this->update($name, Helper::arr2ini($value), 'array') ) {
                    return( $this->_config[$name] = $value );
                }
            } elseif( is_bool($value) ) {
                if( !$this->security->uid || $this->update($name, $value ? 'on' : 'off', 'onoff') ) {
                    return( $this->_config[$name] = $value );
                }
            } else {
                if( !$this->security->uid || $this->update($name, (string) $value, 'string') ) {
                    return( $this->_config[$name] = (string) $value );
                }
            }
        }
        return( null );
    }
    /**
     * Get expanded values as list.
     * 
     * @param int $uid User id (current if omitted)
     * @return array Named list of settings
     */
    public function getExpand( $uid = null ) {
        $public = 0;
        $ret = array();
        if( $uid === null ) {
            $uid = $this->security->uid;
        }
        if( $uid != $this->security->uid ) {
            $public = $where['public'] = 1;
        }
        $where['owner'] = $uid;
        $where['parent'] = $this->context;
        if( $config = $this->database->getAll('SELECT * FROM ?n WHERE ?&', $this->attrs['expander'], $where) ) {
            foreach( $config as $rec ) {
                unset($rec['parent']);
                unset($rec['owner']);
                $rec['key'] = $key = $rec['name'];
                $rec['name'] = '<language name="'.$key.'" />';
                $rec['value'] = $this->getTyped($rec);
                if( $public ) {
                    unset($rec['public']);
                }
                unset($rec['text']);
                $ret[$key] = $rec;
            }
        }
        return($ret);
    }
}

?>