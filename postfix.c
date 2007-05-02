/******************************************************************************/
/*          postlicyd: a postfix policy daemon with a lot of features         */
/*          ~~~~~~~~~                                                         */
/*  ________________________________________________________________________  */
/*                                                                            */
/*  Redistribution and use in source and binary forms, with or without        */
/*  modification, are permitted provided that the following conditions        */
/*  are met:                                                                  */
/*                                                                            */
/*  1. Redistributions of source code must retain the above copyright         */
/*     notice, this list of conditions and the following disclaimer.          */
/*  2. Redistributions in binary form must reproduce the above copyright      */
/*     notice, this list of conditions and the following disclaimer in the    */
/*     documentation and/or other materials provided with the distribution.   */
/*  3. The names of its contributors may not be used to endorse or promote    */
/*     products derived from this software without specific prior written     */
/*     permission.                                                            */
/*                                                                            */
/*  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND   */
/*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE     */
/*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR        */
/*  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS    */
/*  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR    */
/*  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF      */
/*  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS  */
/*  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN   */
/*  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)   */
/*  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF    */
/*  THE POSSIBILITY OF SUCH DAMAGE.                                           */
/******************************************************************************/

/*
 * Copyright © 2006-2007 Pierre Habouzit
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>

#include "job.h"
#include "postfix.h"
#include "buffer.h"
#include "tokens.h"

struct jpriv_t {
    buffer_t ibuf;
    buffer_t obuf;
};

static jpriv_t *postfix_jpriv_init(jpriv_t *jp)
{
    buffer_init(&jp->ibuf);
    buffer_init(&jp->obuf);
    return jp;
}
static void postfix_jpriv_wipe(jpriv_t *jp)
{
    buffer_wipe(&jp->ibuf);
    buffer_wipe(&jp->obuf);
}
DO_NEW(jpriv_t, postfix_jpriv);
DO_DELETE(jpriv_t, postfix_jpriv);

static void postfix_stop(job_t *job)
{
    postfix_jpriv_delete(&job->jdata);
}

static int postfix_parsejob(job_t *job)
{
    const char *p = skipspaces(job->jdata->ibuf.data);

    for (;;) {
        const char *k, *v;
        int klen, vlen;

        while (*p == ' ' || *p == '\t')
            p++;
        p = strchrnul(k = p, '=');
        if (!*p)
            return -1;
        for (klen = p - k; k[klen] == ' ' || k[klen] == '\t'; klen--);
        p += 1; /* skip = */

        while (*p == ' ' || *p == '\t')
            p++;
        p = strstr(v = p, "\r\n");
        if (!p)
            return -1;
        for (vlen = p - v; v[vlen] == ' ' || v[vlen] == '\t'; vlen--);
        p += 2; /* skip \r\n */

        /* do sth with (k,v) */

        assert (p[0] && p[1]);
        if (p[0] == '\r' && p[1] == '\n')
            break;
    }

    return -1;
}

static void postfix_process(job_t *job)
{
    int nb;

    switch (job->mode) {
      case JOB_LISTEN:
        if ((job = job_accept(job, JOB_READ))) {
            job->jdata   = postfix_jpriv_new();
            job->process = &postfix_process;
            job->stop    = &postfix_stop;
        }
        return;

      case JOB_WRITE:
        nb = write(job->fd, job->jdata->obuf.data, job->jdata->obuf.len);
        if (nb < 0) {
            if ((job->error = errno != EINTR && errno != EAGAIN)) {
                syslog(LOG_ERR, "unexpected problem on the socket: %m");
            }
            return;
        }

        buffer_consume(&job->jdata->obuf, nb);
        if (job->jdata->obuf.len)
            return;

        job_update_mode(job, JOB_READ);

        /* fall through */

      case JOB_READ:
        nb = buffer_read(&job->jdata->ibuf, job->fd, -1);
        if (nb < 0) {
            if ((job->error = errno != EINTR && errno != EAGAIN)) {
                syslog(LOG_ERR, "unexpected problem on the socket: %m");
            }
            return;
        }
        if (nb == 0) {
            syslog(LOG_ERR, "unexpected eof");
            job->error = true;
            return;
        }

        if (!strstr(skipspaces(job->jdata->ibuf.data), "\r\n\r\n")) {
            if (job->jdata->ibuf.len > SHRT_MAX) {
                syslog(LOG_ERR, "too much data without CRLFCRLF");
                job->error = true;
            }
            return;
        }

        if (postfix_parsejob(job) < 0) {
            syslog(LOG_ERR, "could not parse postfix request");
            job->error = true;
            return;
        }

        job_update_mode(job, JOB_IDLE);

        /* TODO: run the scenario */
        return;

      default:
        job->error = true;
        return;
    }
}
