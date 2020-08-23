/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2018 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_mprint.h"
#include "zend.h"
#include "zend_smart_str.h"


#define MPRINT_INDENT 4
static void mprint_zval_to_buf(zval *var,smart_str *buf,int indent);
/*
typedef struct {
	zend_string *s;
	size_t a;
} smart_str;	

*/

/*
PHP_FUNCTION(runkit7_zval_inspect)
{
	// TODO: Specify what this should do for php7 (e.g. primitives are no longer refcounted)
	zval *value;
	char *addr;
	int addr_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &value) == FAILURE) {
		return;
	}

	array_init(return_value);

	addr_len = spprintf(&addr, 0, "0x%0lx", (long)value);
	add_assoc_stringl(return_value, "address", addr, addr_len);
	efree(addr);
	addr = NULL;

	if (Z_REFCOUNTED_P(value)) {
		add_assoc_long(return_value, "refcount", Z_REFCOUNT_P(value));
		add_assoc_bool(return_value, "is_ref", Z_ISREF_P(value));
	}

	add_assoc_long(return_value, "type", Z_TYPE_P(value));
}
*/

static void mprint_zval_base_info(zval *var,smart_str *buf,int indent){
	smart_str_append_printf(buf,"%*czval_base_info:[0x%0lx]\n",indent,' ',(long)var);
	indent+=1;
	smart_str_append_printf(buf,"%*cu1.type_info:%x\n",indent,' ',var->u1.type_info);
	smart_str_append_printf(buf,"%*cu1.v.type:%d\n",indent,' ',var->u1.v.type);
	smart_str_append_printf(buf,"%*cu1.v.type_flags:%x\n",indent,' ',var->u1.v.type_flags);
	smart_str_append_printf(buf,"%*cu1.v.type_flags:%x\n",indent,' ',var->u1.v.const_flags);
	smart_str_append_printf(buf,"%*cu1.v.type_flags:%x\n",indent,' ',var->u1.v.reserved);
	smart_str_append_printf(buf,"%*cu2:%x\n",indent,' ',var->u2.next);
	smart_str_append_printf(buf,"%*cu1.value: ",indent,' ');
}

static void mprint_zend_string(zend_string *str,smart_str *buf,int indent){
	int i;
	char *stub;
	int stub_len;
	
	smart_str_append_printf(buf,"%*cString %s\n",indent,' ',ZSTR_IS_INTERNED(str) ? "[interned]":"");

	for (i = 0; i < indent; i++) {
		smart_str_appendc(buf, ' ');
	}

	smart_str_appends(buf,"(\n");
	stub_len = indent+MPRINT_INDENT;
	ap_php_asprintf(&stub,"%*c",stub_len,' ');
	
	smart_str_appends(buf,stub);
	smart_str_append_printf(buf,"refcounf: %u\n",str->gc.refcount ? str->gc.refcount : 1);
	smart_str_appends(buf,stub);
	smart_str_append_printf(buf,"len: %d\n",str->len);
	smart_str_appends(buf,stub);
	smart_str_append_printf(buf,"val: %s\n",str->val);
	for (i = 0; i < indent; i++) {
		smart_str_appendc(buf, ' ');
	}
	free(stub);
	smart_str_appends(buf,")\n");
}

static void mprint_hash(HashTable *ht,smart_str *buf,int indent,zend_long is_object){
	zval *tmp;
	zend_string *string_key;
	zend_ulong num_key;
	int i;

	for (i = 0; i < indent; i++) {
		smart_str_appendc(buf, ' ');
	}
	smart_str_appends(buf, "(\n");
	indent += MPRINT_INDENT;
	ZEND_HASH_FOREACH_KEY_VAL_IND(ht, num_key, string_key, tmp) {
		for (i = 0; i < indent; i++) {
			smart_str_appendc(buf, ' ');
		}
		smart_str_appendc(buf, '[');
		if (string_key) {
			if (is_object) {
				const char *prop_name, *class_name;
				size_t prop_len;
				int mangled = zend_unmangle_property_name_ex(string_key, &class_name, &prop_name, &prop_len);

				smart_str_appendl(buf, prop_name, prop_len);
				if (class_name && mangled == SUCCESS) {
					if (class_name[0] == '*') {
						smart_str_appends(buf, ":protected");
					} else {
						smart_str_appends(buf, ":");
						smart_str_appends(buf, class_name);
						smart_str_appends(buf, ":private");
					}
				}
			} else {
				smart_str_append(buf, string_key);
			}
		} else {
			smart_str_append_long(buf, num_key);
		}
		smart_str_appends(buf, "] => \n");
		mprint_zval_to_buf(tmp, buf, indent+MPRINT_INDENT);
		smart_str_appends(buf, "\n");
	} ZEND_HASH_FOREACH_END();
	indent -= MPRINT_INDENT;
	for (i = 0; i < indent; i++) {
		smart_str_appendc(buf, ' ');
	}
	smart_str_appends(buf, ")\n");

}


