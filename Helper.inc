<?php

/**
 * Helper class contains common utilities as the static functions.
 */
class Helper {
  /**
   * Object to Array
   *
   *  @param object $obj
   *  @param object $remove Remove specified keys
   *  @return array
   */
  static function obj2arr( $obj, $remove = null ) {
    if( is_object($obj) ) {
      $elem = (array) $obj;
    } else {
      $elem = $obj;
    }
    if( is_array($elem) ) {
      if( $remove ) {
          unset($elem['@attributes']);
      }
      return array_map(__METHOD__, $elem);
    }
    return $elem;
  }
  /**
   * Array to Object
   *
   * @param array $arr  
   * @return object
   */
  static function arr2obj( $arr ) {
    return(json_decode(json_encode($arr)));
  }
  /**
   * Array to .INI formatted text
   *
   * @parameter array $a  
   * @parameter array $parent  
   * @return string
   */
  static function arr2ini( array $a, array $parent = array() ) {
    $out = array();
    foreach( $a as $k => $v ) {
      if( is_array($v) ) {
        $parent[] = $k;
        $out[] = '['.join('.', $parent).']'.PHP_EOL;
        $out[] = arr2ini($v, $parent);
      } else {
        $out[] = "$k=$v";
      }
    }
    return implode(PHP_EOL, $out);
  }
  /**
   * .INI formatted text to Array
   *
   * @param string $ini
   * @return array
   */
  static function ini2arr( $ini ) {
    return(parse_ini_string($ini, true));
  }
  /**
   * Get named value from array or default
   *
   * @param mixed $from Array or Object
   * @param string $key 
   * @param mixed $dflt Default value
   * @return mixed
   */
  static function getVar( $from, $key, $dflt = null ) {
    $from = (array) $from;
    if( isset($from[$key]) ) {
      return($from[$key]);
    }
    return($dflt);
  }
  /**
   * Get named value from array or default and unset it
   *
   * @param mixed $from Array or Object
   * @param string $key 
   * @param mixed $dflt
   * @return mixed
   */
  static function eliminate( &$from, $key, $dflt = null ) {
    $afrom = (array) $from;
    if( isset($afrom[$key]) ) {
      $ret = $afrom[$key];
      if( is_array($from) ) {
        unset($from[$key]);
      } else {
        unset($from->$key);
      }
      return($ret);
    } else {
      return($dflt);
    }
  }
  /**
   * Generate random unique string
   *
   * @param int $minlen 
   * @parameter int $maxlen
   * @return string
   */
  static function genPass( $minlen = 8, $maxlen = 16 ) {
    $password = '';
    $pass_len = mt_rand($minlen, $maxlen);
    for( $loop = 0; $loop < $pass_len; $loop++ ) {
      switch( mt_rand(0, 2) ) {
        case 0: $password .= mt_rand(0, 9);
          break; // Number (0-9)
        case 1: $password .= chr(mt_rand(97, 122));
          break; // Alpha Lower (a-z)
        case 2: $password .= chr(mt_rand(65, 90));
          break; // Alpha Upper (A-Z)
      }
    }
    return $password;
  }
  /**
   * Get attributes of XML node as array.
   * 
   * @param mixed $el DOMNode or SimpleXMLElement 
   * @return array|null
   */
  static function attr2arr( $el ) {
    if( $el instanceof DOMNode ) {
      if( !$el->attributes ) {
        return(null);
      }
      $xml = simplexml_import_dom($el);
    } elseif( !($xml = $el) || !($xml instanceof SimpleXMLElement) ) {
      return(null);
    }
    $atts = (array) $xml->attributes();
    return isset($atts['@attributes']) ? $atts['@attributes'] : null;
  }
  /**
   * Remove or clear directory recursively.
   * 
   * @param string $dir Directory name
   * @param bool $empty Not remove root on true (just clear).
   * @return bool
   */
  static function rmDir( $dir, $empty = false ) {
    if( !\file_exists($dir) ) {
      return(true);
    }
    if( !is_dir($dir) ) {
      return \unlink($dir);
    }
    foreach( \scandir($dir) as $item ) {
      if( $item == '.' || $item == '..' ) {
        continue;
      }
      if( !self::rmDir($dir.DIRECTORY_SEPARATOR.$item) ) {
        return(false);
      }
    }
    if( !$empty ) {
      return \rmdir($dir);
    } else {
      return(true);
    }
  }
//////////////////////////////////////////////////
// HTML format functions. returns array of string
//////////////////////////////////////////////////
  /**
   * Produce for each element of array formatted value by template using markers for keys.
   * 
   * First or once template used for scalar elements, second - for complex elements.
   * Named marker are used when keys is same.
   *
   *  @param array $arr 
   *  @parameter mixed $templates Array or String
   *  @param array $marker 
   *  @return string[]
   */
  static function show( $arr, $templates, $marker = null ) {
    $arr = (array) $arr;
    $marker = (array) $marker;
    $templates = (array) $templates;
    $scalar = array_shift($templates);
    $complex = array_shift($templates);
    foreach( $arr as $key => $item ) {
      if( is_scalar($item) ) {
        $arr[$key] = sprintf($scalar, $key, $item, empty($marker[$key]) ? '' : $marker[$key]);
      } else {
        $branch = implode(PHP_EOL, self::show($item, array( $scalar, $complex ), empty($marker[$key])
                                  ? $marker : $marker[$key]));
        if( $complex ) {
          $arr[$key] = sprintf($complex, $key, $branch, empty($marker[$key]) ? ''
                            : $marker[$key]);
        }
      }
    }
    return( $arr );
  }
  /**
   * Produce for each element of array string '$key="$value"'
   *
   * @param array $arr 
   * @return string[]
   */
  static function showAttrs( $arr ) {
    return( self::show($arr, '%1$s="%2$s"') );
  }
  /**
   * Produce tag 'option' for each element of array
   *
   * @param array $arr 
   * @param mixed $selected 
   * @return string[]
   */
  static function showOptions( $arr, $selected = null ) {
    return self::show($arr, array( '<option value="%1$s"%3$s><language name="%2$s" /></option>', '<optgroup label="%1$s">%2$s</optgroup>' ), is_array($selected)
                              ? $selected : (is_string($selected) ? array( $selected => ' selected="selected"' )
                                      : null) );
  }
  /**
   * Produce tag 'input'
   *
   * @param array $attr 
   * @param string $type 
   * @param string $name 
   * @param string $value
   * @return string[]
   */
  static function showInput( $attr = null, $type = '', $name = '', $value = '' ) {
    $ret = array();
    if( $div = self::eliminate($attr, 'div', '') ) {
      $ret[] = '<div '.implode(' ', self::showAttrs($div)).'>';
    }
    $value = self::eliminate($attr, 'value', $value);
    $type = self::eliminate($attr, 'type', $type ? $type : (is_bool($value) ? 'checkbox'
                                      : 'text'));
    $name = self::eliminate($attr, 'name', $name ? $name : ($type.'[]'));
    if( $label = self::eliminate($attr, 'label') ) {
      $ret[] = '<label>'.$label;
    }
    switch( $type ) {
      case 'select':
        $selected = self::eliminate($attr, 'selected', null);
        $ret[] = '<select name="'.$name.'" '.implode(' ', self::showAttrs($attr)).'>';
        $ret[] = implode("\n", self::showOptions((array) $value, $selected));
        $ret[] = '</select>';
        break;
      case 'textarea':
        $ret[] = '<textarea name="'.$name.'" '.implode(' ', self::showAttrs($attr)).'>';
        $ret[] = htmlspecialchars(implode("\n", (array) $value));
        $ret[] = '</textarea>';
        break;
      case 'image':
        $ret[] = '<button name="'.$name.'" '.implode(' ', self::showAttrs($attr)).'>';
        $ret[] = '<img src="'.$value.'" />';
        $ret[] = '</button>';
        break;
      default:
        $ret[] = '<input type="'.$type.'" name="'.$name.'"';
        $ret[] = 'value="'.htmlspecialchars($value).'"';
        if( $attr ) {
          $ret[] = implode(' ', self::showAttrs($attr));
        }
        $ret[] = '/>';
    }
    if( $label ) {
      $ret[] = '</label>';
    }
    if( $div ) {
      $ret[] = '</div>';
    }
    return($ret);
  }
  /**
   * Produce view of variable
   *
   * @param mixed $var
   * @param array $attr
   * @return string[]
   */
  static function showTyped( $var, $attr = null ) {
    if( is_object($var) ) {
      return self::showXMLtree($var, self::eliminate($attr, 'editable', false), '');
    } elseif( is_array($var) ) {
      return self::showTree($var, $attr);
    }
    if( $editable = self::eliminate($attr, 'editable', false) ) {
      $name = $attr['name'];
      $attr['label'] = "<language name=\"{$name}\" as=\"span\" />";
      return self::showInput($attr, 'text', $name, $var);
    }
    return array( (string) $var );
  }
  /**
   * Produce tree as specified in attribute 'as' (or 'ul') from array
   *
   * @param array $tree xD 
   * @param array $attr 
   * @return string[]
   */
  static function showTree( $tree, $attr = null ) {
    $ret = array();
    $editable = self::eliminate($attr, 'editable', false);
    $name = self::getVar($attr, 'name');
    if( $tree = self::obj2arr($tree) ) {
      $rootas = empty($attr['as']) ? 'ul' : $attr['as'];
      switch( $rootas ) {
        case 'ul':
        case 'ol':
          $as = 'li';
          break;
        case 'tr':
          $as = 'td';
          break;
        case 'div':
          $as = 'div';
          break;
        default:
          $as = 'span';
      }
      foreach( $tree as $key => $val ) {
        if( $name ) {
          $iname = $name."[{$key}]";
        } else {
          $iname = $key;
        }
        if( is_array($val) ) {
          $sattr = $attr;
          $sattr['name'] = $iname;
          $ret[] = "<{$rootas}>";
          $ret[] = implode(PHP_EOL, self::showTree($val, $sattr));
          $ret[] = "</{$rootas}>";
        } elseif( $editable ) {
          $ret[] = "<{$as}>".implode(' ', self::showInput(array( 'label' => "<language name=\"{$iname}\" as=\"span\" />" ), 'text', $iname, $val))."</{$as}>";
        } else {
          $ret[] = "<{$as}><language name=\"{$iname}\" as=\"span\" />: {$val}</{$as}>";
        }
      }
    }
    return($ret);
  }
  /**
   * Produce menu as 'ul' from array
   *
   * @param array $menu xD 
   * @param array $attr 
   * @return string[]
   */
  static function showMenu( $menu, $attr = null ) {
    $ret = array();
    if( $menu = self::obj2arr($tree) ) {
      $ret[] = '<ul '.implode(' ', self::showAttrs($attr)).'>';
      foreach( $menu as $key => $val ) {
        if( is_array($val) ) {
          $ret[] = '<li><language name="'.$key.'" /></li>';
          $ret[] = implode(PHP_EOL, self::showMenu($val));
        } else {
          $ret[] = '<li><a href="'.$val.'"><language name="'.$key.'" /></a></li>';
        }
      }
      $ret[] = '</ul>';
    }
    return($ret);
  }
  /**
   * Produce XML tree as <ul>
   *
   * @param SimpleXML $xml 
   * @param string $cname 
   * @parameter string $name
   * @param bool $editable
   * @return string[]
   */
  static function showXMLtree( $xml, $cname, $name = null, $editable = false ) {
    $ret = array();
    if( is_array($xml) ) {
      switch( count($xml) ) {
        case 0: return(null);
        case 1: return(self::showXMLtree(reset($xml), $cname, $name, $editable));
        default:
          foreach( $xml as $key => $val ) {
            $ret[] = implode("\n", self::showXMLtree($val, $cname, $name ? $name.'/'.$key
                                      : $key, $editable));
          }
      }
    } elseif( is_object($xml) ) {
      $attr = self::attr2arr($xml);
      $extend = false;
      $keys = explode('/', $name);
      $attr['name'] = $cname.'['.implode('][', $keys).']';

      switch( $xml['type'] ) {
        case 'onoff':
          if( $editable ) {
            $attr['type'] = 'checkbox';
            $attr['value'] = 'on';
            if( filter_var((string) $xml, FILTER_VALIDATE_BOOLEAN) ) {
              $attr['checked'] = 'checked';
            }
          } else {
            $ret[] = filter_var((string) $acfg, FILTER_VALIDATE_BOOLEAN) ? '<language name="enabled" />'
                      : '<language name="disabled" />';
          }
          break;
        case 'password':
          if( $editable ) {
            $attr['type'] = 'password';
            $attr['value'] = '';
          } else {
            $ret[] = (string) $xml ? '<language name="enabled" />' : '<language name="disabled" />';
          }
          break;
        case 'string':
        case 'text':
          if( $editable ) {
            $attr['type'] = 'text';
            $attr['value'] = (string) $xml;
          } else {
            $ret[] = (string) $xml;
          }
          break;
        case 'file':
          $extend = true;
        case 'directory':
          $base = self::getVar($atts, 'base');
          $text = '<span name="dirname">'.$base.'/</span>';
          if( $editable ) {
            $ret = self::showDirTree($base, '', $attr['name'], $select = (string) $xml, $extend);
          } else {
            $ret[] = '<span name="'.($extend ? 'filename' : 'dirname').'">'.(string) $xml.'</span>';
          }
          break;
        case 'array':
          $ret = self::showTree(parse_ini_string((string) $xml), array( 'name' => $attr['name'], 'editable' => $editable ));
          break;
        case 'xml':
        default:
          foreach( $acfg->children() as $tag => $xml ) {
            $ret[] = implode("\n", self::showXMLtree($xml, $cname, $name ? $name.'/'.$tag
                                      : $tag, $editable));
          }
          break;
      }
    } else {
      $ret[] = (string) $xml;
    }
    if( !$ret && isset($attr['type']) ) {
      $ret[] = '<input ';
      $ret[] = implode(' ', self::showAttrs($attr));
      $ret[] = ' />';
    }
    if( $ret ) {
      array_unshift($ret, '<div name="xmlelement"><div name="xmlelementname">'.$name.'</div><div name="xmlelementvalue">');
      $ret[] = '</div></div>';
    }
    return($ret);
  }
  /**
   * Produce dir tree as <ul>
   *
   * @param string $base 
   * @param string $name string
   * @param string $radioname
   * @param string $select
   * @param bool $files
   * @return string[]
   */
  static function showDirTree( $base, $name, $radioname, $select = '', $files = false ) {
    $arr = array();
    $dir = dir($base ? ($name ? $base.'/'.$name : $base) : $name );
    while( false !== ($entry = $dir->read()) ) {
      if( ($entry[0] != '.') && (($isdir = is_dir($entry)) || $files) ) {
        $dname = $name ? $name.'/'.$entry : $entry;
        $checked = ($dname == $select) ? ' checked="checked"' : '';
        $arr[] = '<li name="'.($isdir ? 'dirname' : 'filename').'">';
        if( $files xor $isdir ) {
          $arr[] = '<input type="radio" name="'.$radioname.'"';
          $arr[] = 'value="'.$dname.'"';
          $arr[] = $checked.'>';
          $arr[] = $entry;
          $arr[] = '</input>';
        } else {
          $arr[] = $entry;
        }
        if( $isdir ) {
          $arr[] = implode("\n", self::showDirTree($base, $dname, $radioname, $select, $files));
        }
        $arr[] = '</li>';
      }
    }
    $dir->close();
    if( $arr ) {
      array_unshift($arr, '<ul name="dirtree">');
      $arr[] = '</ul>';
    }
    return($arr);
  }
}