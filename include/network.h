struct sdfs_packet
{
	uint16_t opcode;
	uint16_t length;
	uint8_t type;
	uint8_t fd;
	uint8_t *data;
};
