/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@php.net>                                 |
   |          Zeev Suraski <zeev@php.net>                                 |
   +----------------------------------------------------------------------+
 */

#include "php.h"
#include "php_streams.h"
#include "php_main.h"
#include "php_globals.h"
#include "php_variables.h"
#include "php_ini.h"
#include "php_standard.h"
#include "php_math.h"
#include "php_http.h"
#include "php_incomplete_class.h"
#include "php_getopt.h"
#include "ext/standard/info.h"
#include "ext/session/php_session.h"
#include "zend_exceptions.h"
#include "zend_operators.h"
#include "ext/standard/php_dns.h"
#include "ext/standard/php_uuencode.h"
#include "ext/standard/php_mt_rand.h"

#ifdef WIN32
#include "win32/php_win32_globals.h"
#include "win32/time.h"
#include "win32/ioutil.h"
#endif

typedef struct yy_buffer_state *YY_BUFFER_STATE;

#include "zend.h"
#include "zend_ini_scanner.h"
#include "zend_language_scanner.h"
#include <zend_language_parser.h>

#include "zend_portability.h"

#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef WIN32
# include <netdb.h>
#else
#include "win32/inet.h"
#endif

#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <string.h>
#include <locale.h>

#if HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#if HAVE_SYS_LOADAVG_H
# include <sys/loadavg.h>
#endif

#ifdef WIN32
# include "win32/unistd.h"
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((zend_ulong) -1)
#endif

#include "zend_globals.h"
#include "php_globals.h"
#include "SAPI.h"
#include "php_ticks.h"

#ifdef ZTS
PHPAPI int basic_globals_id;
#else
PHPAPI php_basic_globals basic_globals;
#endif

#include "php_fopen_wrappers.h"
#include "streamsfuncs.h"
#include "basic_functions_arginfo.h"

static zend_class_entry *incomplete_class_entry = NULL;

typedef struct _user_tick_function_entry {
	zval *arguments;
	int arg_count;
	int calling;
} user_tick_function_entry;

/* some prototypes for local functions */
static void user_shutdown_function_dtor(zval *zv);
static void user_tick_function_dtor(user_tick_function_entry *tick_function_entry);

static const zend_module_dep standard_deps[] = { /* {{{ */
	ZEND_MOD_OPTIONAL("session")
	ZEND_MOD_END
};
/* }}} */

