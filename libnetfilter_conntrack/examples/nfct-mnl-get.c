#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>

#include <libmnl/libmnl.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

#include <linux/netfilter/nf_conntrack_tcp.h>

static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nf_conntrack *ct;
	char buf[4096];

	ct = nfct_new();
	if (ct == NULL)
		return MNL_CB_OK;

	nfct_nlmsg_parse(nlh, ct);

	nfct_snprintf(buf, sizeof(buf), ct, NFCT_T_UNKNOWN, NFCT_O_DEFAULT, 0);
	printf("%s\n", buf);

	nfct_destroy(ct);

	return MNL_CB_OK;
}

int main(void)
{
	struct mnl_socket *nl;
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfh;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	unsigned int seq, portid;
	struct nf_conntrack *ct;
	int ret;

	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	portid = mnl_socket_get_portid(nl);

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = (NFNL_SUBSYS_CTNETLINK << 8) | IPCTNL_MSG_CT_GET;
	nlh->nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	nlh->nlmsg_seq = seq = time(NULL);

	nfh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct nfgenmsg));
	nfh->nfgen_family = AF_INET;
	nfh->version = NFNETLINK_V0;
	nfh->res_id = 0;

	ct = nfct_new();
	if (ct == NULL) {
		perror("nfct_new");
		return 0;
	}

	nfct_set_attr_u8(ct, ATTR_L3PROTO, AF_INET);
	nfct_set_attr_u32(ct, ATTR_IPV4_SRC, inet_addr("1.1.1.1"));
	nfct_set_attr_u32(ct, ATTR_IPV4_DST, inet_addr("2.2.2.2"));

	nfct_set_attr_u8(ct, ATTR_L4PROTO, IPPROTO_TCP);
	nfct_set_attr_u16(ct, ATTR_PORT_SRC, htons(20));
	nfct_set_attr_u16(ct, ATTR_PORT_DST, htons(10));

	nfct_nlmsg_build(nlh, ct);

	ret = mnl_socket_sendto(nl, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		exit(EXIT_FAILURE);
	}

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, NULL);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		exit(EXIT_FAILURE);
	}

	mnl_socket_close(nl);

	return 0;
}
