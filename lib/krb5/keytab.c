/*
 * Copyright (c) 1997, 1998, 1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      H�gskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"

RCSID("$Id$");

static struct krb5_keytab_data *kt_types;
static int num_kt_types;

krb5_error_code
krb5_kt_register(krb5_context context,
		 const krb5_kt_ops *ops)
{
    struct krb5_keytab_data *tmp;
    tmp = realloc(kt_types, (num_kt_types + 1) * sizeof(*kt_types));
    if(tmp == NULL)
	return ENOMEM;
    memcpy(&tmp[num_kt_types], ops, sizeof(tmp[num_kt_types]));
    kt_types = tmp;
    num_kt_types++;
    return 0;
}

krb5_error_code
krb5_kt_resolve(krb5_context context,
		const char *name,
		krb5_keytab *id)
{
    krb5_keytab k;
    int i;
    const char *type, *residual;
    size_t type_len;
    krb5_error_code ret;

    /* register default set of types */
    if(num_kt_types == 0) {
	ret = krb5_kt_register(context, &krb5_fkt_ops);
	if(ret) return ret;
	ret = krb5_kt_register(context, &krb5_mkt_ops);
	if(ret) return ret;
    }
    
    residual = strchr(name, ':');
    if(residual == NULL) {
	type = "FILE";
	type_len = strlen(type);
	residual = name;
    } else {
	type = name;
	type_len = residual - name;
	residual++;
    }
    
    for(i = 0; i < num_kt_types; i++) {
	if(strncmp(type, kt_types[i].prefix, type_len) == 0)
	    break;
    }
    if(i == num_kt_types)
	return KRB5_KT_UNKNOWN_TYPE;
    
    ALLOC(k, 1);
    memcpy(k, &kt_types[i], sizeof(*k));
    k->data = NULL;
    ret = (*k->resolve)(context, residual, k);
    if(ret) {
	free(k);
	k = NULL;
    }
    *id = k;
    return ret;
}

krb5_error_code
krb5_kt_default_name(krb5_context context, char *name, size_t namesize)
{
    strncpy(name, context->default_keytab, namesize);
    if(strlen(context->default_keytab) >= namesize)
	return KRB5_CONFIG_NOTENUFSPACE;
    return 0;
}

krb5_error_code
krb5_kt_default(krb5_context context, krb5_keytab *id)
{
    return krb5_kt_resolve (context, context->default_keytab, id);
}

krb5_error_code
krb5_kt_read_service_key(krb5_context context,
			 krb5_pointer keyprocarg,
			 krb5_principal principal,
			 krb5_kvno vno,
			 krb5_enctype enctype,
			 krb5_keyblock **key)
{
    krb5_keytab keytab;
    krb5_keytab_entry entry;
    krb5_error_code r;

    if (keyprocarg)
	r = krb5_kt_resolve (context, keyprocarg, &keytab);
    else
	r = krb5_kt_default (context, &keytab);

    if (r)
	return r;

    r = krb5_kt_get_entry (context, keytab, principal, vno, enctype, &entry);
    krb5_kt_close (context, keytab);
    if (r)
	return r;
    r = krb5_copy_keyblock (context, &entry.keyblock, key);
    krb5_kt_free_entry(context, &entry);
    return r;
}

krb5_error_code
krb5_kt_remove_entry(krb5_context context,
		     krb5_keytab id,
		     krb5_keytab_entry *entry)
{
    if(id->remove == NULL)
	return KRB5_KT_NOWRITE;
    return (*id->remove)(context, id, entry);
}

krb5_error_code
krb5_kt_get_name(krb5_context context, 
		 krb5_keytab keytab,
		 char *name,
		 size_t namesize)
{
    return (*keytab->get_name)(context, keytab, name, namesize);
}

krb5_error_code
krb5_kt_close(krb5_context context, 
	      krb5_keytab id)
{
    krb5_error_code ret;
    ret = (*id->close)(context, id);
    if(ret == 0)
	free(id);
    return ret;
}

krb5_boolean
krb5_kt_compare(krb5_context context,
		krb5_keytab_entry *entry, 
		krb5_const_principal principal,
		krb5_kvno vno,
		krb5_enctype enctype)
{
    if(principal != NULL && 
       !krb5_principal_compare(context, entry->principal, principal))
	return FALSE;
    if(vno && vno != entry->vno)
	return FALSE;
    if(enctype && enctype != entry->keyblock.keytype)
	return FALSE;
    return TRUE;
}

