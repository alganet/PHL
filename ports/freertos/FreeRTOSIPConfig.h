/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef FREERTOS_IP_CONFIG_H
#define FREERTOS_IP_CONFIG_H

/* Static IP configuration for QEMU SLIRP networking.
 * SLIRP default network: 10.0.2.0/24, gateway 10.0.2.2, DNS 10.0.2.3 */
#define configIP_ADDR0      10
#define configIP_ADDR1      0
#define configIP_ADDR2      2
#define configIP_ADDR3      15

#define configNET_MASK0     255
#define configNET_MASK1     255
#define configNET_MASK2     255
#define configNET_MASK3     0

#define configGATEWAY_ADDR0 10
#define configGATEWAY_ADDR1 0
#define configGATEWAY_ADDR2 2
#define configGATEWAY_ADDR3 2

#define configDNS_SERVER_ADDR0  10
#define configDNS_SERVER_ADDR1  0
#define configDNS_SERVER_ADDR2  2
#define configDNS_SERVER_ADDR3  3

#define configMAC_ADDR0     0x52
#define configMAC_ADDR1     0x54
#define configMAC_ADDR2     0x00
#define configMAC_ADDR3     0x12
#define configMAC_ADDR4     0x34
#define configMAC_ADDR5     0x56

/* TCP/IP stack configuration */
#define ipconfigUSE_DHCP                         0
#define ipconfigUSE_DNS                          0
#define ipconfigUSE_TCP                          1
#define ipconfigUSE_IPv4                         1
#define ipconfigUSE_IPv6                         0

/* Buffer and window sizes */
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS   30
#define ipconfigNETWORK_MTU                      1200
#define ipconfigTCP_MSS                          ( ipconfigNETWORK_MTU - 40 )
#define ipconfigTCP_RX_BUFFER_LENGTH             ( 4 * ipconfigTCP_MSS )
#define ipconfigTCP_TX_BUFFER_LENGTH             ( 4 * ipconfigTCP_MSS )
#define ipconfigIP_TASK_STACK_SIZE_WORDS          ( configMINIMAL_STACK_SIZE * 5 )
#define ipconfigIP_TASK_PRIORITY                 ( configMAX_PRIORITIES - 2 )

/* Event and socket configuration */
#define ipconfigEVENT_QUEUE_LENGTH               ( ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS + 5 )
#define ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME  pdMS_TO_TICKS( 5000 )
#define ipconfigSOCK_DEFAULT_SEND_BLOCK_TIME     pdMS_TO_TICKS( 5000 )

/* ARP */
#define ipconfigARP_CACHE_ENTRIES                6
#define ipconfigMAX_ARP_RETRANSMISSIONS          5
#define ipconfigMAX_ARP_AGE                      150
#define ipconfigUSE_ARP_REVERSED_LOOKUP          0
#define ipconfigUSE_ARP_REMOVE_ENTRY             0

/* Allow loopback */
#define ipconfigUSE_LOOPBACK                     0

/* Byte order: Cortex-M3 is little-endian */
#define ipconfigBYTE_ORDER                       pdFREERTOS_LITTLE_ENDIAN

/* Buffer allocation: use heap */
#define ipconfigBUFFER_PADDING                   0
#define ipconfigZERO_COPY_TX_DRIVER              0
#define ipconfigZERO_COPY_RX_DRIVER              0

/* Driver is called from IP task, not ISR directly */
#define ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES 0
#define ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM      0
#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM      0
#define ipconfigETHERNET_MINIMUM_PACKET_BYTES       0
#define ipconfigTCP_HANG_PROTECTION                  1
#define ipconfigTCP_HANG_PROTECTION_TIME             30

/* Compatibility with multi-interface API */
#define ipconfigIPv4_BACKWARD_COMPATIBLE             1
#define ipconfigCOMPATIBLE_WITH_SINGLE               1
#define ipconfigUSE_NETWORK_EVENT_HOOK               1
#define ipconfigMULTI_INTERFACE                       0
#define ipconfigSUPPORT_SELECT_FUNCTION              0
#define ipconfigUSE_CALLBACKS                        0

#endif /* FREERTOS_IP_CONFIG_H */
