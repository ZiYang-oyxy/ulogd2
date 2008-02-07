/* ulogd_filter_IFINDEX.c, Version $Revision: 1500 $
 *
 * ulogd interpreter plugin for ifindex to ifname conversion
 *
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: ulogd_filter_IFINDEX.c 1500 2005-10-03 16:54:02Z laforge $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ulogd/ulogd.h>
#include <libnfnetlink/libnfnetlink.h>

static struct ulogd_key ifindex_keys[] = {
	{ 
		.type = ULOGD_RET_STRING,
		.flags = ULOGD_RETF_NONE | ULOGD_RETF_FREE,
		.name = "oob.in", 
	},
	{ 
		.type = ULOGD_RET_STRING,
		.flags = ULOGD_RETF_NONE | ULOGD_RETF_FREE,
		.name = "oob.out", 
	},
};

static struct ulogd_key ifindex_inp[] = {
	{ 
		.type = ULOGD_RET_UINT32,
		.name = "oob.ifindex_in", 
	},
	{
		.type = ULOGD_RET_UINT32,
		.name = "oob.ifindex_out",
	},
};

/* we only need one global static cache of ifindex to ifname mappings, 
 * so all state is global (as opposed to per-instance local state in almost
 * all other plugins */
static struct ulogd_fd nlif_u_fd = { .fd = -1 };
static int nlif_users;
static struct nlif_handle *nlif_inst;

static int interp_ifindex(struct ulogd_pluginstance *pi)
{
	struct ulogd_key *ret = pi->output.keys;
	struct ulogd_key *inp = pi->input.keys;

	ret[0].u.value.ptr = calloc(IFNAMSIZ, sizeof(char)); 
	nlif_index2name(nlif_inst, inp[0].u.source->u.value.ui32,
			ret[0].u.value.ptr);
	if (((char *)ret[0].u.value.ptr)[0] == '*')
		((char *)(ret[0].u.value.ptr))[0] = 0; 
	ret[0].flags |= ULOGD_RETF_VALID;

	ret[1].u.value.ptr = calloc(IFNAMSIZ, sizeof(char)); 
	nlif_index2name(nlif_inst, inp[1].u.source->u.value.ui32,
			ret[1].u.value.ptr);
	if (((char *)ret[1].u.value.ptr)[0] == '*')
		((char *)(ret[1].u.value.ptr))[0] = 0; 
	ret[1].flags |= ULOGD_RETF_VALID;

	return 0;
}

static int nlif_read_cb(int fd, unsigned int what, void *param)
{
	if (!(what & ULOGD_FD_READ))
		return 0;

	nlif_catch(nlif_inst);
}

static int ifindex_start(struct ulogd_pluginstance *upi)
{
	int rc;

	/* if we're already initialized, inc usage count and exit */
	if (nlif_u_fd.fd >= 0) {
		nlif_users++;
		return 0;
	}

	/* if we reach here, we need to initialize */
	nlif_inst = nlif_open();
	if (nlif_inst == NULL) {
		return nlif_u_fd.fd;
	}
	nlif_query(nlif_inst);
	
	nlif_u_fd.fd = nlif_fd(nlif_inst);
	nlif_u_fd.when = ULOGD_FD_READ;
	nlif_u_fd.cb = &nlif_read_cb;
	rc = ulogd_register_fd(&nlif_u_fd);
	if (rc < 0)
		goto out_nlif;

	nlif_users++;
	return 0;

out_nlif:
	nlif_close(nlif_inst);
	nlif_u_fd.fd = -1;
	return rc;
}

static int ifindex_fini(struct ulogd_pluginstance *upi)
{
	if (--nlif_users == 0) {
		ulogd_unregister_fd(&nlif_u_fd);
		nlif_close(nlif_inst);
		nlif_u_fd.fd = -1;
	}

	return 0;
}

static struct ulogd_plugin ifindex_plugin = {
	.name = "IFINDEX",
	.input = {
		.keys = ifindex_inp,
		.num_keys = ARRAY_SIZE(ifindex_inp),
		.type = ULOGD_DTYPE_PACKET | ULOGD_DTYPE_FLOW,
		},
	.output = {
		.keys = ifindex_keys,
		.num_keys = ARRAY_SIZE(ifindex_keys),
		.type = ULOGD_DTYPE_PACKET | ULOGD_DTYPE_FLOW,
		},
	.interp = &interp_ifindex,

	.start = &ifindex_start,
	.stop = &ifindex_fini,
	.version = ULOGD_VERSION,
};

void __attribute__ ((constructor)) init(void);

void init(void)
{
	ulogd_register_plugin(&ifindex_plugin);
}
