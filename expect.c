/*
  +----------------------------------------------------------------------+
  | PHP Version 5, 7 and 8                                               |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Michael Spector <michael@php.net>                            |
  +----------------------------------------------------------------------+
*/

/* $ Id: $ */ 

#include "php_expect.h"
#include <string.h>
#include <errno.h>

ZEND_BEGIN_ARG_INFO_EX(arginfo_expect_popen, 0, 0, 1)
	ZEND_ARG_INFO(0, command)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_expect_expectl, 0, 0, 2)
	ZEND_ARG_INFO(0, stream)
	ZEND_ARG_INFO(0, expect_cases)
	ZEND_ARG_INFO(1, match)
ZEND_END_ARG_INFO()

/* {{{ expect_functions[] */
zend_function_entry expect_functions[] = {
	PHP_FE(expect_popen,    arginfo_expect_popen)
	PHP_FE(expect_expectl,  arginfo_expect_expectl)
	{ NULL, NULL, NULL }
};
/* }}} */


ZEND_DECLARE_MODULE_GLOBALS(expect)
static PHP_GINIT_FUNCTION(expect);
static PHP_GSHUTDOWN_FUNCTION(expect);

/* {{{ expect_module_entry
 */
zend_module_entry expect_module_entry = {
	STANDARD_MODULE_HEADER,
	"expect",
	expect_functions,
	PHP_MINIT(expect),
	PHP_MSHUTDOWN(expect),
	NULL,
	NULL,
	PHP_MINFO(expect),
	PHP_EXPECT_VERSION,
	PHP_MODULE_GLOBALS(expect),
	PHP_GINIT(expect),
	PHP_GSHUTDOWN(expect),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_EXPECT
ZEND_GET_MODULE(expect)
#endif

/* {{{ php_expect_init_globals
 */
static PHP_GINIT_FUNCTION(expect)
{
	expect_globals->logfile_stream = NULL;
}
/* }}} */

/* {{{ php_expect_destroy_globals
 */
static PHP_GSHUTDOWN_FUNCTION(expect)
{
	if (expect_globals->logfile_stream) {
		php_stream_close(expect_globals->logfile_stream);
	}
}
/* }}} */

/* {{{ PHP_INI_MH
 *  */
static PHP_INI_MH(OnSetExpectTimeout)
{
	if (new_value) {
#if PHP_MAJOR_VERSION >= 7
		exp_timeout = atoi(ZSTR_VAL(new_value));
#else
        exp_timeout = atoi(new_value);
#endif
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ PHP_INI_MH
 *  */
static PHP_INI_MH(OnSetExpectMatchMax)
{
	if (new_value) {
#if PHP_MAJOR_VERSION >= 7
		exp_match_max = atoi(ZSTR_VAL(new_value));
#else
		exp_match_max = atoi(new_value);
#endif
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */


/* {{{ PHP_INI_MH
 *  */
static PHP_INI_MH(OnSetExpectLogUser)
{
	if (new_value) {
#if PHP_MAJOR_VERSION >= 7
		if (strncasecmp("on", ZSTR_VAL(new_value), sizeof("on")) == 0
			|| strncasecmp("true", ZSTR_VAL(new_value), sizeof("true")) == 0
			|| strncasecmp("yes", ZSTR_VAL(new_value), sizeof("yes")) == 0
			|| strncasecmp("1", ZSTR_VAL(new_value), sizeof("1")) == 0) {
#else
		if (strncasecmp("on", new_value, sizeof("on")) == 0
			|| strncasecmp("true", new_value, sizeof("true")) == 0
			|| strncasecmp("yes", new_value, sizeof("yes")) == 0
			|| strncasecmp("1", new_value, sizeof("1")) == 0) {
#endif
			exp_loguser = 1;
		} else {
			exp_loguser = 0;
		}
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */


/* {{{ PHP_INI_MH
 *  */
static PHP_INI_MH(OnSetExpectLogFile)
{
	if (EXPECT_G(logfile_stream)) {
		php_stream_close(EXPECT_G(logfile_stream));
	}
#if PHP_MAJOR_VERSION >= 7
   if (ZSTR_LEN(new_value) > 0) {
       php_stream* stream = php_stream_open_wrapper (ZSTR_VAL(new_value), "a", 0, NULL);
#else
	if (new_value_length > 0) {
		php_stream* stream = php_stream_open_wrapper (new_value, "a", 0, NULL);
#endif
		if (!stream) {
			php_error_docref (NULL TSRMLS_CC, E_ERROR, "could not open log file for writing");
			return FAILURE;
		}
		stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
		if (php_stream_cast(stream, PHP_STREAM_AS_STDIO, (void **) &exp_logfile, REPORT_ERRORS) != SUCCESS) {
			return FAILURE;
		}
		EXPECT_G(logfile_stream) = stream;
		exp_logfile_all = 1;
	} else {
		EXPECT_G(logfile_stream) = NULL;
		exp_logfile = NULL;
		exp_logfile_all = 0;
	}
	return SUCCESS;
}
/* }}} */


PHP_INI_BEGIN()
	PHP_INI_ENTRY("expect.timeout", "10", PHP_INI_ALL, OnSetExpectTimeout)
	PHP_INI_ENTRY_EX("expect.loguser", "1", PHP_INI_ALL, OnSetExpectLogUser, php_ini_boolean_displayer_cb)
	PHP_INI_ENTRY("expect.logfile", "", PHP_INI_ALL, OnSetExpectLogFile)
	PHP_INI_ENTRY("expect.match_max", "5000", PHP_INI_ALL, OnSetExpectMatchMax)
PHP_INI_END()


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(expect)
{
	php_register_url_stream_wrapper("expect", &php_expect_wrapper TSRMLS_CC);

	REGISTER_LONG_CONSTANT("EXP_GLOB", exp_glob, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXP_EXACT", exp_exact, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXP_REGEXP", exp_regexp, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXP_EOF", EXP_EOF, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXP_TIMEOUT", EXP_TIMEOUT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXP_FULLBUFFER", EXP_FULLBUFFER, CONST_CS | CONST_PERSISTENT);

	REGISTER_INI_ENTRIES();

	Tcl_Interp *interp = Tcl_CreateInterp();
	if (Tcl_Init(interp) == TCL_ERROR) {
		php_error_docref (NULL TSRMLS_CC, E_ERROR, 
			"Unable to initialize TCL interpreter: %s", Tcl_GetStringResult (interp));
		return FAILURE;
	}
	if (Expect_Init(interp) == TCL_ERROR) {
		php_error_docref (NULL TSRMLS_CC, E_ERROR,
			"Unable to initialize Expect: %s", Tcl_GetStringResult (interp));
		return FAILURE;
	}
	
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(expect)
{
	php_unregister_url_stream_wrapper("expect" TSRMLS_CC);

	UNREGISTER_INI_ENTRIES();
	
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(expect)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Expect support", "enabled");
	php_info_print_table_row(2, "Version", PHP_EXPECT_VERSION);
	php_info_print_table_row(2, "Stream wrapper support", "expect://");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */


/* {{{
 * proto resource expect_popen (string command)
 */
PHP_FUNCTION(expect_popen)
{
#if PHP_MAJOR_VERSION >= 7
    zend_string *command = NULL;
#else
	char *command = NULL;
#endif
	int command_len;
	FILE *fp;
	php_stream *stream = NULL;
#if PHP_MAJOR_VERSION >= 7
    zval z_pid;
#else
	zval *z_pid;
#endif

	if (ZEND_NUM_ARGS() != 1) { WRONG_PARAM_COUNT; }

#if PHP_MAJOR_VERSION >= 7
    if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "S", &command) == FAILURE) {
#else
	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "s", &command, &command_len) == FAILURE) {
#endif
		return;
	}

#if PHP_MAJOR_VERSION >= 7
    if ((fp = exp_popen (ZSTR_VAL(command))) != NULL) {
#else
	if ((fp = exp_popen (command)) != NULL) {
#endif
		stream = php_stream_fopen_from_pipe (fp, "");
	}
	if (!stream) {
		RETURN_FALSE;
	}

	stream->flags |= PHP_STREAM_FLAG_NO_SEEK;

#if PHP_MAJOR_VERSION >= 7
    ZVAL_LONG (&z_pid, exp_pid);
#else
	MAKE_STD_ZVAL (z_pid);
	ZVAL_LONG (z_pid, exp_pid);
#endif
	stream->wrapperdata = z_pid;

	php_stream_to_zval(stream, return_value);
}
/* }}} */


/* {{{
 * proto mixed expect_expectl (resource stream, array expect_cases [, array match])
 */
PHP_FUNCTION(expect_expectl)
{
	struct exp_case *ecases, *ecases_ptr, matchedcase;
#if PHP_MAJOR_VERSION >= 7
    zval *z_stream, *z_cases, *z_match=NULL, *z_case, *z_value;
#else
	zval *z_stream, *z_cases, *z_match=NULL, **z_case, **z_value;
#endif
	php_stream *stream;
	int fd, argc;
	ulong key;
	
	if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 3) { WRONG_PARAM_COUNT; }

	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "ra|z", &z_stream, &z_cases, &z_match) == FAILURE) {
		return;
	}

#if PHP_MAJOR_VERSION >= 7
   php_stream_from_zval (stream, z_stream);
#else
	php_stream_from_zval (stream, &z_stream);
#endif

#if PHP_MAJOR_VERSION >= 7
    if (!&(stream->wrapperdata)) {
#else
	if (!stream->wrapperdata) {
#endif
		php_error_docref (NULL TSRMLS_CC, E_ERROR, "supplied argument is not a valid stream resource");
		return;
	}

	if (php_stream_cast (stream, PHP_STREAM_AS_FD, (void*)&fd, REPORT_ERRORS) != SUCCESS || fd < 0) {
		return;
	}

	argc = zend_hash_num_elements (Z_ARRVAL_P(z_cases));
	ecases = (struct exp_case*) safe_emalloc (argc + 1, sizeof(struct exp_case), 0);
	ecases_ptr = ecases;

	zend_hash_internal_pointer_reset (Z_ARRVAL_P(z_cases));

#if PHP_MAJOR_VERSION >= 7
    while ((z_case = zend_hash_get_current_data (Z_ARRVAL_P(z_cases))) != NULL)
    {
        zval *z_pattern, *z_exp_type;
        zend_hash_get_current_key(Z_ARRVAL_P(z_cases), NULL, &key);

        if (Z_TYPE_P(z_case) != IS_ARRAY) {
#else
	while (zend_hash_get_current_data (Z_ARRVAL_P(z_cases), (void **)&z_case) == SUCCESS)
	{
		zval **z_pattern, **z_exp_type;
		zend_hash_get_current_key(Z_ARRVAL_P(z_cases), NULL, &key, 0);

		if (Z_TYPE_PP(z_case) != IS_ARRAY) {
#endif
			efree (ecases);
			php_error_docref (NULL TSRMLS_CC, E_ERROR, "expect case must be an array");
			return;
		}

		ecases_ptr->re = NULL;
		ecases_ptr->type = exp_glob;

		/* Gather pattern */
#if PHP_MAJOR_VERSION >= 7
        if ((z_pattern = zend_hash_index_find(Z_ARRVAL_P(z_case), 0)) == NULL) {
#else
		if (zend_hash_index_find(Z_ARRVAL_PP(z_case), 0, (void **)&z_pattern) != SUCCESS) {
#endif
			efree (ecases);
			php_error_docref (NULL TSRMLS_CC, E_ERROR, "missing parameter for pattern at index: 0");
			return;
		}
#if PHP_MAJOR_VERSION >= 7
        if (Z_TYPE_P(z_pattern) != IS_STRING) {
#else
		if (Z_TYPE_PP(z_pattern) != IS_STRING) {
#endif
			efree (ecases);
			php_error_docref (NULL TSRMLS_CC, E_ERROR, "pattern must be of string type");
			return;
		}
#if PHP_MAJOR_VERSION >= 7
        ecases_ptr->pattern = Z_STRVAL_P(z_pattern);
#else
		ecases_ptr->pattern = Z_STRVAL_PP(z_pattern);
#endif

		/* Gather value */
#if PHP_MAJOR_VERSION >= 7
        if (zend_hash_index_find(Z_ARRVAL_P(z_case), 1) == NULL) {
#else
		if (zend_hash_index_find(Z_ARRVAL_PP(z_case), 1, (void **)&z_value) != SUCCESS) {
#endif
			efree (ecases);
			php_error_docref (NULL TSRMLS_CC, E_ERROR, "missing parameter for value at index: 1");
			return;
		}
		ecases_ptr->value = key;

		/* Gather expression type (optional, default: EXPECT_GLOB) */
#if PHP_MAJOR_VERSION >= 7
        if ((z_exp_type = zend_hash_index_find(Z_ARRVAL_P(z_case), 2)) != NULL) {
            if (Z_TYPE_P(z_exp_type) != IS_LONG) {
#else
		if (zend_hash_index_find(Z_ARRVAL_PP(z_case), 2, (void **)&z_exp_type) == SUCCESS) {
			if (Z_TYPE_PP(z_exp_type) != IS_LONG) {
#endif
				efree (ecases);
				php_error_docref (NULL TSRMLS_CC, E_ERROR, "expression type must be an integer constant");
				return;
			}
#if PHP_MAJOR_VERSION >= 7
            if (Z_LVAL_P(z_exp_type) != exp_glob && Z_LVAL_P(z_exp_type) != exp_exact && Z_LVAL_P(z_exp_type) != exp_regexp) {
#else
			if (Z_LVAL_PP(z_exp_type) != exp_glob && Z_LVAL_PP(z_exp_type) != exp_exact && Z_LVAL_PP(z_exp_type) != exp_regexp) {
#endif
				efree (ecases);
				php_error_docref (NULL TSRMLS_CC, E_ERROR, "expression type must be either EXPECT_GLOB, EXPECT_EXACT or EXPECT_REGEXP");
				return;
			}
#if PHP_MAJOR_VERSION >= 7
            ecases_ptr->type = Z_LVAL_P(z_exp_type);
#else
			ecases_ptr->type = Z_LVAL_PP(z_exp_type);
#endif
		}

		ecases_ptr++;
		zend_hash_move_forward(Z_ARRVAL_P(z_cases));
	}
	ecases_ptr->pattern = NULL;
	ecases_ptr->re = NULL;
	ecases_ptr->value = 0;
	ecases_ptr->type = exp_end;

	int exp_retval = exp_expectv (fd, ecases);
	int case_found = 0;
	if (exp_retval >= 0) {
		key = exp_retval;

		int exp_match_len = exp_match_end - exp_match;
		if (z_match && exp_match && exp_match_len > 0) {
			char *tmp = (char *)emalloc (sizeof(char) * (exp_match_len + 1));
			strlcpy (tmp, exp_match, exp_match_len + 1);
#if PHP_MAJOR_VERSION >= 7
            z_match = zend_try_array_init(z_match);
			if (!z_match) {
				return;
			}
            add_index_string(z_match, 0, tmp);
#else
			zval_dtor (z_match);
			array_init(z_match);
			add_index_string(z_match, 0, tmp, 1);
#endif
			/* Get case that was matched */
			matchedcase = ecases[key];
			/* If there are subpattern matches ... */
			if (matchedcase.re != NULL && matchedcase.re->startp != NULL) {
				int i;
				/* iterate across all possible 9 subpatterns (a limitation of libexpect)
				   and add matching substring to matches array */
				for (i = 1; i <= 9; i++) {
					if (matchedcase.re->startp[i] != NULL) {
						int sub_match_len = matchedcase.re->endp[i] - matchedcase.re->startp[i];
						char *sub_match = (char *)emalloc (sizeof(char) * (sub_match_len + 1));
						strlcpy (sub_match, matchedcase.re->startp[i], sub_match_len + 1);
#if PHP_MAJOR_VERSION >= 7
                        add_next_index_string(z_match, sub_match);
#else
						add_next_index_string(z_match, sub_match, 1);
#endif
						efree (sub_match);
					}
				}
			}
			efree (tmp);
		}

#if PHP_MAJOR_VERSION >= 7
        if ((z_case = zend_hash_index_find (Z_ARRVAL_P(z_cases), key)) != NULL) {
            if ((z_value = zend_hash_index_find(Z_ARRVAL_P(z_case), 1)) != NULL) {
                *return_value = *z_value;
#else
		if (zend_hash_index_find (Z_ARRVAL_P(z_cases), key, (void **)&z_case) == SUCCESS) {
			if (zend_hash_index_find(Z_ARRVAL_PP(z_case), 1, (void **)&z_value) == SUCCESS) {
				*return_value = **z_value;
#endif
				zval_copy_ctor (return_value);
				case_found = 1;
			}
		}
	}

	// Free compiled patterns:
	ecases_ptr = ecases;
	while (ecases_ptr != NULL && ecases_ptr->type != exp_end) {
		if (ecases_ptr->re != NULL) {
			free(ecases_ptr->re);
		}
		ecases_ptr++;
	}
	efree (ecases);

	if (!case_found) {
		RETURN_LONG(exp_retval);
	}
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