zend_module_entry basic_functions_module = { /* {{{ */
	STANDARD_MODULE_HEADER_EX,
	NULL,
	standard_deps,
	"standard",					/* extension name */
	ext_functions,				/* function list */
	PHP_MINIT(basic),			/* process startup */
	PHP_MSHUTDOWN(basic),		/* process shutdown */
	PHP_RINIT(basic),			/* request startup */
	PHP_RSHUTDOWN(basic),		/* request shutdown */
	PHP_MINFO(basic),			/* extension info */
	PHP_STANDARD_VERSION,		/* extension version */
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#if defined(HAVE_PUTENV)
static void php_putenv_destructor(zval *zv) /* {{{ */
{
	putenv_entry *pe = Z_PTR_P(zv);

	if (pe->previous_value) {
# if defined(WIN32)
		/* MSVCRT has a bug in putenv() when setting a variable that
		 * is already set; if the SetEnvironmentVariable() API call
		 * fails, the Crt will double free() a string.
		 * We try to avoid this by setting our own value first */
		SetEnvironmentVariable(pe->key, "bugbug");
# endif
		putenv(pe->previous_value);
# if defined(WIN32)
		efree(pe->previous_value);
# endif
	} else {
# if HAVE_UNSETENV
		unsetenv(pe->key);
# elif defined(WIN32)
		SetEnvironmentVariable(pe->key, NULL);
# ifndef ZTS
		_putenv_s(pe->key, "");
# endif
# else
		char **env;

		for (env = environ; env != NULL && *env != NULL; env++) {
			if (!strncmp(*env, pe->key, pe->key_len) && (*env)[pe->key_len] == '=') {	/* found it */
				*env = "";
				break;
			}
		}
# endif
	}
#ifdef HAVE_TZSET
	/* don't forget to reset the various libc globals that
	 * we might have changed by an earlier call to tzset(). */
	if (!strncmp(pe->key, "TZ", pe->key_len)) {
		tzset();
	}
#endif

	efree(pe->putenv_string);
	efree(pe->key);
	efree(pe);
}
/* }}} */
#endif

static void basic_globals_ctor(php_basic_globals *basic_globals_p) /* {{{ */
{
	BG(mt_rand_is_seeded) = 0;
	BG(mt_rand_mode) = MT_RAND_MT19937;
	BG(umask) = -1;
	BG(next) = NULL;
	BG(left) = -1;
	BG(user_tick_functions) = NULL;
	BG(user_filter_map) = NULL;
	BG(serialize_lock) = 0;

	memset(&BG(serialize), 0, sizeof(BG(serialize)));
	memset(&BG(unserialize), 0, sizeof(BG(unserialize)));

	memset(&BG(url_adapt_session_ex), 0, sizeof(BG(url_adapt_session_ex)));
	memset(&BG(url_adapt_output_ex), 0, sizeof(BG(url_adapt_output_ex)));

	BG(url_adapt_session_ex).type = 1;
	BG(url_adapt_output_ex).type  = 0;

	zend_hash_init(&BG(url_adapt_session_hosts_ht), 0, NULL, NULL, 1);
	zend_hash_init(&BG(url_adapt_output_hosts_ht), 0, NULL, NULL, 1);

#if defined(_REENTRANT)
	memset(&BG(mblen_state), 0, sizeof(BG(mblen_state)));
#endif

	BG(incomplete_class) = incomplete_class_entry;
	BG(page_uid) = -1;
	BG(page_gid) = -1;
}
/* }}} */

static void basic_globals_dtor(php_basic_globals *basic_globals_p) /* {{{ */
{
	if (basic_globals_p->url_adapt_session_ex.tags) {
		zend_hash_destroy(basic_globals_p->url_adapt_session_ex.tags);
		free(basic_globals_p->url_adapt_session_ex.tags);
	}
	if (basic_globals_p->url_adapt_output_ex.tags) {
		zend_hash_destroy(basic_globals_p->url_adapt_output_ex.tags);
		free(basic_globals_p->url_adapt_output_ex.tags);
	}

	zend_hash_destroy(&basic_globals_p->url_adapt_session_hosts_ht);
	zend_hash_destroy(&basic_globals_p->url_adapt_output_hosts_ht);
}
/* }}} */

PHPAPI double php_get_nan(void) /* {{{ */
{
	return ZEND_NAN;
}
/* }}} */

PHPAPI double php_get_inf(void) /* {{{ */
{
	return ZEND_INFINITY;
}
/* }}} */

#define BASIC_MINIT_SUBMODULE(module) \
	if (PHP_MINIT(module)(INIT_FUNC_ARGS_PASSTHRU) != SUCCESS) {\
		return FAILURE; \
	}

#define BASIC_RINIT_SUBMODULE(module) \
	PHP_RINIT(module)(INIT_FUNC_ARGS_PASSTHRU);

#define BASIC_MINFO_SUBMODULE(module) \
	PHP_MINFO(module)(ZEND_MODULE_INFO_FUNC_ARGS_PASSTHRU);

#define BASIC_RSHUTDOWN_SUBMODULE(module) \
	PHP_RSHUTDOWN(module)(SHUTDOWN_FUNC_ARGS_PASSTHRU);

#define BASIC_MSHUTDOWN_SUBMODULE(module) \
	PHP_MSHUTDOWN(module)(SHUTDOWN_FUNC_ARGS_PASSTHRU);

PHP_MINIT_FUNCTION(basic) /* {{{ */
{
#ifdef ZTS
	ts_allocate_id(&basic_globals_id, sizeof(php_basic_globals), (ts_allocate_ctor) basic_globals_ctor, (ts_allocate_dtor) basic_globals_dtor);
#ifdef WIN32
	ts_allocate_id(&php_win32_core_globals_id, sizeof(php_win32_core_globals), (ts_allocate_ctor)php_win32_core_globals_ctor, (ts_allocate_dtor)php_win32_core_globals_dtor );
#endif
#else
	basic_globals_ctor(&basic_globals);
#ifdef WIN32
	php_win32_core_globals_ctor(&the_php_win32_core_globals);
#endif
#endif

	BG(incomplete_class) = incomplete_class_entry = php_create_incomplete_class();

	REGISTER_LONG_CONSTANT("CONNECTION_ABORTED", PHP_CONNECTION_ABORTED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CONNECTION_NORMAL",  PHP_CONNECTION_NORMAL,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CONNECTION_TIMEOUT", PHP_CONNECTION_TIMEOUT, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("INI_USER",   ZEND_INI_USER,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("INI_PERDIR", ZEND_INI_PERDIR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("INI_SYSTEM", ZEND_INI_SYSTEM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("INI_ALL",    ZEND_INI_ALL,    CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("INI_SCANNER_NORMAL", ZEND_INI_SCANNER_NORMAL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("INI_SCANNER_RAW",    ZEND_INI_SCANNER_RAW,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("INI_SCANNER_TYPED",  ZEND_INI_SCANNER_TYPED,  CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("PHP_URL_SCHEME", PHP_URL_SCHEME, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_HOST", PHP_URL_HOST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_PORT", PHP_URL_PORT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_USER", PHP_URL_USER, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_PASS", PHP_URL_PASS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_PATH", PHP_URL_PATH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_QUERY", PHP_URL_QUERY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_URL_FRAGMENT", PHP_URL_FRAGMENT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_QUERY_RFC1738", PHP_QUERY_RFC1738, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_QUERY_RFC3986", PHP_QUERY_RFC3986, CONST_CS | CONST_PERSISTENT);

#define REGISTER_MATH_CONSTANT(x)  REGISTER_DOUBLE_CONSTANT(#x, x, CONST_CS | CONST_PERSISTENT)
	REGISTER_MATH_CONSTANT(M_E);
	REGISTER_MATH_CONSTANT(M_LOG2E);
	REGISTER_MATH_CONSTANT(M_LOG10E);
	REGISTER_MATH_CONSTANT(M_LN2);
	REGISTER_MATH_CONSTANT(M_LN10);
	REGISTER_MATH_CONSTANT(M_PI);
	REGISTER_MATH_CONSTANT(M_PI_2);
	REGISTER_MATH_CONSTANT(M_PI_4);
	REGISTER_MATH_CONSTANT(M_1_PI);
	REGISTER_MATH_CONSTANT(M_2_PI);
	REGISTER_MATH_CONSTANT(M_SQRTPI);
	REGISTER_MATH_CONSTANT(M_2_SQRTPI);
	REGISTER_MATH_CONSTANT(M_LNPI);
	REGISTER_MATH_CONSTANT(M_EULER);
	REGISTER_MATH_CONSTANT(M_SQRT2);
	REGISTER_MATH_CONSTANT(M_SQRT1_2);
	REGISTER_MATH_CONSTANT(M_SQRT3);
	REGISTER_DOUBLE_CONSTANT("INF", ZEND_INFINITY, CONST_CS | CONST_PERSISTENT);
	REGISTER_DOUBLE_CONSTANT("NAN", ZEND_NAN, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("PHP_ROUND_HALF_UP", PHP_ROUND_HALF_UP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_ROUND_HALF_DOWN", PHP_ROUND_HALF_DOWN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_ROUND_HALF_EVEN", PHP_ROUND_HALF_EVEN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_ROUND_HALF_ODD", PHP_ROUND_HALF_ODD, CONST_CS | CONST_PERSISTENT);

#if ENABLE_TEST_CLASS
	test_class_startup();
#endif

	register_phpinfo_constants(INIT_FUNC_ARGS_PASSTHRU);
	register_html_constants(INIT_FUNC_ARGS_PASSTHRU);
	register_string_constants(INIT_FUNC_ARGS_PASSTHRU);

	BASIC_MINIT_SUBMODULE(var)
	BASIC_MINIT_SUBMODULE(file)
	BASIC_MINIT_SUBMODULE(pack)
	BASIC_MINIT_SUBMODULE(browscap)
	BASIC_MINIT_SUBMODULE(standard_filters)
	BASIC_MINIT_SUBMODULE(user_filters)
	BASIC_MINIT_SUBMODULE(password)
	BASIC_MINIT_SUBMODULE(mt_rand)

#if defined(ZTS)
	BASIC_MINIT_SUBMODULE(localeconv)
#endif

#if defined(HAVE_NL_LANGINFO)
	BASIC_MINIT_SUBMODULE(nl_langinfo)
#endif

#if ZEND_INTRIN_SSE4_2_FUNC_PTR
	BASIC_MINIT_SUBMODULE(string_intrin)
#endif

#if ZEND_INTRIN_AVX2_FUNC_PTR || ZEND_INTRIN_SSSE3_FUNC_PTR
	BASIC_MINIT_SUBMODULE(base64_intrin)
#endif

	BASIC_MINIT_SUBMODULE(crypt)
	BASIC_MINIT_SUBMODULE(lcg)

	BASIC_MINIT_SUBMODULE(dir)
#ifdef HAVE_SYSLOG_H
	BASIC_MINIT_SUBMODULE(syslog)
#endif
	BASIC_MINIT_SUBMODULE(array)
	BASIC_MINIT_SUBMODULE(assert)
	BASIC_MINIT_SUBMODULE(url_scanner_ex)
#ifdef PHP_CAN_SUPPORT_PROC_OPEN
	BASIC_MINIT_SUBMODULE(proc_open)
#endif
	BASIC_MINIT_SUBMODULE(exec)

	BASIC_MINIT_SUBMODULE(user_streams)
	BASIC_MINIT_SUBMODULE(imagetypes)

	php_register_url_stream_wrapper("php", &php_stream_php_wrapper);
	php_register_url_stream_wrapper("file", &php_plain_files_wrapper);
#ifdef HAVE_GLOB
	php_register_url_stream_wrapper("glob", &php_glob_stream_wrapper);
#endif
	php_register_url_stream_wrapper("data", &php_stream_rfc2397_wrapper);
	php_register_url_stream_wrapper("http", &php_stream_http_wrapper);
	php_register_url_stream_wrapper("ftp", &php_stream_ftp_wrapper);

#if defined(WIN32) || (HAVE_DNS_SEARCH_FUNC && HAVE_FULL_DNS_FUNCS)
	BASIC_MINIT_SUBMODULE(dns)
#endif

	BASIC_MINIT_SUBMODULE(random)

	BASIC_MINIT_SUBMODULE(hrtime)

	return SUCCESS;
}
/* }}} */

PHP_MSHUTDOWN_FUNCTION(basic) /* {{{ */
{
#ifdef HAVE_SYSLOG_H
	PHP_MSHUTDOWN(syslog)(SHUTDOWN_FUNC_ARGS_PASSTHRU);
#endif
#ifdef ZTS
	ts_free_id(basic_globals_id);
#ifdef WIN32
	ts_free_id(php_win32_core_globals_id);
#endif
#else
	basic_globals_dtor(&basic_globals);
#ifdef WIN32
	php_win32_core_globals_dtor(&the_php_win32_core_globals);
#endif
#endif

	php_unregister_url_stream_wrapper("php");
	php_unregister_url_stream_wrapper("http");
	php_unregister_url_stream_wrapper("ftp");

	BASIC_MSHUTDOWN_SUBMODULE(browscap)
	BASIC_MSHUTDOWN_SUBMODULE(array)
	BASIC_MSHUTDOWN_SUBMODULE(assert)
	BASIC_MSHUTDOWN_SUBMODULE(url_scanner_ex)
	BASIC_MSHUTDOWN_SUBMODULE(file)
	BASIC_MSHUTDOWN_SUBMODULE(standard_filters)
#if defined(ZTS)
	BASIC_MSHUTDOWN_SUBMODULE(localeconv)
#endif
	BASIC_MSHUTDOWN_SUBMODULE(crypt)
	BASIC_MSHUTDOWN_SUBMODULE(random)
	BASIC_MSHUTDOWN_SUBMODULE(password)

	return SUCCESS;
}
/* }}} */

PHP_RINIT_FUNCTION(basic) /* {{{ */
{
	memset(BG(strtok_table), 0, 256);

	BG(serialize_lock) = 0;
	memset(&BG(serialize), 0, sizeof(BG(serialize)));
	memset(&BG(unserialize), 0, sizeof(BG(unserialize)));

	BG(strtok_string) = NULL;
	ZVAL_UNDEF(&BG(strtok_zval));
	BG(strtok_last) = NULL;
	BG(ctype_string) = NULL;
	BG(locale_changed) = 0;
	BG(array_walk_fci) = empty_fcall_info;
	BG(array_walk_fci_cache) = empty_fcall_info_cache;
	BG(user_compare_fci) = empty_fcall_info;
	BG(user_compare_fci_cache) = empty_fcall_info_cache;
	BG(page_uid) = -1;
	BG(page_gid) = -1;
	BG(page_inode) = -1;
	BG(page_mtime) = -1;
#ifdef HAVE_PUTENV
	zend_hash_init(&BG(putenv_ht), 1, NULL, php_putenv_destructor, 0);
#endif
	BG(user_shutdown_function_names) = NULL;

	PHP_RINIT(filestat)(INIT_FUNC_ARGS_PASSTHRU);
#ifdef HAVE_SYSLOG_H
	BASIC_RINIT_SUBMODULE(syslog)
#endif
	BASIC_RINIT_SUBMODULE(dir)
	BASIC_RINIT_SUBMODULE(url_scanner_ex)

	/* Setup default context */
	FG(default_context) = NULL;

	/* Default to global wrappers only */
	FG(stream_wrappers) = NULL;

	/* Default to global filters only */
	FG(stream_filters) = NULL;

	return SUCCESS;
}
/* }}} */

PHP_RSHUTDOWN_FUNCTION(basic) /* {{{ */
{
	zval_ptr_dtor(&BG(strtok_zval));
	ZVAL_UNDEF(&BG(strtok_zval));
	BG(strtok_string) = NULL;
#ifdef HAVE_PUTENV
	tsrm_env_lock();
	zend_hash_destroy(&BG(putenv_ht));
	tsrm_env_unlock();
#endif

	BG(mt_rand_is_seeded) = 0;

	if (BG(umask) != -1) {
		umask(BG(umask));
	}

	/* Check if locale was changed and change it back
	 * to the value in startup environment */
	if (BG(locale_changed)) {
		setlocale(LC_ALL, "C");
		zend_update_current_locale();
		if (BG(ctype_string)) {
			zend_string_release_ex(BG(ctype_string), 0);
			BG(ctype_string) = NULL;
		}
	}

	/* FG(stream_wrappers) and FG(stream_filters) are destroyed
	 * during php_request_shutdown() */

	PHP_RSHUTDOWN(filestat)(SHUTDOWN_FUNC_ARGS_PASSTHRU);
#ifdef HAVE_SYSLOG_H
#ifdef WIN32
	BASIC_RSHUTDOWN_SUBMODULE(syslog)(SHUTDOWN_FUNC_ARGS_PASSTHRU);
#endif
#endif
	BASIC_RSHUTDOWN_SUBMODULE(assert)
	BASIC_RSHUTDOWN_SUBMODULE(url_scanner_ex)
	BASIC_RSHUTDOWN_SUBMODULE(streams)
#ifdef WIN32
	BASIC_RSHUTDOWN_SUBMODULE(win32_core_globals)
#endif

	if (BG(user_tick_functions)) {
		zend_llist_destroy(BG(user_tick_functions));
		efree(BG(user_tick_functions));
		BG(user_tick_functions) = NULL;
	}

	BASIC_RSHUTDOWN_SUBMODULE(user_filters)
	BASIC_RSHUTDOWN_SUBMODULE(browscap)

 	BG(page_uid) = -1;
 	BG(page_gid) = -1;
	return SUCCESS;
}
/* }}} */

PHP_MINFO_FUNCTION(basic) /* {{{ */
{
	php_info_print_table_start();
	BASIC_MINFO_SUBMODULE(dl)
	BASIC_MINFO_SUBMODULE(mail)
	php_info_print_table_end();
	BASIC_MINFO_SUBMODULE(assert)
}
/* }}} */

/* {{{ proto mixed constant(string const_name)
   Given the name of a constant this function will return the constant's associated value */
PHP_FUNCTION(constant)
{
	zend_string *const_name;
	zval *c;
	zend_class_entry *scope;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(const_name)
	ZEND_PARSE_PARAMETERS_END();

	scope = zend_get_executed_scope();
	c = zend_get_constant_ex(const_name, scope, 0);
	if (!c) {
		RETURN_THROWS();
	}

	ZVAL_COPY_OR_DUP(return_value, c);
	if (Z_TYPE_P(return_value) == IS_CONSTANT_AST) {
		if (UNEXPECTED(zval_update_constant_ex(return_value, scope) != SUCCESS)) {
			RETURN_THROWS();
		}
	}
}
/* }}} */

#ifdef HAVE_INET_NTOP
/* {{{ proto string|false inet_ntop(string in_addr)
   Converts a packed inet address to a human readable IP address string */
PHP_FUNCTION(inet_ntop)
{
	char *address;
	size_t address_len;
	int af = AF_INET;
	char buffer[40];

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(address, address_len)
	ZEND_PARSE_PARAMETERS_END();

#ifdef HAVE_IPV6
	if (address_len == 16) {
		af = AF_INET6;
	} else
#endif
	if (address_len != 4) {
		RETURN_FALSE;
	}

	if (!inet_ntop(af, address, buffer, sizeof(buffer))) {
		RETURN_FALSE;
	}

	RETURN_STRING(buffer);
}
/* }}} */
#endif /* HAVE_INET_NTOP */

#ifdef HAVE_INET_PTON
/* {{{ proto string|false inet_pton(string ip_address)
   Converts a human readable IP address to a packed binary string */
PHP_FUNCTION(inet_pton)
{
	int ret, af = AF_INET;
	char *address;
	size_t address_len;
	char buffer[17];

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(address, address_len)
	ZEND_PARSE_PARAMETERS_END();

	memset(buffer, 0, sizeof(buffer));

#ifdef HAVE_IPV6
	if (strchr(address, ':')) {
		af = AF_INET6;
	} else
#endif
	if (!strchr(address, '.')) {
		RETURN_FALSE;
	}

	ret = inet_pton(af, address, buffer);

	if (ret <= 0) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(buffer, af == AF_INET ? 4 : 16);
}
/* }}} */
#endif /* HAVE_INET_PTON */

/* {{{ proto int|false ip2long(string ip_address)
   Converts a string containing an (IPv4) Internet Protocol dotted address into a proper address */
PHP_FUNCTION(ip2long)
{
	char *addr;
	size_t addr_len;
#ifdef HAVE_INET_PTON
	struct in_addr ip;
#else
	zend_ulong ip;
#endif

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(addr, addr_len)
	ZEND_PARSE_PARAMETERS_END();

#ifdef HAVE_INET_PTON
	if (addr_len == 0 || inet_pton(AF_INET, addr, &ip) != 1) {
		RETURN_FALSE;
	}
	RETURN_LONG(ntohl(ip.s_addr));
#else
	if (addr_len == 0 || (ip = inet_addr(addr)) == INADDR_NONE) {
		/* The only special case when we should return -1 ourselves,
		 * because inet_addr() considers it wrong. We return 0xFFFFFFFF and
		 * not -1 or ~0 because of 32/64bit issues. */
		if (addr_len == sizeof("255.255.255.255") - 1 &&
			!memcmp(addr, "255.255.255.255", sizeof("255.255.255.255") - 1)
		) {
			RETURN_LONG(0xFFFFFFFF);
		}
		RETURN_FALSE;
	}
	RETURN_LONG(ntohl(ip));
#endif
}
/* }}} */

/* {{{ proto string|false long2ip(int proper_address)
   Converts an (IPv4) Internet network address into a string in Internet standard dotted format */
PHP_FUNCTION(long2ip)
{
	zend_ulong ip;
	zend_long sip;
	struct in_addr myaddr;
#ifdef HAVE_INET_PTON
	char str[40];
#endif

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(sip)
	ZEND_PARSE_PARAMETERS_END();

	/* autoboxes on 32bit platforms, but that's expected */
	ip = (zend_ulong)sip;

	myaddr.s_addr = htonl(ip);
#ifdef HAVE_INET_PTON
	if (inet_ntop(AF_INET, &myaddr, str, sizeof(str))) {
		RETURN_STRING(str);
	} else {
		RETURN_FALSE;
	}
#else
	RETURN_STRING(inet_ntoa(myaddr));
#endif
}
/* }}} */

/********************
 * System Functions *
 ********************/

/* {{{ proto string|array|false getenv([ string varname[, bool local_only]])
   Get the value of an environment variable or every available environment variable
   if no varname is present  */
PHP_FUNCTION(getenv)
{
	char *ptr, *str = NULL;
	size_t str_len;
	zend_bool local_only = 0;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(str, str_len)
		Z_PARAM_BOOL(local_only)
	ZEND_PARSE_PARAMETERS_END();

	if (!str) {
		array_init(return_value);
		php_import_environment_variables(return_value);
		return;
	}

	if (!local_only) {
		/* SAPI method returns an emalloc()'d string */
		ptr = sapi_getenv(str, str_len);
		if (ptr) {
			// TODO: avoid realocation ???
			RETVAL_STRING(ptr);
			efree(ptr);
			return;
		}
	}
#ifdef WIN32
	{
		wchar_t dummybuf;
		DWORD size;
		wchar_t *keyw, *valw;

		keyw = php_win32_cp_conv_any_to_w(str, str_len, PHP_WIN32_CP_IGNORE_LEN_P);
		if (!keyw) {
				RETURN_FALSE;
		}

		SetLastError(0);
		/*If the given bugger is not large enough to hold the data, the return value is
		the buffer size,  in characters, required to hold the string and its terminating
		null character. We use this return value to alloc the final buffer. */
		size = GetEnvironmentVariableW(keyw, &dummybuf, 0);
		if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
				/* The environment variable doesn't exist. */
				free(keyw);
				RETURN_FALSE;
		}

		if (size == 0) {
				/* env exists, but it is empty */
				free(keyw);
				RETURN_EMPTY_STRING();
		}

		valw = emalloc((size + 1) * sizeof(wchar_t));
		size = GetEnvironmentVariableW(keyw, valw, size);
		if (size == 0) {
				/* has been removed between the two calls */
				free(keyw);
				efree(valw);
				RETURN_EMPTY_STRING();
		} else {
			ptr = php_win32_cp_w_to_any(valw);
			RETVAL_STRING(ptr);
			free(ptr);
			free(keyw);
			efree(valw);
			return;
		}
	}
#else

    tsrm_env_lock();

	/* system method returns a const */
	ptr = getenv(str);

	if (ptr) {
		RETVAL_STRING(ptr);
	}

    tsrm_env_unlock();

    if (ptr) {
        return;
    }

#endif
	RETURN_FALSE;
}
/* }}} */

#ifdef HAVE_PUTENV
/* {{{ proto bool putenv(string setting)
   Set the value of an environment variable */
PHP_FUNCTION(putenv)
{
	char *setting;
	size_t setting_len;
	char *p, **env;
	putenv_entry pe;
#ifdef WIN32
	char *value = NULL;
	int equals = 0;
	int error_code;
#endif

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(setting, setting_len)
	ZEND_PARSE_PARAMETERS_END();

	if (setting_len == 0 || setting[0] == '=') {
		zend_argument_value_error(1, "must have a valid syntax");
		RETURN_THROWS();
	}

	pe.putenv_string = estrndup(setting, setting_len);
	pe.key = estrndup(setting, setting_len);
	if ((p = strchr(pe.key, '='))) {	/* nullify the '=' if there is one */
		*p = '\0';
#ifdef WIN32
		equals = 1;
#endif
	}

	pe.key_len = strlen(pe.key);
#ifdef WIN32
	if (equals) {
		if (pe.key_len < setting_len - 1) {
			value = p + 1;
		} else {
			/* empty string*/
			value = p;
		}
	}
#endif

	tsrm_env_lock();
	zend_hash_str_del(&BG(putenv_ht), pe.key, pe.key_len);

	/* find previous value */
	pe.previous_value = NULL;
	for (env = environ; env != NULL && *env != NULL; env++) {
		if (!strncmp(*env, pe.key, pe.key_len) && (*env)[pe.key_len] == '=') {	/* found it */
#if defined(WIN32)
			/* must copy previous value because MSVCRT's putenv can free the string without notice */
			pe.previous_value = estrdup(*env);
#else
			pe.previous_value = *env;
#endif
			break;
		}
	}

#if HAVE_UNSETENV
	if (!p) { /* no '=' means we want to unset it */
		unsetenv(pe.putenv_string);
	}
	if (!p || putenv(pe.putenv_string) == 0) { /* success */
#else
# ifndef WIN32
	if (putenv(pe.putenv_string) == 0) { /* success */
# else
		wchar_t *keyw, *valw = NULL;

		keyw = php_win32_cp_any_to_w(pe.key);
		if (value) {
			valw = php_win32_cp_any_to_w(value);
		}
		/* valw may be NULL, but the failed conversion still needs to be checked. */
		if (!keyw || !valw && value) {
			efree(pe.putenv_string);
			efree(pe.key);
			free(keyw);
			free(valw);
			RETURN_FALSE;
		}

	error_code = SetEnvironmentVariableW(keyw, valw);

	if (error_code != 0
# ifndef ZTS
	/* We need both SetEnvironmentVariable and _putenv here as some
		dependency lib could use either way to read the environment.
		Obviously the CRT version will be useful more often. But
		generally, doing both brings us on the safe track at least
		in NTS build. */
	&& _wputenv_s(keyw, valw ? valw : L"") == 0
# endif
	) { /* success */
# endif
#endif
		zend_hash_str_add_mem(&BG(putenv_ht), pe.key, pe.key_len, &pe, sizeof(putenv_entry));
#ifdef HAVE_TZSET
		if (!strncmp(pe.key, "TZ", pe.key_len)) {
			tzset();
		}
#endif
		tsrm_env_unlock();
#if defined(WIN32)
		free(keyw);
		free(valw);
#endif
		RETURN_TRUE;
	} else {
		efree(pe.putenv_string);
		efree(pe.key);
#if defined(WIN32)
		free(keyw);
		free(valw);
#endif
		RETURN_FALSE;
	}
}
/* }}} */
#endif

/* {{{ free_argv()
   Free the memory allocated to an argv array. */
static void free_argv(char **argv, int argc)
{
	int i;

	if (argv) {
		for (i = 0; i < argc; i++) {
			if (argv[i]) {
				efree(argv[i]);
			}
		}
		efree(argv);
	}
}
/* }}} */

/* {{{ free_longopts()
   Free the memory allocated to an longopt array. */
static void free_longopts(opt_struct *longopts)
{
	opt_struct *p;

	if (longopts) {
		for (p = longopts; p && p->opt_char != '-'; p++) {
			if (p->opt_name != NULL) {
				efree((char *)(p->opt_name));
			}
		}
	}
}
/* }}} */

/* {{{ parse_opts()
   Convert the typical getopt input characters to the php_getopt struct array */
static int parse_opts(char * opts, opt_struct ** result)
{
	opt_struct * paras = NULL;
	unsigned int i, count = 0;
	unsigned int opts_len = (unsigned int)strlen(opts);

	for (i = 0; i < opts_len; i++) {
		if ((opts[i] >= 48 && opts[i] <= 57) ||
			(opts[i] >= 65 && opts[i] <= 90) ||
			(opts[i] >= 97 && opts[i] <= 122)
		) {
			count++;
		}
	}

	paras = safe_emalloc(sizeof(opt_struct), count, 0);
	memset(paras, 0, sizeof(opt_struct) * count);
	*result = paras;
	while ( (*opts >= 48 && *opts <= 57) || /* 0 - 9 */
			(*opts >= 65 && *opts <= 90) || /* A - Z */
			(*opts >= 97 && *opts <= 122)   /* a - z */
	) {
		paras->opt_char = *opts;
		paras->need_param = *(++opts) == ':';
		paras->opt_name = NULL;
		if (paras->need_param == 1) {
			opts++;
			if (*opts == ':') {
				paras->need_param++;
				opts++;
			}
		}
		paras++;
	}
	return count;
}
/* }}} */

/* {{{ proto array|false getopt(string options [, array longopts [, int &optind]])
   Get options from the command line argument list */
PHP_FUNCTION(getopt)
{
	char *options = NULL, **argv = NULL;
	char opt[2] = { '\0' };
	char *optname;
	int argc = 0, o;
	size_t options_len = 0, len;
	char *php_optarg = NULL;
	int php_optind = 1;
	zval val, *args = NULL, *p_longopts = NULL;
	zval *zoptind = NULL;
	size_t optname_len = 0;
	opt_struct *opts, *orig_opts;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_STRING(options, options_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(p_longopts)
		Z_PARAM_ZVAL(zoptind)
	ZEND_PARSE_PARAMETERS_END();

	/* Init zoptind to 1 */
	if (zoptind) {
		ZEND_TRY_ASSIGN_REF_LONG(zoptind, 1);
	}

	/* Get argv from the global symbol table. We calculate argc ourselves
	 * in order to be on the safe side, even though it is also available
	 * from the symbol table. */
	if ((Z_TYPE(PG(http_globals)[TRACK_VARS_SERVER]) == IS_ARRAY || zend_is_auto_global_str(ZEND_STRL("_SERVER"))) &&
		((args = zend_hash_find_ex_ind(Z_ARRVAL_P(&PG(http_globals)[TRACK_VARS_SERVER]), ZSTR_KNOWN(ZEND_STR_ARGV), 1)) != NULL ||
		(args = zend_hash_find_ex_ind(&EG(symbol_table), ZSTR_KNOWN(ZEND_STR_ARGV), 1)) != NULL)
	) {
		int pos = 0;
		zval *entry;

 		if (Z_TYPE_P(args) != IS_ARRAY) {
 			RETURN_FALSE;
 		}
 		argc = zend_hash_num_elements(Z_ARRVAL_P(args));

		/* Attempt to allocate enough memory to hold all of the arguments
		 * and a trailing NULL */
		argv = (char **) safe_emalloc(sizeof(char *), (argc + 1), 0);

		/* Iterate over the hash to construct the argv array. */
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), entry) {
			zend_string *tmp_arg_str;
			zend_string *arg_str = zval_get_tmp_string(entry, &tmp_arg_str);

			argv[pos++] = estrdup(ZSTR_VAL(arg_str));

			zend_tmp_string_release(tmp_arg_str);
		} ZEND_HASH_FOREACH_END();

		/* The C Standard requires argv[argc] to be NULL - this might
		 * keep some getopt implementations happy. */
		argv[argc] = NULL;
	} else {
		/* Return false if we can't find argv. */
		RETURN_FALSE;
	}

	len = parse_opts(options, &opts);

	if (p_longopts) {
		int count;
		zval *entry;

		count = zend_hash_num_elements(Z_ARRVAL_P(p_longopts));

		/* the first <len> slots are filled by the one short ops
		 * we now extend our array and jump to the new added structs */
		opts = (opt_struct *) erealloc(opts, sizeof(opt_struct) * (len + count + 1));
		orig_opts = opts;
		opts += len;

		memset(opts, 0, count * sizeof(opt_struct));

		/* Iterate over the hash to construct the argv array. */
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(p_longopts), entry) {
			zend_string *tmp_arg_str;
			zend_string *arg_str = zval_get_tmp_string(entry, &tmp_arg_str);

			opts->need_param = 0;
			opts->opt_name = estrdup(ZSTR_VAL(arg_str));
			len = strlen(opts->opt_name);
			if ((len > 0) && (opts->opt_name[len - 1] == ':')) {
				opts->need_param++;
				opts->opt_name[len - 1] = '\0';
				if ((len > 1) && (opts->opt_name[len - 2] == ':')) {
					opts->need_param++;
					opts->opt_name[len - 2] = '\0';
				}
			}
			opts->opt_char = 0;
			opts++;

			zend_tmp_string_release(tmp_arg_str);
		} ZEND_HASH_FOREACH_END();
	} else {
		opts = (opt_struct*) erealloc(opts, sizeof(opt_struct) * (len + 1));
		orig_opts = opts;
		opts += len;
	}

	/* php_getopt want to identify the last param */
	opts->opt_char   = '-';
	opts->need_param = 0;
	opts->opt_name   = NULL;

	/* Initialize the return value as an array. */
	array_init(return_value);

	/* after our pointer arithmetic jump back to the first element */
	opts = orig_opts;

	while ((o = php_getopt(argc, argv, opts, &php_optarg, &php_optind, 0, 1)) != -1) {
		/* Skip unknown arguments. */
		if (o == PHP_GETOPT_INVALID_ARG) {
			continue;
		}

		/* Prepare the option character and the argument string. */
		if (o == 0) {
			optname = opts[php_optidx].opt_name;
		} else {
			if (o == 1) {
				o = '-';
			}
			opt[0] = o;
			optname = opt;
		}

		if (php_optarg != NULL) {
			/* keep the arg as binary, since the encoding is not known */
			ZVAL_STRING(&val, php_optarg);
		} else {
			ZVAL_FALSE(&val);
		}

		/* Add this option / argument pair to the result hash. */
		optname_len = strlen(optname);
		if (!(optname_len > 1 && optname[0] == '0') && is_numeric_string(optname, optname_len, NULL, NULL, 0) == IS_LONG) {
			/* numeric string */
			int optname_int = atoi(optname);
			if ((args = zend_hash_index_find(Z_ARRVAL_P(return_value), optname_int)) != NULL) {
				if (Z_TYPE_P(args) != IS_ARRAY) {
					convert_to_array_ex(args);
				}
				zend_hash_next_index_insert(Z_ARRVAL_P(args), &val);
			} else {
				zend_hash_index_update(Z_ARRVAL_P(return_value), optname_int, &val);
			}
		} else {
			/* other strings */
			if ((args = zend_hash_str_find(Z_ARRVAL_P(return_value), optname, strlen(optname))) != NULL) {
				if (Z_TYPE_P(args) != IS_ARRAY) {
					convert_to_array_ex(args);
				}
				zend_hash_next_index_insert(Z_ARRVAL_P(args), &val);
			} else {
				zend_hash_str_add(Z_ARRVAL_P(return_value), optname, strlen(optname), &val);
			}
		}

		php_optarg = NULL;
	}

	/* Set zoptind to php_optind */
	if (zoptind) {
		ZEND_TRY_ASSIGN_REF_LONG(zoptind, php_optind);
	}

	free_longopts(orig_opts);
	efree(orig_opts);
	free_argv(argv, argc);
}
/* }}} */

/* {{{ proto void flush(void)
   Flush the output buffer */
PHP_FUNCTION(flush)
{
	ZEND_PARSE_PARAMETERS_NONE();

	sapi_flush();
}
/* }}} */

/* {{{ proto int sleep(int seconds)
   Delay for a given number of seconds */
PHP_FUNCTION(sleep)
{
	zend_long num;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(num)
	ZEND_PARSE_PARAMETERS_END();

	if (num < 0) {
		zend_argument_value_error(1, "must be greater than or equal to 0");
		RETURN_THROWS();
	}

	RETURN_LONG(php_sleep((unsigned int)num));
}
/* }}} */

/* {{{ proto void usleep(int micro_seconds)
   Delay for a given number of micro seconds */
PHP_FUNCTION(usleep)
{
#if HAVE_USLEEP
	zend_long num;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(num)
	ZEND_PARSE_PARAMETERS_END();

	if (num < 0) {
		zend_argument_value_error(1, "must be greater than or equal to 0");
		RETURN_THROWS();
	}
	if (usleep((unsigned int)num) < 0) {
#if ZEND_DEBUG
		php_error_docref(NULL, E_NOTICE, "usleep() failed with errno %d: %s",
			errno, strerror(errno));
#endif
	}
#endif
}
/* }}} */

#if HAVE_NANOSLEEP
/* {{{ proto mixed time_nanosleep(int seconds, int nanoseconds)
   Delay for a number of seconds and nano seconds */
PHP_FUNCTION(time_nanosleep)
{
	zend_long tv_sec, tv_nsec;
	struct timespec php_req, php_rem;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(tv_sec)
		Z_PARAM_LONG(tv_nsec)
	ZEND_PARSE_PARAMETERS_END();

	if (tv_sec < 0) {
		zend_argument_value_error(1, "must be greater than or equal to 0");
		RETURN_THROWS();
	}
	if (tv_nsec < 0) {
		zend_argument_value_error(2, "must be greater than or equal to 0");
		RETURN_THROWS();
	}

	php_req.tv_sec = (time_t) tv_sec;
	php_req.tv_nsec = (long)tv_nsec;
	if (!nanosleep(&php_req, &php_rem)) {
		RETURN_TRUE;
	} else if (errno == EINTR) {
		array_init(return_value);
		add_assoc_long_ex(return_value, "seconds", sizeof("seconds")-1, php_rem.tv_sec);
		add_assoc_long_ex(return_value, "nanoseconds", sizeof("nanoseconds")-1, php_rem.tv_nsec);
		return;
	} else if (errno == EINVAL) {
		zend_value_error("Nanoseconds was not in the range 0 to 999 999 999 or seconds was negative");
		RETURN_THROWS();
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool time_sleep_until(float timestamp)
   Make the script sleep until the specified time */
PHP_FUNCTION(time_sleep_until)
{
	double target_secs;
	struct timeval tm;
	struct timespec php_req, php_rem;
	uint64_t current_ns, target_ns, diff_ns;
	const uint64_t ns_per_sec = 1000000000;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_DOUBLE(target_secs)
	ZEND_PARSE_PARAMETERS_END();

	if (gettimeofday((struct timeval *) &tm, NULL) != 0) {
		RETURN_FALSE;
	}

	target_ns = (uint64_t) (target_secs * ns_per_sec);
	current_ns = ((uint64_t) tm.tv_sec) * ns_per_sec + ((uint64_t) tm.tv_usec) * 1000;
	if (target_ns < current_ns) {
		php_error_docref(NULL, E_WARNING, "Sleep until to time is less than current time");
		RETURN_FALSE;
	}

	diff_ns = target_ns - current_ns;
	php_req.tv_sec = (time_t) (diff_ns / ns_per_sec);
	php_req.tv_nsec = (long) (diff_ns % ns_per_sec);

	while (nanosleep(&php_req, &php_rem)) {
		if (errno == EINTR) {
			php_req.tv_sec = php_rem.tv_sec;
			php_req.tv_nsec = php_rem.tv_nsec;
		} else {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */
#endif

/* {{{ proto string get_current_user(void)
   Get the name of the owner of the current PHP script */
PHP_FUNCTION(get_current_user)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_STRING(php_get_current_user());
}
/* }}} */

static void add_config_entries(HashTable *hash, zval *return_value);

/* {{{ add_config_entry
 */
static void add_config_entry(zend_ulong h, zend_string *key, zval *entry, zval *retval)
{
	if (Z_TYPE_P(entry) == IS_STRING) {
		zend_string *str = Z_STR_P(entry);
		if (!ZSTR_IS_INTERNED(str)) {
			if (!(GC_FLAGS(str) & GC_PERSISTENT)) {
				zend_string_addref(str);
			} else {
				str = zend_string_init(ZSTR_VAL(str), ZSTR_LEN(str), 0);
			}
		}

		if (key) {
			add_assoc_str_ex(retval, ZSTR_VAL(key), ZSTR_LEN(key), str);
		} else {
			add_index_str(retval, h, str);
		}
	} else if (Z_TYPE_P(entry) == IS_ARRAY) {
		zval tmp;
		array_init(&tmp);
		add_config_entries(Z_ARRVAL_P(entry), &tmp);
		zend_hash_update(Z_ARRVAL_P(retval), key, &tmp);
	}
}
/* }}} */

/* {{{ add_config_entries
 */
static void add_config_entries(HashTable *hash, zval *return_value) /* {{{ */
{
	zend_ulong h;
	zend_string *key;
	zval *zv;

	ZEND_HASH_FOREACH_KEY_VAL(hash, h, key, zv)
		add_config_entry(h, key, zv, return_value);
	ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ proto string|array|false get_cfg_var(string option_name)
   Get the value of a PHP configuration option */
PHP_FUNCTION(get_cfg_var)
{
	char *varname;
	size_t varname_len;
	zval *retval;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(varname, varname_len)
	ZEND_PARSE_PARAMETERS_END();

	retval = cfg_get_entry(varname, (uint32_t)varname_len);

	if (retval) {
		if (Z_TYPE_P(retval) == IS_ARRAY) {
			array_init(return_value);
			add_config_entries(Z_ARRVAL_P(retval), return_value);
			return;
		} else {
			RETURN_STRING(Z_STRVAL_P(retval));
		}
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/*
	1st arg = error message
	2nd arg = error option
	3rd arg = optional parameters (email address or tcp address)
	4th arg = used for additional headers if email

error options:
	0 = send to php_error_log (uses syslog or file depending on ini setting)
	1 = send via email to 3rd parameter 4th option = additional headers
	2 = send via tcp/ip to 3rd parameter (name or ip:port)
	3 = save to file in 3rd parameter
	4 = send to SAPI logger directly
*/

/* {{{ proto bool error_log(string message [, int message_type [, string destination [, string extra_headers]]])
   Send an error message somewhere */
PHP_FUNCTION(error_log)
{
	char *message, *opt = NULL, *headers = NULL;
	size_t message_len, opt_len = 0, headers_len = 0;
	int opt_err = 0, argc = ZEND_NUM_ARGS();
	zend_long erropt = 0;

	ZEND_PARSE_PARAMETERS_START(1, 4)
		Z_PARAM_STRING(message, message_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(erropt)
		Z_PARAM_PATH(opt, opt_len)
		Z_PARAM_STRING(headers, headers_len)
	ZEND_PARSE_PARAMETERS_END();

	if (argc > 1) {
		opt_err = (int)erropt;
	}

	if (_php_error_log_ex(opt_err, message, message_len, opt, headers) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* For BC (not binary-safe!) */
PHPAPI int _php_error_log(int opt_err, char *message, char *opt, char *headers) /* {{{ */
{
	return _php_error_log_ex(opt_err, message, (opt_err == 3) ? strlen(message) : 0, opt, headers);
}
/* }}} */

PHPAPI int _php_error_log_ex(int opt_err, char *message, size_t message_len, char *opt, char *headers) /* {{{ */
{
	php_stream *stream = NULL;
	size_t nbytes;

	switch (opt_err)
	{
		case 1:		/*send an email */
			if (!php_mail(opt, "PHP error_log message", message, headers, NULL)) {
				return FAILURE;
			}
			break;

		case 2:		/*send to an address */
			php_error_docref(NULL, E_WARNING, "TCP/IP option not available!");
			return FAILURE;
			break;

		case 3:		/*save to a file */
			stream = php_stream_open_wrapper(opt, "a", IGNORE_URL_WIN | REPORT_ERRORS, NULL);
			if (!stream) {
				return FAILURE;
			}
			nbytes = php_stream_write(stream, message, message_len);
			php_stream_close(stream);
			if (nbytes != message_len) {
				return FAILURE;
			}
			break;

		case 4: /* send to SAPI */
			if (sapi_module.log_message) {
				sapi_module.log_message(message, -1);
			} else {
				return FAILURE;
			}
			break;

		default:
			php_log_err_with_severity(message, LOG_NOTICE);
			break;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ proto array error_get_last()
   Get the last occurred error as associative array. Returns NULL if there hasn't been an error yet. */
PHP_FUNCTION(error_get_last)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if (PG(last_error_message)) {
		array_init(return_value);
		add_assoc_long_ex(return_value, "type", sizeof("type")-1, PG(last_error_type));
		add_assoc_string_ex(return_value, "message", sizeof("message")-1, PG(last_error_message));
		add_assoc_string_ex(return_value, "file", sizeof("file")-1, PG(last_error_file)?PG(last_error_file):"-");
		add_assoc_long_ex(return_value, "line", sizeof("line")-1, PG(last_error_lineno));
	}
}
/* }}} */

/* {{{ proto void error_clear_last(void)
   Clear the last occurred error. */
PHP_FUNCTION(error_clear_last)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if (PG(last_error_message)) {
		PG(last_error_type) = 0;
		PG(last_error_lineno) = 0;

		free(PG(last_error_message));
		PG(last_error_message) = NULL;

		if (PG(last_error_file)) {
			free(PG(last_error_file));
			PG(last_error_file) = NULL;
		}
	}
}
/* }}} */

/* {{{ proto mixed call_user_func(mixed function_name [, mixed parameter] [, mixed ...])
   Call a user function which is the first parameter
   Warning: This function is special-cased by zend_compile.c and so is usually bypassed */
PHP_FUNCTION(call_user_func)
{
	zval retval;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;

	ZEND_PARSE_PARAMETERS_START(1, -1)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
	ZEND_PARSE_PARAMETERS_END();

	fci.retval = &retval;

	if (zend_call_function(&fci, &fci_cache) == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {
		if (Z_ISREF(retval)) {
			zend_unwrap_reference(&retval);
		}
		ZVAL_COPY_VALUE(return_value, &retval);
	}
}
/* }}} */

/* {{{ proto mixed call_user_func_array(callable function, array parameters)
   Call a user function which is the first parameter with the arguments contained in array
   Warning: This function is special-cased by zend_compile.c and so is usually bypassed */
PHP_FUNCTION(call_user_func_array)
{
	zval *params, retval;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_ARRAY(params)
	ZEND_PARSE_PARAMETERS_END();

	zend_fcall_info_args(&fci, params);
	fci.retval = &retval;

	if (zend_call_function(&fci, &fci_cache) == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {
		if (Z_ISREF(retval)) {
			zend_unwrap_reference(&retval);
		}
		ZVAL_COPY_VALUE(return_value, &retval);
	}

	zend_fcall_info_args_clear(&fci, 1);
}
/* }}} */

/* {{{ proto mixed forward_static_call(mixed function_name [, mixed parameter] [, mixed ...]) U
   Call a user function which is the first parameter */
PHP_FUNCTION(forward_static_call)
{
	zval retval;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
	zend_class_entry *called_scope;

	ZEND_PARSE_PARAMETERS_START(1, -1)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
	ZEND_PARSE_PARAMETERS_END();

	if (!EX(prev_execute_data)->func->common.scope) {
		zend_throw_error(NULL, "Cannot call forward_static_call() when no class scope is active");
		RETURN_THROWS();
	}

	fci.retval = &retval;

	called_scope = zend_get_called_scope(execute_data);
	if (called_scope && fci_cache.calling_scope &&
		instanceof_function(called_scope, fci_cache.calling_scope)) {
			fci_cache.called_scope = called_scope;
	}

	if (zend_call_function(&fci, &fci_cache) == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {
		if (Z_ISREF(retval)) {
			zend_unwrap_reference(&retval);
		}
		ZVAL_COPY_VALUE(return_value, &retval);
	}
}
/* }}} */

/* {{{ proto mixed forward_static_call_array(callable function, array parameters)
   Call a static method which is the first parameter with the arguments contained in array */
PHP_FUNCTION(forward_static_call_array)
{
	zval *params, retval;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
	zend_class_entry *called_scope;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_ARRAY(params)
	ZEND_PARSE_PARAMETERS_END();

	zend_fcall_info_args(&fci, params);
	fci.retval = &retval;

	called_scope = zend_get_called_scope(execute_data);
	if (called_scope && fci_cache.calling_scope &&
		instanceof_function(called_scope, fci_cache.calling_scope)) {
			fci_cache.called_scope = called_scope;
	}

	if (zend_call_function(&fci, &fci_cache) == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {
		if (Z_ISREF(retval)) {
			zend_unwrap_reference(&retval);
		}
		ZVAL_COPY_VALUE(return_value, &retval);
	}

	zend_fcall_info_args_clear(&fci, 1);
}
/* }}} */

void user_shutdown_function_dtor(zval *zv) /* {{{ */
{
	int i;
	php_shutdown_function_entry *shutdown_function_entry = Z_PTR_P(zv);

	for (i = 0; i < shutdown_function_entry->arg_count; i++) {
		zval_ptr_dtor(&shutdown_function_entry->arguments[i]);
	}
	efree(shutdown_function_entry->arguments);
	efree(shutdown_function_entry);
}
/* }}} */

void user_tick_function_dtor(user_tick_function_entry *tick_function_entry) /* {{{ */
{
	int i;

	for (i = 0; i < tick_function_entry->arg_count; i++) {
		zval_ptr_dtor(&tick_function_entry->arguments[i]);
	}
	efree(tick_function_entry->arguments);
}
/* }}} */

static int user_shutdown_function_call(zval *zv) /* {{{ */
{
    php_shutdown_function_entry *shutdown_function_entry = Z_PTR_P(zv);
	zval retval;

	if (!zend_is_callable(&shutdown_function_entry->arguments[0], 0, NULL)) {
		zend_string *function_name
			= zend_get_callable_name(&shutdown_function_entry->arguments[0]);
		php_error(E_WARNING, "(Registered shutdown functions) Unable to call %s() - function does not exist", ZSTR_VAL(function_name));
		zend_string_release_ex(function_name, 0);
		return 0;
	}

	if (call_user_function(NULL, NULL,
				&shutdown_function_entry->arguments[0],
				&retval,
				shutdown_function_entry->arg_count - 1,
				shutdown_function_entry->arguments + 1) == SUCCESS)
	{
		zval_ptr_dtor(&retval);
	}
	return 0;
}
/* }}} */

static void user_tick_function_call(user_tick_function_entry *tick_fe) /* {{{ */
{
	zval retval;
	zval *function = &tick_fe->arguments[0];

	/* Prevent reentrant calls to the same user ticks function */
	if (! tick_fe->calling) {
		tick_fe->calling = 1;

		if (call_user_function(NULL, NULL,
								function,
								&retval,
								tick_fe->arg_count - 1,
								tick_fe->arguments + 1
								) == SUCCESS) {
			zval_ptr_dtor(&retval);

		} else {
			zval *obj, *method;

			if (Z_TYPE_P(function) == IS_STRING) {
				php_error_docref(NULL, E_WARNING, "Unable to call %s() - function does not exist", Z_STRVAL_P(function));
			} else if (	Z_TYPE_P(function) == IS_ARRAY
						&& (obj = zend_hash_index_find(Z_ARRVAL_P(function), 0)) != NULL
						&& (method = zend_hash_index_find(Z_ARRVAL_P(function), 1)) != NULL
						&& Z_TYPE_P(obj) == IS_OBJECT
						&& Z_TYPE_P(method) == IS_STRING) {
				php_error_docref(NULL, E_WARNING, "Unable to call %s::%s() - function does not exist", ZSTR_VAL(Z_OBJCE_P(obj)->name), Z_STRVAL_P(method));
			} else {
				php_error_docref(NULL, E_WARNING, "Unable to call tick function");
			}
		}

		tick_fe->calling = 0;
	}
}
/* }}} */

static void run_user_tick_functions(int tick_count, void *arg) /* {{{ */
{
	zend_llist_apply(BG(user_tick_functions), (llist_apply_func_t) user_tick_function_call);
}
/* }}} */

static int user_tick_function_compare(user_tick_function_entry * tick_fe1, user_tick_function_entry * tick_fe2) /* {{{ */
{
	zval *func1 = &tick_fe1->arguments[0];
	zval *func2 = &tick_fe2->arguments[0];
	int ret;

	if (Z_TYPE_P(func1) == IS_STRING && Z_TYPE_P(func2) == IS_STRING) {
		ret = zend_binary_zval_strcmp(func1, func2) == 0;
	} else if (Z_TYPE_P(func1) == IS_ARRAY && Z_TYPE_P(func2) == IS_ARRAY) {
		ret = zend_compare_arrays(func1, func2) == 0;
	} else if (Z_TYPE_P(func1) == IS_OBJECT && Z_TYPE_P(func2) == IS_OBJECT) {
		ret = zend_compare_objects(func1, func2) == 0;
	} else {
		ret = 0;
	}

	if (ret && tick_fe1->calling) {
		php_error_docref(NULL, E_WARNING, "Unable to delete tick function executed at the moment");
		return 0;
	}
	return ret;
}
/* }}} */

PHPAPI void php_call_shutdown_functions(void) /* {{{ */
{
	if (BG(user_shutdown_function_names)) {
		zend_try {
			zend_hash_apply(BG(user_shutdown_function_names), user_shutdown_function_call);
		}
		zend_end_try();
	}
}
/* }}} */

PHPAPI void php_free_shutdown_functions(void) /* {{{ */
{
	if (BG(user_shutdown_function_names))
		zend_try {
			zend_hash_destroy(BG(user_shutdown_function_names));
			FREE_HASHTABLE(BG(user_shutdown_function_names));
			BG(user_shutdown_function_names) = NULL;
		} zend_catch {
			/* maybe shutdown method call exit, we just ignore it */
			FREE_HASHTABLE(BG(user_shutdown_function_names));
			BG(user_shutdown_function_names) = NULL;
		} zend_end_try();
}
/* }}} */

/* {{{ proto false|null register_shutdown_function(callback function) U
   Register a user-level function to be called on request termination */
PHP_FUNCTION(register_shutdown_function)
{
	php_shutdown_function_entry shutdown_function_entry;
	int i;

	shutdown_function_entry.arg_count = ZEND_NUM_ARGS();

	if (shutdown_function_entry.arg_count < 1) {
		WRONG_PARAM_COUNT;
	}

	shutdown_function_entry.arguments = (zval *) safe_emalloc(sizeof(zval), shutdown_function_entry.arg_count, 0);

	if (zend_get_parameters_array(ZEND_NUM_ARGS(), shutdown_function_entry.arg_count, shutdown_function_entry.arguments) == FAILURE) {
		efree(shutdown_function_entry.arguments);
		RETURN_FALSE;
	}

	/* Prevent entering of anything but valid callback (syntax check only!) */
	if (!zend_is_callable(&shutdown_function_entry.arguments[0], 0, NULL)) {
		zend_string *callback_name
			= zend_get_callable_name(&shutdown_function_entry.arguments[0]);
		php_error_docref(NULL, E_WARNING, "Invalid shutdown callback '%s' passed", ZSTR_VAL(callback_name));
		efree(shutdown_function_entry.arguments);
		zend_string_release_ex(callback_name, 0);
		RETVAL_FALSE;
	} else {
		if (!BG(user_shutdown_function_names)) {
			ALLOC_HASHTABLE(BG(user_shutdown_function_names));
			zend_hash_init(BG(user_shutdown_function_names), 0, NULL, user_shutdown_function_dtor, 0);
		}

		for (i = 0; i < shutdown_function_entry.arg_count; i++) {
			Z_TRY_ADDREF(shutdown_function_entry.arguments[i]);
		}
		zend_hash_next_index_insert_mem(BG(user_shutdown_function_names), &shutdown_function_entry, sizeof(php_shutdown_function_entry));
	}
}
/* }}} */

PHPAPI zend_bool register_user_shutdown_function(char *function_name, size_t function_len, php_shutdown_function_entry *shutdown_function_entry) /* {{{ */
{
	if (!BG(user_shutdown_function_names)) {
		ALLOC_HASHTABLE(BG(user_shutdown_function_names));
		zend_hash_init(BG(user_shutdown_function_names), 0, NULL, user_shutdown_function_dtor, 0);
	}

	zend_hash_str_update_mem(BG(user_shutdown_function_names), function_name, function_len, shutdown_function_entry, sizeof(php_shutdown_function_entry));
	return 1;
}
/* }}} */

PHPAPI zend_bool remove_user_shutdown_function(char *function_name, size_t function_len) /* {{{ */
{
	if (BG(user_shutdown_function_names)) {
		return zend_hash_str_del(BG(user_shutdown_function_names), function_name, function_len) != FAILURE;
	}

	return 0;
}
/* }}} */

PHPAPI zend_bool append_user_shutdown_function(php_shutdown_function_entry shutdown_function_entry) /* {{{ */
{
	if (!BG(user_shutdown_function_names)) {
		ALLOC_HASHTABLE(BG(user_shutdown_function_names));
		zend_hash_init(BG(user_shutdown_function_names), 0, NULL, user_shutdown_function_dtor, 0);
	}

	return zend_hash_next_index_insert_mem(BG(user_shutdown_function_names), &shutdown_function_entry, sizeof(php_shutdown_function_entry)) != NULL;
}
/* }}} */

ZEND_API void php_get_highlight_struct(zend_syntax_highlighter_ini *syntax_highlighter_ini) /* {{{ */
{
	syntax_highlighter_ini->highlight_comment = INI_STR("highlight.comment");
	syntax_highlighter_ini->highlight_default = INI_STR("highlight.default");
	syntax_highlighter_ini->highlight_html    = INI_STR("highlight.html");
	syntax_highlighter_ini->highlight_keyword = INI_STR("highlight.keyword");
	syntax_highlighter_ini->highlight_string  = INI_STR("highlight.string");
}
/* }}} */

/* {{{ proto bool highlight_file(string file_name [, bool return] )
   Syntax highlight a source file */
PHP_FUNCTION(highlight_file)
{
	char *filename;
	size_t filename_len;
	int ret;
	zend_syntax_highlighter_ini syntax_highlighter_ini;
	zend_bool i = 0;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_PATH(filename, filename_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(i)
	ZEND_PARSE_PARAMETERS_END();

	if (php_check_open_basedir(filename)) {
		RETURN_FALSE;
	}

	if (i) {
		php_output_start_default();
	}

	php_get_highlight_struct(&syntax_highlighter_ini);

	ret = highlight_file(filename, &syntax_highlighter_ini);

	if (ret == FAILURE) {
		if (i) {
			php_output_end();
		}
		RETURN_FALSE;
	}

	if (i) {
		php_output_get_contents(return_value);
		php_output_discard();
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto string php_strip_whitespace(string file_name)
   Return source with stripped comments and whitespace */
PHP_FUNCTION(php_strip_whitespace)
{
	char *filename;
	size_t filename_len;
	zend_lex_state original_lex_state;
	zend_file_handle file_handle;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_PATH(filename, filename_len)
	ZEND_PARSE_PARAMETERS_END();

	php_output_start_default();

	zend_stream_init_filename(&file_handle, filename);
	zend_save_lexical_state(&original_lex_state);
	if (open_file_for_scanning(&file_handle) == FAILURE) {
		zend_restore_lexical_state(&original_lex_state);
		php_output_end();
		RETURN_EMPTY_STRING();
	}

	zend_strip();

	zend_destroy_file_handle(&file_handle);
	zend_restore_lexical_state(&original_lex_state);

	php_output_get_contents(return_value);
	php_output_discard();
}
/* }}} */

/* {{{ proto bool highlight_string(string string [, bool return] )
   Syntax highlight a string or optionally return it */
PHP_FUNCTION(highlight_string)
{
	zval *expr;
	zend_syntax_highlighter_ini syntax_highlighter_ini;
	char *hicompiled_string_description;
	zend_bool i = 0;
	int old_error_reporting = EG(error_reporting);

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ZVAL(expr)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(i)
	ZEND_PARSE_PARAMETERS_END();

	if (!try_convert_to_string(expr)) {
		RETURN_THROWS();
	}

	if (i) {
		php_output_start_default();
	}

	EG(error_reporting) = E_ERROR;

	php_get_highlight_struct(&syntax_highlighter_ini);

	hicompiled_string_description = zend_make_compiled_string_description("highlighted code");

	if (highlight_string(expr, &syntax_highlighter_ini, hicompiled_string_description) == FAILURE) {
		efree(hicompiled_string_description);
		EG(error_reporting) = old_error_reporting;
		if (i) {
			php_output_end();
		}
		RETURN_FALSE;
	}
	efree(hicompiled_string_description);

	EG(error_reporting) = old_error_reporting;

	if (i) {
		php_output_get_contents(return_value);
		php_output_discard();
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto string|false ini_get(string varname)
   Get a configuration option */
PHP_FUNCTION(ini_get)
{
	zend_string *varname, *val;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(varname)
	ZEND_PARSE_PARAMETERS_END();

	val = zend_ini_get_value(varname);

	if (!val) {
		RETURN_FALSE;
	}

	if (ZSTR_IS_INTERNED(val)) {
		RETVAL_INTERNED_STR(val);
	} else if (ZSTR_LEN(val) == 0) {
		RETVAL_EMPTY_STRING();
	} else if (ZSTR_LEN(val) == 1) {
		RETVAL_INTERNED_STR(ZSTR_CHAR((zend_uchar)ZSTR_VAL(val)[0]));
	} else if (!(GC_FLAGS(val) & GC_PERSISTENT)) {
		ZVAL_NEW_STR(return_value, zend_string_copy(val));
	} else {
		ZVAL_NEW_STR(return_value, zend_string_init(ZSTR_VAL(val), ZSTR_LEN(val), 0));
	}
}
/* }}} */

/* {{{ proto array|false ini_get_all([string extension[, bool details = true]])
   Get all configuration options */
PHP_FUNCTION(ini_get_all)
{
	char *extname = NULL;
	size_t extname_len = 0, module_number = 0;
	zend_module_entry *module;
	zend_bool details = 1;
	zend_string *key;
	zend_ini_entry *ini_entry;


	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING_OR_NULL(extname, extname_len)
		Z_PARAM_BOOL(details)
	ZEND_PARSE_PARAMETERS_END();

	zend_ini_sort_entries();

	if (extname) {
		if ((module = zend_hash_str_find_ptr(&module_registry, extname, extname_len)) == NULL) {
			php_error_docref(NULL, E_WARNING, "Unable to find extension '%s'", extname);
			RETURN_FALSE;
		}
		module_number = module->module_number;
	}

	array_init(return_value);
	ZEND_HASH_FOREACH_STR_KEY_PTR(EG(ini_directives), key, ini_entry) {
		zval option;

		if (module_number != 0 && ini_entry->module_number != module_number) {
			continue;
		}

		if (key == NULL || ZSTR_VAL(key)[0] != 0) {
			if (details) {
				array_init(&option);

				if (ini_entry->orig_value) {
					add_assoc_str(&option, "global_value", zend_string_copy(ini_entry->orig_value));
				} else if (ini_entry->value) {
					add_assoc_str(&option, "global_value", zend_string_copy(ini_entry->value));
				} else {
					add_assoc_null(&option, "global_value");
				}

				if (ini_entry->value) {
					add_assoc_str(&option, "local_value", zend_string_copy(ini_entry->value));
				} else {
					add_assoc_null(&option, "local_value");
				}

				add_assoc_long(&option, "access", ini_entry->modifiable);

				zend_symtable_update(Z_ARRVAL_P(return_value), ini_entry->name, &option);
			} else {
				if (ini_entry->value) {
					zval zv;

					ZVAL_STR_COPY(&zv, ini_entry->value);
					zend_symtable_update(Z_ARRVAL_P(return_value), ini_entry->name, &zv);
				} else {
					zend_symtable_update(Z_ARRVAL_P(return_value), ini_entry->name, &EG(uninitialized_zval));
				}
			}
		}
	} ZEND_HASH_FOREACH_END();
}
/* }}} */

static int php_ini_check_path(char *option_name, size_t option_len, char *new_option_name, size_t new_option_len) /* {{{ */
{
	if (option_len + 1 != new_option_len) {
		return 0;
	}

	return !strncmp(option_name, new_option_name, option_len);
}
/* }}} */

/* {{{ proto string|false ini_set(string varname, string newvalue)
   Set a configuration option, returns false on error and the old value of the configuration option on success */
PHP_FUNCTION(ini_set)
{
	zend_string *varname;
	zend_string *new_value;
	zend_string *val;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(varname)
		Z_PARAM_STR(new_value)
	ZEND_PARSE_PARAMETERS_END();

	val = zend_ini_get_value(varname);

	/* copy to return here, because alter might free it! */
	if (val) {
		if (ZSTR_IS_INTERNED(val)) {
			RETVAL_INTERNED_STR(val);
		} else if (ZSTR_LEN(val) == 0) {
			RETVAL_EMPTY_STRING();
		} else if (ZSTR_LEN(val) == 1) {
			RETVAL_INTERNED_STR(ZSTR_CHAR((zend_uchar)ZSTR_VAL(val)[0]));
		} else if (!(GC_FLAGS(val) & GC_PERSISTENT)) {
			ZVAL_NEW_STR(return_value, zend_string_copy(val));
		} else {
			ZVAL_NEW_STR(return_value, zend_string_init(ZSTR_VAL(val), ZSTR_LEN(val), 0));
		}
	} else {
		RETVAL_FALSE;
	}

#define _CHECK_PATH(var, var_len, ini) php_ini_check_path(var, var_len, ini, sizeof(ini))
	/* open basedir check */
	if (PG(open_basedir)) {
		if (_CHECK_PATH(ZSTR_VAL(varname), ZSTR_LEN(varname), "error_log") ||
			_CHECK_PATH(ZSTR_VAL(varname), ZSTR_LEN(varname), "java.class.path") ||
			_CHECK_PATH(ZSTR_VAL(varname), ZSTR_LEN(varname), "java.home") ||
			_CHECK_PATH(ZSTR_VAL(varname), ZSTR_LEN(varname), "mail.log") ||
			_CHECK_PATH(ZSTR_VAL(varname), ZSTR_LEN(varname), "java.library.path") ||
			_CHECK_PATH(ZSTR_VAL(varname), ZSTR_LEN(varname), "vpopmail.directory")) {
			if (php_check_open_basedir(ZSTR_VAL(new_value))) {
				zval_ptr_dtor_str(return_value);
				RETURN_FALSE;
			}
		}
	}
#undef _CHECK_PATH

	if (zend_alter_ini_entry_ex(varname, new_value, PHP_INI_USER, PHP_INI_STAGE_RUNTIME, 0) == FAILURE) {
		zval_ptr_dtor_str(return_value);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto void ini_restore(string varname)
   Restore the value of a configuration option specified by varname */
PHP_FUNCTION(ini_restore)
{
	zend_string *varname;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(varname)
	ZEND_PARSE_PARAMETERS_END();

	zend_restore_ini_entry(varname, PHP_INI_STAGE_RUNTIME);
}
/* }}} */

/* {{{ proto string|false set_include_path(string new_include_path)
   Sets the include_path configuration option */
PHP_FUNCTION(set_include_path)
{
	zend_string *new_value;
	char *old_value;
	zend_string *key;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_PATH_STR(new_value)
	ZEND_PARSE_PARAMETERS_END();

	old_value = zend_ini_string("include_path", sizeof("include_path") - 1, 0);
	/* copy to return here, because alter might free it! */
	if (old_value) {
		RETVAL_STRING(old_value);
	} else {
		RETVAL_FALSE;
	}

	key = zend_string_init("include_path", sizeof("include_path") - 1, 0);
	if (zend_alter_ini_entry_ex(key, new_value, PHP_INI_USER, PHP_INI_STAGE_RUNTIME, 0) == FAILURE) {
		zend_string_release_ex(key, 0);
		zval_ptr_dtor_str(return_value);
		RETURN_FALSE;
	}
	zend_string_release_ex(key, 0);
}
/* }}} */

/* {{{ proto string|false get_include_path()
   Get the current include_path configuration option */
PHP_FUNCTION(get_include_path)
{
	char *str;

	ZEND_PARSE_PARAMETERS_NONE();

	str = zend_ini_string("include_path", sizeof("include_path") - 1, 0);

	if (str == NULL) {
		RETURN_FALSE;
	}

	RETURN_STRING(str);
}
/* }}} */

/* {{{ proto mixed print_r(mixed var [, bool return])
   Prints out or returns information about the specified variable */
PHP_FUNCTION(print_r)
{
	zval *var;
	zend_bool do_return = 0;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ZVAL(var)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(do_return)
	ZEND_PARSE_PARAMETERS_END();

	if (do_return) {
		RETURN_STR(zend_print_zval_r_to_str(var, 0));
	} else {
		zend_print_zval_r(var, 0);
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto int connection_aborted(void)
   Returns true if client disconnected */
PHP_FUNCTION(connection_aborted)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG(PG(connection_status) & PHP_CONNECTION_ABORTED);
}
/* }}} */

/* {{{ proto int connection_status(void)
   Returns the connection status bitfield */
PHP_FUNCTION(connection_status)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG(PG(connection_status));
}
/* }}} */

/* {{{ proto int ignore_user_abort([bool value])
   Set whether we want to ignore a user abort event or not */
PHP_FUNCTION(ignore_user_abort)
{
	zend_bool arg = 0;
	int old_setting;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(arg)
	ZEND_PARSE_PARAMETERS_END();

	old_setting = (unsigned short)PG(ignore_user_abort);

	if (ZEND_NUM_ARGS()) {
		zend_string *key = zend_string_init("ignore_user_abort", sizeof("ignore_user_abort") - 1, 0);
		zend_alter_ini_entry_chars(key, arg ? "1" : "0", 1, PHP_INI_USER, PHP_INI_STAGE_RUNTIME);
		zend_string_release_ex(key, 0);
	}

	RETURN_LONG(old_setting);
}
/* }}} */

#if HAVE_GETSERVBYNAME
/* {{{ proto int|false getservbyname(string service, string protocol)
   Returns port associated with service. Protocol must be "tcp" or "udp" */
PHP_FUNCTION(getservbyname)
{
	char *name, *proto;
	size_t name_len, proto_len;
	struct servent *serv;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(name, name_len)
		Z_PARAM_STRING(proto, proto_len)
	ZEND_PARSE_PARAMETERS_END();


/* empty string behaves like NULL on windows implementation of
   getservbyname. Let be portable instead. */
#ifdef WIN32
	if (proto_len == 0) {
		RETURN_FALSE;
	}
#endif

	serv = getservbyname(name, proto);

#if defined(_AIX)
	/*
        On AIX, imap is only known as imap2 in /etc/services, while on Linux imap is an alias for imap2.
        If a request for imap gives no result, we try again with imap2.
        */
	if (serv == NULL && strcmp(name,  "imap") == 0) {
		serv = getservbyname("imap2", proto);
	}
#endif
	if (serv == NULL) {
		RETURN_FALSE;
	}

	RETURN_LONG(ntohs(serv->s_port));
}
/* }}} */
#endif

#if HAVE_GETSERVBYPORT
/* {{{ proto string|false getservbyport(int port, string protocol)
   Returns service name associated with port. Protocol must be "tcp" or "udp" */
PHP_FUNCTION(getservbyport)
{
	char *proto;
	size_t proto_len;
	zend_long port;
	struct servent *serv;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(port)
		Z_PARAM_STRING(proto, proto_len)
	ZEND_PARSE_PARAMETERS_END();

	serv = getservbyport(htons((unsigned short) port), proto);

	if (serv == NULL) {
		RETURN_FALSE;
	}

	RETURN_STRING(serv->s_name);
}
/* }}} */
#endif

#if HAVE_GETPROTOBYNAME
/* {{{ proto int|false getprotobyname(string name)
   Returns protocol number associated with name as per /etc/protocols */
PHP_FUNCTION(getprotobyname)
{
	char *name;
	size_t name_len;
	struct protoent *ent;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	ent = getprotobyname(name);

	if (ent == NULL) {
		RETURN_FALSE;
	}

	RETURN_LONG(ent->p_proto);
}
/* }}} */
#endif

#if HAVE_GETPROTOBYNUMBER
/* {{{ proto string|false getprotobynumber(int proto)
   Returns protocol name associated with protocol number proto */
PHP_FUNCTION(getprotobynumber)
{
	zend_long proto;
	struct protoent *ent;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(proto)
	ZEND_PARSE_PARAMETERS_END();

	ent = getprotobynumber((int)proto);

	if (ent == NULL) {
		RETURN_FALSE;
	}

	RETURN_STRING(ent->p_name);
}
/* }}} */
#endif

/* {{{ proto bool register_tick_function(string function_name [, mixed arg [, mixed ... ]])
   Registers a tick callback function */
PHP_FUNCTION(register_tick_function)
{
	user_tick_function_entry tick_fe;
	int i;
	zend_string *function_name = NULL;

	tick_fe.calling = 0;
	tick_fe.arg_count = ZEND_NUM_ARGS();

	if (tick_fe.arg_count < 1) {
		WRONG_PARAM_COUNT;
	}

	tick_fe.arguments = (zval *) safe_emalloc(sizeof(zval), tick_fe.arg_count, 0);

	if (zend_get_parameters_array(ZEND_NUM_ARGS(), tick_fe.arg_count, tick_fe.arguments) == FAILURE) {
		efree(tick_fe.arguments);
		RETURN_FALSE;
	}

	if (!zend_is_callable(&tick_fe.arguments[0], 0, &function_name)) {
		efree(tick_fe.arguments);
		zend_argument_type_error(1, "must be a valid tick callback, '%s' given", ZSTR_VAL(function_name));
		zend_string_release_ex(function_name, 0);
		RETURN_THROWS();
	} else if (function_name) {
		zend_string_release_ex(function_name, 0);
	}

	if (Z_TYPE(tick_fe.arguments[0]) != IS_ARRAY && Z_TYPE(tick_fe.arguments[0]) != IS_OBJECT) {
		convert_to_string_ex(&tick_fe.arguments[0]);
	}

	if (!BG(user_tick_functions)) {
		BG(user_tick_functions) = (zend_llist *) emalloc(sizeof(zend_llist));
		zend_llist_init(BG(user_tick_functions),
						sizeof(user_tick_function_entry),
						(llist_dtor_func_t) user_tick_function_dtor, 0);
		php_add_tick_function(run_user_tick_functions, NULL);
	}

	for (i = 0; i < tick_fe.arg_count; i++) {
		Z_TRY_ADDREF(tick_fe.arguments[i]);
	}

	zend_llist_add_element(BG(user_tick_functions), &tick_fe);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto void unregister_tick_function(string function_name)
   Unregisters a tick callback function */
PHP_FUNCTION(unregister_tick_function)
{
	zval *function;
	user_tick_function_entry tick_fe;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(function)
	ZEND_PARSE_PARAMETERS_END();

	if (!BG(user_tick_functions)) {
		return;
	}

	if (Z_TYPE_P(function) != IS_ARRAY && Z_TYPE_P(function) != IS_OBJECT) {
		convert_to_string(function);
	}

	tick_fe.arguments = (zval *) emalloc(sizeof(zval));
	ZVAL_COPY_VALUE(&tick_fe.arguments[0], function);
	tick_fe.arg_count = 1;
	zend_llist_del_element(BG(user_tick_functions), &tick_fe, (int (*)(void *, void *)) user_tick_function_compare);
	efree(tick_fe.arguments);
}
/* }}} */

/* {{{ proto bool is_uploaded_file(string path)
   Check if file was created by rfc1867 upload */
PHP_FUNCTION(is_uploaded_file)
{
	char *path;
	size_t path_len;

	if (!SG(rfc1867_uploaded_files)) {
		RETURN_FALSE;
	}

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_PATH(path, path_len)
	ZEND_PARSE_PARAMETERS_END();

	if (zend_hash_str_exists(SG(rfc1867_uploaded_files), path, path_len)) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto bool move_uploaded_file(string path, string new_path)
   Move a file if and only if it was created by an upload */
PHP_FUNCTION(move_uploaded_file)
{
	char *path, *new_path;
	size_t path_len, new_path_len;
	zend_bool successful = 0;

#ifndef WIN32
	int oldmask; int ret;
#endif

	if (!SG(rfc1867_uploaded_files)) {
		RETURN_FALSE;
	}

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(path, path_len)
		Z_PARAM_PATH(new_path, new_path_len)
	ZEND_PARSE_PARAMETERS_END();

	if (!zend_hash_str_exists(SG(rfc1867_uploaded_files), path, path_len)) {
		RETURN_FALSE;
	}

	if (php_check_open_basedir(new_path)) {
		RETURN_FALSE;
	}

	if (VCWD_RENAME(path, new_path) == 0) {
		successful = 1;
#ifndef WIN32
		oldmask = umask(077);
		umask(oldmask);

		ret = VCWD_CHMOD(new_path, 0666 & ~oldmask);

		if (ret == -1) {
			php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
		}
#endif
	} else if (php_copy_file_ex(path, new_path, STREAM_DISABLE_OPEN_BASEDIR) == SUCCESS) {
		VCWD_UNLINK(path);
		successful = 1;
	}

	if (successful) {
		zend_hash_str_del(SG(rfc1867_uploaded_files), path, path_len);
	} else {
		php_error_docref(NULL, E_WARNING, "Unable to move '%s' to '%s'", path, new_path);
	}

	RETURN_BOOL(successful);
}
/* }}} */

/* {{{ php_simple_ini_parser_cb
 */
static void php_simple_ini_parser_cb(zval *arg1, zval *arg2, zval *arg3, int callback_type, zval *arr)
{
	switch (callback_type) {

		case ZEND_INI_PARSER_ENTRY:
			if (!arg2) {
				/* bare string - nothing to do */
				break;
			}
			Z_TRY_ADDREF_P(arg2);
			zend_symtable_update(Z_ARRVAL_P(arr), Z_STR_P(arg1), arg2);
			break;

		case ZEND_INI_PARSER_POP_ENTRY:
		{
			zval hash, *find_hash;

			if (!arg2) {
				/* bare string - nothing to do */
				break;
			}

			if (!(Z_STRLEN_P(arg1) > 1 && Z_STRVAL_P(arg1)[0] == '0') && is_numeric_string(Z_STRVAL_P(arg1), Z_STRLEN_P(arg1), NULL, NULL, 0) == IS_LONG) {
				zend_ulong key = (zend_ulong) zend_atol(Z_STRVAL_P(arg1), Z_STRLEN_P(arg1));
				if ((find_hash = zend_hash_index_find(Z_ARRVAL_P(arr), key)) == NULL) {
					array_init(&hash);
					find_hash = zend_hash_index_add_new(Z_ARRVAL_P(arr), key, &hash);
				}
			} else {
				if ((find_hash = zend_hash_find(Z_ARRVAL_P(arr), Z_STR_P(arg1))) == NULL) {
					array_init(&hash);
					find_hash = zend_hash_add_new(Z_ARRVAL_P(arr), Z_STR_P(arg1), &hash);
				}
			}

			if (Z_TYPE_P(find_hash) != IS_ARRAY) {
				zval_ptr_dtor_nogc(find_hash);
				array_init(find_hash);
			}

			if (!arg3 || (Z_TYPE_P(arg3) == IS_STRING && Z_STRLEN_P(arg3) == 0)) {
				Z_TRY_ADDREF_P(arg2);
				add_next_index_zval(find_hash, arg2);
			} else {
				array_set_zval_key(Z_ARRVAL_P(find_hash), arg3, arg2);
			}
		}
		break;

		case ZEND_INI_PARSER_SECTION:
			break;
	}
}
/* }}} */

/* {{{ php_ini_parser_cb_with_sections
 */
static void php_ini_parser_cb_with_sections(zval *arg1, zval *arg2, zval *arg3, int callback_type, zval *arr)
{
	if (callback_type == ZEND_INI_PARSER_SECTION) {
		array_init(&BG(active_ini_file_section));
		zend_symtable_update(Z_ARRVAL_P(arr), Z_STR_P(arg1), &BG(active_ini_file_section));
	} else if (arg2) {
		zval *active_arr;

		if (Z_TYPE(BG(active_ini_file_section)) != IS_UNDEF) {
			active_arr = &BG(active_ini_file_section);
		} else {
			active_arr = arr;
		}

		php_simple_ini_parser_cb(arg1, arg2, arg3, callback_type, active_arr);
	}
}
/* }}} */

/* {{{ proto array|false parse_ini_file(string filename [, bool process_sections [, int scanner_mode]])
   Parse configuration file */
PHP_FUNCTION(parse_ini_file)
{
	char *filename = NULL;
	size_t filename_len = 0;
	zend_bool process_sections = 0;
	zend_long scanner_mode = ZEND_INI_SCANNER_NORMAL;
	zend_file_handle fh;
	zend_ini_parser_cb_t ini_parser_cb;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_PATH(filename, filename_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(process_sections)
		Z_PARAM_LONG(scanner_mode)
	ZEND_PARSE_PARAMETERS_END();

	if (filename_len == 0) {
		php_error_docref(NULL, E_WARNING, "Filename cannot be empty!");
		RETURN_FALSE;
	}

	/* Set callback function */
	if (process_sections) {
		ZVAL_UNDEF(&BG(active_ini_file_section));
		ini_parser_cb = (zend_ini_parser_cb_t) php_ini_parser_cb_with_sections;
	} else {
		ini_parser_cb = (zend_ini_parser_cb_t) php_simple_ini_parser_cb;
	}

	/* Setup filehandle */
	zend_stream_init_filename(&fh, filename);

	array_init(return_value);
	if (zend_parse_ini_file(&fh, 0, (int)scanner_mode, ini_parser_cb, return_value) == FAILURE) {
		zend_array_destroy(Z_ARR_P(return_value));
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array|false parse_ini_string(string ini_string [, bool process_sections [, int scanner_mode]])
   Parse configuration string */
PHP_FUNCTION(parse_ini_string)
{
	char *string = NULL, *str = NULL;
	size_t str_len = 0;
	zend_bool process_sections = 0;
	zend_long scanner_mode = ZEND_INI_SCANNER_NORMAL;
	zend_ini_parser_cb_t ini_parser_cb;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_STRING(str, str_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(process_sections)
		Z_PARAM_LONG(scanner_mode)
	ZEND_PARSE_PARAMETERS_END();

	if (INT_MAX - str_len < ZEND_MMAP_AHEAD) {
		RETVAL_FALSE;
	}

	/* Set callback function */
	if (process_sections) {
		ZVAL_UNDEF(&BG(active_ini_file_section));
		ini_parser_cb = (zend_ini_parser_cb_t) php_ini_parser_cb_with_sections;
	} else {
		ini_parser_cb = (zend_ini_parser_cb_t) php_simple_ini_parser_cb;
	}

	/* Setup string */
	string = (char *) emalloc(str_len + ZEND_MMAP_AHEAD);
	memcpy(string, str, str_len);
	memset(string + str_len, 0, ZEND_MMAP_AHEAD);

	array_init(return_value);
	if (zend_parse_ini_string(string, 0, (int)scanner_mode, ini_parser_cb, return_value) == FAILURE) {
		zend_array_destroy(Z_ARR_P(return_value));
		RETVAL_FALSE;
	}
	efree(string);
}
/* }}} */

#if ZEND_DEBUG
/* This function returns an array of ALL valid ini options with values and
 *  is not the same as ini_get_all() which returns only registered ini options. Only useful for devs to debug php.ini scanner/parser! */
PHP_FUNCTION(config_get_hash) /* {{{ */
{
	ZEND_PARSE_PARAMETERS_NONE();

	HashTable *hash = php_ini_get_configuration_hash();

	array_init(return_value);
	add_config_entries(hash, return_value);
}
/* }}} */
#endif

#ifdef HAVE_GETLOADAVG
/* {{{ proto array|false sys_getloadavg()
*/
PHP_FUNCTION(sys_getloadavg)
{
	double load[3];

	ZEND_PARSE_PARAMETERS_NONE();

	if (getloadavg(load, 3) == -1) {
		RETURN_FALSE;
	} else {
		array_init(return_value);
		add_index_double(return_value, 0, load[0]);
		add_index_double(return_value, 1, load[1]);
		add_index_double(return_value, 2, load[2]);
	}
}
/* }}} */
#endif
