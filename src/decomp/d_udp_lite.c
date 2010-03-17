/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file d_udp_lite.c
 * @brief ROHC decompression context for the UDP-Lite profile.
 * @author Didier Barvaux <didier.barvaux@toulouse.viveris.com>
 * @author The hackers from ROHC for Linux
 */

#include "d_udp_lite.h"
#include "rohc_traces.h"


/*
 * Private function prototypes.
 */

int udp_lite_decode_uo_tail_udp(struct d_generic_context *context,
                                const unsigned char *packet,
                                unsigned int length,
                                unsigned char *dest);


/**
 * @brief Create the UDP-Lite decompression context.
 *
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @return The newly-created UDP-Lite decompression context
 */
void * d_udp_lite_create(void)
{
	struct d_generic_context *context;
	struct d_udp_lite_context *udp_lite_context;

	/* create the generic context */
	context = d_generic_create();
	if(context == NULL)
		goto quit;

	/* create the UDP-Lite-specific part of the context */
	udp_lite_context = malloc(sizeof(struct d_udp_lite_context));
	if(udp_lite_context == NULL)
	{
		rohc_debugf(0, "cannot allocate memory for the UDP-Lite-specific context\n");
		goto destroy_context;
	}
	bzero(udp_lite_context, sizeof(struct d_udp_lite_context));
	context->specific = udp_lite_context;

	/* the UDP-Lite checksum coverage field present flag will be initialized
	 * with the IR or IR-DYN packets */
	udp_lite_context->cfp = -1;
	udp_lite_context->cfi = -1;

	/* some UDP-Lite-specific values and functions */
	context->next_header_len = sizeof(struct udphdr);
	context->build_next_header = udp_lite_build_uncompressed_udp;
	context->decode_static_next_header = udp_decode_static_udp;
	context->decode_dynamic_next_header = udp_lite_decode_dynamic_udp;
	context->decode_uo_tail = udp_lite_decode_uo_tail_udp;
	context->compute_crc_static = udp_compute_crc_static;
	context->compute_crc_dynamic = udp_compute_crc_dynamic;

	/* create the UDP-Lite-specific part of the header changes */
	context->last1->next_header_len = sizeof(struct udphdr);
	context->last1->next_header = malloc(sizeof(struct udphdr));
	if(context->last1->next_header == NULL)
	{
		rohc_debugf(0, "cannot allocate memory for the UDP-Lite-specific "
		               "part of the header changes last1\n");
		goto free_udp_context;
	}
	bzero(context->last1->next_header, sizeof(struct udphdr));

	context->last2->next_header_len = sizeof(struct udphdr);
	context->last2->next_header = malloc(sizeof(struct udphdr));
	if(context->last2->next_header == NULL)
	{
		rohc_debugf(0, "cannot allocate memory for the UDP-Lite-specific "
		               "part of the header changes last2\n");
		goto free_last1_next_header;
	}
	bzero(context->last2->next_header, sizeof(struct udphdr));

	context->active1->next_header_len = sizeof(struct udphdr);
	context->active1->next_header = malloc(sizeof(struct udphdr));
	if(context->active1->next_header == NULL)
	{
		rohc_debugf(0, "cannot allocate memory for the UDP-Lite-specific "
		               "part of the header changes active1\n");
		goto free_last2_next_header;
	}
	bzero(context->active1->next_header, sizeof(struct udphdr));

	context->active2->next_header_len = sizeof(struct udphdr);
	context->active2->next_header = malloc(sizeof(struct udphdr));
	if(context->active2->next_header == NULL)
	{
		rohc_debugf(0, "cannot allocate memory for the UDP-Lite-specific "
		               "part of the header changes active2\n");
		goto free_active1_next_header;
	}
	bzero(context->active2->next_header, sizeof(struct udphdr));

	/* set next header to UDP-Lite */
	context->next_header_proto = IPPROTO_UDPLITE;

	return context;

free_active1_next_header:
	zfree(context->active1->next_header);
free_last2_next_header:
	zfree(context->last2->next_header);
free_last1_next_header:
	zfree(context->last1->next_header);
free_udp_context:
	zfree(udp_lite_context);
destroy_context:
	d_generic_destroy(context);
quit:
	return NULL;
}


/**
 * @brief Destroy the context.
 * 
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param context The compression context
 */
