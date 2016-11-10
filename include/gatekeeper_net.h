/*
 * Gatekeeper - DoS protection system.
 * Copyright (C) 2016 Digirati LTDA.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GATEKEEPER_NET_H_
#define _GATEKEEPER_NET_H_

#include <stdint.h>

#include <rte_atomic.h>

/* Size of the secret key of the RSS hash. */
#define GATEKEEPER_RSS_KEY_LEN (40)

/*
 * A Gatekeeper interface is specified by a set of PCI addresses
 * that map to DPDK port numbers. If multiple ports are specified,
 * then the ports are bonded.
 */
struct gatekeeper_if {
	/* The ports (in PCI address format) that compose this interface. */
	char		**pci_addrs;

	/* The number of ports that in this interface (length of @pci_addrs). */
	uint8_t		num_ports;

	/* Name of the interface. Needed for setting/getting bonded port. */
	char		*name;

	/* Number of RX and TX queues for this interface. */
	uint16_t	num_rx_queues;
	uint16_t	num_tx_queues;

	/*
	 * The fields below are for internal use.
	 * Configuration files should not refer to them.
	 */

	/* DPDK port IDs corresponding to each address in @pci_addrs. */
	uint8_t		*ports;

	/*
	 * The DPDK port ID for this interface.
	 *
	 * If @ports only has one element, then @id is that port.
	 * If @ports has multiple elements, then @id is the DPDK
	 * *bonded* port ID representing all of those ports.
	 */
	uint8_t         id;

	/* The RX and TX queue assignments on this interface for each lcore. */
	int16_t		rx_queues[RTE_MAX_LCORE];
	int16_t		tx_queues[RTE_MAX_LCORE];

	/*
	 * The next RX and TX queues to be assigned on this interface.
	 * We need atomic here in case multiple blocks are trying to
	 * configure their queues on the same interface at the same time.
	 */
	rte_atomic16_t	rx_queue_id;
	rte_atomic16_t	tx_queue_id;
};

/*
 * The atomic counters for @rx_queue_id and @tx_queue_id are
 * signed, so we get about 2^15 possible queues available for use,
 * which is much more than is needed.
 *
 * Use this constant as an out-of-band value to represent that
 * a queue has not been allocated; if one of the atomic counters
 * reaches this value, we have exceeded the number of possible
 * queues.
 */
#define GATEKEEPER_QUEUE_UNALLOCATED	(INT16_MIN)

enum queue_type {
	QUEUE_TYPE_RX,
	QUEUE_TYPE_TX,
	QUEUE_TYPE_MAX,
};

int get_queue_id(struct gatekeeper_if *iface, enum queue_type ty,
	unsigned int lcore);

/* Configuration for the Network. */
struct net_config {
	/*
	 * The fields below are for internal use.
	 * Configuration files should not refer to them.
	 */
	struct gatekeeper_if	front;
	struct gatekeeper_if	back;

	uint32_t		num_ports;
	uint32_t		numa_nodes;
	struct rte_mempool 	**gatekeeper_pktmbuf_pool;

	/*
	 * Set to true while network devices are being configured,
	 * and set to false when all network devices have started.
	 * This is needed to enforce the ordering:
	 *  configure devices -> configure per-block queues -> start devices
	 */
	volatile int		configuring;
};

extern uint8_t default_rss_key[GATEKEEPER_RSS_KEY_LEN];

int lua_init_iface(struct gatekeeper_if *iface, const char *iface_name,
	const char **pci_addrs, uint8_t num_pci_addrs);
void lua_free_iface(struct gatekeeper_if *iface);

struct net_config *get_net_conf(void);
struct gatekeeper_if *get_if_front(struct net_config *net_conf);
struct gatekeeper_if *get_if_back(struct net_config *net_conf);
int gatekeeper_setup_rss(uint8_t portid, uint16_t *queues, uint16_t num_queues);
int gatekeeper_init_network(struct net_config *net_conf);
int gatekeeper_start_network(void);
void gatekeeper_free_network(void);

#endif /* _GATEKEEPER_NET_H_ */