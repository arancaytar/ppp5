<?php
// $Id: spiderweb.http.inc 487 2010-08-16 23:15:19Z arancaytar $

/**
 * @file spiderweb.http.inc
 *   Contains functions for connecting to the Spiderweb forums via HTTP.
 */
 
define('SPIDERWEB_URL', 'http://www.spiderwebforums.com/forum');
define('SPIDERWEB_CHARSET', 'WINDOWS-1252');

/**
 * Get stored cookies, or store new cookie.
 * It will not log in automatically.
 *
 * @param $new_cookies
 *   When setting new cookies, pass them as an array [ string $name => string $value ],
 *
 * @return
 *   If cookies for login are stored or set, a string of cookies that can be sent to the server as is.
 */
function spiderweb_cookies($new_cookies = array()) {
  static $cookies = array();
  
  if ($new_cookies) {
    foreach ($new_cookies as $name => $info) {
      $cookies[$name] = $info;
    }
    variable_set('spiderweb_cookies', $cookies);
    return;
  }
  
  if (empty($cookies)) {
    $cookies = variable_get('spiderweb_cookies', array());
  }
  
  $out = array();
  foreach ($cookies as $name => $info) {
    $out[] = "$name={$info['value']}";
  }
  return implode('; ', $out);
}

/**
 * Perform a login at the Spiderweb boards. The resulting session cookies
 * will be automatically stored.
 *
 * @param string $user
 *   The login name of the account to use.
 * @param string $password
 *   The password of the account.
 * @return
 *   The resulting session cookies.
 */
function spiderweb_login($user, $pass) {
  $form = array(
    'ubb' => 'start_page',
    'Loginname' => $user,
    'Loginpass' => $pass,
    'rememberme' => 1,
    'firstlogin' => 1,
    'ON_COMPLETION_URL' => '',
    'buttlogin' => 'Log In',
  );
  
  $response = spiderweb_post($form);
  return $response->cookies;
}

/**
 * Send a POST request to the Spiderweb forum.
 *
 * @param array $fields
 *   An array of form fields to send, [ string $name => string $value ]
 * @param string $page
 *   If specified, a non-default page on the server will be targeted. Default is ubbthreads.php
 * @param boolean $cookies
 *   [optional] If set to FALSE, spiderweb_cookies() will not be called. Beware of accidental recursion.
 *
 * @return
 *   A full HTTP response object as returned by drupal_http_request().
 */
function spiderweb_post($fields, $page = 'ubbthreads.php', $cookies = array()) {
  $url = SPIDERWEB_URL . "/$page";
  $options = array(
    'method' => 'POST',
    'headers' => array(
      'Content-Type' => 'application/x-www-form-urlencoded',
      'Cookie' => $cookies,
    ),
    'data' => drupal_http_build_query($fields),
  );
  return drupal_http_request($url, $options); 
}

/**
 * Send a GET request to the Spiderweb forum,
 *
 * @param array $fields
 *   An array of form fields to send, [ string $name => string $value ]
 *
 * @return
 *   A full HTTP response object as returned by drupal_http_request().
 */
function spiderweb_get($fields, $page = 'ubbthreads.php', $cookies = array()) {
  $url = SPIDERWEB_URL . "/$page";
  $options = array(
   'headers' => array('Cookie' => $cookies),
  );
  $query = drupal_http_build_query($fields);
  $response = drupal_http_request("$url?$query", $options);
  
  // Make sure we get back valid UTF-8.
  // We can't trust UBB's headers. They claim ISO-8859-1, but send Cp1252/Windows.
  $response->data = iconv(SPIDERWEB_CHARSET, 'UTF-8', $response->data);
  return $response;
}