void d_udp_lite_destroy(void *context)
{
	struct d_generic_context *c;

	if(context != NULL)
	{
		c = context;

		if(c->last1 != NULL && c->last1->next_header != NULL)
			zfree(c->last1->next_header);
		if(c->last2 != NULL && c->last2->next_header != NULL)
			zfree(c->last2->next_header);
		if(c->active1 != NULL && c->active1->next_header != NULL)
			zfree(c->active1->next_header);
		if(c->active2 != NULL && c->active2->next_header != NULL)
			zfree(c->active2->next_header);

		d_generic_destroy(context);
	}
}

/**
 * @brief Get the size of the static part of an IR packet
 * @return the size
 */
int udp_lite_get_static_part(void)
{
	return 4;	
}

/**
 * @brief Decode one IR packet for the UDP-Lite profile.
 *
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param decomp          The ROHC decompressor
 * @param context         The decompression context
 * @param packet          The ROHC packet to decode
 * @param copy_size       The length of the ROHC packet to decode
 * @param large_cid_len   The length of the large CID field
 * @param is_addcid_used  Whether the add-CID field is present or not
 * @param dest            The decoded IP packet
 * @return                The length of the uncompressed IP packet
 *                        or ROHC_OK_NO_DATA if packet is feedback only
 *                        or ROHC_ERROR if an error occurs
 */
int d_udp_lite_decode_ir(struct rohc_decomp *decomp,
                         struct d_context *context,
                         unsigned char *packet,
                         int copy_size,
                         int large_cid_len,
                         int is_addcid_used,
                         unsigned char *dest)
{
	struct d_generic_context *g_context = context->specific;
	struct d_udp_lite_context *udp_lite_context = g_context->specific;

	udp_lite_context->cfp = -1;
	udp_lite_context->cfi = -1;

	return d_generic_decode_ir(decomp, context, packet, copy_size,
	                           large_cid_len, is_addcid_used, dest);
}


/**
 * @brief Find the length of the IR header.
 *
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * \verbatim

 Basic structure of the IR packet (5.7.7.1):

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  |         Add-CID octet         |  if for small CIDs and CID != 0
    +---+---+---+---+---+---+---+---+
 2  | 1   1   1   1   1   1   0 | D |
    +---+---+---+---+---+---+---+---+
    |                               |
 3  /    0-2 octets of CID info     /  1-2 octets if for large CIDs
    |                               |
    +---+---+---+---+---+---+---+---+
 4  |            Profile            |  1 octet
    +---+---+---+---+---+---+---+---+
 5  |              CRC              |  1 octet
    +---+---+---+---+---+---+---+---+
    |                               |
 6  |         Static chain          |  variable length
    |                               |
    +---+---+---+---+---+---+---+---+
    |                               |
 7  |         Dynamic chain         |  present if D = 1, variable length
    |                               |
    +---+---+---+---+---+---+---+---+
 8  |             SN                |  2 octets if not RTP
    +---+---+---+---+---+---+---+---+
    |                               |
 9  |           Payload             |  variable length
    |                               |
     - - - - - - - - - - - - - - - -

\endverbatim
 *
 * The function computes the length of the fields 2 + 4-8, ie. the first byte,
 * the Profile and CRC fields, the static and dynamic chains (outer and inner
 * IP headers + UDP-Lite header) and the SN.
 *
 * @param context         The decompression context
 * @param packet          The pointer on the IR packet minus the Add-CID byte
 *                        (ie. the field 2 in the figure)
 * @param plen            The length of the IR packet minus the Add-CID byte
 * @param large_cid_len   The size of the large CID field
 *                        (ie. field 3 in the figure)
 * @return                The length of the IR header,
 *                        0 if an error occurs
 */
unsigned int udp_lite_detect_ir_size(struct d_context *context,
                                     unsigned char *packet,
                                     unsigned int plen,
                                     unsigned int large_cid_len)
{
	unsigned int length, d;

	/* Profile and CRC fields + IP static & dynamic chains + UDP static &
	 * dynamic chain + SN */
	length = udp_detect_ir_size(context, packet, plen, large_cid_len);
	if(length == 0)
		goto quit;

	/* UDP-Lite dynamic part (if included) needs 2 bytes more than the UDP
	 * dynamic part (see 5.7.7.5 in RFC 3095 and 5.2.1 in RFC 4019) */
	d = GET_BIT_0(packet);
	if(d)
		length += 2;

quit:
	return length;
}


