/*
 *  This file is part of vban_emitter.
 *  Copyright (c) 2018 by Benoît Quiniou <quiniouben@yahoo.fr>
 *
 *  vban_emitter is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  vban_emitter is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with vban_emitter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include "vban/vban.h"
#include "common/version.h"
#include "common/socket.h"
#include "common/logger.h"
#include "common/packet.h"

struct config_t
{
    struct socket_config_t      socket;
    char                        stream_name[VBAN_STREAM_NAME_SIZE];
};

struct main_t
{
    socket_handle_t             socket;
    char                        buffer[VBAN_PROTOCOL_MAX_SIZE];
};

void usage()
{
    printf("\nUsage: vban_ping [OPTIONS] MESSAGE\n\n");
    printf("-i, --ipaddress=IP      : MANDATORY. ipaddress to send stream to\n");
    printf("-p, --port=PORT         : MANDATORY. port to use\n");
    printf("-l, --loglevel=LEVEL    : Log level, from 0 (FATAL) to 4 (DEBUG). default is 1 (ERROR)\n");
    printf("-h, --help              : display this message\n\n");
}

int get_options(struct config_t* config, int argc, char* const* argv)
{
    int c = 0;
    int ret = 0;

    static const struct option options[] =
    {
        {"ipaddress",   required_argument,  0, 'i'},
        {"port",        required_argument,  0, 'p'},
        {"loglevel",    required_argument,  0, 'l'},
        {"help",        no_argument,        0, 'h'},
        {0,             0,                  0,  0 }
    };

    // default values
    config->socket.direction    = SOCKET_OUT;

    /* yes, I assume config is not 0 */
    while (1)
    {
        c = getopt_long(argc, argv, "i:p:s:b:n:f:l:h", options, 0);
        if (c == -1)
            break;

        switch (c)
        {
            case 'i':
                strncpy(config->socket.ip_address, optarg, SOCKET_IP_ADDRESS_SIZE-1);
                break;

            case 'p':
                config->socket.port = atoi(optarg);
                break;

            case 's':
                strncpy(config->stream_name, optarg, VBAN_STREAM_NAME_SIZE-1);
                break;

            case 'l':
                logger_set_output_level(atoi(optarg));
                break;

            case 'h':
            default:
                usage();
                return 1;
        }

        if (ret)
        {
            return ret;
        }
    }

    /** check if we got all arguments */
    if ((config->socket.ip_address[0] == 0)
        || (config->socket.port == 0))
    {
        logger_log(LOG_FATAL, "Missing ip address, port or stream name");
        usage();
        return 1;
    }

    if (optind < argc - 1)
    {
        logger_log(LOG_FATAL, "Too many arguments");
        usage();
        return 1;
    }

    return 0;
}


int main(int argc, char* const* argv)
{
    int ret = 0;
    struct config_t config;
    struct main_t   main_s;
    char const* msg = 0;
    size_t len = 0;
    struct VBanHeader* const hdr = (struct VBanHeader*)&main_s.buffer;
    struct VBanServiceData* const hdr_d = (struct VBanServiceData*)(main_s.buffer + sizeof(struct VBanHeader));

    printf("%s version %s\n\n", argv[0], VBAN_VERSION);

    memset(&config, 0, sizeof(struct config_t));
    memset(&main_s, 0, sizeof(struct main_t));

    ret = get_options(&config, argc, argv);
    if (ret != 0)
    {
        return ret;
    }

    msg = argv[argc-1];
    len = strlen(msg);
    if (len > VBAN_DATA_MAX_SIZE-1)
    {
        logger_log(LOG_FATAL, "Message too long. max length is %d", VBAN_DATA_MAX_SIZE-1);
        usage();
        return 1;
    }

    hdr_d->bitType = 1; // Simple receptor
    hdr_d->bitfeature = (1 | 2); //Audio + Audio over IP
    hdr_d->bitfeatureEx = (1 | 2);
    hdr_d->MinRate = vban_sr_from_value(32000);
    hdr_d->MaxRate = vban_sr_from_value(48000);
    hdr_d->PreferedRate = vban_sr_from_value(48000);
    strcpy(hdr_d->LangCode_ascii, "en");
    strcpy(hdr_d->DeviceName_ascii, "Raspberry Pi 3 A+");
    strcpy(hdr_d->ManufacturerName_ascii, "Raspberry Pi");
    strcpy(hdr_d->HostName_ascii, "pithree");
    strcpy(hdr_d->UserName_utf8, "radio95");
    strcpy(hdr_d->UserComment_utf8, "radio95 broadcast computer");
    strcpy(hdr_d->DistantIP_ascii, "192.168.1.22");
    hdr_d->DistantPort = 6980;
    strncpy((char*)(hdr_d + 1), msg, len);  // Copy message into the data buffer right after hdr_d

    ret = socket_init(&main_s.socket, &config.socket);
    if (ret != 0)
    {
        return ret;
    }

    hdr->vban       = VBAN_HEADER_FOURC;
    hdr->format_SR  = VBAN_PROTOCOL_SERVICE;
    hdr->format_nbc = 0;
    hdr->format_nbs = 0;
    hdr->format_bit = 0;
    strncpy(hdr->streamname, "VBAN Service", VBAN_STREAM_NAME_SIZE);
    hdr->nuFrame    = 0;

    logger_log(LOG_DEBUG, "%s: packet is vban: %u, sr: %d, nbs: %d, nbc: %d, bit: %d, name: %s, nu: %u, msg: %s",
        __func__, hdr->vban, hdr->format_SR, hdr->format_nbs, hdr->format_nbc, hdr->format_bit, hdr->streamname, hdr->nuFrame, (char*)(hdr_d + 1));

    ret = socket_write(main_s.socket, main_s.buffer, sizeof(struct VBanHeader) + sizeof(struct VBanServiceData) + len);

    socket_release(&main_s.socket);

    return ret;
}
