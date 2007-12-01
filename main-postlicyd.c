/******************************************************************************/
/*          pfixtools: a collection of postfix related tools                  */
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

#include <getopt.h>

#include "threads.h"
#include "policy.h"

#define DAEMON_NAME             "postlicyd"

/* administrivia {{{ */

static int main_initialize(void)
{
    openlog("postlicyd", LOG_PID, LOG_MAIL);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  &common_sighandler);
    signal(SIGTERM, &common_sighandler);
    signal(SIGSEGV, &common_sighandler);
    syslog(LOG_INFO, "Starting...");
    return 0;
}

static void main_shutdown(void)
{
    closelog();
}

module_init(main_initialize);
module_exit(main_shutdown);

void usage(void)
{
    fputs("usage: "DAEMON_NAME" [options] config\n"
          "\n"
          "Options:\n"
          "    -p <pidfile> file to write our pid to\n"
         , stderr);
}

/* }}} */

static int main_loop(void)
{
    int exitcode = EXIT_SUCCESS;
    int sock = -1;

    while (!sigint) {
        int fd = accept(sock, NULL, 0);
        if (fd < 0) {
            if (errno != EINTR || errno != EAGAIN)
                UNIXERR("accept");
            continue;
        }

        thread_launch(policy_run, fd, NULL);
        threads_join();
    }

    close(sock);
    return exitcode;
}

int main(int argc, char *argv[])
{
    const char *pidfile = NULL;
    int res;

    for (int c = 0; (c = getopt(argc, argv, "h" "p:")) >= 0; ) {
        switch (c) {
          case 'p':
            pidfile = optarg;
            break;
          default:
            usage();
            return EXIT_FAILURE;
        }
    }

    if (argc - optind != 1) {
        usage();
        return EXIT_FAILURE;
    }

    if (pidfile_open(pidfile) < 0) {
        syslog(LOG_CRIT, "unable to write pidfile %s", pidfile);
        return EXIT_FAILURE;
    }

    if (daemon_detach() < 0) {
        syslog(LOG_CRIT, "unable to fork");
        return EXIT_FAILURE;
    }

    pidfile_refresh();
    res = main_loop();
    syslog(LOG_INFO, "Stopping...");
    return res;
}