/**
 * @brief Find the length of the IR-DYN header.
 * 
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * \verbatim

 Basic structure of the IR-DYN packet (5.7.7.2):

      0   1   2   3   4   5   6   7
     --- --- --- --- --- --- --- ---
 1  :         Add-CID octet         : if for small CIDs and CID != 0
    +---+---+---+---+---+---+---+---+
 2  | 1   1   1   1   1   0   0   0 | IR-DYN packet type
    +---+---+---+---+---+---+---+---+
    :                               :
 3  /     0-2 octets of CID info    / 1-2 octets if for large CIDs
    :                               :
    +---+---+---+---+---+---+---+---+
 4  |            Profile            | 1 octet
    +---+---+---+---+---+---+---+---+
 5  |              CRC              | 1 octet
    +---+---+---+---+---+---+---+---+
    |                               |
 6  /         Dynamic chain         / variable length
    |                               |
    +---+---+---+---+---+---+---+---+
 7  |             SN                | 2 octets if not RTP
    +---+---+---+---+---+---+---+---+
    :                               :
 8  /           Payload             / variable length
    :                               :
     - - - - - - - - - - - - - - - -

\endverbatim
 *
 * The function computes the length of the fields 2 + 4-8, ie. the first byte,
 * the Profile and CRC fields, the dynamic chains (outer and inner IP headers +
 * UDP-Lite header) and the SN.
 *
 * @param context         The decompression context
 * @param packet          The IR-DYN packet after the Add-CID byte if present
 *                        (ie. field 2 in the figure)
 * @param plen            The length of the IR-DYN packet minus the Add-CID byte
 * @param large_cid_len   The size of the large CID field
 *                        (ie. field 3 in the figure)
 * @return                The length of the IR-DYN header,
 *                        0 if an error occurs
 */
unsigned int udp_lite_detect_ir_dyn_size(struct d_context *context,
                                         unsigned char *packet,
                                         unsigned int plen,
                                         unsigned int large_cid_len)
{
	unsigned int length;

	/* Profile and CRC fields + IP dynamic chains + UDP dynamic chain */
	length = udp_detect_ir_dyn_size(context, packet, plen, large_cid_len);
	if(length == 0)
		goto quit;
	
	/* UDP-Lite dynamic part needs 2 bytes more than the UDP dynamic part
	 * (see 5.7.7.5 in RFC 3095 and 5.2.1 in RFC 4019) */
	length += 2;

quit:
	return length;
}


/**
 * @brief Decode one IR-DYN, UO-0, UO-1 or UOR-2 packet, but not IR packet.
 *
 * This function is one of the functions that must exist in one profile for the
 * framework to work.
 *
 * @param decomp      The ROHC decompressor
 * @param context     The decompression context
 * @param packet      The ROHC packet to decode
 * @param size        The length of the ROHC packet
 * @param second_byte The offset for the second byte of the ROHC packet
 *                    (depends on the CID encoding and the packet type)
 * @param dest        The decoded IP packet
 * @return            The length of the uncompressed IP packet
 *                    or ROHC_ERROR if an error occurs
 */
int d_udp_lite_decode(struct rohc_decomp *decomp,
                      struct d_context *context,
                      unsigned char *packet,
                      int size,
                      int second_byte,
                      unsigned char *dest)
{
	struct d_generic_context *g_context = context->specific;
	struct d_udp_lite_context *udp_lite_context = g_context->specific;
	int packet_type;

	/* check if the ROHC packet is large enough to read the one byte */
	if(size < 2)
	{
		rohc_debugf(0, "ROHC packet too small (len = %d)\n", size);
		goto error;
	}

	/* find whether the IR packet owns an Coverage Checksum Extension or not */
	switch(*packet)
	{
		case 0xf9: /* CCE() */
			rohc_debugf(2, "CCE()\n");
			udp_lite_context->cce_packet = PACKET_CCE;
			break;
		case 0xfa: /* CEC(ON) */
			rohc_debugf(2, "CCE(ON)\n");
			udp_lite_context->cfp = 1;
			udp_lite_context->cce_packet = PACKET_CCE;
			break;
		case 0xfb: /* CCE(OFF) */
			rohc_debugf(2, "CCE(OFF)\n");
			udp_lite_context->cfp = 0;
			udp_lite_context->cce_packet = PACKET_CCE_OFF;
			break;
		default:
			rohc_debugf(2, "CCE not present\n");
			udp_lite_context->cce_packet = 0;
	}

	/* if the CE extension is present, skip the first byte
	 * and go to the second byte */
	if(udp_lite_context->cce_packet)
	{
		packet += second_byte;
		second_byte = 1;
		size--;
	}

	/* reset the coverage infos if IR-DYN packet */
	packet_type = find_packet_type(decomp, context, packet, second_byte);
	if(packet_type == PACKET_IR_DYN)
	{
		udp_lite_context->cfp = -1;
		udp_lite_context->cfi = -1;
	}

	return d_generic_decode(decomp, context, packet, size,
	                        second_byte, dest);

error:
	return ROHC_ERROR;
}


