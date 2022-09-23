/*
 * Excelfore gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of Excelfore-gptp.
 *
 * Excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
#ifndef __IX_TIMESTAMP_H_
#define __IX_TIMESTAMP_H_

// return 0:got TxTS, -1:got data but no TxTS, -2:error in read, 1:no data
// return edtxts->ts64=-1 when this read regular data.
int ix_timestamp_txts(int fd, struct msghdr *msg, int dvi, uint16_t ovip_port,
		      event_data_txts_t *edtxts);

#endif
