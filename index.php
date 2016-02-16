<?php

/**
 * Entry point to the framework.
 * <code>
 * URL/page[/path][/action].type
 * </code>
 * Default action = 'main'.
 * 
 * Examples:
 * <code>
 *  URL/page.html => page='page', path='', action='main', type='html'
 *  URL/article/add.json => page='article', path='', action='add', type='json'
 *  URL/publication/2015/05/21/show.xml => page='publication', path='2015/05/21', action='show', type='xml'
 *  URL/graphic/b818ac164d8d627f4ffa1de374945eef/image.jpg => page='graphic', path='b818ac164d8d627f4ffa1de374945eef', action='image', type='jpg'
 * </code>
 *
 * @author Iurii Prudius <hardwork.mouse@gmail.com>
 * @example .htaccess 
 */
require_once('Core.inc');
\Core::init(true);
session_start();
\Core::cache('./.cache');

/**
 * Autoloading classes.
 * Echo included file name or error string if file not exists.
 *
 * @param string $class '[namespace\]class'
 */
function loader( $class ) {
  if( file_exists($path = $class . '.inc') || file_exists($path = "plugins/{$class}/controller.inc") ) {
    echo $path;
    include($path);
  } else {
    echo $class . ' class not found';
  }
}

spl_autoload_register('loader');

require_once('Controller.inc');
require_once('Helper.inc');

/**
 * Application controller.
 * 
 * @uses Helper
 */
class Application
  extends Controller {

  /**
   * @param array $args node attributes
   * @return Controller $this
   */
  function __invoke( $args = null ) {
    return($this);  // on
  }

  /**
   * Build view on specified model.
   * 
   * Send headers and buffered output to client
   * Or build new DOMNode view.
   * 
   * @param mixed $model DOMNode or XML file name to load. Find model file from context if omitted.
   * @return bool|DOMNode true on binary sent or false on text sent or processed DOMNode for chaining
   */
  public function view( $model = null ) {
    if( ($view = parent::view($model)) && ($view instanceof DOMNode) ) {
      if( $controller = $this->uses('model\\Application') ) {
        return $controller->view($view);
      }
      $this->sendHeader('Global error', 500);
      echo 'Server misconfigured';
    } elseif( !$view ) {
      $this->sendHeader('Not Found', 404);
      echo 'Data "' . $this->page . ($this->path ? '/' . $this->path : '') . '/' . $this->action . '.' . $this->type . '" not found';
    }
    return(false);
  }

  /**
   * Application processing.
   * 
   * Parent processing then process tags:
   * - var
   * - language 
   * - cookie
   * - header
   * 
   * @param DOMNode $model
   * @return DOMNode|false View from model or false
   */
  function process( $model ) {
    if( ($view = parent::process($model)) && ($node = self::merge($view, $this->type)) ) {
      $this->processNodes($node, 'var');
      $this->processNodes($node, 'language');
      $this->processNodes($node, 'cookie');
      $this->processNodes($node, 'xheader');
    }
    return($view);
  }

  /**
   * Replace 'var' tags to values.
   * 
   * @param DOMElement $el 'var' node
   * @return bool TRUE on OK | FALSE on not defined
   */
  protected function varProcess( $el ) {
    if( ($vnam = $this->useName($el, '')) && isset($this->set[$vnam]) ) {
      $el->nodeValue = (string) $this->set[$vnam];
      return(true);
    }
    return(false);
  }

  /**
   * Replace 'language' tags from `language` plugin.
   * 
   * @param DOMElement $el 'language' node
   * @return bool TRUE
   */
  protected function languageProcess( $el ) {
    if( ($language = $this->language) || ($language = $this->language = $this->uses('language')) ) {
      $txt = $language->translate($el);
      self::removeChildren($el);
      $rep = $el->ownerDocument->createTextNode($txt);
    } elseif( $name = $el->getAttribute('name') ) {
      $rep = $el->ownerDocument->createTextNode($name);
    }
    $el->insertBefore($rep, $el->firstChild);
    return(true);
  }

  /**
   * Set cookie from 'cookie' tag.
   * 
   * @param DOMElement $el 'cookie' node
   * @return bool FALSE
   */
  protected function cookieProcess( $el ) {
    if( $hnam = $el->getAttribute('name') ) {
      $val = $el->nodeValue;
      if( $exp = intval($el->getAttribute('expire')) ) {
        $exp += \time();
      }
      $path = $el->getAttribute('context');
      $domain = $el->getAttribute('domain');
      $secure = !!$el->getAttribute('secure');
      setcookie($hnam, $val, $exp, $path, $domain, $secure);
    }
    return(false);
  }

  /**
   * Send header 'xheader' tag.
   * 
   * @param DOMElement $el 'xheader' node
   * @return bool FALSE
   */
  protected function xheaderProcess( $el ) {
    $val = $el->nodeValue;
    if( ($cod = $el->getAttribute('code')) && is_numeric($cod) ) {
      $this->sendHeader($val, $cod);
    } else {
      $hnam = $el->getAttribute('name');
      $this->sendHeader("{$hnam}: {$val}");
    }
    return(false);
  }

}

//--------------------------------------
ini_set('open_basedir', \realpath('./'));
//--------------------------------------

if( ($core = new Application()) && !($core->view()) ) {
  echo '<!-- ';
  if( $log = $core->observe() ) {
    echo PHP_EOL . implode(PHP_EOL, $log) . PHP_EOL;
  }
  echo $core->total() . ' -->';
}
exit(0);
?>