/**
 * @brief Decode the UDP-Lite dynamic part of the ROHC packet.
 *
 * @param context      The generic decompression context
 * @param packet       The ROHC packet to decode
 * @param length       The length of the ROHC packet
 * @param dest         The decoded UDP header
 * @return             The number of bytes read in the ROHC packet,
 *                     -1 in case of failure
 */
int udp_lite_decode_dynamic_udp(struct d_generic_context *context,
                                const unsigned char *packet,
                                unsigned int length,
                                unsigned char *dest)
{
	struct d_udp_lite_context *udp_lite_context;
	struct udphdr *udp_lite;
	int dynamic_length;
	int udp_lite_length;
	int read = 0;
	int ret;

	udp_lite_context = context->specific;
	udp_lite = (struct udphdr *) dest;

	dynamic_length = (udp_lite_context->cfp != 0 ? 2 : 0) + 2;

	/* check the minimal length to decode the UDP-Lite dynamic part */
	if(length < dynamic_length)
	{
		rohc_debugf(0, "ROHC packet too small (len = %d)\n", length);
		goto error;
	}

	/* compute the length of the UDP-Lite packet: the IR and IR-DYN packets
	 * own a 2-byte SN field after the dynamic chain, the UO* packets do not */
	udp_lite_length = length - dynamic_length + sizeof(struct udphdr);
	if(context->packet_type == PACKET_IR ||
	   context->packet_type == PACKET_IR_DYN)
		udp_lite_length -= 2;

	/* checksum coverage if present or uninitialized */
	if(udp_lite_context->cfp != 0)
	{
		/* retrieve the checksum coverage field from the ROHC packet */
		udp_lite->len = GET_NEXT_16_BITS(packet);
		rohc_debugf(2, "checksum coverage = 0x%04x\n", ntohs(udp_lite->len));
		read += 2;
		packet += 2;

		/* init the Coverage Field Present (CFP) if uninitialized (see 5.2.2
		 * in RFC 4019) */
		if(udp_lite_context->cfp < 0)
		{
			udp_lite_context->cfp = (udp_lite_length != ntohs(udp_lite->len));
			rohc_debugf(1, "init CFP to %d (length = %d, CC = %d)\n",
			            udp_lite_context->cfp, udp_lite_length,
			            ntohs(udp_lite->len));
		}
	}

	/* init Coverage Field Inferred (CFI) if uninitialized (see 5.2.2 in
	 * RFC 4019) */
	if(udp_lite_context->cfi < 0)
	{
		udp_lite_context->cfi = (udp_lite_length == ntohs(udp_lite->len));
		rohc_debugf(1, "init CFI to %d (length = %d, CC = %d)\n",
		            udp_lite_context->cfi, udp_lite_length, ntohs(udp_lite->len));
	}

	/* retrieve the checksum field from the ROHC packet */
	udp_lite->check = GET_NEXT_16_BITS(packet);
	rohc_debugf(2, "checksum = 0x%04x\n", ntohs(udp_lite->check));
	packet += 2;
	read += 2;

	/* SN field */
	ret = ip_decode_dynamic_ip(context, packet, length - read, dest + read);
	if(ret == -1)
		goto error;
	packet += ret;
	read += ret;

	return read;

error:
	return -1;
}


/**
 * @brief Decode the UDP-Lite tail of the UO* ROHC packets.
 *
 * @param context      The generic decompression context
 * @param packet       The ROHC packet to decode
 * @param length       The length of the ROHC packet
 * @param dest         The decoded UDP-Lite header
 * @return             The number of bytes read in the ROHC packet,
 *                     -1 in case of failure
 */
