/*
 * pslash-systemd - systemd integration for psplash
 *
 * Copyright (c) 2020 Toradex
 *
 * Author: Stefan Agner <stefan.agner@toradex.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include "psplash.h"

#define PSPLASH_UPDATE_USEC 1000000

typedef uint64_t usec_t;

static int pipe_fd;

int get_progress(void)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *message = NULL;
	static double current_progress = 0;
	double progress = 0;
	double depth = 0;
	sd_bus *bus = NULL;
	char *msg_info = NULL;
	int r;
	char buffer[20];
	int len;

        /* Connect to the system bus */
	r = sd_bus_open_system(&bus);
	if (r < 0)
		goto finish;

	r = sd_bus_get_property(bus,
		"de.pengutronix.rauc",
		"/",
		"de.pengutronix.rauc.Installer",
		"Progress",
		&error,
		&message,
		"(isi)");
	
	if (r < 0) {
		fprintf(stderr, "[ERROR]: %s\n", error.message);
		goto finish;
	}

	/*
	 * Systemd's progress seems go backwards at times. Prevent that
	 * progress bar on psplash goes backward by just communicating the
	 * highest observed progress so far.
	 */

	sd_bus_message_read(message,"(isi)",&current_progress,&msg_info,&depth);
	DBG("[INFO] RAUC Information %d %s %d", current_progress, msg_info, depth);

	if (current_progress < progress)
		current_progress = progress;

	len = snprintf(buffer, 20, "PROGRESS %d", (int)current_progress);
	write(pipe_fd, buffer, len + 1);

	if (progress == 100) {
		printf("Rauc reported progress of 100\%.\n");
		write(pipe_fd, "MSG Update finished.", 24);
		r = -1;
	}

finish:
	sd_bus_error_free(&error);
	sd_bus_unref(bus);

	return r;
}

int psplash_handler(sd_event_source *s,
			uint64_t usec,
			void *userdata)
{
	sd_event *event = userdata;
	int r;

	r = get_progress();
	if (r < 0)
		goto err;

	r = sd_event_source_set_time(s, usec + PSPLASH_UPDATE_USEC);
	if (r < 0)
		goto err;

	return 0;
err:
	sd_event_exit(event, EXIT_FAILURE);

	return r;
}

int main()
{
	sd_event *event;
	sd_event_source *event_source = NULL;
	int r;
	sigset_t ss;
	usec_t time_now;
	char *rundir;

	/* Open pipe for psplash */
	rundir = getenv("PSPLASH_FIFO_DIR");

	if (!rundir)
		rundir = "/run";

	chdir(rundir);

	if ((pipe_fd = open (PSPLASH_FIFO,O_WRONLY|O_NONBLOCK)) == -1) {
		fprintf(stderr, "Error unable to open fifo");
		exit(EXIT_FAILURE);
	}

	r = sd_event_default(&event);
	if (r < 0)
		goto finish;

	if (sigemptyset(&ss) < 0 ||
	    sigaddset(&ss, SIGTERM) < 0 ||
	    sigaddset(&ss, SIGINT) < 0) {
		r = -errno;
		goto finish;
	}

	/* Block SIGTERM first, so that the event loop can handle it */
	if (sigprocmask(SIG_BLOCK, &ss, NULL) < 0) {
		r = -errno;
		goto finish;
	}

	/* Let's make use of the default handler and "floating" reference
	 * features of sd_event_add_signal() */
	r = sd_event_add_signal(event, NULL, SIGTERM, NULL, NULL);
	if (r < 0)
		goto finish;

	r = sd_event_add_signal(event, NULL, SIGINT, NULL, NULL);
	if (r < 0)
		goto finish;

	r = sd_event_now(event, CLOCK_MONOTONIC, &time_now);
	if (r < 0)
		goto finish;

	r = sd_event_add_time(event, &event_source, CLOCK_MONOTONIC,
			      time_now, 0, psplash_handler, event);
	if (r < 0)
		goto finish;

	r = sd_event_source_set_enabled(event_source, SD_EVENT_ON);
	if (r < 0)
		goto finish;

	r = sd_event_loop(event);
finish:
	event = sd_event_unref(event);
	close(pipe_fd);

	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
