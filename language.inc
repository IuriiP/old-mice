<?php

require_once('Controller.inc');
require_once('Helper.inc');

/**
 * Core class for expanded language support
 *
 * <pre>
 * plugin name="language" default="en"
 * </pre>
 *
 * @table `lang_{code}` (
 * `id` int(10) NOT NULL AUTO_INCREMENT,
 * `key` varchar(255) NOT NULL DEFAULT '',
 * `subkey` varchar(255) NOT NULL DEFAULT '',
 * `value` varchar(255) NOT NULL DEFAULT '',
 * `text` text,
 * `translator` int(10) NOT NULL DEFAULT 0,
 * PRIMARY KEY (`id`),
 * UNIQUE KEY `uniq` (`key`,`subkey`) 
 * ) DEFAULT CHARSET=utf8;
 */
class language
        extends \Controller {
  private $attrs = null;
  private $_language = null;
  private $table = null;
  private $dictionary = null;
  private $format = null;

  /**
   *  Readonly properties
   */
  public function __get( $name ) {
    switch( $name ) {
      case 'language':
        return($this->_language);
      default:
        return( parent::__get($name) );
    }
  }
  /**
   * Restore language from cookie/request/session or calculate it from browser.
   * 
   * @param array $args Node attributes
   */
  function __construct( $args = null ) {
    $this->attrs = (array) $args;
    $this->database = self::uses('database');
    $this->dictionary = array();
    if( !$this->set(\Helper::getVar(\Helper::getVar($_REQUEST, 'language'), 'use')) ||
            !$this->set(\Helper::getVar($_SESSION, 'language')) ) {
      // not in cookie or request or session
      $langs = explode(',', $_SERVER['HTTP_ACCEPT_LANGUAGE']);
      foreach( $langs as $key => $lang ) {
        $parts = explode(';', $lang);
        if( count($parts) > 0 ) {
          $langs[$key] = $parts[0];
        } else {
          $langs[$key] = null;
        }
      }
      $langs = array_filter($langs);
      $lang = reset($langs);
      do {
        if( empty($this->config[$lang]) ) {
          $lang = null;
        }
      } while( !$lang && ($lang = next($langs)) );
      $this->set($lang ? $lang : $this->attrs['default']);
    }
    register_shutdown_function(array( &$this, 'done' ));
  }
  /**
   * Store current language into session.
   */
  public function done() {
    $_SESSION['language'] = $this->_language;
  }
  /**
   *  Invoking
   *
   *  @return Controller $this
   */
  public function __invoke( $args = null ) {
//    return(null);
    return($this);
  }
  /**
   * Set current language.
   *
   * @param string $lang Language name as IETF language tag.
   * @return string $lang
   */
  public function set( $lang ) {
    if( $lang && !empty($this->config[$lang]) ) {
      setcookie('language[use]', $_SESSION['language'] = $this->_language = $lang, PHP_INT_MAX, '/');
      $this->table = $this->config[$lang];
      if( !($this->dictionary = @parse_ini_file("/i18n/{$lang}/dictionary.ini", true)) ) {
        $this->dictionary = array();
      }
      if( !($this->format = @parse_ini_file("/i18n/{$lang}/format.ini", true)) ) {
        $this->format = array();
      }
      return($lang);
    }
    return(null);
  }
  /**
   * Select element from array by match it's key with value of used child.
   * 
   * Examples:
   * <pre>
   * 0 = default
   * '=17' == 17
   * '<10' <10
   * 'what' contains 'what'
   * '?/3$/' last char is '3'
   * </pre>
   * 
   * @param array $array Array to select
   * @param array $children 
   * @param int $use Used child from list
   * @return mixed Matched element from array
   */
  private function adapt( array $array, $children, $use ) {
    if( $children && (count($children) > $use) && ($val = $children[$use]) ) {
      foreach( $array as $key => $ret ) {
        if( is_string($key) ) {
          $op = $key[0];
          $const = substr($key, 1);
          switch( $op ) {
            case '=':
              if( $val == $const ) {
                return($ret);
              } break;
            case '<':
              if( $val < $const ) {
                return($ret);
              } break;
            case '>':
              if( $val > $const ) {
                return($ret);
              } break;
            case '?':
              if( preg_match('/'.preg_quote($const, '/').'/', $val) ) {
                return($ret);
              } break;
            default:
              if( strpos($val, $key) !== false ) {
                return($ret);
              }
          }
        }
      }
    }
    return reset($array);
  }
  /**
   * Convert formatted text.
   * 
   * Adaptive select element from array and format it by replacing 
   * '?n' (where n=0..9) with text content of nodes from list.
   * 
   * @param string $format String to format
   * @param array $children List of children 
   * @return string Translated text from dictionary or text content of node
   */
  private function convert( $format, $children = null, $use = 0 ) {
    if( is_array($format) ) {
      $branch = $this->adapt($format, $children, $use);
      return $this->convert($branch, $children, $use + 1);
    }

    return \preg_replace_callback('/\?(\d)/', function($matches) use ($children) {
      if( count($children) > $matches[1] ) {
        return $children[$matches[1]];
      }
      return('');
    }, $format);
  }
  /**
   * Translate text from dictionary.
   * 
   * @param DOMElement $node 'language' node
   * @return string Translated text from dictionary or text content of node
   */
  public function translate( $node ) {
    $text = '';
    if( $node ) {
      $text = $node->textContent;
      $children = array();
      foreach( $node->childNodes as $child ) {
        if( XML_COMMENT_NODE != $child->nodeType ) {
          if( 'language' == $child->nodeName ) {
            $new = $child->ownerDocument->createTextNode($this->translate($child));
            $child->parentNode->replaceChild($new, $child);
            $children[] = trim($new->textContent);
          } else {
            $children[] = trim($child->textContent);
          }
        }
      }
      if( ( ($key = $name = $node->getAttribute('name')) || ($key = $text) ) ) {
        // get from dictionary
        if( array_key_exists($key, $this->dictionary) ) {
          $text = $this->convert($this->dictionary[$key], $children);
        } elseif( $name ) {
          $text = $key.' '.implode(' ', $children);
        }
      } else {
        $text = implode(' ', $children);
      }
    }
    return($text);
  }
  /**
   * Select text from database.
   *
   * @param string $key Base key
   * @param string $subkey Text selector
   * @return string Translated text from database
   */
  public function select( $key, $subkey ) {
    if( $row = $this->database->getRow('SELECT `value`,`text` FROM ?n WHERE `key`=?s AND `subkey`=?s', $this->table, $key, $subkey) ) {
      return $row['text'] ? $row['text'] : $row['value'];
    } else {
      return(null);
    }
  }
  /**
   * Append text to database.
   *
   * @param string $key Basic key
   * @param string $subkey Element name
   * @param string $value Tags
   * @param string $text Text
   * @param int $translator Translator uid (0 - automatic)
   * @return string|null Text or null
   */
  public function append( $key, $subkey, $value, $text, $translator = 0 ) {
    if( $this->database->query('INSERT INTO ?n (`key`,`subkey`,`value`,`text`,`translator`) VALUES (?s,?s,?s,?s,?i)'
                    .' ON DUPLICATE KEY UPDATE `value`=?s,`text`=?s,`translator`=?i', $this->table, $key, $subkey, $value, $text, $translator, $value, $text, $translator) ) {
      return($text);
    }
    return(null);
  }
  /**
   * Autotranslate text.
   *
   * @param string $text Text
   * @return string Translated text | null on error
   */
  public function autoTranslate( $text, $to = null, $from = null ) {
    if( !$to ) {
      $to = $this->_language;
    }
    if( !$from ) {
      $from = 'en';
    }
    // Create GT Object
    $gt = new GoogleTranslater();
    // Translate text
    // Usage: GoogleTranslater::translateText(string $text, string $fromLanguage, string $tolanguage, bool $translit = false)
    return $gt->translateText($text, $from, $to);
  }
}

