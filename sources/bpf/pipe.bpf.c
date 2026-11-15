#include <linux/types.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define PIPE_STATE_SINK_UNUSED 0

struct pipe_state {
	__u32 sink;
};

struct {
	__uint(type, BPF_MAP_TYPE_SOCKMAP);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u32));
	__uint(max_entries, 65536);
} client_sockets SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_SOCKMAP);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u32));
	__uint(max_entries, 65536);
} server_sockets SEC(".maps");

/* client port -> state */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(struct pipe_state));
	__uint(max_entries, 65536);
} client_states SEC(".maps");

/* server port -> state */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(struct pipe_state));
	__uint(max_entries, 65536);
} server_states SEC(".maps");

SEC("sk_skb/stream_verdict")
int client_fwd(struct __sk_buff *skb)
{
	__u32 key = bpf_ntohl(skb->remote_port);

	struct pipe_state *st = bpf_map_lookup_elem(&client_states, &key);
	if (st == NULL || st->sink == PIPE_STATE_SINK_UNUSED) {
		return SK_PASS;
	}

	bpf_sk_redirect_map(skb, &server_sockets, st->sink, 0);

	return SK_PASS;
}

SEC("sk_skb/stream_verdict")
int server_fwd(struct __sk_buff *skb)
{
	__u32 key = skb->local_port;

	struct pipe_state *st = bpf_map_lookup_elem(&server_states, &key);
	if (st == NULL || st->sink == PIPE_STATE_SINK_UNUSED) {
		return SK_PASS;
	}

	bpf_sk_redirect_map(skb, &client_sockets, st->sink, 0);
	return SK_PASS;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