int udp_lite_decode_uo_tail_udp(struct d_generic_context *context,
                                const unsigned char *packet,
                                unsigned int length,
                                unsigned char *dest)

{
	struct d_udp_lite_context *udp_lite_context;
	struct udphdr *udp_lite;
	int dynamic_length;
	int read = 0; /* number of bytes read from the packet */

	udp_lite_context = context->specific;
	udp_lite = (struct udphdr *) dest;

	dynamic_length = (udp_lite_context->cfp != 0 ? 2 : 0) + 2;

	/* check the minimal length to decode the tail of UO* packet */
	if(length < dynamic_length)
	{
		rohc_debugf(0, "ROHC packet too small (len = %d)\n", length);
		goto error;
	}

	/* checksum coverage if present */
	if(udp_lite_context->cfp > 0)
	{
		/* retrieve the checksum coverage field from the ROHC packet */
		udp_lite->len = GET_NEXT_16_BITS(packet);
		rohc_debugf(2, "checksum coverage = 0x%04x\n", ntohs(udp_lite->len));
		read += 2;
		packet += 2;
	}
	else if(udp_lite_context->cfp < 0)
	{
		rohc_debugf(0, "cfp not initialized and packet is not one IR packet\n");
		goto error;
	}

	/* check if Coverage Field Inferred (CFI) is uninitialized */
	if(udp_lite_context->cfi < 0)
	{
		rohc_debugf(0, "cfi not initialized and packet is not one IR packet\n");
		goto error;
	}

	/* retrieve the checksum field from the ROHC packet */
	udp_lite->check = GET_NEXT_16_BITS(packet);
	rohc_debugf(2, "checksum = 0x%04x\n", ntohs(udp_lite->check));
	packet += 2;
	read += 2;

	return read;

error:
	return -1;
}


/**
 * @brief Build an uncompressed UDP-Lite header.
 *
 * @param context      The generic decompression context
 * @param active       The UDP-Lite header changes
 * @param dest         The buffer to store the UDP-Lite header (MUST be at least
 *                     of sizeof(struct udphdr) length)
 * @param payload_size The length of the UDP-Lite payload
 * @return             The length of the next header (ie. the UDP-Lite header),
 *                     -1 in case of error
 */
int udp_lite_build_uncompressed_udp(struct d_generic_context *context,
                                    struct d_generic_changes *active,
                                    unsigned char *dest,
                                    int payload_size)
{
	struct d_udp_lite_context *udp_lite_context = context->specific;
	struct udphdr *udp_lite_active = (struct udphdr *) active->next_header;
	struct udphdr *udp_lite = (struct udphdr *) dest;

	/* static + checksum + checksum coverage */
	memcpy(dest, udp_lite_active, sizeof(struct udphdr));
	rohc_debugf(2, "checksum = 0x%04x\n", ntohs(udp_lite->check));

	rohc_debugf(2, "CFP = %d, CFI = %d, cce_packet = %d\n",
	            udp_lite_context->cfp, udp_lite_context->cfi,
	            udp_lite_context->cce_packet);

	/* set checksum coverage if inferred,
	 * already set to the right value otherwise */
	if(!udp_lite_context->cfp &&
	   !udp_lite_context->cce_packet)
	{
		if(udp_lite_context->cfi)
		{
			udp_lite->len = htons(payload_size + sizeof(struct udphdr));
			rohc_debugf(2, "checksum coverage (%d) is inferred\n", udp_lite->len);
		}
		else
			rohc_debugf(2, "checksum coverage (%d) is not inferred\n", udp_lite->len);
	}

	return sizeof(struct udphdr);
}


/**
 * @brief Define the decompression part of the UDP-Lite profile as described
 *        in the RFC 4019.
 */
struct d_profile d_udplite_profile =
{
	ROHC_PROFILE_UDPLITE,        /* profile ID (see 7 in RFC 4019) */
	"1.0",                       /* profile version */
	"UDP-Lite / Decompressor",   /* profile description */
	d_udp_lite_decode,           /* profile handlers */
	d_udp_lite_decode_ir,
	d_udp_lite_create,
	d_udp_lite_destroy,
	udp_lite_detect_ir_size,
	udp_lite_detect_ir_dyn_size,
	udp_lite_get_static_part,
	d_generic_get_sn,
};