static void mprint_zval_to_buf(zval *var,smart_str *buf,int indent){
	mprint_zval_base_info(var,buf,indent);
	indent +=1;
	switch(Z_TYPE_P(var)){
		case IS_FALSE:
			smart_str_appends(buf,"[False]\n");
			break;
		case IS_TRUE:
			smart_str_appends(buf,"[True]\n");
			break;
		case IS_UNDEF:
			smart_str_appends(buf,"[Undefined]\n");
			break;
		case IS_NULL:
			smart_str_appends(buf,"[Null]\n");
			break;
		case IS_DOUBLE:
			{
				zend_string *str =zval_get_string(var);
				smart_str_appends(buf,"[Double] ");
				smart_str_append(buf,str);
				smart_str_appendc(buf,'\n');
				zend_string_release(str);
			}
			break;
		case IS_LONG:
			{
				zend_string *str =zval_get_string(var);
				smart_str_appends(buf,"[Long] ");
				smart_str_append(buf,str);
				smart_str_appendc(buf,'\n');
				zend_string_release(str);
			}
			break;
		case IS_STRING:
			smart_str_append_printf(buf,"[0x%0lx]\n",(long)(Z_STR_P(var)));
			mprint_zend_string(Z_STR_P(var),buf,indent+1);
			//var->value.str->val[0]='p';
			break;
		case IS_ARRAY:
			smart_str_append_printf(buf,"[0x%0lx]\n",(long)(Z_ARR_P(var)));
			smart_str_append_printf(buf,"%*cArray %s\n",indent+1,' ',HT_IS_PACKED(Z_ARR_P(var)) ? "[packed]" : "");
			if (ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(var)) &&
			    ++Z_ARRVAL_P(var)->u.v.nApplyCount>1) {
				smart_str_appends(buf, " *RECURSION*");
				Z_ARRVAL_P(var)->u.v.nApplyCount--;
				return;
			}
			mprint_hash(Z_ARR_P(var),buf,indent+1,0);
			break;
		case IS_OBJECT:
			{	
				smart_str_append_printf(buf,"[0x%0lx]\n",(long)(Z_OBJ_P(var)));
				HashTable *properties;
				int is_temp;
				smart_str_append_printf(buf,"%*cObject\n",indent+1,' ');
				if (Z_OBJ_APPLY_COUNT_P(var) > 0) {
					smart_str_appends(buf, " *RECURSION*");
					return;
				}
				if ((properties = Z_OBJDEBUG_P(var, is_temp)) == NULL) {
					break;
				}
				mprint_hash(properties,buf,indent+1,1);
			}
			break;
		case IS_REFERENCE://maybe never see ?
			smart_str_appends(buf,"[Reference]\n");
			break;
		case IS_RESOURCE:
			smart_str_appends(buf,"[Resource]\n");
			break;
	}
}


static void mprint_zval_r(zval *var){
	smart_str buf = {0};
	mprint_zval_to_buf(var,&buf,1);
	smart_str_0(&buf);
	ZEND_WRITE(ZSTR_VAL(buf.s),ZSTR_LEN(buf.s));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_local_print, 0, 0, 1)
    ZEND_ARG_INFO(1, var)
ZEND_END_ARG_INFO()

PHP_FUNCTION(local_print)
{
	zval *var;
	

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(var)
	ZEND_PARSE_PARAMETERS_END();

	mprint_zval_r(var);
	
}

PHP_MINIT_FUNCTION(mprint)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mprint)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mprint)
{
#if defined(COMPILE_DL_MPRINT) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mprint)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mprint)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "mprint support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ mprint_functions[]
 *
 * Every user visible function must have an entry in mprint_functions[].
 */
const zend_function_entry mprint_functions[] = {
	PHP_FE(local_print,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in mprint_functions[] */
};
/* }}} */

/* {{{ mprint_module_entry
 */
zend_module_entry mprint_module_entry = {
	STANDARD_MODULE_HEADER,
	"mprint",
	mprint_functions,
	PHP_MINIT(mprint),
	PHP_MSHUTDOWN(mprint),
	PHP_RINIT(mprint),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(mprint),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(mprint),
	PHP_MPRINT_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MPRINT
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(mprint)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
