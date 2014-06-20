#ifndef PUSH_INTERFACE_H
#define PUSH_INTERFACE_H

/** \file push_service.h
	源自：
http://cr.admin.weibo.com/w/projects/titans/%E6%8E%A5%E5%8F%A3%E6%96%87%E6%A1%A3/#push
*/

/** cond 0 */
#include <stdint.h>
/** endcond */

#define	PUSH_VERSION		1

#define CMD_REQ_EST         0x01  //客户端创建连接
#define CMD_REQ_HB          0x02  //客户端HeartBeat
#define CMD_REQ_TOKEN       0x03  //客户端上传Token

#define CMD_RSP_NOTIFY      0x91  //Notify通知

/** Generic push message */ 
struct push_msg_header_st {
	uint32_t version:8;		/**< Message version */
	uint32_t command:8;		/**< Message command */
	uint32_t padding:16;	/**< Padding */
	uint32_t body_length;	/**< Length of body part */
	uint8_t body[0];		/**< Body, protobuf format, see push_interface.proto for more details */
};

#endif

