/*:ts=8*/
/*****************************************************************************
 * FIDOGATE --- Gateway UNIX Mail/News <-> FIDO NetMail/EchoMail
 *
 *
 * Functions to process RFC822 header lines from messages
 *
 *****************************************************************************
 * Copyright (C) 1990-2002
 *  _____ _____
 * |     |___  |   Martin Junius             <mj@fidogate.org>
 * | | | |   | |   Radiumstr. 18
 * |_|_|_|@home|   D-51069 Koeln, Germany
 *
 * This file is part of FIDOGATE.
 *
 * FIDOGATE is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * FIDOGATE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FIDOGATE; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "fidogate.h"

/*
 * header_ca_rfc() --- Output ^ARFC-Xxxx kludges
 */
void header_ca_rfc(RFCHeader *header, FILE * out, int rfc_level)
{
    static char *rfc_lvl_1[] = { RFC_LVL_1_HEADERS, NULL };
    static char *rfc_lvl_3[] = { RFC_LVL_3_HEADERS, NULL };
    Textlist *headers = &header->headers;

    /* RFC level 0 - no ^ARFC-Xxxx kludges */
    if (rfc_level <= 0) {
        return;
    }

    /* RFC level 1 - selected set of ^ARFC-Xxxx kludges */
    else if (rfc_level == 1) {
        char **name;
        Textline *p;
        int len;
        int ok = FALSE;

        for (p = headers->first; p; p = p->next) {
            if (*p->line && !is_space(p->line[0])) {
                ok = FALSE;
                for (name = rfc_lvl_1; *name; name++) {
                    len = strlen(*name);
                    if (!strnicmp(p->line, *name, len)
                        && (p->line[len] == ':' || p->line[len] == ' ')) {
                        ok = TRUE;  /* OK to output */
                        break;
                    }
                }
            }
            if (ok)
#ifndef RECODE_ALL_RFC
                fprintf(out, "\001RFC-%s\r\n", p->line);
#else
                fprintf(out, "\001RFC-%s\r\n", xlat_s(p->line, NULL));
#endif                          /* RECODE_ALL_RFC */
        }
    }

    /* RFC level 2 - all ^ARFC-Xxxx kludges */
    else if (rfc_level == 2) {
        Textline *p;
        char *crlf;
        int i;

        crlf = "";
        for (p = headers->first; p; p = p->next) {
            if (*p->line && !is_space(p->line[0])) {
#ifndef RECODE_ALL_RFC
                fprintf(out, "%s\001RFC-%s", crlf, p->line);
#else
                fprintf(out, "%s\001RFC-%s", crlf, xlat_s(p->line, NULL));
#endif                          /* RECODE_ALL_RFC */
            } else {
                for (i = 0; is_space(p->line[i]); i++) ;
                fprintf(out, " %s", &(p->line[i]));
            }
            crlf = "\r\n";
        }
        fprintf(out, "%s", crlf);
    }

    /* RFC level 3 - all ^ARFC-Xxxx kludges, excluding some */
    else if (rfc_level >= 3) {
        Textline *p;
        char **name;
        int len;
        int ok = FALSE;
        char *crlf;
        int i;

        crlf = "";
        for (p = headers->first; p; p = p->next) {
            if (*p->line && !is_space(p->line[0])) {
                ok = FALSE;
                for (name = rfc_lvl_3; *name; name++) {
                    len = strlen(*name);
                    if (!strnicmp(p->line, *name, len)
                        && (p->line[len] == ':' || p->line[len] == ' ')) {
                        ok = TRUE;
                        break;
                    }
                }
                if (ok)
#ifndef RECODE_ALL_RFC
                    fprintf(out, "%s\001RFC-%s", crlf, p->line);
#else
                    fprintf(out, "%s\001RFC-%s", crlf, xlat_s(p->line, NULL));
#endif                          /* RECODE_ALL_RFC */

            } else {
                if (ok) {
                    for (i = 0; is_space(p->line[i]); i++) ;
                    fprintf(out, " %s", &(p->line[i]));
                }
            }
            if (ok)
                crlf = "\r\n";
        }
        fprintf(out, "%s", crlf);
    }

    return;
}

void header_free(RFCHeader *header)
{
    tl_clear(&header->headers);
    free(header);
}

static RFCHeader *header_new(Textlist *tl)
{
    RFCHeader *header;

    header = xmalloc(sizeof(*header));
    memset(header, 0, sizeof(*header));

    if (tl != NULL)
        header->headers = *tl;

    return header;
}

