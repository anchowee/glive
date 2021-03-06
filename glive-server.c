/*
 *  GLIVE server application, part of the GLIVE project
 *  GLIVE = Gstreamer Live Example
 *
 *  (c) John Weber, rjohnweber@gmail.com
 *  Avnet Electronics Marketing
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licensei, version 2,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>
#include <signal.h>
#include <string.h>
#include <stropts.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/timerfd.h> /* For timerfd functions */
#include <netinet/in.h>

/* Defaults */
#define DEFAULT_LAUNCH_IMX6  "( mfw_v4l2src capture-mode=4 ! vpuenc codec=6 bitrate=3000000 ! rtph264pay name=pay0 pt=96 )"
#define DEFAULT_LAUNCH_PC "( v4l2src ! x264enc bitrate=1000 ! rtph264pay name=pay0 pt=96 )"
#define DEFAULT_SERVER_PORT 8554

#define LS_BUFSIZE 512

typedef struct _vs_cfg_data {
	int server_port;
	char * launch_string;
} vs_cfg_data;

/* Global data */
vs_cfg_data cfg_data;

/* Initialize a vs_cfg_data structure */
void vs_init_data(vs_cfg_data *data) {

	/* Zero out the structure */
	memset(data, 0, sizeof(vs_cfg_data));

}


static gboolean
vs_rtsp_timeout (GstRTSPServer * server, gboolean ignored)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}


int vs_rtsp_mainloop(vs_cfg_data* cfg) {

	GMainLoop *loop;
	GstRTSPServer *server;
	GstRTSPMediaMapping *mapping;
	GstRTSPMediaFactory *factory;

	loop = g_main_loop_new(NULL, FALSE);
	server = gst_rtsp_server_new();
	mapping = gst_rtsp_server_get_media_mapping(server);
	factory = gst_rtsp_media_factory_new();

	/* If we are ARM architecture, then assume that we are an i.MX processor and build
	   the pipeline to decode and display using the i.MX plugins */
#ifdef __arm__
	int assume_imx = 1;
#else
	int assume_imx = 0;
#endif

	/* Set up RTSP launch command */
	if(!strlen(cfg->launch_string)) {
		if (assume_imx)
			strcpy(cfg->launch_string, DEFAULT_LAUNCH_IMX6);
		else
			strcpy(cfg->launch_string, DEFAULT_LAUNCH_PC);
	}

	printf( "Setting RTSP pipeline to: \n   %s\n", cfg->launch_string);
	gst_rtsp_media_factory_set_launch(factory,
		cfg->launch_string);					

	/* TODO: Add ability to change server port */
	gst_rtsp_media_factory_set_shared(factory, TRUE);
	gst_rtsp_media_mapping_add_factory(mapping, "/camera", factory);
	g_object_unref(mapping);

	/* attach the server to the default main context */
	if (gst_rtsp_server_attach(server, NULL ) == 0)
		goto failed;

	/* add a timeout for the session cleanup */
	g_timeout_add_seconds(2, (GSourceFunc) vs_rtsp_timeout, server);

	/* start serving, this never stops */
	g_print ("Stream ready at rtsp://<THIS IP ADDRESS>:8554/camera\n");
	g_main_loop_run(loop);

	return 0;

	failed: {
		g_print("RTSP: Failed to attach the server\n");
		return -1;
	}
}

/*******************************************************************************
 Main
 *******************************************************************************/

int main(int argc, char *argv[]) {
	int opt;
	int quiet = 0;

	/* Initialize the app_data structure */
	vs_init_data(&cfg_data);
	cfg_data.launch_string = malloc(LS_BUFSIZE+1);

	/* Parse command line */
	while ((opt = getopt(argc, argv, "dqrhl:")) != -1) {
		switch (opt) {
		case 'q':
			/* Quiet - do not print startup messages */
			quiet = 1;
			break;
		case 'h':
			printf("Usage: %s [-qhl <launch string>]\n", argv[0]);
			printf("  -q: Quiet: Do not print copyright information\n"  
			       "  -h: Help: print this message\n"
			       "  -l <launch string>: launch string to use instead of default\n" );
			break;
		case 'l':
			if(optarg)
				sprintf(cfg_data.launch_string,"( %s )", optarg);
		default: /* '?' */
			break;
		}
	}
	
	if(!quiet){
		printf("glive-server - Gstreamer RTSP Server \n"
			"(C) John Weber, Avnet Electronics Marketing\n");
	}	

	/* Initialize configuration */
	cfg_data.server_port = DEFAULT_SERVER_PORT;
	printf("Mode: RTSP on port %d.\n", cfg_data.server_port);
	
	/* RTSP loop */
	gst_init(&argc, &argv);

	vs_rtsp_mainloop(&cfg_data);

	printf("gstreamer main loop exiting.\n");
	free(cfg_data.launch_string);

	return 0;

}
