/*-
 * Copyright (c) 2015-2020 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * SPDX-License-Identifier: BSD-2-Clause
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * PARAM(typ, fld, ...)
 *
 * typ: parameter type
 * fld: struct params field name
 * ...: struct parspec fields, except the non-const string fields
 */

/*lint -save -e525 -e539 -e835 */

/*--------------------------------------------------------------------
 *  * Simple parameters
 *   */

#define PARAM_SIMPLE(nm, typ, ...) \
	PARAM(typ, nm, nm, tweak_##typ, &mgt_param.nm, __VA_ARGS__)

#if defined(PLATFORM_FLAGS)
#  error "Temporary macro PLATFORM_FLAGS already defined"
#endif

#if defined(HAVE_ACCEPT_FILTERS)
#  define PLATFORM_FLAGS MUST_RESTART
#else
#  define PLATFORM_FLAGS NOT_IMPLEMENTED
#endif
PARAM_SIMPLE(
	/* name */	accept_filter,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",	/* default adjusted in mgt_param.c */
	/* units */	"bool",
	/* descr */
	"Enable kernel accept-filters. This may require a kernel module to "
	"be loaded to have an effect when enabled.\n\n"
	"Enabling accept_filter may prevent some requests to reach Varnish "
	"in the first place. Malformed requests may go unnoticed and not "
	"increase the client_req_400 counter. GET or HEAD requests with a "
	"body may be blocked altogether.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	NULL,
	/* dyn_def_reason */	"on (if your platform supports accept filters)"
)
#undef PLATFORM_FLAGS

PARAM_SIMPLE(
	/* name */	acceptor_sleep_decay,
	/* type */	double,
	/* min */	"0",
	/* max */	"1",
	/* def */	"0.9",
	/* units */	NULL,
	/* descr */
	"If we run out of resources, such as file descriptors or worker "
	"threads, the acceptor will sleep between accepts.\n"
	"This parameter (multiplicatively) reduce the sleep duration for "
	"each successful accept. (ie: 0.9 = reduce by 10%)",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	acceptor_sleep_incr,
	/* type */	timeout,
	/* min */	"0",
	/* max */	"1",
	/* def */	"0",
	/* units */	"seconds",
	/* descr */
	"If we run out of resources, such as file descriptors or worker "
	"threads, the acceptor will sleep between accepts.\n"
	"This parameter control how much longer we sleep, each time we "
	"fail to accept a new connection.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	acceptor_sleep_max,
	/* type */	timeout,
	/* min */	"0",
	/* max */	"10",
	/* def */	"0.05",
	/* units */	"seconds",
	/* descr */
	"If we run out of resources, such as file descriptors or worker "
	"threads, the acceptor will sleep between accepts.\n"
	"This parameter limits how long it can sleep between attempts to "
	"accept new connections.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	auto_restart,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",
	/* units */	"bool",
	/* descr */
	"Automatically restart the child/worker process if it dies."
)

PARAM_SIMPLE(
	/* name */	ban_dups,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",
	/* units */	"bool",
	/* descr */
	"Eliminate older identical bans when a new ban is added.  This saves "
	"CPU cycles by not comparing objects to identical bans.\n"
	"This is a waste of time if you have many bans which are never "
	"identical."
)

PARAM_SIMPLE(
	/* name */	ban_cutoff,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"0",
	/* units */	"bans",
	/* descr */
	"Expurge long tail content from the cache to keep the number of bans "
	"below this value. 0 disables.\n\n"
	"When this parameter is set to a non-zero value, the ban lurker "
	"continues to work the ban list as usual top to bottom, but when it "
	"reaches the ban_cutoff-th ban, it treats all objects as if they "
	"matched a ban and expurges them from cache. As actively used objects "
	"get tested against the ban list at request time and thus are likely "
	"to be associated with bans near the top of the ban list, with "
	"ban_cutoff, least recently accessed objects (the \"long tail\") are "
	"removed.\n\n"
	"This parameter is a safety net to avoid bad response times due to "
	"bans being tested at lookup time. Setting a cutoff trades response "
	"time for cache efficiency. The recommended value is proportional to "
	"rate(bans_lurker_tests_tested) / n_objects while the ban lurker is "
	"working, which is the number of bans the system can sustain. The "
	"additional latency due to request ban testing is in the order of "
	"ban_cutoff / rate(bans_lurker_tests_tested). For example, for "
	"rate(bans_lurker_tests_tested) = 2M/s and a tolerable latency of "
	"100ms, a good value for ban_cutoff may be 200K.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	ban_lurker_age,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"60",
	/* units */	"seconds",
	/* descr */
	"The ban lurker will ignore bans until they are this old.  "
	"When a ban is added, the active traffic will be tested against it "
	"as part of object lookup.  Because many applications issue bans in "
	"bursts, this parameter holds the ban-lurker off until the rush is "
	"over.\n"
	"This should be set to the approximate time which a ban-burst takes."
)

PARAM_SIMPLE(
	/* name */	ban_lurker_batch,
	/* type */	uint,
	/* min */	"1",
	/* max */	NULL,
	/* def */	"1000",
	/* units */	NULL,
	/* descr */
	"The ban lurker sleeps ${ban_lurker_sleep} after examining this "
	"many objects."
	"  Use this to pace the ban-lurker if it eats too many resources."
)

PARAM_SIMPLE(
	/* name */	ban_lurker_sleep,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"0.010",
	/* units */	"seconds",
	/* descr */
	"How long the ban lurker sleeps after examining ${ban_lurker_batch} "
	"objects."
	"  Use this to pace the ban-lurker if it eats too many resources.\n"
	"A value of zero will disable the ban lurker entirely."
)

PARAM_SIMPLE(
	/* name */	ban_lurker_holdoff,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"0.010",
	/* units */	"seconds",
	/* descr */
	"How long the ban lurker sleeps when giving way to lookup"
	" due to lock contention.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	req_total_timeout,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"0",
	/* units */	"seconds",
	/* descr */
	"Global timeout for all request/response processing intiated by "
	"an incoming client request.\n"
	"This is the timeout for all processing stages, including (but not "
	"limited to) backend requests, ESI includes, restarts and retries.\n"
	"Set to 0s to disable (default)."
)

PARAM_SIMPLE(
	/* name */	first_byte_timeout,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"60",
	/* units */	"seconds",
	/* descr */
	"Default timeout for receiving first byte from backend. We only "
	"wait for this many seconds for the first byte before giving up.\n"
	"VCL can override this default value for each backend and backend "
	"request.\n"
	"This parameter does not apply to pipe'ed requests."
)

PARAM_SIMPLE(
	/* name */	between_bytes_timeout,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"60",
	/* units */	"seconds",
	/* descr */
	"We only wait for this many seconds between bytes received from "
	"the backend before giving up the fetch.\n"
	"VCL values, per backend or per backend request take precedence.\n"
	"This parameter does not apply to pipe'ed requests."
)

PARAM_SIMPLE(
	/* name */	backend_idle_timeout,
	/* type */	timeout,
	/* min */	"1",
	/* max */	NULL,
	/* def */	"60",
	/* units */	"seconds",
	/* descr */
	"Timeout before we close unused backend connections."
)

PARAM_SIMPLE(
	/* name */	backend_local_error_holddown,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"10.000",
	/* units */	"seconds",
	/* descr */
	"When connecting to backends, certain error codes "
	"(EADDRNOTAVAIL, EACCESS, EPERM) signal a local resource shortage "
	"or configuration issue for which retrying connection attempts "
	"may worsen the situation due to the complexity of the operations "
	"involved in the kernel.\n"
	"This parameter prevents repeated connection attempts for the "
	"configured duration.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	backend_remote_error_holddown,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"0.250",
	/* units */	"seconds",
	/* descr */
	"When connecting to backends, certain error codes (ECONNREFUSED, "
	"ENETUNREACH) signal fundamental connection issues such as the backend "
	"not accepting connections or routing problems for which repeated "
	"connection attempts are considered useless\n"
	"This parameter prevents repeated connection attempts for the "
	"configured duration.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	cli_limit,
	/* type */	bytes_u,
	/* min */	"128b",
	/* max */	"99999999b",
	/* def */	"48k",
	/* units */	"bytes",
	/* descr */
	"Maximum size of CLI response.  If the response exceeds this "
	"limit, the response code will be 201 instead of 200 and the last "
	"line will indicate the truncation."
)

PARAM_SIMPLE(
	/* name */	cli_timeout,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"60.000",
	/* units */	"seconds",
	/* descr */
	"Timeout for the childs replies to CLI requests from the "
	"mgt_param."
)

PARAM_SIMPLE(
	/* name */	clock_skew,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"10",
	/* units */	"seconds",
	/* descr */
	"How much clockskew we are willing to accept between the backend "
	"and our own clock."
)

PARAM_SIMPLE(
	/* name */	clock_step,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"1.000",
	/* units */	"seconds",
	/* descr */
	"How much observed clock step we are willing to accept before "
	"we panic."
)

PARAM_SIMPLE(
	/* name */	connect_timeout,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"3.500",
	/* units */	"seconds",
	/* descr */
	"Default connection timeout for backend connections. We only try "
	"to connect to the backend for this many seconds before giving up. "
	"VCL can override this default value for each backend and backend "
	"request."
)

PARAM_SIMPLE(
	/* name */	critbit_cooloff,
	/* type */	timeout,
	/* min */	"60.000",
	/* max */	"254.000",
	/* def */	"180.000",
	/* units */	"seconds",
	/* descr */
	"How long the critbit hasher keeps deleted objheads on the cooloff "
	"list.",
	/* flags */	WIZARD
)

PARAM_SIMPLE(
	/* name */	default_grace,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"10.000",
	/* units */	"seconds",
	/* descr */
	"Default grace period.  We will deliver an object this long after "
	"it has expired, provided another thread is attempting to get a "
	"new copy.",
	/* flags */	OBJ_STICKY
)

PARAM_SIMPLE(
	/* name */	default_keep,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"0.000",
	/* units */	"seconds",
	/* descr */
	"Default keep period.  We will keep a useless object around this "
	"long, making it available for conditional backend fetches.  That "
	"means that the object will be removed from the cache at the end "
	"of ttl+grace+keep.",
	/* flags */	OBJ_STICKY
)

PARAM_SIMPLE(
	/* name */	default_ttl,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"120.000",
	/* units */	"seconds",
	/* descr */
	"The TTL assigned to objects if neither the backend nor the VCL "
	"code assigns one.",
	/* flags */	OBJ_STICKY
)

PARAM_SIMPLE(
	/* name */	http1_iovs,
	/* type */	uint,
	/* min */	"5",
	/* max */	"1024",		// XXX stringify IOV_MAX
	/* def */	"64",
	/* units */	"struct iovec (=16 bytes)",
	/* descr */
	"Number of io vectors to allocate for HTTP1 protocol transmission."
	"  A HTTP1 header needs 7 + 2 per HTTP header field."
	"  Allocated from workspace_thread.",
	/* flags */	WIZARD
)

PARAM_SIMPLE(
	/* name */	fetch_chunksize,
	/* type */	bytes,
	/* min */	"4k",
	/* max */	NULL,
	/* def */	"16k",
	/* units */	"bytes",
	/* descr */
	"The default chunksize used by fetcher. This should be bigger than "
	"the majority of objects with short TTLs.\n"
	"Internal limits in the storage_file module makes increases above "
	"128kb a dubious idea.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	fetch_maxchunksize,
	/* type */	bytes,
	/* min */	"64k",
	/* max */	NULL,
	/* def */	"0.25G",
	/* units */	"bytes",
	/* descr */
	"The maximum chunksize we attempt to allocate from storage. Making "
	"this too large may cause delays and storage fragmentation.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	gzip_buffer,
	/* type */	bytes_u,
	/* min */	"2k",
	/* max */	NULL,
	/* def */	"32k",
	/* units */	"bytes",
	/* descr */
	"Size of malloc buffer used for gzip processing.\n"
	"These buffers are used for in-transit data, for instance "
	"gunzip'ed data being sent to a client.Making this space to small "
	"results in more overhead, writes to sockets etc, making it too "
	"big is probably just a waste of memory.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	gzip_level,
	/* type */	uint,
	/* min */	"0",
	/* max */	"9",
	/* def */	"6",
	/* units */	NULL,
	/* descr */
	"Gzip compression level: 0=debug, 1=fast, 9=best"
)

PARAM_SIMPLE(
	/* name */	gzip_memlevel,
	/* type */	uint,
	/* min */	"1",
	/* max */	"9",
	/* def */	"8",
	/* units */	NULL,
	/* descr */
	"Gzip memory level 1=slow/least, 9=fast/most compression.\n"
	"Memory impact is 1=1k, 2=2k, ... 9=256k."
)

PARAM_SIMPLE(
	/* name */	http_gzip_support,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",
	/* units */	"bool",
	/* descr */
	"Enable gzip support. When enabled Varnish request compressed "
	"objects from the backend and store them compressed. If a client "
	"does not support gzip encoding Varnish will uncompress compressed "
	"objects on demand. Varnish will also rewrite the Accept-Encoding "
	"header of clients indicating support for gzip to:\n"
	"  Accept-Encoding: gzip\n"
	"\n"
	"Clients that do not support gzip will have their Accept-Encoding "
	"header removed. For more information on how gzip is implemented "
	"please see the chapter on gzip in the Varnish reference.\n"
	"\n"
	"When gzip support is disabled the variables beresp.do_gzip and "
	"beresp.do_gunzip have no effect in VCL."
	/* XXX: what about the effect on beresp.filters? */
)

PARAM_SIMPLE(
	/* name */	http_max_hdr,
	/* type */	uint,
	/* min */	"32",
	/* max */	"65535",
	/* def */	"64",
	/* units */	"header lines",
	/* descr */
	"Maximum number of HTTP header lines we allow in "
	"{req|resp|bereq|beresp}.http (obj.http is autosized to the exact "
	"number of headers).\n"
	"Cheap, ~20 bytes, in terms of workspace memory.\n"
	"Note that the first line occupies five header lines."
)

PARAM_SIMPLE(
	/* name */	http_range_support,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",
	/* units */	"bool",
	/* descr */
	"Enable support for HTTP Range headers."
	/* XXX: what about the effect on beresp.filters? */
)

PARAM_SIMPLE(
	/* name */	http_req_hdr_len,
	/* type */	bytes_u,
	/* min */	"40b",
	/* max */	NULL,
	/* def */	"8k",
	/* units */	"bytes",
	/* descr */
	"Maximum length of any HTTP client request header we will allow.  "
	"The limit is inclusive its continuation lines."
)

PARAM_SIMPLE(
	/* name */	http_req_size,
	/* type */	bytes_u,
	/* min */	"0.25k",
	/* max */	NULL,
	/* def */	"32k",
	/* units */	"bytes",
	/* descr */
	"Maximum number of bytes of HTTP client request we will deal with. "
	" This is a limit on all bytes up to the double blank line which "
	"ends the HTTP request.\n"
	"The memory for the request is allocated from the client workspace "
	"(param: workspace_client) and this parameter limits how much of "
	"that the request is allowed to take up."
)

PARAM_SIMPLE(
	/* name */	http_resp_hdr_len,
	/* type */	bytes_u,
	/* min */	"40b",
	/* max */	NULL,
	/* def */	"8k",
	/* units */	"bytes",
	/* descr */
	"Maximum length of any HTTP backend response header we will allow. "
	" The limit is inclusive its continuation lines."
)

PARAM_SIMPLE(
	/* name */	http_resp_size,
	/* type */	bytes_u,
	/* min */	"0.25k",
	/* max */	NULL,
	/* def */	"32k",
	/* units */	"bytes",
	/* descr */
	"Maximum number of bytes of HTTP backend response we will deal "
	"with.  This is a limit on all bytes up to the double blank line "
	"which ends the HTTP response.\n"
	"The memory for the response is allocated from the backend workspace "
	"(param: workspace_backend) and this parameter limits how much "
	"of that the response is allowed to take up."
)

#if defined(SO_SNDTIMEO_WORKS)
#  define PLATFORM_FLAGS DELAYED_EFFECT
#else
#  define PLATFORM_FLAGS NOT_IMPLEMENTED
#endif
PARAM_SIMPLE(
	/* name */	idle_send_timeout,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"60.000",
	/* units */	"seconds",
	/* descr */
	"Send timeout for individual pieces of data on client connections."
	" May get extended if 'send_timeout' applies.\n\n"
	"When this timeout is hit, the session is closed.\n\n"
	"See the man page for `setsockopt(2)` or `socket(7)` under"
	" ``SO_SNDTIMEO`` for more information.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS
)
#undef PLATFORM_FLAGS

PARAM_SIMPLE(
	/* name */	listen_depth,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"1024",
	/* units */	"connections",
	/* descr */
	"Listen queue depth.",
	/* flags */	MUST_RESTART
)

PARAM_SIMPLE(
	/* name */	lru_interval,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"2.000",
	/* units */	"seconds",
	/* descr */
	"Grace period before object moves on LRU list.\n"
	"Objects are only moved to the front of the LRU list if they have "
	"not been moved there already inside this timeout period.  This "
	"reduces the amount of lock operations necessary for LRU list "
	"access.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	max_esi_depth,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"5",
	/* units */	"levels",
	/* descr */
	"Maximum depth of esi:include processing."
)

PARAM_SIMPLE(
	/* name */	max_restarts,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"4",
	/* units */	"restarts",
	/* descr */
	"Upper limit on how many times a request can restart."
)

PARAM_SIMPLE(
	/* name */	max_retries,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"4",
	/* units */	"retries",
	/* descr */
	"Upper limit on how many times a backend fetch can retry."
)

PARAM_SIMPLE(
	/* name */	nuke_limit,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"50",
	/* units */	"allocations",
	/* descr */
	"Maximum number of objects we attempt to nuke in order to make "
	"space for a object body.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	ping_interval,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"3",
	/* units */	"seconds",
	/* descr */
	"Interval between pings from parent to child.\n"
	"Zero will disable pinging entirely, which makes it possible to "
	"attach a debugger to the child.",
	/* flags */	MUST_RESTART
)

PARAM_SIMPLE(
	/* name */	pipe_sess_max,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"0",
	/* units */	"connections",
	/* descr */
	"Maximum number of sessions dedicated to pipe transactions."
)

PARAM_SIMPLE(
	/* name */	pipe_timeout,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"60.000",
	/* units */	"seconds",
	/* descr */
	"Idle timeout for PIPE sessions. If nothing have been received in "
	"either direction for this many seconds, the session is closed."
)

PARAM_SIMPLE(
	/* name */	prefer_ipv6,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"off",
	/* units */	"bool",
	/* descr */
	"Prefer IPv6 address when connecting to backends which have both "
	"IPv4 and IPv6 addresses."
)

PARAM_SIMPLE(
	/* name */	rush_exponent,
	/* type */	uint,
	/* min */	"2",
	/* max */	NULL,
	/* def */	"3",
	/* units */	"requests per request",
	/* descr */
	"How many parked request we start for each completed request on "
	"the object.\n"
	"NB: Even with the implict delay of delivery, this parameter "
	"controls an exponential increase in number of worker threads.",
	/* flags */	EXPERIMENTAL
)

#if defined(SO_SNDTIMEO_WORKS)
#  define PLATFORM_FLAGS DELAYED_EFFECT
#else
#  define PLATFORM_FLAGS NOT_IMPLEMENTED
#endif
PARAM_SIMPLE(
	/* name */	send_timeout,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"600.000",
	/* units */	"seconds",
	/* descr */
	"Total timeout for ordinary HTTP1 responses. Does not apply to some"
	" internally generated errors and pipe mode.\n\n"
	"When 'idle_send_timeout' is hit while sending an HTTP1 response, the"
	" timeout is extended unless the total time already taken for sending"
	" the response in its entirety exceeds this many seconds.\n\n"
	"When this timeout is hit, the session is closed",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS
)
#undef PLATFORM_FLAGS

PARAM_SIMPLE(
	/* name */	shortlived,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"10.000",
	/* units */	"seconds",
	/* descr */
	"Objects created with (ttl+grace+keep) shorter than this are "
	"always put in transient storage."
)

PARAM_SIMPLE(
	/* name */	sigsegv_handler,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",
	/* units */	"bool",
	/* descr */
	"Install a signal handler which tries to dump debug information on "
	"segmentation faults, bus errors and abort signals.",
	/* flags */	MUST_RESTART
)

PARAM_SIMPLE(
	/* name */	syslog_cli_traffic,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"on",
	/* units */	"bool",
	/* descr */
	"Log all CLI traffic to syslog(LOG_INFO)."
)

#if defined(HAVE_TCP_FASTOPEN)
#  define PLATFORM_FLAGS MUST_RESTART
#else
#  define PLATFORM_FLAGS NOT_IMPLEMENTED
#endif
PARAM_SIMPLE(
	/* name */	tcp_fastopen,
	/* type */	boolean,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"off",
	/* units */	"bool",
	/* descr */
	"Enable TCP Fast Open extension.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS
)
#undef PLATFORM_FLAGS

#if defined(HAVE_TCP_KEEP)
#  define PLATFORM_FLAGS EXPERIMENTAL
#else
#  define PLATFORM_FLAGS NOT_IMPLEMENTED
#endif
PARAM_SIMPLE(
	/* name */	tcp_keepalive_intvl,
	/* type */	timeout,
	/* min */	"1",
	/* max */	"100",
	/* def */	NULL,
	/* units */	"seconds",
	/* descr */
	"The number of seconds between TCP keep-alive probes. "
	"Ignored for Unix domain sockets.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	NULL,
	/* dyn_def_reason */	"platform dependent"
)

PARAM_SIMPLE(
	/* name */	tcp_keepalive_probes,
	/* type */	uint,
	/* min */	"1",
	/* max */	"100",
	/* def */	NULL,
	/* units */	"probes",
	/* descr */
	"The maximum number of TCP keep-alive probes to send before giving "
	"up and killing the connection if no response is obtained from the "
	"other end. Ignored for Unix domain sockets.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	NULL,
	/* dyn_def_reason */	"platform dependent"
)

PARAM_SIMPLE(
	/* name */	tcp_keepalive_time,
	/* type */	timeout,
	/* min */	"1",
	/* max */	"7200",
	/* def */	NULL,
	/* units */	"seconds",
	/* descr */
	"The number of seconds a connection needs to be idle before TCP "
	"begins sending out keep-alive probes. "
	"Ignored for Unix domain sockets.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	NULL,
	/* dyn_def_reason */	"platform dependent"
)
#undef PLATFORM_FLAGS

#if defined(SO_RCVTIMEO_WORKS)
#  define PLATFORM_FLAGS 0
#else
#  define PLATFORM_FLAGS NOT_IMPLEMENTED
#endif
PARAM_SIMPLE(
	/* name */	timeout_idle,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"5.000",
	/* units */	"seconds",
	/* descr */
	"Idle timeout for client connections.\n\n"
	"A connection is considered idle until we have received the full"
	" request headers.\n\n"
	"This parameter is particularly relevant for HTTP1 keepalive "
	" connections which are closed unless the next request is received"
	" before this timeout is reached.",
	/* flags */	PLATFORM_DEPENDENT | PLATFORM_FLAGS
)
#undef PLATFORM_FLAGS

PARAM_SIMPLE(
	/* name */	timeout_linger,
	/* type */	timeout,
	/* min */	"0.000",
	/* max */	NULL,
	/* def */	"0.050",
	/* units */	"seconds",
	/* descr */
	"How long the worker thread lingers on an idle session before "
	"handing it over to the waiter.\n"
	"When sessions are reused, as much as half of all reuses happen "
	"within the first 100 msec of the previous request completing.\n"
	"Setting this too high results in worker threads not doing "
	"anything for their keep, setting it too low just means that more "
	"sessions take a detour around the waiter.",
	/* flags */	EXPERIMENTAL
)

PARAM_SIMPLE(
	/* name */	vary_notice,
	/* type */	uint,
	/* min */	"1",
	/* max */	NULL,
	/* def */	"10",
	/* units */	"variants",
	/* descr */
	"How many variants need to be evaluated to log a Notice that there "
	"might be too many variants."
)

PARAM_SIMPLE(
	/* name */	vcl_cooldown,
	/* type */	timeout,
	/* min */	"1.000",
	/* max */	NULL,
	/* def */	"600.000",
	/* units */	"seconds",
	/* descr */
	"How long a VCL is kept warm after being replaced as the "
	"active VCL (granularity approximately 30 seconds)."
)

PARAM_SIMPLE(
	/* name */	max_vcl_handling,
	/* type */	uint,
	/* min */	"0",
	/* max */	"2",
	/* def */	"1",
	/* units */	NULL,
	/* descr */
	"Behaviour when attempting to exceed max_vcl loaded VCL.\n"
	"\n*  0 - Ignore max_vcl parameter.\n"
	"\n*  1 - Issue warning.\n"
	"\n*  2 - Refuse loading VCLs."
)

PARAM_SIMPLE(
	/* name */	max_vcl,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"100",
	/* units */	NULL,
	/* descr */
	"Threshold of loaded VCL programs.  (VCL labels are not counted.)"
	"  Parameter max_vcl_handling determines behaviour."
)

PARAM_SIMPLE(
	/* name */	vsm_free_cooldown,
	/* type */	timeout,
	/* min */	"10.000",
	/* max */	"600.000",
	/* def */	"60.000",
	/* units */	"seconds",
	/* descr */
	"How long VSM memory is kept warm after a deallocation "
	"(granularity approximately 2 seconds)."
)

PARAM_SIMPLE(
	/* name */	vsl_buffer,
	/* type */	vsl_buffer,
	/* min */	"267",
	/* max */	NULL,
	/* def */	"4k",
	/* units */	"bytes",
	/* descr */
	"Bytes of (req-/backend-)workspace dedicated to buffering VSL "
	"records.\n"
	"When this parameter is adjusted, most likely workspace_client "
	"and workspace_backend will have to be adjusted by the same amount.\n\n"
	"Setting this too high costs memory, setting it too low will cause "
	"more VSL flushes and likely increase lock-contention on the VSL "
	"mutex.",
	/* flags */	0,
	/* dyn_min_reason */	"vsl_reclen + 12 bytes"
)

PARAM_SIMPLE(
	/* name */	vsl_reclen,
	/* type */	vsl_reclen,
	/* min */	"16b",
	/* max */	NULL,
	/* def */	"255b",
	/* units */	"bytes",
	/* descr */
	"Maximum number of bytes in SHM log record.",
	/* flags */	0,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	"vsl_buffer - 12 bytes"
)

PARAM_SIMPLE(
	/* name */	vsl_space,
	/* type */	bytes,
	/* min */	"1M",
	/* max */	"4G",
	/* def */	"80M",
	/* units */	"bytes",
	/* descr */
	"The amount of space to allocate for the VSL fifo buffer in the "
	"VSM memory segment.  If you make this too small, "
	"varnish{ncsa|log} etc will not be able to keep up.  Making it too "
	"large just costs memory resources.",
	/* flags */	MUST_RESTART
)

PARAM_SIMPLE(
	/* name */	vsm_space,
	/* type */	bytes,
	/* min */	"1M",
	/* max */	"1G",
	/* def */	"1M",
	/* units */	"bytes",
	/* descr */
	"DEPRECATED: This parameter is ignored.\n"
	"There is no global limit on amount of shared memory now."
)

PARAM_SIMPLE(
	/* name */	workspace_backend,
	/* type */	bytes_u,
	/* min */	"1k",
	/* max */	NULL,
	/* def */	"64k",
	/* units */	"bytes",
	/* descr */
	"Bytes of HTTP protocol workspace for backend HTTP req/resp.  If "
	"larger than 4k, use a multiple of 4k for VM efficiency.",
	/* flags */	DELAYED_EFFECT
)

PARAM_SIMPLE(
	/* name */	workspace_client,
	/* type */	bytes_u,
	/* min */	"9k",
	/* max */	NULL,
	/* def */	"64k",
	/* units */	"bytes",
	/* descr */
	"Bytes of HTTP protocol workspace for clients HTTP req/resp.  Use a "
	"multiple of 4k for VM efficiency.\n"
	"For HTTP/2 compliance this must be at least 20k, in order to "
	"receive fullsize (=16k) frames from the client.   That usually "
	"happens only in POST/PUT bodies.  For other traffic-patterns "
	"smaller values work just fine.",
	/* flags */	DELAYED_EFFECT
)

PARAM_SIMPLE(
	/* name */	workspace_session,
	/* type */	bytes_u,
	/* min */	"0.25k",
	/* max */	NULL,
	/* def */	"0.75k",
	/* units */	"bytes",
	/* descr */
	"Allocation size for session structure and workspace.    The "
	"workspace is primarily used for TCP connection addresses.  If "
	"larger than 4k, use a multiple of 4k for VM efficiency.",
	/* flags */	DELAYED_EFFECT
)

PARAM_SIMPLE(
	/* name */	workspace_thread,
	/* type */	bytes_u,
	/* min */	"0.25k",
	/* max */	"8k",
	/* def */	"2k",
	/* units */	"bytes",
	/* descr */
	"Bytes of auxiliary workspace per thread.\n"
	"This workspace is used for certain temporary data structures "
	"during the operation of a worker thread.\n"
	"One use is for the IO-vectors used during delivery. Setting "
	"this parameter too low may increase the number of writev() "
	"syscalls, setting it too high just wastes space.  ~0.1k + "
	"UIO_MAXIOV * sizeof(struct iovec) (typically = ~16k for 64bit) "
	"is considered the maximum sensible value under any known "
	"circumstances (excluding exotic vmod use).",
	/* flags */	DELAYED_EFFECT
)

PARAM_SIMPLE(
	/* name */	h2_rx_window_low_water,
	/* type */	bytes_u,
	/* min */	"65535",
	/* max */	"1G",
	/* def */	"10M",
	/* units */	"bytes",
	/* descr */
	"HTTP2 Receive Window low water mark.\n"
	"We try to keep the window at least this big\n"
	"Only affects incoming request bodies (ie: POST, PUT etc.)",
	/* flags */	WIZARD
)

PARAM_SIMPLE(
	/* name */	h2_rx_window_increment,
	/* type */	bytes_u,
	/* min */	"1M",
	/* max */	"1G",
	/* def */	"1M",
	/* units */	"bytes",
	/* descr */
	"HTTP2 Receive Window Increments.\n"
	"How big credits we send in WINDOW_UPDATE frames\n"
	"Only affects incoming request bodies (ie: POST, PUT etc.)",
	/* flags */	WIZARD
)

PARAM_SIMPLE(
	/* name */	h2_header_table_size,
	/* type */	bytes_u,
	/* min */	"0b",
	/* max */	NULL,
	/* def */	"4k",
	/* units */	"bytes",
	/* descr */
	"HTTP2 header table size.\n"
	"This is the size that will be used for the HPACK dynamic\n"
	"decoding table."
)

PARAM_SIMPLE(
	/* name */	h2_max_concurrent_streams,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"100",
	/* units */	"streams",
	/* descr */
	"HTTP2 Maximum number of concurrent streams.\n"
	"This is the number of requests that can be active\n"
	"at the same time for a single HTTP2 connection."
)

PARAM_SIMPLE(
	/* name */	h2_initial_window_size,
	/* type */	bytes_u,
	/* min */	"0",
	/* max */	"2147483647b",
	/* def */	"65535b",
	/* units */	"bytes",
	/* descr */
	"HTTP2 initial flow control window size."
)

PARAM_SIMPLE(
	/* name */	h2_max_frame_size,
	/* type */	bytes_u,
	/* min */	"16k",
	/* max */	"16777215b",
	/* def */	"16k",
	/* units */	"bytes",
	/* descr */
	"HTTP2 maximum per frame payload size we are willing to accept."
)

PARAM_SIMPLE(
	/* name */	h2_max_header_list_size,
	/* type */	bytes_u,
	/* min */	"0b",
	/* max */	NULL,
	/* def */	"2147483647b",
	/* units */	"bytes",
	/* descr */
	"HTTP2 maximum size of an uncompressed header list."
)

/*--------------------------------------------------------------------
 * Memory pool parameters
 */

#define PARAM_MEMPOOL(nm, def, descr)					\
	PARAM(poolparam, nm, nm, tweak_poolparam, &mgt_param.nm,	\
	    NULL, NULL, def, NULL,					\
	    descr							\
	    "The three numbers are:\n"					\
	    "\tmin_pool\tminimum size of free pool.\n"			\
	    "\tmax_pool\tmaximum size of free pool.\n"			\
	    "\tmax_age\tmax age of free element.")

PARAM_MEMPOOL(
		/* name */	pool_req,
		/* def */	"10,100,10",
		/* descr */
		"Parameters for per worker pool request memory pool.\n\n"
)

PARAM_MEMPOOL(
		/* name */	pool_sess,
		/* def */	"10,100,10",
		/* descr */
		"Parameters for per worker pool session memory pool.\n\n"
)

PARAM_MEMPOOL(
		/* name */	pool_vbo,
		/* def */	"10,100,10",
		/* descr */
		"Parameters for backend object fetch memory pool.\n\n"
)

/*--------------------------------------------------------------------
 * Thread pool parameters
 */

#define PARAM_THREAD(nm, fld, typ, ...)			\
	PARAM(typ, wthread_ ## fld, nm, tweak_ ## typ,	\
	    &mgt_param.wthread_ ## fld, __VA_ARGS__)

PARAM_THREAD(
	/* name */	thread_pools,
	/* field */	pools,
	/* type */	uint,
	/* min */	"1",
	/* max */	NULL,
	/* def */	"2",
	/* units */	"pools",
	/* descr */
	"Number of worker thread pools.\n"
	"\n"
	"Increasing the number of worker pools decreases lock "
	"contention. Each worker pool also has a thread accepting "
	"new connections, so for very high rates of incoming new "
	"connections on systems with many cores, increasing the "
	"worker pools may be required.\n"
	"\n"
	"Too many pools waste CPU and RAM resources, and more than one "
	"pool for each CPU is most likely detrimental to performance.\n"
	"\n"
	"Can be increased on the fly, but decreases require a "
	"restart to take effect, unless the drop_pools experimental "
	"debug flag is set.",
	/* flags */	EXPERIMENTAL | DELAYED_EFFECT
)

PARAM_THREAD(
	/* name */	thread_pool_max,
	/* field */	max,
	/* type */	thread_pool_max,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"5000",
	/* units */	"threads",
	/* descr */
	"The maximum number of worker threads in each pool.\n"
	"\n"
	"Do not set this higher than you have to, since excess "
	"worker threads soak up RAM and CPU and generally just get "
	"in the way of getting work done.",
	/* flags */	DELAYED_EFFECT,
	/* dyn_min_reason */	"thread_pool_min"
)

PARAM_THREAD(
	/* name */	thread_pool_min,
	/* field */	min,
	/* type */	thread_pool_min,
	/* min */	"5" /* TASK_QUEUE__END */,
	/* max */	NULL,
	/* def */	"100",
	/* units */	"threads",
	/* descr */
	"The minimum number of worker threads in each pool.\n"
	"\n"
	"Increasing this may help ramp up faster from low load "
	"situations or when threads have expired.\n"
	"\n"
	"Technical minimum is 5 threads, but this parameter is "
	/*                    ^ TASK_QUEUE__END */
	"strongly recommended to be at least 10",
	/*               2 * TASK_QUEUE__END ^^ */
	/* flags */	DELAYED_EFFECT,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	"thread_pool_max"
)

PARAM_THREAD(
	/* name */	thread_pool_reserve,
	/* field */	reserve,
	/* type */	uint,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"0",
	/* units */	"threads",
	/* descr */
	"The number of worker threads reserved for vital tasks "
	"in each pool.\n"
	"\n"
	"Tasks may require other tasks to complete (for example, "
	"client requests may require backend requests, http2 sessions "
	"require streams, which require requests). This reserve is to "
	"ensure that lower priority tasks do not prevent higher "
	"priority tasks from running even under high load.\n"
	"\n"
	"The effective value is at least 5 (the number of internal "
	/*                               ^ TASK_QUEUE__END */
	"priority classes), irrespective of this parameter.",
	/* flags */	DELAYED_EFFECT,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	"95% of thread_pool_min",
	/* dyn_def_reason */	"0 (auto-tune: 5% of thread_pool_min)"
)

PARAM_THREAD(
	/* name */	thread_pool_timeout,
	/* field */	timeout,
	/* type */	timeout,
	/* min */	"10",
	/* max */	NULL,
	/* def */	"300",
	/* units */	"seconds",
	/* descr */
	"Thread idle threshold.\n"
	"\n"
	"Threads in excess of thread_pool_min, which have been idle "
	"for at least this long, will be destroyed.",
	/* flags */	EXPERIMENTAL | DELAYED_EFFECT
)

PARAM_THREAD(
	/* name */	thread_pool_watchdog,
	/* field */	watchdog,
	/* type */	timeout,
	/* min */	"0.1",
	/* max */	NULL,
	/* def */	"60",
	/* units */	"seconds",
	/* descr */
	"Thread queue stuck watchdog.\n"
	"\n"
	"If no queued work have been released for this long,"
	" the worker process panics itself.",
	/* flags */	EXPERIMENTAL
)

PARAM_THREAD(
	/* name */	thread_pool_destroy_delay,
	/* field */	destroy_delay,
	/* type */	timeout,
	/* min */	"0.01",
	/* max */	NULL,
	/* def */	"1",
	/* units */	"seconds",
	/* descr */
	"Wait this long after destroying a thread.\n"
	"\n"
	"This controls the decay of thread pools when idle(-ish).",
	/* flags */	EXPERIMENTAL | DELAYED_EFFECT
)

PARAM_THREAD(
	/* name */	thread_pool_add_delay,
	/* field */	add_delay,
	/* type */	timeout,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"0",
	/* units */	"seconds",
	/* descr */
	"Wait at least this long after creating a thread.\n"
	"\n"
	"Some (buggy) systems may need a short (sub-second) "
	"delay between creating threads.\n"
	"Set this to a few milliseconds if you see the "
	"'threads_failed' counter grow too much.\n"
	"\n"
	"Setting this too high results in insufficient worker threads.",
	/* flags */	EXPERIMENTAL
)

PARAM_THREAD(
	/* name */	thread_pool_fail_delay,
	/* field */	fail_delay,
	/* type */	timeout,
	/* min */	"10e-3",
	/* max */	NULL,
	/* def */	"0.2",
	/* units */	"seconds",
	/* descr */
	"Wait at least this long after a failed thread creation "
	"before trying to create another thread.\n"
	"\n"
	"Failure to create a worker thread is often a sign that "
	" the end is near, because the process is running out of "
	"some resource.  "
	"This delay tries to not rush the end on needlessly.\n"
	"\n"
	"If thread creation failures are a problem, check that "
	"thread_pool_max is not too high.\n"
	"\n"
	"It may also help to increase thread_pool_timeout and "
	"thread_pool_min, to reduce the rate at which treads are "
	"destroyed and later recreated.",
	/* flags */	EXPERIMENTAL
)

PARAM_THREAD(
	/* name */	thread_stats_rate,
	/* field */	stats_rate,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"10",
	/* units */	"requests",
	/* descr */
	"Worker threads accumulate statistics, and dump these into "
	"the global stats counters if the lock is free when they "
	"finish a job (request/fetch etc.)\n"
	"This parameters defines the maximum number of jobs "
	"a worker thread may handle, before it is forced to dump "
	"its accumulated stats into the global counters.",
	/* flags */	EXPERIMENTAL
)

PARAM_THREAD(
	/* name */	thread_queue_limit,
	/* field */	queue_limit,
	/* type */	uint,
	/* min */	"0",
	/* max */	NULL,
	/* def */	"20",
	/* units */	"requests",
	/* descr */
	"Permitted request queue length per thread-pool.\n"
	"\n"
	"This sets the number of requests we will queue, waiting "
	"for an available thread.  Above this limit sessions will "
	"be dropped instead of queued.",
	/* flags */	EXPERIMENTAL
)

PARAM_THREAD(
	/* name */	thread_pool_stack,
	/* field */	stacksize,
	/* type */	bytes,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	NULL,	/* default set in mgt_param.c */
	/* units */	"bytes",
	/* descr */
	"Worker thread stack size.\n"
	"This will likely be rounded up to a multiple of 4k"
	" (or whatever the page_size might be) by the kernel.\n"
	"\n"
	"The required stack size is primarily driven by the"
	" depth of the call-tree. The most common relevant"
	" determining factors in varnish core code are GZIP"
	" (un)compression, ESI processing and regular"
	" expression matches. VMODs may also require"
	" significant amounts of additional stack. The"
	" nesting depth of VCL subs is another factor,"
	" although typically not predominant.\n"
	"\n"
	"The stack size is per thread, so the maximum total"
	" memory required for worker thread stacks is in the"
	" order of size = thread_pools x thread_pool_max x"
	" thread_pool_stack.\n"
	"\n"
	"Thus, in particular for setups with many threads,"
	" keeping the stack size at a minimum helps reduce"
	" the amount of memory required by Varnish.\n"
	"\n"
	"On the other hand, thread_pool_stack must be large"
	" enough under all circumstances, otherwise varnish"
	" will crash due to a stack overflow. Usually, a"
	" stack overflow manifests itself as a segmentation"
	" fault (aka segfault / SIGSEGV) with the faulting"
	" address being near the stack pointer (sp).\n"
	"\n"
	"Unless stack usage can be reduced,"
	" thread_pool_stack must be increased when a stack"
	" overflow occurs. Setting it in 150%-200%"
	" increments is recommended until stack overflows"
	" cease to occur.",
	/* flags */	DELAYED_EFFECT,
	/* dyn_min_reason */	NULL,
	/* dyn_max_reason */	NULL,
	/* dyn_def_reason */	"sysconf(_SC_THREAD_STACK_MIN)"
)

#if defined(PARAM_ALL)

/*--------------------------------------------------------------------
 * String parameters
 */

#  define PARAM_STRING(nm, pv, def, ...) \
	PARAM(, , nm, tweak_string, pv, NULL, NULL, def, NULL, __VA_ARGS__)

PARAM_STRING(
	/* name */	cc_command,
	/* priv */	&mgt_cc_cmd,
	/* def */	VCC_CC,
	/* descr */
	"Command used for compiling the C source code to a "
	"dlopen(3) loadable object.  Any occurrence of %s in "
	"the string will be replaced with the source file name, "
	"and %o will be replaced with the output file name.",
	/* flags */	MUST_RELOAD
)

PARAM_STRING(
	/* name */	vcl_path,
	/* priv */	&mgt_vcl_path,
	/* def */	VARNISH_VCL_DIR,
	/* descr */
	"Directory (or colon separated list of directories) "
	"from which relative VCL filenames (vcl.load and "
	"include) are to be found.  By default Varnish searches "
	"VCL files in both the system configuration and shared "
	"data directories to allow packages to drop their VCL "
	"files in a standard location where relative includes "
	"would work."
)

PARAM_STRING(
	/* name */	vmod_path,
	/* priv */	&mgt_vmod_path,
	/* def */	VARNISH_VMOD_DIR,
	/* descr */
	"Directory (or colon separated list of directories) "
	"where VMODs are to be found."
)

/*--------------------------------------------------------------------
 * VCC parameters
 */

#  define PARAM_VCC(nm, def, descr) \
	PARAM(, , nm, tweak_boolean, &mgt_ ## nm, NULL, NULL, def, "bool", descr)

PARAM_VCC(
	/* name */	vcc_err_unref,
	/* def */	"on",
	/* descr */
	"Unreferenced VCL objects result in error."
)

PARAM_VCC(
	/* name */	vcc_allow_inline_c,
	/* def */	"off",
	/* descr */
	"Allow inline C code in VCL."
)

PARAM_VCC(
	/* name */	vcc_unsafe_path,
	/* def */	"on",
	/* descr */
	"Allow '/' in vmod & include paths.\n"
	"Allow 'import ... from ...'."
)

/*--------------------------------------------------------------------
 * PCRE parameters
 */

#  define PARAM_PCRE(nm, pv, min, def, descr)			\
	PARAM(, , nm, tweak_uint, &mgt_param.vre_limits.pv,	\
	    min, NULL, def, NULL, descr)

PARAM_PCRE(
	/* name */	pcre_match_limit,
	/* priv */	match,
	/* min */	"1",
	/* def */	"10000",
	/* descr */
	"The limit for the number of calls to the internal match()"
	" function in pcre_exec().\n\n"
	"(See: PCRE_EXTRA_MATCH_LIMIT in pcre docs.)\n\n"
	"This parameter limits how much CPU time"
	" regular expression matching can soak up."
)

PARAM_PCRE(
	/* name */	pcre_match_limit_recursion,
	/* priv */	match_recursion,
	/* min */	"1",
	/* def */	"20",
	/* descr */
	"The recursion depth-limit for the internal match() function"
	" in a pcre_exec().\n\n"
	"(See: PCRE_EXTRA_MATCH_LIMIT_RECURSION in pcre docs.)\n\n"
	"This puts an upper limit on the amount of stack used"
	" by PCRE for certain classes of regular expressions.\n\n"
	"We have set the default value low in order to"
	" prevent crashes, at the cost of possible regexp"
	" matching failures.\n\n"
	"Matching failures will show up in the log as VCL_Error"
	" messages with regexp errors -27 or -21.\n\n"
	"Testcase r01576 can be useful when tuning this parameter."
)

#  undef PARAM_ALL
#  undef PARAM_PCRE
#  undef PARAM_STRING
#  undef PARAM_VCC
#endif /* defined(PARAM_ALL) */

#undef PARAM_MEMPOOL
#undef PARAM_SIMPLE
#undef PARAM_THREAD
#undef PARAM

#if 0 /* NOT ACTUALLY DEFINED HERE */
/* actual location mgt_param_bits.c*/
/* see tbl/debug_bits.h */
PARAM(
	/* name */	debug,
	/* type */	debug,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	NULL,
	/* units */	NULL,
	/* descr */
	"Enable/Disable various kinds of debugging.\n"
	"	none	Disable all debugging\n"
	"\n"
	"Use +/- prefix to set/reset individual bits:\n"
	"	req_state	VSL Request state engine\n"
	"	workspace	VSL Workspace operations\n"
	"	waiter	VSL Waiter internals\n"
	"	waitinglist	VSL Waitinglist events\n"
	"	syncvsl	Make VSL synchronous\n"
	"	hashedge	Edge cases in Hash\n"
	"	vclrel	Rapid VCL release\n"
	"	lurker	VSL Ban lurker\n"
	"	esi_chop	Chop ESI fetch to bits\n"
	"	flush_head	Flush after http1 head\n"
	"	vtc_mode	Varnishtest Mode"
)

/* actual location mgt_param_bits.c*/
/* See tbl/feature_bits.h */
PARAM(
	/* name */	feature,
	/* type */	feature,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	NULL,
	/* units */	NULL,
	/* descr */
	"Enable/Disable various minor features.\n"
	"	none	Disable all features.\n"
	"\n"
	"Use +/- prefix to enable/disable individual feature:\n"
	"	short_panic	Short panic message.\n"
	"	wait_silo	Wait for persistent silo.\n"
	"	no_coredump	No coredumps.\n"
	"	esi_ignore_https	Treat HTTPS as HTTP in ESI:includes\n"
	"	esi_disable_xml_check	Don't check of body looks like XML\n"
	"	esi_ignore_other_elements	Ignore non-esi XML-elements\n"
	"	esi_remove_bom	Remove UTF-8 BOM"
)

/* actual location mgt_param_bits.c*/
PARAM(
	/* name */	vsl_mask,
	/* type */	vsl_mask,
	/* min */	NULL,
	/* max */	NULL,
	/* def */	"default",
	/* units */	NULL,
	/* descr */
	"Mask individual VSL messages from being logged.\n"
	"	default	Set default value\n"
	"\n"
	"Use +/- prefix in front of VSL tag name to unmask/mask "
	"individual VSL messages."
)
#endif /* NOT ACTUALLY DEFINED HERE */

/*lint -restore */