/**
 * GoogleTranslater is a PHP interface for http://translate.google.com/
 * 
 * It sends a request to the google translate service, gets a response and
 * provides the translated text.
 *
 * @author Andrew Kulakov <avk@8xx8.ru>
 * @version 1.0.0
 * @license https://github.com/Andrew8xx8/GoogleTranslater/blob/master/MIT-LICENSE.txt
 * @copyright Andrew Kulakov (c) 2011
 */
class GoogleTranslater {
  /**
   * @var string Some errors
   */
  private $_errors = "";

  /**
   * Constructor
   */
  public function _construct() {
    if( !function_exists('curl_init') )
      $this->_errors = "No CURL support";
  }
  /**
   * Translate text.
   * @param  string $text          Source text to translate
   * @param  string $fromLanguage  Source language
   * @param  string $toLanguage    Destenation language
   * @param  bool   $translit      If true function return transliteration of source text
   * @return string|bool           Translated text or false if exists errors
   */
  public function translateText( $text, $fromLanguage = "en", $toLanguage = "ru", $translit
  = false ) {
    if( empty($this->_errors) ) {
      $result = "";
      for( $i = 0; $i < strlen($text); $i += 1000 ) {
        $subText = substr($text, $i, 1000);
        $response = $this->_curlToGoogle("http://translate.google.com/translate_a/t?client=te&text=".urlencode($subText)."&hl=$toLanguage&sl=$fromLanguage&tl=i$toLanguage&multires=1&otf=1&ssel=0&tsel=0&uptl=ru&sc=1");
        $result .= $this->_parceGoogleResponse($response, $translit);
//                sleep(1);
      }
      return $result;
    } else
      return false;
  }
  /**
   * Translate array.
   * @param  array  $array         Array with source text to translate
   * @param  string $fromLanguage  Source language
   * @param  string $toLanguage    Destenation language
   * @param  bool   $translit      If true function return transliteration of source text
   * @return array|bool            Array  with translated text or false if exists errors
   */
  public function translateArray( $array, $fromLanguage = "en", $toLanguage = "ru", $translit
  = false ) {
    if( empty($this->_errors) ) {
      $text = implode("[<#>]", $array);
      $response = $this->translateText($text, $fromLanguage, $toLanguage, $translit);
      return $this->_explode($response);
    } else
      return false;
  }
  public function getLanguages() {
    if( empty($this->_errors) ) {
      $page = $this->_curlToGoogle('http://translate.google.com/');
      preg_match('%<select[^<]*?tl[^<]*?>(.*?)</select>%is', $page, $match);
      preg_match_all("%<option.*?value=\"(.*?)\">(.*?)</option>%is", $match[0], $languages);
      $result = Array();
      for( $i = 0; $i < count($languages[0]); $i++ ) {
        $result[$languages[1][$i]] = $languages[2][$i];
      }
      return $result;
    } else
      return false;
  }
  public function getLanguagesHTML() {
    if( empty($this->_errors) ) {
      $page = $this->_curlToGoogle('http://translate.google.com/');
      preg_match('%<select[^<]*?tl[^<]*?>(.*?)</select>%is', $page, $match);
      return $match[1];
    } else
      return false;
  }
  public function getErrors() {
    return $this->_errors;
  }
  private function _explode( $text ) {
    $text = preg_replace("%\[\s*<\s*#\s*>\s*\]%", "[<#>]", $text);
    return array_map('trim', explode('[<#>]', $text));
  }
  private function _curlToGoogle( $url ) {
    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, $url);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
    if( isset($_SERVER['HTTP_REFERER']) ) {
      curl_setopt($curl, CURLOPT_REFERER, $_SERVER['HTTP_REFERER']);
    }
    curl_setopt($curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/534.24 (KHTML, like Gecko) Chrome/11.0.696.71 Safari/534.24");
    $response = curl_exec($curl);
    // Check if any error occured
    if( curl_errno($curl) ) {
      $this->_errors .= "Curl Error: ".curl_error($curl);
      return false;
    }
    curl_close($curl);
    return $response;
  }
  private function _parceGoogleResponse( $response, $translit = false ) {
    if( empty($this->_errors) ) {
      $result = "";
      $json = json_decode($response);
      foreach( $json->sentences as $sentence ) {
        $result .= $translit ? $sentence->translit : $sentence->trans;
      }
      return $result;
    } else {
      return false;
    }
  }
}

?>