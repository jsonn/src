/* $NetBSD: seeq8005reg.h,v 1.2.2.2 2000/11/20 11:40:54 bouyer Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * SEEQ 8005 registers
 *
 * Note that A0 is only used to distinguish halves of 16-bit registers in
 * 8-bit mode.
 */

#define EA_8005_COMMAND		0x0
#define EA_8005_STATUS		0x0
#define EA_8005_CONFIG1		0x2
#define EA_8005_CONFIG2		0x4
#define EA_8005_RX_END		0x6
#define EA_8005_BUFWIN		0x8
#define EA_8005_RX_PTR		0xa
#define EA_8005_TX_PTR		0xc
#define EA_8005_DMA_ADDR	0xe

#define EA_CMD_DMA_INTEN	(1 << 0) /* s/DMA/TEST/ on 80C04? */
#define EA_CMD_RX_INTEN		(1 << 1)
#define EA_CMD_TX_INTEN		(1 << 2)
#define EA_CMD_BW_INTEN		(1 << 3)
#define EA_CMD_DMA_INTACK	(1 << 4)
#define EA_CMD_RX_INTACK	(1 << 5)
#define EA_CMD_TX_INTACK	(1 << 6)
#define EA_CMD_BW_INTACK	(1 << 7)
#define EA_CMD_DMA_ON		(1 << 8)
#define EA_CMD_RX_ON		(1 << 9)
#define EA_CMD_TX_ON		(1 << 10)
#define EA_CMD_DMA_OFF		(1 << 11)
#define EA_CMD_RX_OFF		(1 << 12)
#define EA_CMD_TX_OFF		(1 << 13)
#define EA_CMD_FIFO_READ	(1 << 14)
#define EA_CMD_FIFO_WRITE	(1 << 15)

#define EA_STATUS_DMA_INT	(1 << 4) /* s/DMA/TEST/ on 80C04? */
#define EA_STATUS_RX_INT	(1 << 5)
#define EA_STATUS_TX_INT	(1 << 6)
#define EA_STATUS_RX_ON		(1 << 9)
#define EA_STATUS_TX_ON		(1 << 10)
#define EA_STATUS_FIFO_FULL	(1 << 13)
#define EA_STATUS_FIFO_EMPTY	(1 << 14)
#define EA_STATUS_FIFO_DIR	(1 << 15)
#define EA_STATUS_FIFO_READ	(1 << 15)

#define EA_CFG1_DMA_BURST_CONT	0x00	/* 8005 only? */
#define EA_CFG1_DMA_BURST_800	0x10	/* 8005 only? */
#define EA_CFG1_DMA_BURST_1600	0x20	/* 8005 only? */
#define EA_CFG1_DMA_BURST_3200	0x30	/* 8005 only? */
#define EA_CFG1_DMA_BSIZE_1	0x00	/* 8005 only? */
#define EA_CFG1_DMA_BSIZE_4	0x40	/* 8005 only? */
#define EA_CFG1_DMA_BSIZE_8	0x80	/* 8005 only? */
#define EA_CFG1_DMA_BSIZE_16	0xc0	/* 8005 only? */

#define EA_CFG1_STATION_ADDR0	(1 << 8)	/* 8005 only? */
#define EA_CFG1_STATION_ADDR1	(1 << 9)	/* 8005 only? */
#define EA_CFG1_STATION_ADDR2	(1 << 10)	/* 8005 only? */
#define EA_CFG1_STATION_ADDR3	(1 << 11)	/* 8005 only? */
#define EA_CFG1_STATION_ADDR4	(1 << 12)	/* 8005 only? */
#define EA_CFG1_STATION_ADDR5	(1 << 13)	/* 8005 only? */
#define EA_CFG1_SPECIFIC	((0 << 15) | (0 << 14))
#define EA_CFG1_BROADCAST	((0 << 15) | (1 << 14))
#define EA_CFG1_MULTICAST	((1 << 15) | (0 << 14))
#define EA_CFG1_PROMISCUOUS	((1 << 15) | (1 << 14))

