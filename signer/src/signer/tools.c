/*
 * $Id$
 *
 * Copyright (c) 2009 NLNet Labs. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * Zone signing tools.
 *
 */

#include "config.h"
#include "adapter/adapter.h"
#include "daemon/engine.h"
#include "shared/file.h"
#include "shared/locks.h"
#include "shared/log.h"
#include "signer/tools.h"
#include "signer/zone.h"
#include "util/se_malloc.h"

#include <unistd.h> /* unlink() */

static const char* tools_str = "tools";


/**
 * Read zone from input adapter.
 *
 */
ods_status
tools_input(zone_type* zone)
{
    ods_status status = ODS_STATUS_OK;
    char* axfrname = NULL;
    int error = 0;
    time_t start = 0;
    time_t end = 0;

    if (!zone) {
        ods_log_error("[%s] unable to read zone: no zone", tools_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zone);
    ods_log_assert(zone->adinbound);
    ods_log_assert(zone->adinbound->data);
    ods_log_assert(zone->signconf);
    ods_log_assert(zone->stats);

    zone->stats->sort_done = 0;
    zone->stats->sort_count = 0;
    zone->stats->sort_time = 0;
    start = time(NULL);

    if (zone->adinbound->type == ADAPTER_FILE) {
        ods_log_assert(zone->adinbound->data->file);
        ods_log_assert(zone->adinbound->data->file->filename);

        if (zone->fetch) {
            ods_log_verbose("fetch zone %s",
                zone->name?zone->name:"(null)");
            axfrname = ods_build_path(
                zone->adinbound->data->file->filename, ".axfr", 0);
            error = ods_file_copy(axfrname,
                zone->adinbound->data->file->filename);
            if (error) {
                ods_log_error("[%s] unable to copy axfr file %s to %s",
                    tools_str, axfrname,
                    zone->adinbound->data->file->filename);
                free((void*)axfrname);
                return ODS_STATUS_ERR;
            }
            free((void*)axfrname);
        }
    }

    status = adapter_read(zone);

    end = time(NULL);
    if (!error) {
        zone_backup_state(zone);
        zone->stats->start_time = start;
        zone->stats->sort_time = (end-start);
    } else {
        zonedata_cancel_update(zone->zonedata);
        status = ODS_STATUS_ERR;
    }
    return status;
}


/**
 * Update zone with pending changes.
 *
 */
int
tools_update(zone_type* zone)
{
    int error = 0;
    char* inbound = NULL;
    char* unsorted = NULL;
    ods_log_assert(zone);
    ods_log_assert(zone->signconf);
    ods_log_verbose("[%s] update zone %s", tools_str, zone->name?zone->name:"(null)");
    error = zone_update_zonedata(zone);
    if (!error) {
        ods_log_verbose("[%s] zone %s updated to serial %u", tools_str,
            zone->name?zone->name:"(null)", zone->zonedata->internal_serial);

        inbound = ods_build_path(zone->name, ".inbound", 0);
        unsorted = ods_build_path(zone->name, ".unsorted", 0);
        error = ods_file_copy(inbound, unsorted);
        if (!error) {
            zone_backup_state(zone);
            zone->stats->sort_done = 1;
            unlink(inbound);
        }
        free((void*)inbound);
        free((void*)unsorted);
    }
    return error;
}


/**
 * Nsecify zone.
 *
 */
ods_status
tools_nsecify(zone_type* zone)
{
    ods_status status = ODS_STATUS_OK;
    int error = 0;
    time_t start = 0;
    time_t end = 0;

    if (!zone) {
        ods_log_error("[%s] unable to nsecify zone: no zone", tools_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zone);

    if (!zone->zonedata) {
        ods_log_error("[%s] unable to nsecify zone %s: no zonedata",
            tools_str, zone->name);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zone->zonedata);

    if (!zone->signconf) {
        ods_log_error("[%s] unable to nsecify zone %s: no signconf",
            tools_str, zone->name);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zone->signconf);

    ods_log_assert(zone->stats);

    ods_log_verbose("[%s] nsecify zone %s", tools_str, zone->name?zone->name:"(null)");
    start = time(NULL);
    error = zone_nsecify(zone);
    end = time(NULL);
    if (!error) {
        if (!zone->stats->start_time) {
            zone->stats->start_time = start;
        }
        zone->stats->nsec_time = (end-start);
    } else {
        status = ODS_STATUS_ERR;
    }
    return status;
}


/**
 * Add NSEC(3) records to zone.
 *
 */
int
tools_sign(zone_type* zone)
{
    int error = 0;
    time_t start = 0;
    time_t end = 0;
    ods_log_assert(zone);
    ods_log_assert(zone->signconf);
    ods_log_assert(zone->stats);
    ods_log_verbose("[%s] sign zone %s", tools_str, zone->name?zone->name:"(null)");
    start = time(NULL);
    error = zone_sign(zone);
    end = time(NULL);
    if (!error) {
        ods_log_verbose("[%s] zone %s signed, new serial %u", tools_str,
            zone->name?zone->name:"(null)", zone->zonedata->internal_serial);
        if (!zone->stats->start_time) {
            zone->stats->start_time = start;
        }
        zone->stats->sig_time = (end-start);
        zone_backup_state(zone);
    }
    return error;
}


/**
 * Audit zone.
 *
 */
int
tools_audit(zone_type* zone, char* working_dir, char* cfg_filename)
{
    char* finalized = NULL;
    char str[SYSTEM_MAXLEN];
    int error = 0;
    time_t start = 0;
    time_t end = 0;
    ods_log_assert(zone);
    ods_log_assert(zone->signconf);

    if (zone->stats->sort_done == 0 &&
        (zone->stats->sig_count <= zone->stats->sig_soa_count)) {
        return 0;
    }
    if (zone->signconf->audit) {
        ods_log_verbose("[%s] audit zone %s", tools_str, zone->name?zone->name:"(null)");
        finalized = ods_build_path(zone->name, ".finalized", 0);
        error = adfile_write(zone, finalized);
        if (error != 0) {
            ods_log_error("[%s] audit zone %s failed: unable to write zone",
                tools_str, zone->name?zone->name:"(null)");
            free((void*)finalized);
            return 1;
        }

        snprintf(str, SYSTEM_MAXLEN, "%s -c %s -s %s/%s -z %s > /dev/null",
            ODS_SE_AUDITOR,
            cfg_filename?cfg_filename:ODS_SE_CFGFILE,
            working_dir?working_dir:"",
            finalized?finalized:"(null)",
            zone->name?zone->name:"(null)");

        start = time(NULL);
        ods_log_debug("system call: %s", str);
        error = system(str);
        if (finalized) {
            if (!error) {
                unlink(finalized);
            }
            free((void*)finalized);
        }
        end = time(NULL);
        zone->stats->audit_time = (end-start);
    }
    return error;
}


/**
 * Write zone to output adapter.
 *
 */
ods_status
tools_output(zone_type* zone)
{
    ods_status status = ODS_STATUS_OK;
    char str[SYSTEM_MAXLEN];
    int error = 0;

    if (!zone) {
        ods_log_error("[%s] unable to write zone: no zone", tools_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zone);
    ods_log_assert(zone->signconf);
    ods_log_assert(zone->adoutbound);
    ods_log_assert(zone->stats);

    if (zone->stats->sort_done == 0 &&
        (zone->stats->sig_count <= zone->stats->sig_soa_count)) {
        ods_log_verbose("skip write zone %s serial %u (zone not changed)",
            zone->name?zone->name:"(null)", zone->zonedata->internal_serial);
        stats_clear(zone->stats);
        return 0;
    }

    zone->zonedata->outbound_serial = zone->zonedata->internal_serial;
    ods_log_verbose("[%s] write zone %s serial %u", tools_str,
        zone->name?zone->name:"(null)", zone->zonedata->outbound_serial);

    status = adapter_write(zone);
    if (status != ODS_STATUS_OK) {
        return status;
    }

    /* kick the nameserver */
    if (zone->notify_ns) {
        ods_log_verbose("[%s] notify nameserver: %s", tools_str, zone->notify_ns);
        snprintf(str, SYSTEM_MAXLEN, "%s > /dev/null",
            zone->notify_ns);
        error = system(str);
        if (error) {
           ods_log_error("[%s] failed to notify nameserver", tools_str);
           status = ODS_STATUS_ERR;
        }
    }
    /* log stats */
    zone->stats->end_time = time(NULL);
    ods_log_debug("[%s] log stats for zone %s", tools_str, zone->name?zone->name:"(null)");
    stats_log(zone->stats, zone->name, zone->signconf->nsec_type);
    stats_clear(zone->stats);

    return status;
}