krb5_error_code
krb5_kt_get_entry(krb5_context context,
		  krb5_keytab id,
		  krb5_const_principal principal,
		  krb5_kvno kvno,
		  krb5_enctype enctype,
		  krb5_keytab_entry *entry)
{
    krb5_keytab_entry tmp;
    krb5_error_code r;
    krb5_kt_cursor cursor;

    if(id->get) return (*id->get)(context, id, principal, kvno, enctype, entry);

    r = krb5_kt_start_seq_get (context, id, &cursor);
    if (r)
	return KRB5_KT_NOTFOUND; /* XXX i.e. file not found */

    entry->vno = 0;
    while (krb5_kt_next_entry(context, id, &tmp, &cursor) == 0) {
	if (krb5_kt_compare(context, &tmp, principal, 0, enctype)) {
	    if (kvno == tmp.vno) {
		krb5_kt_copy_entry_contents (context, &tmp, entry);
		krb5_kt_free_entry (context, &tmp);
		krb5_kt_end_seq_get(context, id, &cursor);
		return 0;
	    } else if (kvno == 0 && tmp.vno > entry->vno) {
		if (entry->vno)
		    krb5_kt_free_entry (context, entry);
		krb5_kt_copy_entry_contents (context, &tmp, entry);
	    }
	}
	krb5_kt_free_entry(context, &tmp);
    }
    krb5_kt_end_seq_get (context, id, &cursor);
    if (entry->vno)
	return 0;
    else
	return KRB5_KT_NOTFOUND;
}

krb5_error_code
krb5_kt_copy_entry_contents(krb5_context context,
			    const krb5_keytab_entry *in,
			    krb5_keytab_entry *out)
{
    krb5_error_code ret;

    memset(out, 0, sizeof(*out));
    out->vno = in->vno;

    ret = krb5_copy_principal (context, in->principal, &out->principal);
    if (ret)
	goto fail;
    ret = krb5_copy_keyblock_contents (context,
				       &in->keyblock,
				       &out->keyblock);
    if (ret)
	goto fail;
    return 0;
fail:
    krb5_kt_free_entry (context, out);
    return ret;
}

krb5_error_code
krb5_kt_free_entry(krb5_context context,
		   krb5_keytab_entry *entry)
{
  krb5_free_principal (context, entry->principal);
  krb5_free_keyblock_contents (context, &entry->keyblock);
  return 0;
}

#if 0
static int
xxxlock(int fd, int write)
{
    if(flock(fd, (write ? LOCK_EX : LOCK_SH) | LOCK_NB) < 0) {
	sleep(1);
	if(flock(fd, (write ? LOCK_EX : LOCK_SH) | LOCK_NB) < 0) 
	    return -1;
    }
    return 0;
}

static void
xxxunlock(int fd)
{
    flock(fd, LOCK_UN);
}
#endif

krb5_error_code
krb5_kt_start_seq_get(krb5_context context,
		      krb5_keytab id,
		      krb5_kt_cursor *cursor)
{
    if(id->start_seq_get == NULL)
	return HEIM_ERR_OPNOTSUPP;
    return (*id->start_seq_get)(context, id, cursor);
}

krb5_error_code
krb5_kt_next_entry(krb5_context context,
		   krb5_keytab id,
		   krb5_keytab_entry *entry,
		   krb5_kt_cursor *cursor)
{
    if(id->next_entry)
	return HEIM_ERR_OPNOTSUPP;
    return (*id->next_entry)(context, id, entry, cursor);
}


krb5_error_code
krb5_kt_end_seq_get(krb5_context context,
		    krb5_keytab id,
		    krb5_kt_cursor *cursor)
{
    if(id->end_seq_get)
	return HEIM_ERR_OPNOTSUPP;
    return (*id->end_seq_get)(context, id, cursor);
}

krb5_error_code
krb5_kt_add_entry(krb5_context context,
		  krb5_keytab id,
		  krb5_keytab_entry *entry)
{
    if(id->add == NULL)
	return KRB5_KT_NOWRITE;
    return (*id->add)(context, id,entry);
}
