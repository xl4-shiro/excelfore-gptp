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
#include "gptpbasetypes.h"

// 14.12 Common Mean Link Delay Service Default Parameter Data Set
typedef CMLDServiceDefaultDS {
	ClockIdentity clockIdentity;
	uint8_t numberLinkPorts;
	uint8_t sdoId;
} CMLDServiceDefaultDS;

// 14.13 Common Mean Link Delay Service Link Port Parameter Data Set
typedef struct CMLDServiceLinkPortDS {
	PortIdentity portIdentity;
	bool cmldsLinkPortEnabled;
	bool isMeasuringDelay;
	bool asCapableAcrossDomains;
	UScaledNs neighborPropDelay;
	UScaledNs neighborPropDelayThresh;
	UScaledNs delayAsymmetry;
	double neighborRateRatio;
	int8_t initialLogPdelayReqInterval;
	int8_t currentLogPdelayReqInterval;
	bool useMgtSettableLogPdelayReqInterval;
	int8_t mgtSettableLogPdelayReqInterval;
	uint16_t allowedLostResponses;
	uint16_t allowedFaults;
	uint8_t versionNumber;
	UInteger48 pdelayTruncatedTimestampsArray[4];
	uint8_t minorVersionNumber;
} CMLDServiceLinkPortDS;