#define EA_CFG2_BYTESWAP	(1 << 0)
#define EA_CFG2_REA_AUTOUPDATE	(1 << 1)	/* 80C04 only */
#define EA_CFG2_RX_TX_DISABLE	(1 << 2)	/* 80C04 only */
#define EA_CFG2_CRC_ERR_ENABLE	(1 << 3)
#define EA_CFG2_DRIB_ERR_ENABLE (1 << 4)
#define EA_CFG2_PASS_SHORT	(1 << 5)
#define EA_CFG2_SLOT_SELECT	(1 << 6)	/* 8005 only? */
#define EA_CFG2_PREAM_SELECT	(1 << 7)
#define EA_CFG2_ADDR_LENGTH	(1 << 8)	/* 8005 only? */
#define EA_CFG2_RX_CRC		(1 << 9)
#define EA_CFG2_NO_TX_CRC	(1 << 10)
#define EA_CFG2_LOOPBACK	(1 << 11)
#define EA_CFG2_OUTPUT		(1 << 12)
#define EA_CFG2_RESET		(1 << 15)

#define EA_BUFCODE_STATION_ADDR0	0x00
#define EA_BUFCODE_STATION_ADDR1	0x01	/* 8005 only? */
#define EA_BUFCODE_STATION_ADDR2	0x02	/* 8005 only? */
#define EA_BUFCODE_STATION_ADDR3	0x03	/* 8005 only? */
#define EA_BUFCODE_STATION_ADDR4	0x04	/* 8005 only? */
#define EA_BUFCODE_STATION_ADDR5	0x05	/* 8005 only? */
#define EA_BUFCODE_ADDRESS_PROM		0x06
#define EA_BUFCODE_TX_EAP		0x07
#define EA_BUFCODE_LOCAL_MEM		0x08
#define EA_BUFCODE_INT_VECTOR		0x09	/* 8005 only? */
#define EA_BUFCODE_TX_COLLS		0x0b	/* 80C04 only */
#define EA_BUFCODE_CONFIG3		0x0c	/* 80C04 only */
#define EA_BUFCODE_PRODUCTID		0x0d	/* 80C04 only */
#define EA_BUFCODE_TESTENABLE		0x0e	/* 80C04 only */
#define EA_BUFCODE_MULTICAST		0x0f	/* 80C04 only */

#define EA_PKTHDR_TX		(1 << 7)
#define EA_PKTHDR_RX		(0 << 7)
#define EA_PKTHDR_CHAIN_CONT	(1 << 6)
#define EA_PKTHDR_DATA_FOLLOWS	(1 << 5)

#define EA_PKTHDR_DONE		(1 << 7)

#define EA_TXHDR_BABBLE		(1 << 0)
#define EA_TXHDR_COLLISION	(1 << 1)
#define EA_TXHDR_COLLISION16	(1 << 2)
#define EA_TXHDR_COLLISIONMASK	(0x78)		/* 80C04 only */
#define EA_TXHDR_ERROR_MASK	(0x07)		/* 80C04 only */

#define EA_TXHDR_BABBLE_INT	(1 << 0)
#define EA_TXHDR_COLLISION_INT	(1 << 1)
#define EA_TXHDR_COLLISION16_INT	(1 << 2)
#define EA_TXHDR_XMIT_SUCCESS_INT	(1 << 3)
#define EB_TXHDR_SQET_TEST_INT	(1 << 3)	/* 80C04 only */
#define EA_TXHDR_ERROR_MASK	(0x07)

#define EA_RXHDR_OVERSIZE	(1 << 0)
#define EA_RXHDR_CRC_ERROR	(1 << 1)
#define EA_RXHDR_DRIBBLE_ERROR	(1 << 2)
#define EA_RXHDR_SHORT_FRAME	(1 << 3)

#define EA_BUFFER_SIZE		0x10000