static void header_append(RFCHeader *header, char *line)
{
    tl_append(&header->headers, line);
}

/*
 * header_read() --- read header lines from file
 */
RFCHeader *header_read(FILE *file)
{
    static char buf[BUFFERSIZE];
    static char queue[BUFFERSIZE];
    short int first = TRUE;
    Textlist *headers;
    RFCHeader *header;

    header = xmalloc(sizeof(*header));
    memset(header, 0, sizeof(*header));

    headers = &header->headers;
    queue[0] = '\0';

    while (read_line(buf, sizeof(buf), file)) {
        if (*buf == '\r' || *buf == '\n')
            break;
        strip_crlf(buf);
        if (is_blank(buf[0])) {
            BUF_APPEND(queue, buf);
        } else {
            if (!first)
                tl_append(headers, queue);
            else
                first = FALSE;
            BUF_COPY(queue, buf);
        }
    }
    if (strlen(queue) > 1)
        tl_append(headers, queue);

    return header;
}

/*
 * header_read_list() --- read header lines from Textlist
 */
RFCHeader *header_read_list(Textlist *body)
{
    static char buf[BUFFERSIZE];
    static char queue[BUFFERSIZE];
    short int first = TRUE;
    Textline *line;
    Textlist *headers;
    RFCHeader *header;

    if (body == NULL)
        return NULL;

    header = xmalloc(sizeof(*header));
    memset(header, 0, sizeof(*header));

    headers = &header->headers;

    queue[0] = '\0';

    for (line = body->first; line != NULL; line = line->next) {
        strncpy(buf, line->line, BUFFERSIZE - 1);
        buf[BUFFERSIZE - 1] = '\0';
        if (*buf == '\r' || *buf == '\n')
            break;
        strip_crlf(buf);
        if (is_blank(buf[0])) {
            BUF_APPEND(queue, buf);
        } else {
            if (!first)
                tl_append(headers, queue);
            else
                first = FALSE;
            BUF_COPY(queue, buf);
        }
    }
    if (strlen(queue) > 1)
        tl_append(headers, queue);

    return header;
}

/* delete rfc header from the beginning of body */

int header_delete_from_body(Textlist * body)
{
    char *buf;
    Textline *line;

    if (body == NULL)
        return ERROR;

    for (line = body->first;; line = body->first) {
        buf = line->line;
        if (*buf == '\r' || *buf == '\n') {
            tl_delete(body, line);
            break;
        }
        tl_delete(body, line);
    }
    return OK;
}

/*
 * header_hops() --- return # of hops (Received headers) of message
 */
int header_hops(RFCHeader *header)
{
    char *name = "Received";
    Textline *p;
    int len, hops;
    Textlist *headers = &header->headers;

    len = strlen(name);
    hops = 0;

    for (p = headers->first; p; p = p->next) {
#ifdef RECEIVED_BY_MAILER
        if (!strnicmp(p->line, RECEIVED_BY_MAILER, strlen(RECEIVED_BY_MAILER)))
            continue;
#endif                          /* RECEIVED_BY_MAILER */
        if (!strnicmp(p->line, name, len) && p->line[len] == ':')
            hops++;
    }

    return hops;
}

static char *header_get_from_tl(RFCHeader *header, Textlist *tl, char *name)
{
    Textline *p;
    int len;
    char *s;

    len = strlen(name);

    for (p = tl->first; p; p = p->next) {
        if (!strnicmp(p->line, name, len) && p->line[len] == ':') {
            for (s = p->line + len + 1; is_space(*s); s++) ;
            if (header != NULL)
                header->last_header = p;
            return s;
        }
    }

    if (header != NULL)
        header->last_header = NULL;
    return NULL;
}

/* Keep for legacy ftn2rfc code */
char *rfcheader_get(Textlist *tl, char *name)
{
    return header_get_from_tl(NULL, tl, name);
}

char *header_get(RFCHeader *header, char *name)
{
    if (header == NULL)
        return NULL;
    return header_get_from_tl(header, &header->headers, name);
}

/*
 * header_geth() --- get 1st/next header line from Textlist
 *
 * Modi:   name="X",  first=TRUE	return 1st X header line
 *	   name=NULL, first=FALSE	return continuation header lines only
 *	   name="X",  first=FALSE       return continuation header lines and
 *					new X header lines
 *
 * Return: contents of header lines or NULL.
 */
