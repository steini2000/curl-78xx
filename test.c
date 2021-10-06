#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* somewhat unix-specific */
#include <sys/time.h>
#include <unistd.h>

/* curl stuff */
#include <curl/curl.h>
#include <curl/mprintf.h>

#ifndef CURLPIPE_MULTIPLEX
/* This little trick will just make sure that we don't enable pipelining for
   libcurls old enough to not have this symbol. It is _not_ defined to zero in
   a recent libcurl header. */
#define CURLPIPE_MULTIPLEX 0
#endif

#define BASE_URL "https://192.168.9.222:8443/curl-issue-78xx"
#define HTTPS_CLIENT_SSL_KEY_FILE "/tmp/https_client.key"



static void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size) {

	  size_t i;
	  size_t c;
	  unsigned int width=0x40;

	  fprintf(stream, "%s, %5.5ld bytes (0x%4.4lx)\n",
	          text, (long)size, (long)size);

	  for(i=0; i<size; i+= width) {
	    fprintf(stream, "%4.4lx: ", (long)i);

#if 0
	    /* show hex to the left */
	    for(c = 0; c < width; c++) {
	      if(i+c < size)
	        fprintf(stream, "%02x ", ptr[i+c]);
	      else
	        fputs("   ", stream);
	    }
#endif

	    /* show data on the right */
	    for(c = 0; (c < width) && (i+c < size); c++) {
	      char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
	      fputc(x, stream);
	    }

	    fputc('\n', stream); /* newline */
	  }
}

static int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  const char *text;
  (void)handle; /* prevent compiler warning */
  (void)userp;

  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */
    return 0;

  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }

  dump(text, stderr, (unsigned char *)data, size);
  return 0;
}

static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	(void)size;
	(void)nmemb;
	static size_t total = 0;

	FILE *i = userdata;
	size_t retcode = fread(ptr, 1, 11565, i);
	total += retcode;
	fprintf(stderr, "read_callback: allowed %zu, return %zu -> new total %zu\n", size * nmemb, retcode, total);
	return retcode;
}

static CURL* post_setup(void) {
	FILE *in;
	const char *filename = "post.data";
	char url[128];
	CURL *hnd;

	hnd = curl_easy_init();

	in = fopen("/dev/urandom", "rb");
	if (!in) {
		in = fopen(filename, "rb");
		if (!in) {
			fprintf(stderr, "error: could not open file %s for reading: %s\n", filename, strerror(errno));
			exit(1);
		}
	}

	snprintf(url, sizeof(url), "%s/%s", BASE_URL, "post");

	curl_easy_setopt(hnd, CURLOPT_READFUNCTION, read_callback);
	curl_easy_setopt(hnd, CURLOPT_READDATA, in);

	curl_easy_setopt(hnd, CURLOPT_URL, url);

	/* POST */
	curl_easy_setopt(hnd, CURLOPT_POST, 1L);

	/* be verbose */
	curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(hnd, CURLOPT_DEBUGFUNCTION, my_trace);

	/* HTTP/2 */
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

	/* we use a self-signed test server, skip verification during debugging */
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 0L);

	return hnd;
}

/*
 * Upload all files over HTTP/2, using the same physical connection!
 */
int main(int argc, char **argv) {
	CURL *post_hnd;
	CURLM *multi_handle;
	int still_running = 0; /* keep number of running handles */

	(void)argc;
	(void)argv;

	setenv("SSLKEYLOGFILE", HTTPS_CLIENT_SSL_KEY_FILE, 1);
	fprintf(stderr, "SSL key log file enabled: %s\n", HTTPS_CLIENT_SSL_KEY_FILE);

	post_hnd = post_setup();

	/* init a multi stack */
	multi_handle = curl_multi_init();
	curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

	/* First request */
	curl_multi_add_handle(multi_handle, post_hnd);

	do {
		CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
		if (still_running == 0)
			break;

		/* wait for activity, timeout or "nothing" */
		mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);

		//fprintf(stderr, ".\n");

		if (mc)
			break;
	} while (still_running);

	curl_multi_remove_handle(multi_handle, post_hnd);
	curl_easy_cleanup(post_hnd);
	curl_multi_cleanup(multi_handle);

	return 0;
}

