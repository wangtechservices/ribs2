/*
    This file is part of RIBS2.0 (Robust Infrastructure for Backend Systems).
    RIBS is an infrastructure for building great SaaS applications (but not
    limited to).

    Copyright (C) 2012 Adap.tv, Inc.

    RIBS is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 2.1 of the License.

    RIBS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with RIBS.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ribs.h"
#include <getopt.h>
/*
 * file server
 */
void simple_file_server(void) {
    /* get the current http server context */
    struct http_server_context *ctx = http_server_get_context();

    /* uri decode the uri in the context */
    http_server_decode_uri(ctx->uri);

    /* remove the leading slash, we will be serving files from the
       current directory */
    const char *file = ctx->uri;
    if (*file == '/') ++file;

    int res = http_server_sendfile(file, "", NULL);
    if (0 > res) {
        /* not found */
        if (HTTP_SERVER_NOT_FOUND == res)
            http_server_response_sprintf(HTTP_STATUS_404,
                                         HTTP_CONTENT_TYPE_TEXT_PLAIN, "not found: %s", file);
        else
            http_server_response_sprintf(HTTP_STATUS_500,
                                         HTTP_CONTENT_TYPE_TEXT_PLAIN, "500 Internal Server Error: %s", file);
    }
    else if (0 < res) {
        /* directory */
        if (0 > http_server_generate_dir_list(ctx->uri)) {
            http_server_response_sprintf(HTTP_STATUS_404,
                                         HTTP_CONTENT_TYPE_TEXT_PLAIN, "dir not found: %s", ctx->uri);
            return;
        }
        http_server_response(HTTP_STATUS_200,
                             HTTP_CONTENT_TYPE_TEXT_HTML);
    }
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"port", 1, 0, 'p'},
        {"daemonize", 0, 0, 'd'},
        {"help", 0, 0, 1},
        {0, 0, 0, 0}
    };

    int port = 8080;
    int daemon_mode = 0;
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "p:d", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'p':
            port = atoi(optarg);
            break;
        case 'd':
            daemon_mode = 1;
            break;
        }
    }

    /* server config */
    struct http_server server = {
        /* port number */
        .port = port,

        /* call simple_file_server upon receiving http request */
        .user_func = simple_file_server,

        /* set idle connection timeout to 60 seconds */
        .timeout_handler.timeout = 60000,

        /* set fiber's stack size to 64K */
        .stack_size = 64*1024,

        /* start the server with 100 stacks */
        /* more stacks will be created if necessary */
        .num_stacks =  100,

        /* we expect most of our requests to be less than 8K */
        .init_request_size = 8192,

        /* we expect most of our response headers to be less than
           8K */
        .init_header_size = 8192,

        /* we expect most of our response payloads to be less than
           8K */
        .init_payload_size = 8192,

        /* no limit on the request size, this should be set to
           something reasonable if you want to protect your server
           against denial of service attack */
        .max_req_size = 0,

        /* no additional space is needed in the context to store app
           specified data (fiber local storage) */
        .context_size = 0
    };

    /* initialize server, but don't accept connections yet */
    if (0 > http_server_init(&server))
        exit(EXIT_FAILURE);

    /* run as daemon if specified */
    if (daemon_mode)
        daemonize();

    /* fork should be called here in case of multiple processes
       listening on the same socket. for file server, one process is
       more than enough */
    /* fork(); */

    /* initialize the event loop */
    if (epoll_worker_init() < 0)
        exit(EXIT_FAILURE);

    /* start accepting connections, must be called after initializing
       epoll worker */
    if (0 > http_server_init_acceptor(&server))
        exit(EXIT_FAILURE);

    epoll_worker_loop();
    return 0;
}