char *header_geth(RFCHeader *header, char *name, int first)
{
    static Textline *p_last;
    Textline *p;
    int len;
    char *s;
    Textlist *tl = &header->headers;

    if (header == NULL)
        return NULL;

    if (first) {
        /* Restart search */
        p_last = NULL;
        p = tl->first;
    } else if (p_last) {
        /* Continue search */
        p_last = p_last->next;
        p = p_last;
        /* Check for continuation header, white space at start of line */
        if (p_last && is_space(p_last->line[0])) {
            for (s = p_last->line; is_space(*s); s++) ;
            return s;
        }
    } else {
        p = NULL;
    }

    /* If p or name is NULL, stop here */
    if (!p || !name) {
        p_last = NULL;
        return NULL;
    }

    /* Search for header line starting with NAME: */
    len = strlen(name);
    for (; p; p = p->next) {
        if (!strnicmp(p->line, name, len) && p->line[len] == ':') {
            for (s = p->line + len + 1; is_space(*s); s++) ;
            p_last = p;
            return s;
        }
    }

    p_last = NULL;
    return NULL;
}

/*
 * header_getnext() --- get next header line
 */

char *header_getnext(RFCHeader *header)
{
    char *s;

    if (header->last_header == NULL)
        return NULL;

    header->last_header = header->last_header->next;
    if (header->last_header == NULL)
        return NULL;
    if (!is_space(header->last_header->line[0])) {
        header->last_header = NULL;
        return NULL;
    }

    for (s = header->last_header->line; is_space(*s); s++) ;
    return s;
}

/*
 * Return complete header line, concat continuation lines if necessary.
 */
#define TMPS_LEN_GETCOMPLETE BUFFERSIZE

char *s_header_getcomplete(RFCHeader *header, char *name)
{
    char *p;
    TmpS *s;

    if ((p = header_get(header, name))) {
        s = tmps_alloc(TMPS_LEN_GETCOMPLETE);
        str_copy(s->s, s->len, p);

        while ((p = header_getnext(header))) {
            str_append(s->s, s->len, " ");
            str_append(s->s, s->len, p);
        }

        tmps_stripsize(s);
        return s->s;
    }

    return NULL;
}

/*
 * Change header line
 */

int header_alter(RFCHeader *header, char *name, char *newval)
{
    Textline *line;
    char *new_header;
    Textlist *tl = &header->headers;

    if (header == NULL || name == NULL)
        return ERROR;

    for (line = tl->first; line != NULL; line = line->next) {
        if (!strneq(line->line, name, strlen(name)))
            continue;

        if (newval == NULL) {
            tl_delete(tl, line);
            return OK;
        }
        /* name + ": " + newval + '\n' */
        new_header = xmalloc(strlen(name) + 2 + strlen(newval) + 1);
        strcpy(new_header, name);
        strcat(new_header, ": ");
        strcat(new_header, newval);
        xfree(line->line);
        line->line = new_header;
        return OK;
    }
    return ERROR;
}

/*
 * addr_token() --- get addresses from string (',' separated)
 */
char *addr_token(char *line)
{
    static char *save_line = NULL;
    static char *save_p = NULL;
    int level;
    char *s, *p;

    if (line) {
        /*
         * First call
         */
        xfree(save_line);
        save_p = save_line = strsave(line);
    }

    if (save_p == NULL)
        return NULL;
    if (!*save_p) {
        save_p = NULL;
        return NULL;
    }

    level = 0;
    for (p = s = save_p; *p; p++) {
        if (*p == '(')
            level++;
        if (*p == ')')
            level--;
        if (*p == ',' && level <= 0)
            break;
    }
    if (*p)
        *p++ = 0;
    save_p = p;

    return s;
}

/*
 * Decodes the mimed headers and recodes them to @to charset
 * (source charset is part of the mime encoding)
 */
RFCHeader *header_decode(RFCHeader *header, char *to, char *ch_fallback)
{
    TextlistIterator iter;
    Textline *line;
    RFCHeader *decoded;

    decoded = header_new(NULL);

    tl_iterator_start(&iter, &header->headers);
    line = tl_iterator_next(&iter);

    for (; line; line = tl_iterator_next(&iter)) {

        mime_header_dec(buffer, sizeof(buffer), line->line,
                        to, ch_fallback, header);
        header_append(decoded, buffer);
    }

    return decoded;
}
